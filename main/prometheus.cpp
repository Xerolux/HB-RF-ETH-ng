/*
 *  prometheus.cpp is part of the HB-RF-ETH firmware v2.0
 *
 *  Original work Copyright 2022 Alexander Reinert
 *  https://github.com/alexreinert/HB-RF-ETH
 *
 *  Modified work Copyright 2025 Xerolux
 *  Modernized fork - Updated to ESP-IDF 6.0 and modern toolchains
 *
 *  The HB-RF-ETH firmware is licensed under a
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 *
 *  You should have received a copy of the license along with this
 *  work.  If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#include "prometheus.h"
#include "metrics.h"
#include "monitoring.h"
#include "sysinfo.h"
#include "ethernet.h"
#include "radiomoduledetector.h"
#include "mqtt_handler.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include <atomic>
#include <string.h>
#include <errno.h>
#include <stdio.h>

// Accessors defined in monitoring.cpp (C++ linkage, declared extern here
// following the same pattern as mqtt_handler.cpp lines 64-68).
extern SysInfo *monitoring_get_sysinfo(void);
extern Ethernet *monitoring_get_ethernet(void);
extern RadioModuleDetector *monitoring_get_radiomodule(void);

static const char *TAG = "prometheus";

static std::atomic<bool>         s_running{false};
static std::atomic<bool>         s_restart_requested{false};
static std::atomic<TaskHandle_t> s_task{NULL};
static std::atomic<int>          s_listen_sock{-1};
static std::atomic<int>          s_client_sock{-1};
static std::atomic<uint16_t>     s_port{0};
static char                      s_allowed_hosts[256] = "*";
// Deferred restart settings. Never mutate the active worker's host filter or
// port while its old cycle can still read them.
static uint16_t                  s_pending_port = 0;
static char                      s_pending_allowed_hosts[256] = "*";
static StaticSemaphore_t         s_lifecycle_mutex_buffer;

static SemaphoreHandle_t prometheus_lifecycle_mutex()
{
    // This mutex is required precisely during stop/error cleanup. Reserve its
    // control block statically so transient heap pressure cannot permanently
    // disable the exporter lifecycle until reboot.
    static SemaphoreHandle_t mutex =
        xSemaphoreCreateMutexStatic(&s_lifecycle_mutex_buffer);
    return mutex;
}

static void publish_socket(std::atomic<int> &slot, int sock)
{
    SemaphoreHandle_t lifecycle = prometheus_lifecycle_mutex();
    xSemaphoreTake(lifecycle, portMAX_DELAY);
    slot.store(sock, std::memory_order_release);
    xSemaphoreGive(lifecycle);
}

static void worker_close_socket(std::atomic<int> &slot, int sock)
{
    if (sock < 0) return;
    SemaphoreHandle_t lifecycle = prometheus_lifecycle_mutex();
    xSemaphoreTake(lifecycle, portMAX_DELAY);
    if (slot.load(std::memory_order_acquire) == sock) {
        slot.store(-1, std::memory_order_release);
    }
    shutdown(sock, SHUT_RDWR);
    close(sock);
    xSemaphoreGive(lifecycle);
}

static bool send_all_with_deadline(int sock, const char *data, size_t len,
                                   int64_t deadline_us)
{
    while (len > 0 && s_running.load(std::memory_order_acquire)) {
        if (esp_timer_get_time() >= deadline_us) return false;
        ssize_t written = send(sock, data, len, 0);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) return false;
        data += written;
        len -= static_cast<size_t>(written);
    }
    return len == 0;
}

// Render the static (non-counter) section. Counters are appended afterwards
// by metrics_render_prometheus() to avoid duplicating the table layout here.
static size_t render_static(char *out, size_t cap)
{
    if (cap == 0) return 0;
    size_t len = 0;
#define EMIT(...) do { \
        if (len + 1 < cap) { \
            int n = snprintf(out + len, cap - len, __VA_ARGS__); \
            if (n > 0) len += (size_t)n < (cap - len) ? (size_t)n : (cap - len - 1); \
        } \
    } while (0)

    const esp_app_desc_t *desc = esp_app_get_description();
    EMIT("# HELP hbrfeth_info Firmware / build identification\n");
    EMIT("# TYPE hbrfeth_info gauge\n");
    EMIT("hbrfeth_info{version=\"%s\",project=\"%s\"} 1\n",
         desc ? desc->version : "unknown",
         desc ? desc->project_name : "hb-rf-eth-ng");

    uint64_t uptime_s = (uint64_t)(esp_timer_get_time() / 1000000ULL);
    EMIT("# HELP hbrfeth_uptime_seconds Device uptime since boot\n");
    EMIT("# TYPE hbrfeth_uptime_seconds counter\n");
    EMIT("hbrfeth_uptime_seconds %llu\n", (unsigned long long)uptime_s);

    EMIT("# HELP hbrfeth_heap_free_bytes Free heap in bytes\n");
    EMIT("# TYPE hbrfeth_heap_free_bytes gauge\n");
    EMIT("hbrfeth_heap_free_bytes{type=\"internal\"} %u\n",
         (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    EMIT("hbrfeth_heap_free_bytes{type=\"default\"} %u\n",
         (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT));

    EMIT("# HELP hbrfeth_heap_largest_free_block Largest contiguous free block\n");
    EMIT("# TYPE hbrfeth_heap_largest_free_block gauge\n");
    EMIT("hbrfeth_heap_largest_free_block %u\n",
         (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));

    SysInfo *si = monitoring_get_sysinfo();
    if (si) {
        EMIT("# HELP hbrfeth_cpu_usage_percent CPU usage in percent\n");
        EMIT("# TYPE hbrfeth_cpu_usage_percent gauge\n");
        EMIT("hbrfeth_cpu_usage_percent %.2f\n", si->getCpuUsage());
        EMIT("# HELP hbrfeth_memory_usage_percent Memory usage in percent\n");
        EMIT("# TYPE hbrfeth_memory_usage_percent gauge\n");
        EMIT("hbrfeth_memory_usage_percent %.2f\n", si->getMemoryUsage());
    }

    Ethernet *eth = monitoring_get_ethernet();
    EMIT("# HELP hbrfeth_eth_link_up Ethernet link state (1=up, 0=down)\n");
    EMIT("# TYPE hbrfeth_eth_link_up gauge\n");
    EMIT("hbrfeth_eth_link_up %d\n", eth ? (eth->isConnected() ? 1 : 0) : 0);

    EMIT("# HELP hbrfeth_mqtt_connected MQTT broker connection state (1=connected)\n");
    EMIT("# TYPE hbrfeth_mqtt_connected gauge\n");
    EMIT("hbrfeth_mqtt_connected %d\n", mqtt_handler_is_connected() ? 1 : 0);

    RadioModuleDetector *rmd = monitoring_get_radiomodule();
    if (rmd) {
        const char *type = "none";
        radio_module_type_t t = rmd->getRadioModuleType();
        if (t == RADIO_MODULE_HM_MOD_RPI_PCB)      type = "HM-MOD-RPI-PCB";
        else if (t == RADIO_MODULE_RPI_RF_MOD)     type = "RPI-RF-MOD";
        EMIT("# HELP hbrfeth_rf_module HomeMatic radio module type (1=present)\n");
        EMIT("# TYPE hbrfeth_rf_module gauge\n");
        EMIT("hbrfeth_rf_module{type=\"%s\"} %d\n", type, t != RADIO_MODULE_NONE ? 1 : 0);
    }
#undef EMIT
    return len;
}

static bool client_allowed(const char *client_ip)
{
    if (s_allowed_hosts[0] == '\0' || strcmp(s_allowed_hosts, "*") == 0) {
        return true;
    }
    char hosts_copy[sizeof(s_allowed_hosts)];
    strncpy(hosts_copy, s_allowed_hosts, sizeof(hosts_copy) - 1);
    hosts_copy[sizeof(hosts_copy) - 1] = '\0';
    char *saveptr = NULL;
    for (char *token = strtok_r(hosts_copy, ",", &saveptr); token; token = strtok_r(NULL, ",", &saveptr)) {
        while (*token == ' ') token++;
        size_t l = strlen(token);
        while (l > 0 && token[l - 1] == ' ') token[--l] = '\0';
        if (l > 0 && strcmp(token, client_ip) == 0) return true;
    }
    return false;
}

static void prometheus_worker_cycle()
{
    // Allocate the 8 KiB response workspace lazily once and retain it for the
    // worker lifetime. Repeated malloc/free on every normal 15-second scrape
    // needlessly fragments the WROOM-32 heap over long uptimes. If the first
    // allocation hits transient pressure, later scrapes may retry.
    static const size_t RESP_CAP = 8 * 1024;
    char *body = NULL;
    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int opt = 1;
    struct sockaddr_in addr = {};
    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    const uint16_t listen_port = s_port.load(std::memory_order_acquire);
    if (listen_sock >= 0) publish_socket(s_listen_sock, listen_sock);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "socket() failed");
        goto task_done;
    }

    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(listen_port);

    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "bind(%u) failed: %s", listen_port, strerror(errno));
        goto task_done;
    }
    if (listen(listen_sock, 4) < 0) {
        ESP_LOGE(TAG, "listen() failed: %s", strerror(errno));
        goto task_done;
    }

    setsockopt(listen_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    ESP_LOGI(TAG, "Prometheus exporter listening on :%u", listen_port);

    while (s_running.load()) {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        int csock = accept(listen_sock, (struct sockaddr *)&caddr, &clen);
        if (csock < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue;
            }
            if (s_running.load(std::memory_order_acquire)) {
                ESP_LOGW(TAG, "accept() errno=%d; reopening listener", errno);
            }
            break;
        }

        publish_socket(s_client_sock, csock);
        struct timeval client_timeout = { .tv_sec = 2, .tv_usec = 0 };
        setsockopt(csock, SOL_SOCKET, SO_RCVTIMEO,
                   &client_timeout, sizeof(client_timeout));
        setsockopt(csock, SOL_SOCKET, SO_SNDTIMEO,
                   &client_timeout, sizeof(client_timeout));
        if (!s_running.load(std::memory_order_acquire)) {
            worker_close_socket(s_client_sock, csock);
            break;
        }

        char ip[16];
        inet_ntoa_r(caddr.sin_addr, ip, sizeof(ip));
        if (!client_allowed(ip)) {
            ESP_LOGW(TAG, "client %s not allowed", ip);
            worker_close_socket(s_client_sock, csock);
            continue;
        }

        // Build response on heap so a very large counter table cannot blow
        // the task stack. 8 KB comfortably covers all current metrics plus
        // all registered counters.
        if (!body) body = (char *)malloc(RESP_CAP);
        if (!body) {
            static const char *oom = "HTTP/1.0 503 Service Unavailable\r\nContent-Length: 0\r\n\r\n";
            const int64_t deadline = esp_timer_get_time() + 2000000;
            (void)send_all_with_deadline(csock, oom, strlen(oom), deadline);
            worker_close_socket(s_client_sock, csock);
            continue;
        }
        size_t blen = render_static(body, RESP_CAP);
        blen = metrics_render_prometheus(body, RESP_CAP, blen);

        char header[128];
        int hlen = snprintf(header, sizeof(header),
                            "HTTP/1.0 200 OK\r\n"
                            "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"
                            "Content-Length: %zu\r\n\r\n", blen);
        const int64_t deadline = esp_timer_get_time() + 2000000;
        if (hlen > 0) {
            (void)(send_all_with_deadline(csock, header,
                                         static_cast<size_t>(hlen), deadline) &&
                   send_all_with_deadline(csock, body, blen, deadline));
        }

        worker_close_socket(s_client_sock, csock);
    }

task_done:
    if (s_client_sock.load(std::memory_order_acquire) >= 0) {
        worker_close_socket(s_client_sock,
                            s_client_sock.load(std::memory_order_acquire));
    }
    worker_close_socket(s_listen_sock, listen_sock);
    free(body);
    ESP_LOGI(TAG, "Prometheus exporter stopped");
}

static void prometheus_task(void *)
{
    for (;;) {
        prometheus_worker_cycle();
        SemaphoreHandle_t lifecycle = prometheus_lifecycle_mutex();
        xSemaphoreTake(lifecycle, portMAX_DELAY);
        if (s_running.load(std::memory_order_acquire)) {
            // A transient socket/bind/listen/allocation failure must not turn
            // an enabled exporter permanently offline. Keep task ownership
            // and retry with backoff; stop() flips s_running and wakes I/O.
            xSemaphoreGive(lifecycle);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        if (s_restart_requested.exchange(false, std::memory_order_acq_rel)) {
            // A stop timed out and rollback requested the exporter again while
            // the old sockets were still unwinding. Reuse this task only after
            // the worker-owned close path has completed.
            s_listen_sock.store(-1, std::memory_order_release);
            s_client_sock.store(-1, std::memory_order_release);
            s_port.store(s_pending_port, std::memory_order_release);
            strncpy(s_allowed_hosts, s_pending_allowed_hosts,
                    sizeof(s_allowed_hosts) - 1);
            s_allowed_hosts[sizeof(s_allowed_hosts) - 1] = '\0';
            s_running.store(true, std::memory_order_release);
            xSemaphoreGive(lifecycle);
            ESP_LOGI(TAG, "Prometheus deferred restart completed");
            continue;
        }
        s_running.store(false, std::memory_order_release);
        s_task.store(NULL, std::memory_order_release);
        xSemaphoreGive(lifecycle);
        break;
    }
    vTaskDelete(NULL);
}

esp_err_t prometheus_start(const prometheus_config_t *config)
{
    if (!config || !config->enabled) {
        return ESP_OK;
    }
    SemaphoreHandle_t lifecycle = prometheus_lifecycle_mutex();
    if (!lifecycle) return ESP_ERR_NO_MEM;
    xSemaphoreTake(lifecycle, portMAX_DELAY);

    TaskHandle_t existing = s_task.load(std::memory_order_acquire);
    if (existing != NULL) {
        const bool running = s_running.load(std::memory_order_acquire);
        if (!running) {
            s_pending_port = config->port ? config->port : 9100;
            strncpy(s_pending_allowed_hosts, config->allowed_hosts,
                    sizeof(s_pending_allowed_hosts) - 1);
            s_pending_allowed_hosts[sizeof(s_pending_allowed_hosts) - 1] = '\0';
            s_restart_requested.store(true, std::memory_order_release);
        }
        xSemaphoreGive(lifecycle);
        if (running) {
            ESP_LOGW(TAG, "already running");
            return ESP_OK;
        }
        ESP_LOGW(TAG, "exporter restart queued until cleanup completes");
        return ESP_OK;
    }

    s_port.store(config->port ? config->port : 9100,
                 std::memory_order_release);
    strncpy(s_allowed_hosts, config->allowed_hosts, sizeof(s_allowed_hosts) - 1);
    s_allowed_hosts[sizeof(s_allowed_hosts) - 1] = '\0';
    s_listen_sock.store(-1, std::memory_order_release);
    s_client_sock.store(-1, std::memory_order_release);
    s_restart_requested.store(false, std::memory_order_release);
    s_running.store(true, std::memory_order_release);

    TaskHandle_t h = NULL;
    // 6 KB stack: response build + snprintf + sockaddr ops.
    if (xTaskCreate(prometheus_task, "prom_exp", 6144, NULL, 5, &h) != pdPASS) {
        ESP_LOGE(TAG, "task create failed");
        s_running.store(false, std::memory_order_release);
        xSemaphoreGive(lifecycle);
        return ESP_FAIL;
    }
    s_task.store(h, std::memory_order_release);
    xSemaphoreGive(lifecycle);
    return ESP_OK;
}

esp_err_t prometheus_stop(void)
{
    SemaphoreHandle_t lifecycle = prometheus_lifecycle_mutex();
    if (!lifecycle) return ESP_ERR_NO_MEM;
    xSemaphoreTake(lifecycle, portMAX_DELAY);
    TaskHandle_t task = s_task.load(std::memory_order_acquire);
    // A deliberate stop supersedes any restore queued by a previous caller.
    s_restart_requested.store(false, std::memory_order_release);
    if (!task) {
        s_running.store(false, std::memory_order_release);
        xSemaphoreGive(lifecycle);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "stopping");
    s_running.store(false, std::memory_order_release);

    // Only wake sockets here. The worker remains the sole close owner and
    // clears the published fd before close(), preventing fd-reuse races.
    int client = s_client_sock.load(std::memory_order_acquire);
    if (client >= 0) shutdown(client, SHUT_RDWR);
    int listener = s_listen_sock.load(std::memory_order_acquire);
    if (listener >= 0) shutdown(listener, SHUT_RDWR);
    xSemaphoreGive(lifecycle);

    for (int i = 0; i < 50 &&
         s_task.load(std::memory_order_acquire) != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (s_task.load(std::memory_order_acquire) != NULL) {
        ESP_LOGW(TAG, "exporter still stopping after timeout");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

bool prometheus_is_running(void) { return s_running.load() && s_listen_sock.load() >= 0; }
int  prometheus_listen_port(void) {
    return (int)s_port.load(std::memory_order_acquire);
}
