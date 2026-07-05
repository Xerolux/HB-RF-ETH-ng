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
static std::atomic<TaskHandle_t> s_task{NULL};
static std::atomic<int>          s_listen_sock{-1};
static uint16_t                  s_port = 0;
static char                      s_allowed_hosts[256] = "*";

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

static void prometheus_task(void *pv)
{
    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    s_listen_sock.store(listen_sock);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "socket() failed");
        s_running.store(false);
        s_task.store(NULL);
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(s_port);

    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "bind(%u) failed: %s", s_port, strerror(errno));
        close(listen_sock);
        s_listen_sock.store(-1);
        s_running.store(false);
        s_task.store(NULL);
        vTaskDelete(NULL);
        return;
    }
    if (listen(listen_sock, 4) < 0) {
        ESP_LOGE(TAG, "listen() failed: %s", strerror(errno));
        close(listen_sock);
        s_listen_sock.store(-1);
        s_running.store(false);
        s_task.store(NULL);
        vTaskDelete(NULL);
        return;
    }

    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(listen_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    ESP_LOGI(TAG, "Prometheus exporter listening on :%u", s_port);

    while (s_running.load()) {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        int csock = accept(listen_sock, (struct sockaddr *)&caddr, &clen);
        if (csock < 0) {
            if (s_running.load() && errno != EAGAIN && errno != EWOULDBLOCK) {
                ESP_LOGW(TAG, "accept() errno=%d", errno);
            }
            continue;
        }

        char ip[16];
        inet_ntoa_r(caddr.sin_addr, ip, sizeof(ip));
        if (!client_allowed(ip)) {
            ESP_LOGW(TAG, "client %s not allowed", ip);
            close(csock);
            continue;
        }

        // Build response on heap so a very large counter table cannot blow
        // the task stack. 8 KB comfortably covers all current metrics plus
        // all registered counters.
        static const size_t RESP_CAP = 8 * 1024;
        char *body = (char *)malloc(RESP_CAP);
        if (!body) {
            static const char *oom = "HTTP/1.0 503 Service Unavailable\r\nContent-Length: 0\r\n\r\n";
            send(csock, oom, strlen(oom), 0);
            close(csock);
            continue;
        }
        size_t blen = render_static(body, RESP_CAP);
        blen = metrics_render_prometheus(body, RESP_CAP, blen);

        char header[128];
        int hlen = snprintf(header, sizeof(header),
                            "HTTP/1.0 200 OK\r\n"
                            "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"
                            "Content-Length: %zu\r\n\r\n", blen);
        send(csock, header, (size_t)hlen, 0);

        size_t sent = 0;
        while (sent < blen) {
            ssize_t w = send(csock, body + sent, blen - sent, 0);
            if (w <= 0) {
                if (errno == EINTR) continue;
                break;
            }
            sent += (size_t)w;
        }

        free(body);
        close(csock);
    }

    int sock = s_listen_sock.exchange(-1);
    if (sock >= 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
    }
    ESP_LOGI(TAG, "Prometheus exporter stopped");
    s_running.store(false);
    s_task.store(NULL);
    vTaskDelete(NULL);
}

esp_err_t prometheus_start(const prometheus_config_t *config)
{
    if (!config || !config->enabled) {
        return ESP_OK;
    }
    if (s_running.load()) {
        ESP_LOGW(TAG, "already running");
        return ESP_OK;
    }

    s_port = config->port ? config->port : 9100;
    strncpy(s_allowed_hosts, config->allowed_hosts, sizeof(s_allowed_hosts) - 1);
    s_allowed_hosts[sizeof(s_allowed_hosts) - 1] = '\0';
    s_running.store(true);

    TaskHandle_t h = NULL;
    // 6 KB stack: response build + snprintf + sockaddr ops.
    if (xTaskCreate(prometheus_task, "prom_exp", 6144, NULL, 5, &h) != pdPASS) {
        ESP_LOGE(TAG, "task create failed");
        s_running.store(false);
        return ESP_FAIL;
    }
    s_task.store(h);
    return ESP_OK;
}

esp_err_t prometheus_stop(void)
{
    if (!s_running.load()) return ESP_OK;
    ESP_LOGI(TAG, "stopping");
    s_running.store(false);

    int sock = s_listen_sock.exchange(-1);
    if (sock >= 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
    }
    for (int i = 0; i < 25 && s_task.load() != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    TaskHandle_t h = s_task.load();
    if (h) {
        ESP_LOGW(TAG, "task did not exit cleanly, force-deleting");
        s_task.store(NULL);
        vTaskDelete(h);
    }
    return ESP_OK;
}

bool prometheus_is_running(void) { return s_running.load() && s_listen_sock.load() >= 0; }
int  prometheus_listen_port(void) { return (int)s_port; }
