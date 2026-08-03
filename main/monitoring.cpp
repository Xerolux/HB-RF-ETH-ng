/*
 *  monitoring.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "monitoring.h"
#include "mqtt_handler.h"
#include "prometheus.h"
#include "syslog.h"
#include "events.h"
#include "settings.h"
#include "log_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "esp_app_format.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <atomic>
#include "ethernet.h"
#include "radiomoduledetector.h"
#include "systemclock.h"
#include "reset_info.h"
#include "crash_blackbox.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "sysinfo.h"
#include "nvs_storage_lock.h"

static const char *TAG = "MONITORING";
SemaphoreHandle_t g_net_fetch_mutex = NULL;
static StaticSemaphore_t net_fetch_mutex_buffer;

// True while a manually uploaded firmware image is being written. See
// net_fetch_set_ota_active().
static std::atomic<bool> g_ota_active{false};
// True from the beginning of the manual OTA service-stop transaction until a
// failure rollback completes. Network-ready callbacks must not resurrect MQTT
// in that interval.
static std::atomic<bool> g_monitoring_ota_paused{false};

void net_fetch_set_ota_active(bool active)
{
    g_ota_active.store(active);
}

bool net_fetch_ota_active(void)
{
    return g_ota_active.load();
}

static monitoring_config_t current_config = {};
static SemaphoreHandle_t config_mutex = NULL;
static std::atomic<bool> checkmk_running{false};
static std::atomic<bool> checkmk_restart_requested{false};
static std::atomic<bool> mqtt_start_deferred{false};
static std::atomic<bool> mqtt_retry_task_running{false};
static TimerHandle_t mqtt_retry_guard_timer = NULL;
static StaticTimer_t mqtt_retry_guard_timer_buffer;
static std::atomic<TaskHandle_t> checkmk_task_handle{NULL};
static std::atomic<int> checkmk_listen_sock{-1};
static checkmk_config_t checkmk_config_snapshot = {};
static StaticSemaphore_t checkmk_lifecycle_mutex_buffer;
static esp_event_handler_instance_t mqtt_ip_event_instance = NULL;
// Currently-connected client socket, or -1. stop() only calls shutdown() to
// wake I/O; the worker remains the sole close owner so descriptor reuse cannot
// race an external cleanup path.
static std::atomic<int> checkmk_client_sock{-1};

static SemaphoreHandle_t checkmk_lifecycle_mutex()
{
    static SemaphoreHandle_t mutex =
        xSemaphoreCreateMutexStatic(&checkmk_lifecycle_mutex_buffer);
    return mutex;
}

static void checkmk_publish_socket(std::atomic<int> &slot, int sock)
{
    SemaphoreHandle_t lifecycle = checkmk_lifecycle_mutex();
    xSemaphoreTake(lifecycle, portMAX_DELAY);
    slot.store(sock, std::memory_order_release);
    xSemaphoreGive(lifecycle);
}

static void checkmk_worker_close_socket(std::atomic<int> &slot, int sock)
{
    if (sock < 0) return;
    SemaphoreHandle_t lifecycle = checkmk_lifecycle_mutex();
    xSemaphoreTake(lifecycle, portMAX_DELAY);
    if (slot.load(std::memory_order_acquire) == sock) {
        slot.store(-1, std::memory_order_release);
    }
    shutdown(sock, SHUT_RDWR);
    close(sock);
    xSemaphoreGive(lifecycle);
}

enum OtaPausedService : uint32_t {
    OTA_PAUSED_CHECKMK    = 1u << 0,
    OTA_PAUSED_PROMETHEUS = 1u << 1,
    OTA_PAUSED_SYSLOG     = 1u << 2,
    OTA_PAUSED_NOTIFY     = 1u << 3,
    OTA_PAUSED_MQTT       = 1u << 4,
};

// NVS keys
#define NVS_NAMESPACE "monitoring"
#define NVS_TXN_NAMESPACE "monitoring_txn"
#define NVS_TXN_PENDING "pending"
#define NVS_CHECKMK_ENABLED "cmk_en"
#define NVS_CHECKMK_PORT "cmk_port"
#define NVS_CHECKMK_HOSTS "cmk_hosts"
#define NVS_MQTT_ENABLED "mqtt_en"
#define NVS_MQTT_SERVER "mqtt_srv"
#define NVS_MQTT_PORT "mqtt_port"
#define NVS_MQTT_USER "mqtt_usr"
#define NVS_MQTT_PASS "mqtt_pw"
#define NVS_MQTT_PREFIX "mqtt_pfx"
#define NVS_MQTT_HA_ENABLED "mqtt_ha_en"
#define NVS_MQTT_HA_PREFIX "mqtt_ha_pfx"
#define NVS_MQTT_TLS_EN     "mqtt_tls_en"
#define NVS_MQTT_TLS_SKIP   "mqtt_tls_skip"
#define NVS_MQTT_TLS_CA     "mqtt_tls_ca"
#define NVS_MQTT_TLS_CRT    "mqtt_tls_crt"
#define NVS_MQTT_TLS_KEY    "mqtt_tls_key"
#define NVS_MQTT_CMD_EN     "mqtt_cmd_en"   // command topic enabled
#define NVS_MQTT_CMD_TOK    "mqtt_cmd_tok"  // optional shared-secret

// Prometheus (Phase A)
#define NVS_PROM_ENABLED    "prom_en"
#define NVS_PROM_PORT       "prom_port"
#define NVS_PROM_HOSTS      "prom_hosts"

// Syslog (Phase B)
#define NVS_SYSLOG_ENABLED  "syslog_en"
#define NVS_SYSLOG_SERVER   "syslog_srv"
#define NVS_SYSLOG_PORT     "syslog_port"
#define NVS_SYSLOG_XPORT    "syslog_xp"
#define NVS_SYSLOG_SEV      "syslog_sev"
#define NVS_SYSLOG_HOST     "syslog_host"

// Notifications (Phase C/D)
#define NVS_NOTIFY_ENABLED  "notify_en"
#define NVS_NOTIFY_CHANS    "notify_ch"
#define NVS_NOTIFY_WHOOK    "notify_wh"
#define NVS_NOTIFY_WSECRET  "notify_ws"
#define NVS_NOTIFY_TGTOKEN  "notify_tg_t"
#define NVS_NOTIFY_TGCHAT   "notify_tg_c"
#define NVS_NOTIFY_SMTPSRV  "notify_smtp_s"
#define NVS_NOTIFY_SMTPPORT "notify_smtp_p"
#define NVS_NOTIFY_SMTPTLS  "notify_smtp_tls"
#define NVS_NOTIFY_SMTPUSER "notify_smtp_u"
#define NVS_NOTIFY_SMTPPW   "notify_smtp_pw"
#define NVS_NOTIFY_SMTPFROM "notify_smtp_f"
#define NVS_NOTIFY_SMTPTO   "notify_smtp_to"
#define NVS_NOTIFY_COOLDOWN "notify_cd"

// Global pointers
static SysInfo* g_sysInfo = NULL;
static Ethernet* g_ethernet = NULL;
static RadioModuleDetector* g_radioModuleDetector = NULL;
static SystemClock* g_systemClock = NULL;
static Settings* g_settings = NULL;

// One atomic state serializes monitoring configuration writes with every
// exclusive flash/NVS/restart operation. Two independent boolean gates cannot
// safely implement this exclusion on a dual-core CPU: both cores could publish
// their own flag and still observe the other flag's previous value.
enum class OperationState : uint32_t {
    IDLE = 0,
    MONITORING_UPDATE = 1,
    EXCLUSIVE = 2,
};

static std::atomic<uint32_t> g_operation_state{
    static_cast<uint32_t>(OperationState::IDLE)};

static bool operation_try_begin(OperationState desired)
{
    uint32_t expected = static_cast<uint32_t>(OperationState::IDLE);
    return g_operation_state.compare_exchange_strong(
        expected, static_cast<uint32_t>(desired),
        std::memory_order_acq_rel, std::memory_order_acquire);
}

static void operation_finish(OperationState owned)
{
    uint32_t expected = static_cast<uint32_t>(owned);
    if (!g_operation_state.compare_exchange_strong(
            expected, static_cast<uint32_t>(OperationState::IDLE),
            std::memory_order_release, std::memory_order_relaxed)) {
        ESP_LOGE(TAG, "Operation gate release mismatch (expected=%u, actual=%u)",
                 static_cast<unsigned>(owned),
                 static_cast<unsigned>(expected));
    }
}

static bool operation_active(OperationState state)
{
    return g_operation_state.load(std::memory_order_acquire) ==
           static_cast<uint32_t>(state);
}

bool ota_operation_try_begin(void)
{
    return operation_try_begin(OperationState::EXCLUSIVE);
}

void ota_operation_finish(void)
{
    operation_finish(OperationState::EXCLUSIVE);
}

bool ota_operation_active(void)
{
    return operation_active(OperationState::EXCLUSIVE);
}

bool monitoring_config_update_active(void)
{
    return operation_active(OperationState::MONITORING_UPDATE);
}

bool monitoring_ota_pause_active(void)
{
    return g_monitoring_ota_paused.load(std::memory_order_acquire);
}

// Provider accessors for mqtt_handler.cpp
Ethernet* monitoring_get_ethernet(void) { return g_ethernet; }
RadioModuleDetector* monitoring_get_radiomodule(void) { return g_radioModuleDetector; }
SystemClock* monitoring_get_systemclock(void) { return g_systemClock; }

void monitoring_set_settings(Settings* settings) { g_settings = settings; }
Settings* monitoring_get_settings(void) { return g_settings; }

void monitoring_set_providers(Ethernet* ethernet,
                              RadioModuleDetector* radioModuleDetector,
                              SystemClock* systemClock)
{
    g_ethernet = ethernet;
    g_radioModuleDetector = radioModuleDetector;
    g_systemClock = systemClock;
}

// Get firmware version from app descriptor
static const char* get_firmware_version(void)
{
    const esp_app_desc_t *app_desc = esp_app_get_description();
    return app_desc->version;
}

// Get system uptime
static void get_system_uptime(uint32_t *days, uint32_t *hours, uint32_t *minutes)
{
    uint64_t uptime_ms = esp_timer_get_time() / 1000;
    uint32_t uptime_sec = uptime_ms / 1000;
    *days = uptime_sec / 86400;
    uptime_sec %= 86400;
    *hours = uptime_sec / 3600;
    uptime_sec %= 3600;
    *minutes = uptime_sec / 60;
}

// Helper to access global pointers from other files (like mqtt_handler)
SysInfo* monitoring_get_sysinfo(void) {
    return g_sysInfo;
}

static void checkmk_agent_cycle(const checkmk_config_t &config)
{
    struct sockaddr_in server_addr;

    ESP_LOGI(TAG, "CheckMK Agent starting on port %d", config.port);

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock >= 0) checkmk_publish_socket(checkmk_listen_sock, listen_sock);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket");
        return;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(config.port);

    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Socket bind failed");
        checkmk_worker_close_socket(checkmk_listen_sock, listen_sock);
        return;
    }

    if (listen(listen_sock, 5) < 0) {
        ESP_LOGE(TAG, "Socket listen failed");
        checkmk_worker_close_socket(checkmk_listen_sock, listen_sock);
        return;
    }

    // Periodic timeout so the task can check checkmk_running even if no
    // client connects and even if shutdown() does not unblock accept() on
    // this lwIP version.
    struct timeval accept_tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(listen_sock, SOL_SOCKET, SO_RCVTIMEO, &accept_tv, sizeof(accept_tv));

    ESP_LOGI(TAG, "CheckMK Agent listening on port %d", config.port);

    while (checkmk_running.load()) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            // A receive timeout is expected and only re-checks the stop flag.
            // Any other error invalidates this socket cycle.  Returning lets
            // the outer worker close/recreate it after a bounded delay instead
            // of spinning at priority 5 and starving the watchdog idle task.
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue;
            }
            if (checkmk_running.load(std::memory_order_acquire)) {
                ESP_LOGE(TAG, "Accept failed: errno %d; reopening listener", errno);
            }
            break;
        }

        checkmk_publish_socket(checkmk_client_sock, client_sock);
        struct timeval client_tv = { .tv_sec = 2, .tv_usec = 0 };
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO,
                   &client_tv, sizeof(client_tv));
        setsockopt(client_sock, SOL_SOCKET, SO_SNDTIMEO,
                   &client_tv, sizeof(client_tv));
        if (!checkmk_running.load(std::memory_order_acquire)) {
            checkmk_worker_close_socket(checkmk_client_sock, client_sock);
            break;
        }

        char client_ip[16];
        inet_ntoa_r(client_addr.sin_addr, client_ip, sizeof(client_ip));
        ESP_LOGI(TAG, "CheckMK client connected from %s", client_ip);

        // Check if client IP is allowed (exact match per comma-separated entry)
        bool allowed = false;
        if (strlen(config.allowed_hosts) == 0 || strcmp(config.allowed_hosts, "*") == 0) {
            allowed = true;
        } else {
            // Parse comma-separated list and match each entry exactly
            char hosts_copy[sizeof(config.allowed_hosts)];
            strncpy(hosts_copy, config.allowed_hosts, sizeof(hosts_copy) - 1);
            hosts_copy[sizeof(hosts_copy) - 1] = '\0';
            char *saveptr = NULL;
            char *token = strtok_r(hosts_copy, ",", &saveptr);
            while (token != NULL) {
                // Trim leading/trailing spaces
                while (*token == ' ') token++;
                size_t token_len = strlen(token);
                while (token_len > 0 && token[token_len - 1] == ' ') {
                    token[--token_len] = '\0';
                }
                if (token_len > 0 && strcmp(token, client_ip) == 0) {
                    allowed = true;
                    break;
                }
                token = strtok_r(NULL, ",", &saveptr);
            }
        }

        if (!allowed) {
            ESP_LOGW(TAG, "Client %s not in allowed hosts list", client_ip);
            checkmk_worker_close_socket(checkmk_client_sock, client_sock);
            continue;
        }

        // Send CheckMK agent output
        char output[2048];
        size_t len = 0;
        int ret;

        #define APPEND_CHECKMK(...) \
            do { \
                if (len < sizeof(output) - 1) { \
                    ret = snprintf(output + len, sizeof(output) - len, __VA_ARGS__); \
                    if (ret > 0) { \
                        if ((size_t)ret >= sizeof(output) - len) { \
                            len = sizeof(output) - 1; \
                        } else { \
                            len += ret; \
                        } \
                    } \
                } \
            } while(0)

        // Version section
        APPEND_CHECKMK("<<<check_mk>>>\n");
        APPEND_CHECKMK("Version: HB-RF-ETH-%s\n", get_firmware_version());
        APPEND_CHECKMK("AgentOS: ESP-IDF\n");

        // Uptime section
        uint32_t days, hours, minutes;
        get_system_uptime(&days, &hours, &minutes);
        APPEND_CHECKMK("<<<uptime>>>\n");
        APPEND_CHECKMK("%lu\n", (unsigned long)(days * 86400 + hours * 3600 + minutes * 60));

        // Memory section
        APPEND_CHECKMK("<<<mem>>>\n");
        APPEND_CHECKMK("MemTotal: %lu kB\n",
                       (unsigned long)(heap_caps_get_total_size(MALLOC_CAP_DEFAULT) / 1024));
        APPEND_CHECKMK("MemFree: %lu kB\n",
                       (unsigned long)(heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024));

        // CPU section
        APPEND_CHECKMK("<<<cpu>>>\n");
        APPEND_CHECKMK("esp32 0 0 0\n");

        #undef APPEND_CHECKMK

        // send() may legally write only part of the buffer. Finish the bounded
        // 2 KB response or fail explicitly instead of silently truncating it.
        size_t sent = 0;
        while (sent < len) {
            ssize_t written = send(client_sock, output + sent, len - sent, 0);
            if (written > 0) {
                sent += (size_t)written;
            } else if (written < 0 && errno == EINTR) {
                continue;
            } else {
                ESP_LOGW(TAG, "CheckMK response send failed after %u/%u bytes (errno %d)",
                         (unsigned)sent, (unsigned)len, errno);
                break;
            }
        }

        checkmk_worker_close_socket(checkmk_client_sock, client_sock);
        ESP_LOGI(TAG, "CheckMK client disconnected");
    }

    if (checkmk_client_sock.load(std::memory_order_acquire) >= 0) {
        checkmk_worker_close_socket(
            checkmk_client_sock,
            checkmk_client_sock.load(std::memory_order_acquire));
    }
    checkmk_worker_close_socket(checkmk_listen_sock, listen_sock);
    ESP_LOGI(TAG, "CheckMK Agent stopped");
}

// CheckMK Agent Task
static void checkmk_agent_task(void *)
{
    for (;;) {
        checkmk_config_t config;
        SemaphoreHandle_t lifecycle = checkmk_lifecycle_mutex();
        xSemaphoreTake(lifecycle, portMAX_DELAY);
        memcpy(&config, &checkmk_config_snapshot, sizeof(config));
        xSemaphoreGive(lifecycle);

        checkmk_agent_cycle(config);

        xSemaphoreTake(lifecycle, portMAX_DELAY);
        if (checkmk_running.load(std::memory_order_acquire)) {
            // socket/bind/listen can fail transiently during link churn. An
            // enabled service remains owned by this task and retries instead
            // of silently disappearing until the next reboot.
            xSemaphoreGive(lifecycle);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        if (checkmk_restart_requested.exchange(
                false, std::memory_order_acq_rel)) {
            checkmk_listen_sock.store(-1, std::memory_order_release);
            checkmk_client_sock.store(-1, std::memory_order_release);
            checkmk_running.store(true, std::memory_order_release);
            xSemaphoreGive(lifecycle);
            ESP_LOGI(TAG, "CheckMK deferred restart completed");
            continue;
        }
        checkmk_running.store(false, std::memory_order_release);
        checkmk_task_handle.store(NULL, std::memory_order_release);
        xSemaphoreGive(lifecycle);
        break;
    }
    vTaskDelete(NULL);
}

// CheckMK Functions
esp_err_t checkmk_start(const checkmk_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;
    if (!config->enabled) return ESP_OK;
    SemaphoreHandle_t lifecycle = checkmk_lifecycle_mutex();
    if (!lifecycle) return ESP_ERR_NO_MEM;
    xSemaphoreTake(lifecycle, portMAX_DELAY);

    TaskHandle_t existing =
        checkmk_task_handle.load(std::memory_order_acquire);
    if (existing != NULL) {
        if (!checkmk_running.load(std::memory_order_acquire)) {
            memcpy(&checkmk_config_snapshot, config,
                   sizeof(checkmk_config_snapshot));
            checkmk_config_snapshot.allowed_hosts[
                sizeof(checkmk_config_snapshot.allowed_hosts) - 1] = '\0';
            checkmk_restart_requested.store(true,
                                             std::memory_order_release);
            xSemaphoreGive(lifecycle);
            ESP_LOGW(TAG, "CheckMK restart queued until cleanup completes");
            return ESP_OK;
        }
        xSemaphoreGive(lifecycle);
        ESP_LOGW(TAG, "CheckMK agent already running");
        return ESP_OK;
    }

    memcpy(&checkmk_config_snapshot, config,
           sizeof(checkmk_config_snapshot));
    checkmk_config_snapshot.allowed_hosts[
        sizeof(checkmk_config_snapshot.allowed_hosts) - 1] = '\0';
    checkmk_restart_requested.store(false, std::memory_order_release);
    checkmk_listen_sock.store(-1, std::memory_order_release);
    checkmk_client_sock.store(-1, std::memory_order_release);
    checkmk_running.store(true, std::memory_order_release);

    // 8192 bytes: large output buffer (2048) + sockaddr/IP string operations
    TaskHandle_t cmk_handle = NULL;
    BaseType_t ret = xTaskCreate(checkmk_agent_task, "checkmk_agent", 8192,
                                  NULL, 5, &cmk_handle);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create CheckMK agent task");
        checkmk_running.store(false, std::memory_order_release);
        xSemaphoreGive(lifecycle);
        return ESP_FAIL;
    }

    checkmk_task_handle.store(cmk_handle, std::memory_order_release);
    xSemaphoreGive(lifecycle);
    return ESP_OK;
}

esp_err_t checkmk_stop(void)
{
    SemaphoreHandle_t lifecycle = checkmk_lifecycle_mutex();
    if (!lifecycle) return ESP_ERR_NO_MEM;
    xSemaphoreTake(lifecycle, portMAX_DELAY);
    TaskHandle_t task =
        checkmk_task_handle.load(std::memory_order_acquire);
    checkmk_restart_requested.store(false, std::memory_order_release);
    if (!task) {
        checkmk_running.store(false, std::memory_order_release);
        xSemaphoreGive(lifecycle);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping CheckMK agent");
    checkmk_running.store(false, std::memory_order_release);

    // Only wake socket I/O here. The task that created each descriptor remains
    // its sole close owner, preventing fd reuse/double-close races.
    int listen_sock = checkmk_listen_sock.load(std::memory_order_acquire);
    if (listen_sock >= 0) {
        shutdown(listen_sock, SHUT_RDWR);
    }
    int client_sock = checkmk_client_sock.load(std::memory_order_acquire);
    if (client_sock >= 0) shutdown(client_sock, SHUT_RDWR);
    xSemaphoreGive(lifecycle);

    // Accept and client I/O have 1-2 second timeouts; five seconds includes
    // scheduler margin without ever force-deleting a task that owns a socket.
    for (int i = 0; i < 50 &&
         checkmk_task_handle.load(std::memory_order_acquire) != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (checkmk_task_handle.load(std::memory_order_acquire) != NULL) {
        ESP_LOGW(TAG, "CheckMK worker still stopping after timeout");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

// NVS set_* calls become durable immediately in ESP-IDF 6; commit() is a
// compatibility no-op. Preserve capacity in the deployed 16 KiB partition by
// journaling only fields which actually change. If power fails after the
// pending marker is written, boot replays the compact old-value journal before
// loading any field, so a partially written form never becomes active.
enum class ConfigNvsType : uint8_t { U8 = 1, U16 = 2, STR = 3, BLOB = 4 };

struct ConfigFieldDesc {
    const char *key;
    ConfigNvsType type;
    size_t offset;
    size_t capacity;
};

#define CFG_OFF(member) offsetof(monitoring_config_t, member)
#define CFG_SIZE(member) sizeof(((monitoring_config_t *)0)->member)
#define CFG_U8(member, key)  { key, ConfigNvsType::U8,  CFG_OFF(member), CFG_SIZE(member) }
#define CFG_U16(member, key) { key, ConfigNvsType::U16, CFG_OFF(member), CFG_SIZE(member) }
#define CFG_STR(member, key) { key, ConfigNvsType::STR, CFG_OFF(member), CFG_SIZE(member) }
#define CFG_BLOB(member, key) { key, ConfigNvsType::BLOB, CFG_OFF(member), CFG_SIZE(member) }

static const ConfigFieldDesc CONFIG_FIELDS[] = {
    CFG_U8(checkmk.enabled, NVS_CHECKMK_ENABLED),
    CFG_U16(checkmk.port, NVS_CHECKMK_PORT),
    CFG_STR(checkmk.allowed_hosts, NVS_CHECKMK_HOSTS),
    CFG_U8(mqtt.enabled, NVS_MQTT_ENABLED),
    CFG_STR(mqtt.server, NVS_MQTT_SERVER),
    CFG_U16(mqtt.port, NVS_MQTT_PORT),
    CFG_STR(mqtt.user, NVS_MQTT_USER),
    CFG_STR(mqtt.password, NVS_MQTT_PASS),
    CFG_STR(mqtt.topic_prefix, NVS_MQTT_PREFIX),
    CFG_U8(mqtt.ha_discovery_enabled, NVS_MQTT_HA_ENABLED),
    CFG_STR(mqtt.ha_discovery_prefix, NVS_MQTT_HA_PREFIX),
    CFG_U8(mqtt.tls_enable, NVS_MQTT_TLS_EN),
    CFG_U8(mqtt.tls_skip_verify, NVS_MQTT_TLS_SKIP),
    CFG_BLOB(mqtt.tls_ca_certs, NVS_MQTT_TLS_CA),
    CFG_BLOB(mqtt.tls_certfile, NVS_MQTT_TLS_CRT),
    CFG_BLOB(mqtt.tls_keyfile, NVS_MQTT_TLS_KEY),
    CFG_U8(mqtt.command_enabled, NVS_MQTT_CMD_EN),
    CFG_STR(mqtt.command_token, NVS_MQTT_CMD_TOK),
    CFG_U8(prometheus.enabled, NVS_PROM_ENABLED),
    CFG_U16(prometheus.port, NVS_PROM_PORT),
    CFG_STR(prometheus.allowed_hosts, NVS_PROM_HOSTS),
    CFG_U8(syslog.enabled, NVS_SYSLOG_ENABLED),
    CFG_STR(syslog.server, NVS_SYSLOG_SERVER),
    CFG_U16(syslog.port, NVS_SYSLOG_PORT),
    CFG_U8(syslog.transport, NVS_SYSLOG_XPORT),
    CFG_U8(syslog.min_severity, NVS_SYSLOG_SEV),
    CFG_STR(syslog.hostname, NVS_SYSLOG_HOST),
    CFG_U8(notify.enabled, NVS_NOTIFY_ENABLED),
    CFG_U8(notify.channels, NVS_NOTIFY_CHANS),
    CFG_STR(notify.webhook_url, NVS_NOTIFY_WHOOK),
    CFG_STR(notify.webhook_secret, NVS_NOTIFY_WSECRET),
    CFG_STR(notify.telegram_token, NVS_NOTIFY_TGTOKEN),
    CFG_STR(notify.telegram_chatid, NVS_NOTIFY_TGCHAT),
    CFG_STR(notify.smtp_server, NVS_NOTIFY_SMTPSRV),
    CFG_U16(notify.smtp_port, NVS_NOTIFY_SMTPPORT),
    CFG_U8(notify.smtp_tls, NVS_NOTIFY_SMTPTLS),
    CFG_STR(notify.smtp_user, NVS_NOTIFY_SMTPUSER),
    CFG_STR(notify.smtp_password, NVS_NOTIFY_SMTPPW),
    CFG_STR(notify.smtp_from, NVS_NOTIFY_SMTPFROM),
    CFG_STR(notify.smtp_to, NVS_NOTIFY_SMTPTO),
    CFG_U16(notify.cooldown_seconds, NVS_NOTIFY_COOLDOWN),
};

#undef CFG_OFF
#undef CFG_SIZE
#undef CFG_U8
#undef CFG_U16
#undef CFG_STR
#undef CFG_BLOB

static esp_err_t config_field_value_len(const ConfigFieldDesc &field,
                                        const uint8_t *value, size_t *len)
{
    if (field.type == ConfigNvsType::U8) {
        *len = 1;
    } else if (field.type == ConfigNvsType::U16) {
        *len = 2;
    } else {
        const size_t string_len = strnlen(
            reinterpret_cast<const char *>(value), field.capacity);
        if (string_len >= field.capacity) return ESP_ERR_INVALID_SIZE;
        *len = string_len + 1;
    }
    return ESP_OK;
}

static esp_err_t write_config_field(nvs_handle_t handle,
                                    const ConfigFieldDesc &field,
                                    const uint8_t *value, size_t len)
{
    if (field.type == ConfigNvsType::U8 && len == 1) {
        return nvs_set_u8(handle, field.key, value[0]);
    }
    if (field.type == ConfigNvsType::U16 && len == 2) {
        uint16_t number;
        memcpy(&number, value, sizeof(number));
        return nvs_set_u16(handle, field.key, number);
    }
    if ((field.type == ConfigNvsType::STR ||
         field.type == ConfigNvsType::BLOB) &&
        len > 0 && len <= field.capacity && value[len - 1] == '\0') {
        // Store all bounded text as blobs. Unlike nvs_set_str(), blobs may
        // span pages, so the entry-count preflight below is sufficient even
        // when unrelated namespaces have fragmented the 16 KiB partition.
        return nvs_set_blob(handle, field.key, value, len);
    }
    return ESP_ERR_INVALID_SIZE;
}

// The deployed NVS partition is only 16 KiB and is shared with Settings,
// reset diagnostics and WebUI metadata.  Keeping a second 6-8 KiB copy of all
// MQTT certificate material is therefore impossible in the worst case.
//
// A tiny marker in a separate namespace provides capacity-neutral fail-safe
// semantics instead: it is committed before the monitoring namespace is
// replaced. If power is lost anywhere during the replacement, boot erases the
// partial namespace and starts with safe monitoring defaults; it never
// activates a mixed generation. Capacity is proven before the known-good
// namespace is touched, so an oversized candidate is rejected without losing
// the old configuration. A physical flash failure after replacement starts
// can still lose optional monitoring settings, but cannot create malformed
// live TLS strings or ports.
static esp_err_t recover_config_transaction(nvs_handle_t config_handle)
{
    nvs_handle_t transaction_handle;
    esp_err_t err = nvs_open(NVS_TXN_NAMESPACE, NVS_READONLY,
                             &transaction_handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;

    uint8_t pending = 0;
    err = nvs_get_u8(transaction_handle, NVS_TXN_PENDING, &pending);
    nvs_close(transaction_handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;
    if (pending == 0) return ESP_OK;

    ESP_LOGW(TAG,
             "Interrupted monitoring config write detected; restoring safe defaults");
    err = nvs_erase_all(config_handle);
    if (err == ESP_OK) err = nvs_commit(config_handle);
    if (err != ESP_OK) return err;

    err = nvs_open(NVS_TXN_NAMESPACE, NVS_READWRITE, &transaction_handle);
    if (err != ESP_OK) return err;
    err = nvs_erase_key(transaction_handle, NVS_TXN_PENDING);
    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    if (err == ESP_OK) err = nvs_commit(transaction_handle);
    nvs_close(transaction_handle);
    return err;
}

static esp_err_t config_nvs_entry_requirement(
    const monitoring_config_t *config, size_t *required_entries)
{
    if (config == NULL || required_entries == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *required_entries = 0;
    for (const ConfigFieldDesc &field : CONFIG_FIELDS) {
        const uint8_t *value =
            reinterpret_cast<const uint8_t *>(config) + field.offset;
        size_t value_len = 0;
        const esp_err_t validation =
            config_field_value_len(field, value, &value_len);
        if (validation != ESP_OK) return validation;
        if (field.type == ConfigNvsType::U8 ||
            field.type == ConfigNvsType::U16) {
            *required_entries += 1;
        } else {
            *required_entries += 2 + (value_len + 31) / 32;
        }
    }
    return ESP_OK;
}

static esp_err_t verify_config_nvs_capacity(nvs_handle_t config_handle,
                                             size_t required_entries)
{
    nvs_stats_t stats = {};
    size_t old_config_entries = 0;
    esp_err_t err = nvs_get_stats(NULL, &stats);
    if (err == ESP_OK) {
        err = nvs_get_used_entry_count(config_handle, &old_config_entries);
    }
    // Other application namespaces use the same global lock, so they cannot
    // consume entries between this check and the final commit. Keep a modest
    // reserve for the transaction marker and NVS page bookkeeping.
    static constexpr size_t CONFIG_NVS_RESERVE_ENTRIES = 16;
    const size_t prospective_entries =
        stats.available_entries + old_config_entries;
    if (err != ESP_OK || stats.available_entries < 2 ||
        required_entries + CONFIG_NVS_RESERVE_ENTRIES > prospective_entries) {
        if (err == ESP_OK) err = ESP_ERR_NVS_NOT_ENOUGH_SPACE;
        ESP_LOGE(TAG,
                 "Monitoring config needs %u NVS entries; only %u are available after reclaim",
                 (unsigned)required_entries,
                 (unsigned)prospective_entries);
        return err;
    }
    return ESP_OK;
}

static esp_err_t save_config_to_nvs(const monitoring_config_t *config)
{
    if (config == NULL) return ESP_ERR_INVALID_ARG;

    // Validate every bounded string before changing any persistent state.
    size_t required_entries = 0;
    esp_err_t err =
        config_nvs_entry_requirement(config, &required_entries);
    if (err != ESP_OK) return err;

    NvsStorageLock storage_lock;
    if (!storage_lock) return ESP_ERR_NO_MEM;

    nvs_handle_t config_handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &config_handle);
    if (err != ESP_OK) return err;

    err = recover_config_transaction(config_handle);
    if (err != ESP_OK) {
        nvs_close(config_handle);
        return err;
    }

    // NVS values require the complete new value to fit. Estimate the
    // exact documented entry cost and include entries reclaimed by erasing
    // this namespace. Preserve a small reserve for the transaction namespace
    // and NVS bookkeeping. If the larger candidate cannot fit, leave the old
    // generation untouched and report the error to the caller.
    err = verify_config_nvs_capacity(config_handle, required_entries);
    if (err != ESP_OK) {
        nvs_close(config_handle);
        return err;
    }

    nvs_handle_t transaction_handle;
    err = nvs_open(NVS_TXN_NAMESPACE, NVS_READWRITE,
                   &transaction_handle);
    if (err != ESP_OK) {
        nvs_close(config_handle);
        return err;
    }

    // Once this commit succeeds, every failure path deliberately leaves the
    // marker set.  The next save attempt or boot then discards any partial
    // generation before reading it.
    err = nvs_set_u8(transaction_handle, NVS_TXN_PENDING, 1);
    if (err == ESP_OK) err = nvs_commit(transaction_handle);
    if (err != ESP_OK) {
        nvs_close(transaction_handle);
        nvs_close(config_handle);
        return err;
    }

    err = nvs_erase_all(config_handle);
    if (err == ESP_OK) err = nvs_commit(config_handle);
    for (const ConfigFieldDesc &field : CONFIG_FIELDS) {
        if (err != ESP_OK) break;
        const uint8_t *value =
            reinterpret_cast<const uint8_t *>(config) + field.offset;
        size_t value_len = 0;
        err = config_field_value_len(field, value, &value_len);
        if (err == ESP_OK) {
            err = write_config_field(config_handle, field, value, value_len);
        }
    }
    if (err == ESP_OK) err = nvs_commit(config_handle);

    if (err == ESP_OK) {
        esp_err_t clear_result =
            nvs_erase_key(transaction_handle, NVS_TXN_PENDING);
        if (clear_result == ESP_ERR_NVS_NOT_FOUND) clear_result = ESP_OK;
        if (clear_result == ESP_OK) {
            clear_result = nvs_commit(transaction_handle);
        }
        err = clear_result;
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Monitoring config write failed (%s); recovery marker retained",
                 esp_err_to_name(err));
    }

    nvs_close(transaction_handle);
    nvs_close(config_handle);
    return err;
}

esp_err_t monitoring_validate_config_storage(
    const monitoring_config_t *config)
{
    size_t required_entries = 0;
    esp_err_t err =
        config_nvs_entry_requirement(config, &required_entries);
    if (err != ESP_OK) return err;

    NvsStorageLock storage_lock;
    if (!storage_lock) return ESP_ERR_NO_MEM;
    nvs_handle_t config_handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &config_handle);
    if (err != ESP_OK) return err;
    err = recover_config_transaction(config_handle);
    if (err == ESP_OK) {
        err = verify_config_nvs_capacity(config_handle, required_entries);
    }
    nvs_close(config_handle);
    return err;
}

// New firmware stores all text as page-spanning blobs to make capacity
// preflight deterministic. Accept legacy NVS strings during migration and
// reject any value which is not a complete bounded C string.
static esp_err_t load_config_text(nvs_handle_t handle, const char *key,
                                  char *destination, size_t capacity)
{
    if (destination == NULL || capacity == 0) return ESP_ERR_INVALID_ARG;

    nvs_type_t type = NVS_TYPE_ANY;
    esp_err_t err = nvs_find_key(handle, key, &type);
    if (err != ESP_OK) return err;

    size_t value_len = capacity;
    if (type == NVS_TYPE_STR) {
        err = nvs_get_str(handle, key, destination, &value_len);
    } else if (type == NVS_TYPE_BLOB) {
        err = nvs_get_blob(handle, key, destination, &value_len);
    } else {
        return ESP_ERR_NVS_TYPE_MISMATCH;
    }

    if (err != ESP_OK || value_len == 0 || value_len > capacity ||
        destination[value_len - 1] != '\0') {
        destination[0] = '\0';
        destination[capacity - 1] = '\0';
        return err == ESP_OK ? ESP_ERR_INVALID_SIZE : err;
    }
    destination[capacity - 1] = '\0';
    return ESP_OK;
}

// Missing keys are expected when upgrading from an older firmware. Every
// other NVS result means the stored value exists but could not be read safely
// (wrong type/length, flash I/O failure, etc.) and must be propagated rather
// than silently replacing security settings with permissive defaults.
static esp_err_t load_optional_integrity_bool(nvs_handle_t handle,
                                              const char *key,
                                              bool *destination)
{
    uint8_t value = 0;
    esp_err_t err = nvs_get_u8(handle, key, &value);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;
    if (value > 1) return ESP_ERR_INVALID_STATE;
    *destination = value != 0;
    return ESP_OK;
}

static esp_err_t load_optional_integrity_u8(nvs_handle_t handle,
                                            const char *key,
                                            uint8_t *destination,
                                            uint8_t maximum)
{
    uint8_t value = 0;
    esp_err_t err = nvs_get_u8(handle, key, &value);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;
    if (value > maximum) return ESP_ERR_INVALID_STATE;
    *destination = value;
    return ESP_OK;
}

static esp_err_t load_optional_integrity_u16(nvs_handle_t handle,
                                             const char *key,
                                             uint16_t *destination,
                                             bool zero_allowed = false)
{
    uint16_t value = 0;
    esp_err_t err = nvs_get_u16(handle, key, &value);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;
    if (value == 0 && !zero_allowed) return ESP_ERR_INVALID_STATE;
    *destination = value;
    return ESP_OK;
}

static esp_err_t load_optional_integrity_text(nvs_handle_t handle,
                                              const char *key,
                                              char *destination,
                                              size_t capacity)
{
    esp_err_t err = load_config_text(handle, key, destination, capacity);
    return err == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : err;
}

static esp_err_t log_integrity_read_error(const char *key, esp_err_t err)
{
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Monitoring NVS key '%s' failed integrity read: %s",
                 key, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t load_integrity_sensitive_config(
    nvs_handle_t handle, monitoring_config_t *config)
{
    esp_err_t err = ESP_OK;
#define LOAD_INTEGRITY(key, expression)                                      \
    do {                                                                      \
        err = (expression);                                                   \
        if (err != ESP_OK) return log_integrity_read_error((key), err);      \
    } while (0)

    // These listeners are unauthenticated by design, so a corrupt enable bit,
    // port or allowlist must abort startup instead of falling back to an
    // allow-all default. Missing keys still retain upgrade/factory defaults.
    LOAD_INTEGRITY(NVS_CHECKMK_ENABLED,
                   load_optional_integrity_bool(
                       handle, NVS_CHECKMK_ENABLED,
                       &config->checkmk.enabled));
    LOAD_INTEGRITY(NVS_CHECKMK_PORT,
                   load_optional_integrity_u16(
                       handle, NVS_CHECKMK_PORT, &config->checkmk.port));
    LOAD_INTEGRITY(NVS_CHECKMK_HOSTS,
                   load_optional_integrity_text(
                       handle, NVS_CHECKMK_HOSTS,
                       config->checkmk.allowed_hosts,
                       sizeof(config->checkmk.allowed_hosts)));
    LOAD_INTEGRITY(NVS_PROM_ENABLED,
                   load_optional_integrity_bool(
                       handle, NVS_PROM_ENABLED,
                       &config->prometheus.enabled));
    LOAD_INTEGRITY(NVS_PROM_PORT,
                   load_optional_integrity_u16(
                       handle, NVS_PROM_PORT,
                       &config->prometheus.port));
    LOAD_INTEGRITY(NVS_PROM_HOSTS,
                   load_optional_integrity_text(
                       handle, NVS_PROM_HOSTS,
                       config->prometheus.allowed_hosts,
                       sizeof(config->prometheus.allowed_hosts)));

    // MQTT endpoint, authentication, TLS, HA discovery and command controls.
    LOAD_INTEGRITY(NVS_MQTT_ENABLED,
                   load_optional_integrity_bool(
                       handle, NVS_MQTT_ENABLED, &config->mqtt.enabled));
    LOAD_INTEGRITY(NVS_MQTT_SERVER,
                   load_optional_integrity_text(
                       handle, NVS_MQTT_SERVER, config->mqtt.server,
                       sizeof(config->mqtt.server)));
    LOAD_INTEGRITY(NVS_MQTT_PORT,
                   load_optional_integrity_u16(
                       handle, NVS_MQTT_PORT, &config->mqtt.port));
    LOAD_INTEGRITY(NVS_MQTT_USER,
                   load_optional_integrity_text(
                       handle, NVS_MQTT_USER, config->mqtt.user,
                       sizeof(config->mqtt.user)));
    LOAD_INTEGRITY(NVS_MQTT_PASS,
                   load_optional_integrity_text(
                       handle, NVS_MQTT_PASS, config->mqtt.password,
                       sizeof(config->mqtt.password)));
    LOAD_INTEGRITY(NVS_MQTT_PREFIX,
                   load_optional_integrity_text(
                       handle, NVS_MQTT_PREFIX, config->mqtt.topic_prefix,
                       sizeof(config->mqtt.topic_prefix)));
    LOAD_INTEGRITY(NVS_MQTT_HA_ENABLED,
                   load_optional_integrity_bool(
                       handle, NVS_MQTT_HA_ENABLED,
                       &config->mqtt.ha_discovery_enabled));
    LOAD_INTEGRITY(NVS_MQTT_HA_PREFIX,
                   load_optional_integrity_text(
                       handle, NVS_MQTT_HA_PREFIX,
                       config->mqtt.ha_discovery_prefix,
                       sizeof(config->mqtt.ha_discovery_prefix)));
    LOAD_INTEGRITY(NVS_MQTT_TLS_EN,
                   load_optional_integrity_bool(
                       handle, NVS_MQTT_TLS_EN,
                       &config->mqtt.tls_enable));
    LOAD_INTEGRITY(NVS_MQTT_TLS_SKIP,
                   load_optional_integrity_bool(
                       handle, NVS_MQTT_TLS_SKIP,
                       &config->mqtt.tls_skip_verify));
    LOAD_INTEGRITY(NVS_MQTT_TLS_CA,
                   load_optional_integrity_text(
                       handle, NVS_MQTT_TLS_CA, config->mqtt.tls_ca_certs,
                       sizeof(config->mqtt.tls_ca_certs)));
    LOAD_INTEGRITY(NVS_MQTT_TLS_CRT,
                   load_optional_integrity_text(
                       handle, NVS_MQTT_TLS_CRT, config->mqtt.tls_certfile,
                       sizeof(config->mqtt.tls_certfile)));
    LOAD_INTEGRITY(NVS_MQTT_TLS_KEY,
                   load_optional_integrity_text(
                       handle, NVS_MQTT_TLS_KEY, config->mqtt.tls_keyfile,
                       sizeof(config->mqtt.tls_keyfile)));
    LOAD_INTEGRITY(NVS_MQTT_CMD_EN,
                   load_optional_integrity_bool(
                       handle, NVS_MQTT_CMD_EN,
                       &config->mqtt.command_enabled));
    LOAD_INTEGRITY(NVS_MQTT_CMD_TOK,
                   load_optional_integrity_text(
                       handle, NVS_MQTT_CMD_TOK, config->mqtt.command_token,
                       sizeof(config->mqtt.command_token)));

    // A corrupt transport must never normalize TLS back to UDP/plain TCP.
    LOAD_INTEGRITY(NVS_SYSLOG_ENABLED,
                   load_optional_integrity_bool(
                       handle, NVS_SYSLOG_ENABLED,
                       &config->syslog.enabled));
    LOAD_INTEGRITY(NVS_SYSLOG_SERVER,
                   load_optional_integrity_text(
                       handle, NVS_SYSLOG_SERVER, config->syslog.server,
                       sizeof(config->syslog.server)));
    LOAD_INTEGRITY(NVS_SYSLOG_PORT,
                   load_optional_integrity_u16(
                       handle, NVS_SYSLOG_PORT, &config->syslog.port));
    LOAD_INTEGRITY(NVS_SYSLOG_XPORT,
                   load_optional_integrity_u8(
                       handle, NVS_SYSLOG_XPORT,
                       &config->syslog.transport, 2));
    LOAD_INTEGRITY(NVS_SYSLOG_SEV,
                   load_optional_integrity_u8(
                       handle, NVS_SYSLOG_SEV,
                       &config->syslog.min_severity, 7));
    LOAD_INTEGRITY(NVS_SYSLOG_HOST,
                   load_optional_integrity_text(
                       handle, NVS_SYSLOG_HOST, config->syslog.hostname,
                       sizeof(config->syslog.hostname)));

    // Notification routing, TLS mode, endpoints and all channel secrets are a
    // single integrity domain: a partial fallback could leak or misroute data.
    LOAD_INTEGRITY(NVS_NOTIFY_ENABLED,
                   load_optional_integrity_bool(
                       handle, NVS_NOTIFY_ENABLED,
                       &config->notify.enabled));
    LOAD_INTEGRITY(NVS_NOTIFY_CHANS,
                   load_optional_integrity_u8(
                       handle, NVS_NOTIFY_CHANS,
                       &config->notify.channels, 0x07));
    LOAD_INTEGRITY(NVS_NOTIFY_WHOOK,
                   load_optional_integrity_text(
                       handle, NVS_NOTIFY_WHOOK, config->notify.webhook_url,
                       sizeof(config->notify.webhook_url)));
    LOAD_INTEGRITY(NVS_NOTIFY_WSECRET,
                   load_optional_integrity_text(
                       handle, NVS_NOTIFY_WSECRET,
                       config->notify.webhook_secret,
                       sizeof(config->notify.webhook_secret)));
    LOAD_INTEGRITY(NVS_NOTIFY_TGTOKEN,
                   load_optional_integrity_text(
                       handle, NVS_NOTIFY_TGTOKEN,
                       config->notify.telegram_token,
                       sizeof(config->notify.telegram_token)));
    LOAD_INTEGRITY(NVS_NOTIFY_TGCHAT,
                   load_optional_integrity_text(
                       handle, NVS_NOTIFY_TGCHAT,
                       config->notify.telegram_chatid,
                       sizeof(config->notify.telegram_chatid)));
    LOAD_INTEGRITY(NVS_NOTIFY_SMTPSRV,
                   load_optional_integrity_text(
                       handle, NVS_NOTIFY_SMTPSRV,
                       config->notify.smtp_server,
                       sizeof(config->notify.smtp_server)));
    LOAD_INTEGRITY(NVS_NOTIFY_SMTPPORT,
                   load_optional_integrity_u16(
                       handle, NVS_NOTIFY_SMTPPORT,
                       &config->notify.smtp_port));
    LOAD_INTEGRITY(NVS_NOTIFY_SMTPTLS,
                   load_optional_integrity_u8(
                       handle, NVS_NOTIFY_SMTPTLS,
                       &config->notify.smtp_tls, 2));
    LOAD_INTEGRITY(NVS_NOTIFY_SMTPUSER,
                   load_optional_integrity_text(
                       handle, NVS_NOTIFY_SMTPUSER,
                       config->notify.smtp_user,
                       sizeof(config->notify.smtp_user)));
    LOAD_INTEGRITY(NVS_NOTIFY_SMTPPW,
                   load_optional_integrity_text(
                       handle, NVS_NOTIFY_SMTPPW,
                       config->notify.smtp_password,
                       sizeof(config->notify.smtp_password)));
    LOAD_INTEGRITY(NVS_NOTIFY_SMTPFROM,
                   load_optional_integrity_text(
                       handle, NVS_NOTIFY_SMTPFROM,
                       config->notify.smtp_from,
                       sizeof(config->notify.smtp_from)));
    LOAD_INTEGRITY(NVS_NOTIFY_SMTPTO,
                   load_optional_integrity_text(
                       handle, NVS_NOTIFY_SMTPTO, config->notify.smtp_to,
                       sizeof(config->notify.smtp_to)));
    LOAD_INTEGRITY(NVS_NOTIFY_COOLDOWN,
                   load_optional_integrity_u16(
                       handle, NVS_NOTIFY_COOLDOWN,
                       &config->notify.cooldown_seconds, true));

#undef LOAD_INTEGRITY
    return ESP_OK;
}

// Load configuration from NVS
static esp_err_t load_config_from_nvs(monitoring_config_t *config)
{
    // NVS namespaces survive nvs_erase_all() as empty namespaces. Always
    // establish a complete baseline before overlaying stored keys; otherwise
    // a factory reset leaves checkmk.port at the struct's zero-initialized
    // value and the combined monitoring form can no longer save MQTT.
    monitoring_config_set_defaults(config);

    NvsStorageLock storage_lock;
    if (!storage_lock) return ESP_ERR_NO_MEM;

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved monitoring configuration, using defaults");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not open monitoring NVS: %s",
                 esp_err_to_name(err));
        return err;
    }
    err = recover_config_transaction(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not recover monitoring NVS transaction: %s",
                 esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = load_integrity_sensitive_config(nvs_handle, config);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }

    // One-time migration: the old firmware default was "hb-rf-eth". Update
    // devices that still carry the legacy preset; custom prefixes stay intact.
    if (strcmp(config->mqtt.topic_prefix, "hb-rf-eth") == 0) {
        strncpy(config->mqtt.topic_prefix, "hb-rf-eth-ng", sizeof(config->mqtt.topic_prefix) - 1);
        config->mqtt.topic_prefix[sizeof(config->mqtt.topic_prefix) - 1] = '\0';
    }

    monitoring_config_normalize(config);
    nvs_close(nvs_handle);
    return ESP_OK;
}

// Low-heap watchdog: this is not a "cleaner" (there is no GC/page-cache on
// the ESP32 to reclaim) - it is a last-resort safety net for leaks we don't
// yet know about. If free heap stays below the critical threshold for
// several consecutive samples, restart cleanly rather than risk a hard
// crash/lockup from a failed allocation deep in the network or TLS stack.
static constexpr size_t HEAP_WATCHDOG_CRITICAL_BYTES = 20 * 1024;
static constexpr int HEAP_WATCHDOG_CONSECUTIVE_HITS = 8;     // ~8 * 60s = 8 min sustained
static constexpr TickType_t HEAP_WATCHDOG_INTERVAL_TICKS = pdMS_TO_TICKS(60000);

static void heap_watchdog_task(void *pvParameters)
{
    (void)pvParameters;
    int low_heap_streak = 0;

    for (;;)
    {
        vTaskDelay(HEAP_WATCHDOG_INTERVAL_TICKS);

        // Skip checks during an active firmware/OTA write: flash erase +
        // buffer allocations routinely push free heap below the threshold,
        // and a restart here would interrupt and fail the upgrade. The manual
        // upload path toggles this via ota_operation_try_begin/finish.
        if (ota_operation_active())
        {
            low_heap_streak = 0;
            continue;
        }

        size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        // Record a pre-crash snapshot in RTC memory so the next boot can tell
        // us whether heap exhaustion preceded a sudden watchdog/panic reboot
        // (issue #362). Done every cycle regardless of the threshold so the
        // last sample is always recent.
        {
            size_t largest_now = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
            size_t min_ever = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
            size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            uint32_t secs = (uint32_t)((uint64_t)xTaskGetTickCount() * portTICK_PERIOD_MS / 1000ULL);
            crash_blackbox_update((uint32_t)free_heap, (uint32_t)largest_now,
                                  (uint32_t)min_ever, (uint32_t)internal_free,
                                  secs, (uint32_t)low_heap_streak);
        }
        if (free_heap < HEAP_WATCHDOG_CRITICAL_BYTES)
        {
            low_heap_streak++;
            ESP_LOGW(TAG, "Low heap: %u bytes free (streak %d/%d)",
                     (unsigned)free_heap, low_heap_streak, HEAP_WATCHDOG_CONSECUTIVE_HITS);

            if (low_heap_streak >= HEAP_WATCHDOG_CONSECUTIVE_HITS)
            {
                size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
                size_t min_ever = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
                uint32_t secs = (uint32_t)((uint64_t)xTaskGetTickCount() * portTICK_PERIOD_MS / 1000ULL);
                // Keep in sync with last_diag_buffer[96] in reset_info.cpp —
                // the snapshot stored here is later surfaced through that
                // buffer, so it must not exceed 96 bytes.
                char diag[80];
                snprintf(diag, sizeof(diag),
                         "low heap: free=%u largest=%u min_ever=%u uptime=%us",
                         (unsigned)free_heap, (unsigned)largest,
                         (unsigned)min_ever, (unsigned)secs);
                ESP_LOGE(TAG, "Heap critically low for %d consecutive checks - restarting (%s)",
                         low_heap_streak, diag);
                // Persist a tail of the in-memory log so the user can see
                // what led to the restart. Best-effort: if NVS or heap is
                // too tight to format, the diag string above still carries
                // the headline numbers.
                LogManager::instance().saveCrashTailNvs("heap_watchdog");
                ResetInfo::storeResetReason(RESET_REASON_WATCHDOG, diag);
                vTaskDelay(pdMS_TO_TICKS(200));
                esp_restart();
            }
        }
        else
        {
            low_heap_streak = 0;
        }
    }
}

static void schedule_mqtt_retry();

static void mqtt_retry_guard_timer_callback(TimerHandle_t)
{
    // xTaskCreate() can transiently fail under TLS/HTTP heap pressure. This
    // statically allocated periodic guard is independent of that heap and
    // retries worker creation until the deferred request is consumed.
    if (mqtt_start_deferred.load(std::memory_order_acquire) &&
        !mqtt_retry_task_running.load(std::memory_order_acquire)) {
        schedule_mqtt_retry();
    }
}

static void mqtt_retry_task(void *)
{
    while (mqtt_start_deferred.load(std::memory_order_acquire)) {
        if (!g_monitoring_ota_paused.load(std::memory_order_acquire) &&
            !monitoring_config_update_active() &&
            config_mutex != NULL &&
            (g_ethernet == NULL || g_ethernet->isConnected())) {
            mqtt_config_t *snapshot =
                static_cast<mqtt_config_t *>(malloc(sizeof(mqtt_config_t)));
            if (snapshot != NULL) {
                xSemaphoreTake(config_mutex, portMAX_DELAY);
                const bool transition_active =
                    g_monitoring_ota_paused.load(std::memory_order_acquire) ||
                    monitoring_config_update_active();
                if (!transition_active) {
                    memcpy(snapshot, &current_config.mqtt, sizeof(*snapshot));
                }

                if (transition_active) {
                    // Keep the flag armed.  Holding config_mutex across this
                    // decision closes the window where a config transaction
                    // could begin after the check but before an old snapshot
                    // starts.  The updater must acquire the same mutex before
                    // stopping/replacing MQTT.
                } else if (!snapshot->enabled) {
                    mqtt_start_deferred.store(false,
                                              std::memory_order_release);
                } else {
                    esp_err_t result = mqtt_handler_start(snapshot);
                    if (result == ESP_OK) {
                        mqtt_start_deferred.store(false,
                                                  std::memory_order_release);
                    } else {
                        ESP_LOGW(TAG, "Deferred MQTT retry failed: %s",
                                 esp_err_to_name(result));
                    }
                }
                xSemaphoreGive(config_mutex);
                free(snapshot);
            } else {
                ESP_LOGW(TAG, "Deferred MQTT retry allocation failed");
            }
        }
        if (mqtt_start_deferred.load(std::memory_order_acquire)) {
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    }

    mqtt_retry_task_running.store(false, std::memory_order_release);
    // Close a schedule-vs-exit race: a caller may have re-armed the flag just
    // before this task published its idle state.
    if (mqtt_start_deferred.load(std::memory_order_acquire)) {
        schedule_mqtt_retry();
    }
    vTaskDelete(NULL);
}

static void schedule_mqtt_retry()
{
    bool expected = false;
    if (!mqtt_retry_task_running.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) {
        return;
    }
    if (xTaskCreate(mqtt_retry_task, "mqtt_retry", 3072,
                    NULL, 2, NULL) != pdPASS) {
        mqtt_retry_task_running.store(false, std::memory_order_release);
        ESP_LOGE(TAG,
                 "Could not create deferred MQTT retry task; static guard will retry");
    }
}

// Arm the deferred flag before checking the link state. This ordering closes
// the race where GOT_IP arrives between the check and arming the callback:
// either this function or mqtt_network_ready_handler() consumes the flag and
// starts MQTT exactly once.
static void start_mqtt_when_network_ready(const mqtt_config_t *mqtt_config)
{
    if (mqtt_config == NULL || !mqtt_config->enabled) {
        mqtt_start_deferred.store(false);
        return;
    }

    mqtt_start_deferred.store(true);
    if (g_ethernet == NULL || !g_ethernet->isConnected()) {
        if (mqtt_ip_event_instance == NULL) {
            mqtt_start_deferred.store(false);
            ESP_LOGW(TAG, "IPv4-ready handler unavailable; starting MQTT with reconnect fallback");
            esp_err_t result = mqtt_handler_start(mqtt_config);
            if (result != ESP_OK) {
                mqtt_start_deferred.store(true, std::memory_order_release);
                schedule_mqtt_retry();
            }
            return;
        }
        ESP_LOGI(TAG, "MQTT start deferred until IPv4 is ready");
        schedule_mqtt_retry();
        return;
    }

    if (mqtt_start_deferred.exchange(false)) {
        esp_err_t result = mqtt_handler_start(mqtt_config);
        if (result != ESP_OK) {
            mqtt_start_deferred.store(true, std::memory_order_release);
            schedule_mqtt_retry();
            ESP_LOGW(TAG, "MQTT start failed, retry scheduled: %s",
                     esp_err_to_name(result));
        }
    }
}

static void mqtt_network_ready_handler(void *handler_args,
                                       esp_event_base_t event_base,
                                       int32_t event_id,
                                       void *event_data)
{
    (void)handler_args;
    (void)event_data;

    if (event_base != IP_EVENT || event_id != IP_EVENT_ETH_GOT_IP ||
        config_mutex == NULL) {
        return;
    }
    // Offload the full MQTT-start sequence (config snapshot +
    // mqtt_handler_start which can hold mqtt_lifecycle_mutex for up to 15 s)
    // to the retry worker. Running it inline blocks the default event loop
    // and starves every other event subscriber (Ethernet link, NTP, etc.).
    if (mqtt_start_deferred.load(std::memory_order_acquire)) {
        schedule_mqtt_retry();
    }
}

// Initialize monitoring subsystem
esp_err_t monitoring_init(const monitoring_config_t *config, SysInfo* sysInfo)
{
    ESP_LOGI(TAG, "Initializing monitoring subsystem");

    // This ownershipless binary gate protects the ESP32's limited heap from
    // concurrent TLS handshakes. MQTT reconnect callbacks can acquire it and
    // their cleanup task can safely release it if ESP-MQTT exits without a
    // terminal callback—semantics a FreeRTOS ownership mutex cannot provide.
    g_net_fetch_mutex =
        xSemaphoreCreateBinaryStatic(&net_fetch_mutex_buffer);
    if (g_net_fetch_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create net-fetch mutex");
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreGive(g_net_fetch_mutex);

    config_mutex = xSemaphoreCreateMutex();
    if (config_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create config mutex");
        return ESP_ERR_NO_MEM;
    }

    g_sysInfo = sysInfo;

    // Establish a complete, integrity-checked generation before creating any
    // monitoring worker/timer which could observe or act on the configuration.
    if (config == NULL) {
        esp_err_t load_result = load_config_from_nvs(&current_config);
        if (load_result != ESP_OK) return load_result;
    } else {
        memcpy(&current_config, config, sizeof(monitoring_config_t));
        esp_err_t save_result = save_config_to_nvs(&current_config);
        if (save_result != ESP_OK) return save_result;
    }

    // Initialize MQTT handler
    esp_err_t mqtt_init_result = mqtt_handler_init();
    if (mqtt_init_result != ESP_OK) return mqtt_init_result;

    // A static auto-reload timer guarantees recovery if the dynamically
    // created retry worker cannot be allocated during a transient low-heap
    // window. It performs no blocking MQTT work itself; it only retries task
    // creation every ten seconds while a request is armed.
    if (mqtt_retry_guard_timer == NULL) {
        mqtt_retry_guard_timer = xTimerCreateStatic(
            "mqtt_retry_guard", pdMS_TO_TICKS(10000), pdTRUE, NULL,
            mqtt_retry_guard_timer_callback, &mqtt_retry_guard_timer_buffer);
        if (mqtt_retry_guard_timer == NULL ||
            xTimerStart(mqtt_retry_guard_timer,
                        pdMS_TO_TICKS(1000)) != pdPASS) {
            ESP_LOGE(TAG, "Failed to start static MQTT retry guard");
            return ESP_ERR_NO_MEM;
        }
    }

    esp_err_t mqtt_ip_event_result = esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_ETH_GOT_IP,
        mqtt_network_ready_handler,
        NULL,
        &mqtt_ip_event_instance);
    if (mqtt_ip_event_result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MQTT IPv4-ready handler: %s",
                 esp_err_to_name(mqtt_ip_event_result));
    }

    // Start CheckMK if enabled
    if (current_config.checkmk.enabled) {
        checkmk_start(&current_config.checkmk);
    }

    // Start Prometheus exporter if enabled
    if (current_config.prometheus.enabled) {
        prometheus_start(&current_config.prometheus);
    }

    // Start syslog forwarder if enabled
    if (current_config.syslog.enabled) {
        syslog_start(&current_config.syslog);
    }

    // Start notification worker if enabled
    events_init();
    if (current_config.notify.enabled) {
        events_start(&current_config.notify);
    }

    // MQTT must not connect before the Ethernet netif owns an IPv4 address.
    // If GOT_IP already fired, isConnected() starts it here; otherwise the
    // registered event handler starts it as soon as the address is available.
    start_mqtt_when_network_ready(&current_config.mqtt);

    // Heap watchdog runs regardless of monitoring config - it's a safety
    // net for the whole firmware, not a monitoring feature.
    BaseType_t wd_ret = xTaskCreate(heap_watchdog_task, "heap_watchdog", 4096, NULL, 2, NULL);
    if (wd_ret != pdPASS) {
        ESP_LOGW(TAG, "Failed to start heap watchdog task");
    }

    return ESP_OK;
}

struct bounded_config_snapshot_t {
    checkmk_config_t checkmk;
    syslog_config_t syslog;
    prometheus_config_t prometheus;
    notify_config_t notify;
};

static void rollback_bounded_config_transitions(
    const bounded_config_snapshot_t *previous_config,
    uint32_t stopped_mask, uint32_t started_mask)
{
    // Stop any replacement workers which were already started, then restore
    // every old worker whose stop completed. Failures are logged explicitly;
    // the original transition error remains the caller-visible result.
    if (started_mask & OTA_PAUSED_MQTT) {
        esp_err_t err = mqtt_handler_stop();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Could not stop replacement MQTT worker: %s",
                     esp_err_to_name(err));
        }
    }
    if (started_mask & OTA_PAUSED_CHECKMK) {
        esp_err_t err = checkmk_stop();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Could not stop replacement CheckMK worker: %s",
                     esp_err_to_name(err));
        }
    }
    if (started_mask & OTA_PAUSED_NOTIFY) {
        esp_err_t err = events_stop();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Could not stop replacement Events worker: %s",
                     esp_err_to_name(err));
        }
    }
    if (started_mask & OTA_PAUSED_PROMETHEUS) {
        esp_err_t err = prometheus_stop();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Could not stop replacement Prometheus worker: %s",
                     esp_err_to_name(err));
        }
    }
    if (started_mask & OTA_PAUSED_SYSLOG) {
        esp_err_t err = syslog_stop();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Could not stop replacement Syslog worker: %s",
                     esp_err_to_name(err));
        }
    }

    if ((stopped_mask & OTA_PAUSED_SYSLOG) &&
        previous_config->syslog.enabled) {
        esp_err_t err = syslog_start(&previous_config->syslog);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Could not restore previous Syslog worker: %s",
                     esp_err_to_name(err));
        }
    }
    if ((stopped_mask & OTA_PAUSED_PROMETHEUS) &&
        previous_config->prometheus.enabled) {
        esp_err_t err = prometheus_start(&previous_config->prometheus);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Could not restore previous Prometheus worker: %s",
                     esp_err_to_name(err));
        }
    }
    if ((stopped_mask & OTA_PAUSED_NOTIFY) &&
        previous_config->notify.enabled) {
        esp_err_t err = events_start(&previous_config->notify);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Could not restore previous Events worker: %s",
                     esp_err_to_name(err));
        }
    }
    if ((stopped_mask & OTA_PAUSED_CHECKMK) &&
        previous_config->checkmk.enabled) {
        esp_err_t err = checkmk_start(&previous_config->checkmk);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Could not restore previous CheckMK worker: %s",
                     esp_err_to_name(err));
        }
    }
    if (stopped_mask & OTA_PAUSED_MQTT) {
        // current_config is intentionally still the old committed snapshot
        // until the whole transaction (including persistence) succeeds.
        start_mqtt_when_network_ready(&current_config.mqtt);
    }
}

// Update configuration
esp_err_t monitoring_update_config(const monitoring_config_t *config)
{
    if (config == NULL) return ESP_ERR_INVALID_ARG;
    if (config_mutex == NULL) return ESP_ERR_INVALID_STATE;
    if (g_monitoring_ota_paused.load(std::memory_order_acquire)) {
        return ESP_ERR_INVALID_STATE;
    }

    // Take a snapshot of the current config under mutex to determine what changed.
    // Release the mutex before any blocking stop/start calls so GET requests
    // are never blocked for more than a memcpy duration.
    // Do not put monitoring_config_t on this task's stack: its embedded MQTT
    // certificates make it roughly 6.5 KiB, while the update task has an
    // 8 KiB stack. Rollback only needs the three bounded worker configs.
    bounded_config_snapshot_t previous_config;
    xSemaphoreTake(config_mutex, portMAX_DELAY);
    memcpy(&previous_config.checkmk, &current_config.checkmk,
           sizeof(previous_config.checkmk));
    memcpy(&previous_config.syslog, &current_config.syslog,
           sizeof(previous_config.syslog));
    memcpy(&previous_config.prometheus, &current_config.prometheus,
           sizeof(previous_config.prometheus));
    memcpy(&previous_config.notify, &current_config.notify,
           sizeof(previous_config.notify));
    bool checkmk_changed    = (memcmp(&current_config.checkmk,    &config->checkmk,    sizeof(checkmk_config_t))    != 0);
    bool mqtt_changed       = (memcmp(&current_config.mqtt,       &config->mqtt,       sizeof(mqtt_config_t))       != 0);
    bool prometheus_changed = (memcmp(&current_config.prometheus, &config->prometheus, sizeof(prometheus_config_t)) != 0);
    bool syslog_changed     = (memcmp(&current_config.syslog,     &config->syslog,     sizeof(syslog_config_t))     != 0);
    bool notify_changed     = (memcmp(&current_config.notify,     &config->notify,     sizeof(notify_config_t))     != 0);
    bool checkmk_was_enabled    = current_config.checkmk.enabled;
    bool mqtt_was_enabled       = current_config.mqtt.enabled;
    bool prometheus_was_enabled = current_config.prometheus.enabled;
    bool syslog_was_enabled     = current_config.syslog.enabled;
    bool notify_was_enabled     = current_config.notify.enabled;
    xSemaphoreGive(config_mutex);

    // Stop every changed old worker before starting any replacement. This
    // keeps peak task-stack/TLS heap bounded and prevents an old and new MQTT
    // handshake from overlapping during one configuration transaction.
    uint32_t bounded_stopped = 0;
    uint32_t bounded_started = 0;
    if (mqtt_changed && mqtt_was_enabled) {
        esp_err_t stop_result = mqtt_handler_stop();
        if (stop_result != ESP_OK) {
            ESP_LOGE(TAG, "Could not stop MQTT for config update: %s",
                     esp_err_to_name(stop_result));
            start_mqtt_when_network_ready(&current_config.mqtt);
            return stop_result;
        }
        bounded_stopped |= OTA_PAUSED_MQTT;
    }
    if (syslog_changed && syslog_was_enabled) {
        esp_err_t stop_result = syslog_stop();
        if (stop_result != ESP_OK) {
            ESP_LOGE(TAG, "Could not stop syslog for config update: %s",
                     esp_err_to_name(stop_result));
            // start() queues this snapshot if the old worker is still in its
            // cooperative cleanup path, preventing an enabled config from
            // becoming silently offline once that cleanup eventually ends.
            esp_err_t restore_result = syslog_start(&previous_config.syslog);
            if (restore_result != ESP_OK) {
                ESP_LOGE(TAG, "Could not queue previous Syslog config: %s",
                         esp_err_to_name(restore_result));
            }
            rollback_bounded_config_transitions(
                &previous_config, bounded_stopped, bounded_started);
            return stop_result;
        }
        bounded_stopped |= OTA_PAUSED_SYSLOG;
    }
    if (notify_changed && notify_was_enabled) {
        esp_err_t stop_result = events_stop();
        if (stop_result != ESP_OK) {
            ESP_LOGE(TAG, "Could not stop Events for config update: %s",
                     esp_err_to_name(stop_result));
            esp_err_t restore_result = events_start(&previous_config.notify);
            if (restore_result != ESP_OK) {
                ESP_LOGE(TAG, "Could not queue previous Events config: %s",
                         esp_err_to_name(restore_result));
            }
            rollback_bounded_config_transitions(
                &previous_config, bounded_stopped, bounded_started);
            return stop_result;
        }
        bounded_stopped |= OTA_PAUSED_NOTIFY;
    }
    if (prometheus_changed && prometheus_was_enabled) {
        esp_err_t stop_result = prometheus_stop();
        if (stop_result != ESP_OK) {
            ESP_LOGE(TAG, "Could not stop Prometheus for config update: %s",
                     esp_err_to_name(stop_result));
            esp_err_t restore_result =
                prometheus_start(&previous_config.prometheus);
            if (restore_result != ESP_OK) {
                ESP_LOGE(TAG,
                         "Could not queue previous Prometheus config: %s",
                         esp_err_to_name(restore_result));
            }
            rollback_bounded_config_transitions(
                &previous_config, bounded_stopped, bounded_started);
            return stop_result;
        }
        bounded_stopped |= OTA_PAUSED_PROMETHEUS;
    }
    if (checkmk_changed && checkmk_was_enabled) {
        esp_err_t stop_result = checkmk_stop();
        if (stop_result != ESP_OK) {
            ESP_LOGE(TAG, "Could not stop CheckMK for config update: %s",
                     esp_err_to_name(stop_result));
            (void)checkmk_start(&previous_config.checkmk);
            rollback_bounded_config_transitions(
                &previous_config, bounded_stopped, bounded_started);
            return stop_result;
        }
        bounded_stopped |= OTA_PAUSED_CHECKMK;
    }

    if (g_monitoring_ota_paused.load(std::memory_order_acquire)) {
        // A restart owns worker state now. Do not start replacements under a
        // netif which is about to be stopped; the imminent reboot restores
        // the still-committed NVS/current_config generation.
        return ESP_ERR_INVALID_STATE;
    }

    // All changed old workers are now gone. Start replacements one by one;
    // every return value participates in rollback before persistence.
    if (checkmk_changed && config->checkmk.enabled) {
        esp_err_t start_result = checkmk_start(&config->checkmk);
        if (start_result != ESP_OK) {
            ESP_LOGE(TAG, "Could not start updated CheckMK config: %s",
                     esp_err_to_name(start_result));
            rollback_bounded_config_transitions(
                &previous_config, bounded_stopped, bounded_started);
            return start_result;
        }
        bounded_started |= OTA_PAUSED_CHECKMK;
    }
    if (prometheus_changed && config->prometheus.enabled) {
        esp_err_t start_result = prometheus_start(&config->prometheus);
        if (start_result != ESP_OK) {
            ESP_LOGE(TAG, "Could not start updated Prometheus config: %s",
                     esp_err_to_name(start_result));
            rollback_bounded_config_transitions(
                &previous_config, bounded_stopped, bounded_started);
            return start_result;
        }
        bounded_started |= OTA_PAUSED_PROMETHEUS;
    }
    if (syslog_changed && config->syslog.enabled) {
        esp_err_t start_result = syslog_start(&config->syslog);
        if (start_result != ESP_OK) {
            ESP_LOGE(TAG, "Could not start updated Syslog config: %s",
                     esp_err_to_name(start_result));
            rollback_bounded_config_transitions(
                &previous_config, bounded_stopped, bounded_started);
            return start_result;
        }
        bounded_started |= OTA_PAUSED_SYSLOG;
    }
    if (notify_changed && config->notify.enabled) {
        esp_err_t start_result = events_start(&config->notify);
        if (start_result != ESP_OK) {
            ESP_LOGE(TAG, "Could not start updated Events config: %s",
                     esp_err_to_name(start_result));
            rollback_bounded_config_transitions(
                &previous_config, bounded_stopped, bounded_started);
            return start_result;
        }
        bounded_started |= OTA_PAUSED_NOTIFY;
    }
    if (mqtt_changed && config->mqtt.enabled) {
        // Start directly with the candidate snapshot. ESP-MQTT handles a
        // temporarily absent link itself; using the deferred boot helper here
        // would race GOT_IP against the not-yet-committed current_config.
        esp_err_t start_result = mqtt_handler_start(&config->mqtt);
        if (start_result != ESP_OK) {
            ESP_LOGE(TAG, "Could not start updated MQTT config: %s",
                     esp_err_to_name(start_result));
            rollback_bounded_config_transitions(
                &previous_config, bounded_stopped, bounded_started);
            return start_result;
        }
        bounded_started |= OTA_PAUSED_MQTT;
    }

    if (g_monitoring_ota_paused.load(std::memory_order_acquire)) {
        rollback_bounded_config_transitions(
            &previous_config, 0, bounded_started);
        return ESP_ERR_INVALID_STATE;
    }

    // Persist before publishing the new in-memory snapshot. NVS commit is the
    // last fallible step which can still be rolled back using current_config,
    // which deliberately remains the old configuration until this succeeds.
    esp_err_t save_result = save_config_to_nvs(config);
    if (save_result != ESP_OK) {
        ESP_LOGE(TAG, "Could not persist updated monitoring config: %s",
                 esp_err_to_name(save_result));
        rollback_bounded_config_transitions(
            &previous_config, bounded_stopped, bounded_started);
        return save_result;
    }

    // Commit the new config under mutex so concurrent GET requests see a
    // consistent snapshot (no partial memcpy visible)
    xSemaphoreTake(config_mutex, portMAX_DELAY);
    memcpy(&current_config, config, sizeof(monitoring_config_t));
    xSemaphoreGive(config_mutex);

    return ESP_OK;
}

static esp_err_t rollback_ota_pause(uint32_t paused_mask,
                                    const char *failed_service,
                                    esp_err_t stop_result)
{
    ESP_LOGE(TAG, "OTA pause failed while stopping %s: %s; resuming mask 0x%02x",
             failed_service, esp_err_to_name(stop_result),
             (unsigned)paused_mask);
    g_monitoring_ota_paused.store(false, std::memory_order_release);
    monitoring_resume_after_ota(paused_mask);
    return stop_result;
}

esp_err_t monitoring_pause_for_ota(uint32_t *paused_mask)
{
    if (!paused_mask) return ESP_ERR_INVALID_ARG;
    *paused_mask = 0;
    if (config_mutex == NULL) return ESP_ERR_INVALID_STATE;
    bool expected_pause = false;
    if (!g_monitoring_ota_paused.compare_exchange_strong(
            expected_pause, true, std::memory_order_acq_rel)) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(config_mutex, portMAX_DELAY);
    bool checkmk_enabled    = current_config.checkmk.enabled;
    bool prometheus_enabled = current_config.prometheus.enabled;
    bool syslog_enabled     = current_config.syslog.enabled;
    bool notify_enabled     = current_config.notify.enabled;
    bool mqtt_enabled       = current_config.mqtt.enabled;
    xSemaphoreGive(config_mutex);

    uint32_t paused = 0;
    ESP_LOGI(TAG, "Pausing monitoring for OTA (free heap: %u KB)",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024));

    if (mqtt_enabled) {
        mqtt_start_deferred.store(false);
        esp_err_t result = mqtt_handler_stop();
        if (result != ESP_OK) {
            return rollback_ota_pause(paused, "MQTT", result);
        }
        paused |= OTA_PAUSED_MQTT;
    }
    if (notify_enabled) {
        esp_err_t result = events_stop();
        if (result != ESP_OK) {
            return rollback_ota_pause(paused, "Events", result);
        }
        paused |= OTA_PAUSED_NOTIFY;
    }
    if (syslog_enabled) {
        esp_err_t result = syslog_stop();
        if (result != ESP_OK) {
            return rollback_ota_pause(paused, "Syslog", result);
        }
        paused |= OTA_PAUSED_SYSLOG;
    }
    if (prometheus_enabled) {
        esp_err_t result = prometheus_stop();
        if (result != ESP_OK) {
            return rollback_ota_pause(paused, "Prometheus", result);
        }
        paused |= OTA_PAUSED_PROMETHEUS;
    }
    if (checkmk_enabled) {
        esp_err_t result = checkmk_stop();
        if (result != ESP_OK) {
            return rollback_ota_pause(paused, "CheckMK", result);
        }
        paused |= OTA_PAUSED_CHECKMK;
    }

    ESP_LOGI(TAG, "Monitoring paused for OTA (mask=0x%02x, free heap: %u KB)",
             (unsigned)paused,
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024));
    *paused_mask = paused;
    return ESP_OK;
}

void monitoring_resume_after_ota(uint32_t paused_mask)
{
    g_monitoring_ota_paused.store(false, std::memory_order_release);
    if (paused_mask == 0 || config_mutex == NULL) return;

    // monitoring_config_t embeds roughly 6 KiB of MQTT certificate/key text.
    // This function can run on the 8 KiB HTTP server stack after a failed OTA,
    // so snapshot the small service configs individually and put MQTT's large
    // config on the heap only when it actually needs to be resumed.
    checkmk_config_t checkmk = {};
    prometheus_config_t prometheus = {};
    syslog_config_t syslog = {};
    notify_config_t notify = {};
    mqtt_config_t *mqtt = NULL;
    if (paused_mask & OTA_PAUSED_MQTT) {
        mqtt = static_cast<mqtt_config_t *>(malloc(sizeof(*mqtt)));
        if (!mqtt) {
            ESP_LOGE(TAG, "Could not allocate MQTT snapshot for OTA resume");
        }
    }
    xSemaphoreTake(config_mutex, portMAX_DELAY);
    memcpy(&checkmk, &current_config.checkmk, sizeof(checkmk));
    memcpy(&prometheus, &current_config.prometheus, sizeof(prometheus));
    memcpy(&syslog, &current_config.syslog, sizeof(syslog));
    memcpy(&notify, &current_config.notify, sizeof(notify));
    if (mqtt) memcpy(mqtt, &current_config.mqtt, sizeof(*mqtt));
    xSemaphoreGive(config_mutex);

    ESP_LOGI(TAG, "Resuming monitoring after failed OTA (mask=0x%02x)",
             (unsigned)paused_mask);

    if ((paused_mask & OTA_PAUSED_CHECKMK) && checkmk.enabled) {
        esp_err_t result = checkmk_start(&checkmk);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Could not resume CheckMK after OTA pause: %s",
                     esp_err_to_name(result));
        }
    }
    if ((paused_mask & OTA_PAUSED_PROMETHEUS) && prometheus.enabled) {
        esp_err_t result = prometheus_start(&prometheus);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Could not resume Prometheus after OTA pause: %s",
                     esp_err_to_name(result));
        }
    }
    if ((paused_mask & OTA_PAUSED_SYSLOG) && syslog.enabled) {
        esp_err_t result = syslog_start(&syslog);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Could not resume Syslog after OTA pause: %s",
                     esp_err_to_name(result));
        }
    }
    if ((paused_mask & OTA_PAUSED_NOTIFY) && notify.enabled) {
        esp_err_t result = events_start(&notify);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Could not resume Events after OTA pause: %s",
                     esp_err_to_name(result));
        }
    }
    if (mqtt && mqtt->enabled) {
        start_mqtt_when_network_ready(mqtt);
    }
    free(mqtt);
}

struct monitoring_update_job_t {
    monitoring_config_t config;
    monitoring_update_completion_t completion;
    void *completion_context;
};

// Task that applies a pending config update asynchronously
static void apply_config_task(void *pvParameters)
{
    monitoring_update_job_t *job =
        static_cast<monitoring_update_job_t *>(pvParameters);
    esp_err_t update_result = monitoring_update_config(&job->config);
    if (update_result != ESP_OK) {
        ESP_LOGE(TAG, "Asynchronous monitoring config update failed: %s",
                 esp_err_to_name(update_result));
    }

    monitoring_update_completion_t completion = job->completion;
    void *completion_context = job->completion_context;
    free(job);

    // Preserve the one-worker/OTA exclusion gate through final completion, so
    // a second request cannot overlap the first worker's response hand-off.
    if (completion != NULL) {
        completion(update_result, completion_context);
    }
    operation_finish(OperationState::MONITORING_UPDATE);
    vTaskDelete(NULL);
}

// Schedule configuration update asynchronously - returns immediately, update runs in background
esp_err_t monitoring_schedule_update_config(
    const monitoring_config_t *config,
    monitoring_update_completion_t completion,
    void *completion_context)
{
    if (config == NULL) return ESP_ERR_INVALID_ARG;
    if (config_mutex == NULL) return ESP_ERR_INVALID_STATE;
    // A single CAS chooses exactly one owner across both cores. This excludes
    // config writes from manual firmware uploads, restores and restarts without
    // relying on a racy pair of independently published boolean flags.
    if (!operation_try_begin(OperationState::MONITORING_UPDATE)) {
        ESP_LOGW(TAG, "Monitoring update rejected while another operation is active");
        return ESP_ERR_INVALID_STATE;
    }

    monitoring_update_job_t *job = static_cast<monitoring_update_job_t *>(
        malloc(sizeof(monitoring_update_job_t)));
    if (!job) {
        ESP_LOGE(TAG, "Failed to allocate memory for async config update");
        operation_finish(OperationState::MONITORING_UPDATE);
        return ESP_ERR_NO_MEM;
    }
    memcpy(&job->config, config, sizeof(job->config));
    job->completion = completion;
    job->completion_context = completion_context;

    // 8192 bytes: NVS write + MQTT client init/stop/start + CheckMK socket
    // operations require significantly more than 4096 bytes of stack.
    // Priority 3 (below httpd=5): ensures the httpd task can still serve HTTP
    // requests while the config update is in progress.
    BaseType_t ret = xTaskCreate(apply_config_task, "mon_update", 8192,
                                 job, 3, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create config update task");
        free(job);
        operation_finish(OperationState::MONITORING_UPDATE);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Config update scheduled (async)");
    return ESP_OK;
}

// Get current configuration
esp_err_t monitoring_get_config(monitoring_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(config_mutex, portMAX_DELAY);
    memcpy(config, &current_config, sizeof(monitoring_config_t));
    xSemaphoreGive(config_mutex);
    return ESP_OK;
}

esp_err_t monitoring_save_config_for_restore(const monitoring_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Do not publish the restore candidate into current_config before reboot.
    // Running workers still use the old snapshot, and restart cleanup decides
    // which workers to stop from current_config.enabled. Publishing the new
    // values here could therefore leave an old-but-now-disabled worker alive
    // while Ethernet is taken down.
    return save_config_to_nvs(config);
}

static esp_err_t tcp_probe_endpoint(const char *host, uint16_t port, int timeout_ms, char *message, size_t message_len)
{
    if (host == NULL || host[0] == '\0') {
        snprintf(message, message_len, "No server configured");
        return ESP_ERR_INVALID_ARG;
    }

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", port);

    struct addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *results = NULL;
    int gai_err = getaddrinfo(host, port_str, &hints, &results);
    if (gai_err != 0 || results == NULL) {
        snprintf(message, message_len, "DNS resolution failed for %s:%u", host, port);
        return ESP_FAIL;
    }

    esp_err_t probe_result = ESP_FAIL;

    for (struct addrinfo *addr = results; addr != NULL; addr = addr->ai_next) {
        int sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (sock < 0) {
            continue;
        }

        int flags = fcntl(sock, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        }

        int ret = connect(sock, addr->ai_addr, addr->ai_addrlen);
        if (ret == 0) {
            probe_result = ESP_OK;
        } else if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
            if (sock < 0 || sock >= FD_SETSIZE) {
                ESP_LOGE(TAG, "socket fd %d exceeds FD_SETSIZE %d", sock, FD_SETSIZE);
                close(sock);
                continue;
            }
            fd_set writefds;
            FD_ZERO(&writefds);
            FD_SET(sock, &writefds);

            struct timeval timeout = {
                .tv_sec = timeout_ms / 1000,
                .tv_usec = (timeout_ms % 1000) * 1000
            };

            ret = select(sock + 1, NULL, &writefds, NULL, &timeout);
            if (ret > 0) {
                int so_error = 0;
                socklen_t optlen = sizeof(so_error);
                if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &optlen) == 0 && so_error == 0) {
                    probe_result = ESP_OK;
                }
            }
        }

        close(sock);

        if (probe_result == ESP_OK) {
            snprintf(message, message_len, "TCP connection to %s:%u succeeded", host, port);
            break;
        }
    }

    freeaddrinfo(results);

    if (probe_result != ESP_OK) {
        snprintf(message, message_len, "TCP connection to %s:%u failed", host, port);
    }

    return probe_result;
}

esp_err_t monitoring_run_diagnostic(const char *target, bool *ok,
                                    char *code, size_t code_len,
                                    char *message, size_t message_len,
                                    char *host, size_t host_len,
                                    uint16_t *port,
                                    bool *tls_enabled)
{
    if (target == NULL || ok == NULL || code == NULL || message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *ok = false;
    code[0] = '\0';
    message[0] = '\0';
    if (host && host_len) host[0] = '\0';
    if (port) *port = 0;
    if (tls_enabled) *tls_enabled = false;

    // Snapshot the needed fields under the config mutex - apply_config_task
    // may memcpy the whole config struct concurrently, and tcp_probe_endpoint
    // below blocks for seconds, so don't read current_config directly.
    bool checkmk_enabled, mqtt_enabled, mqtt_tls, prom_enabled, syslog_enabled;
    bool notify_enabled;
    uint8_t notify_channels;
    uint16_t checkmk_port, mqtt_port, prom_port, syslog_port;
    char mqtt_server[sizeof(current_config.mqtt.server)];
    char syslog_server[sizeof(current_config.syslog.server)];

    if (config_mutex)
        xSemaphoreTake(config_mutex, portMAX_DELAY);
    checkmk_enabled = current_config.checkmk.enabled;
    checkmk_port = current_config.checkmk.port;
    mqtt_enabled = current_config.mqtt.enabled;
    mqtt_port = current_config.mqtt.port;
    mqtt_tls = current_config.mqtt.tls_enable;
    strncpy(mqtt_server, current_config.mqtt.server, sizeof(mqtt_server) - 1);
    mqtt_server[sizeof(mqtt_server) - 1] = '\0';
    prom_enabled = current_config.prometheus.enabled;
    prom_port = current_config.prometheus.port;
    syslog_enabled = current_config.syslog.enabled;
    syslog_port = current_config.syslog.port;
    strncpy(syslog_server, current_config.syslog.server, sizeof(syslog_server) - 1);
    syslog_server[sizeof(syslog_server) - 1] = '\0';
    notify_enabled = current_config.notify.enabled;
    notify_channels = current_config.notify.channels;
    if (config_mutex)
        xSemaphoreGive(config_mutex);

    if (strcmp(target, "checkmk") == 0) {
        if (!checkmk_enabled) {
            snprintf(code, code_len, "monitoring.diag.checkmk.disabled");
            snprintf(message, message_len, "CheckMK is disabled");
            return ESP_OK;
        }

        *ok = checkmk_running.load() && checkmk_listen_sock.load() >= 0;
        if (*ok) {
            snprintf(code, code_len, "monitoring.diag.checkmk.listening");
            snprintf(message, message_len, "CheckMK agent listening on TCP port %u", checkmk_port);
        } else {
            snprintf(code, code_len, "monitoring.diag.checkmk.not_ready");
            snprintf(message, message_len, "CheckMK is enabled but listener is not ready");
        }
        if (port) *port = checkmk_port;
        return ESP_OK;
    }

    if (strcmp(target, "mqtt") == 0) {
        if (tls_enabled) *tls_enabled = mqtt_tls;

        if (!mqtt_enabled) {
            snprintf(code, code_len, "monitoring.diag.mqtt.disabled");
            snprintf(message, message_len, "MQTT is disabled");
            if (host && host_len) {
                strncpy(host, mqtt_server, host_len - 1);
                host[host_len - 1] = '\0';
            }
            if (port) *port = mqtt_port;
            return ESP_OK;
        }

        if (host && host_len) {
            strncpy(host, mqtt_server, host_len - 1);
            host[host_len - 1] = '\0';
        }
        if (port) *port = mqtt_port;

        esp_err_t probe = tcp_probe_endpoint(mqtt_server, mqtt_port, 3000, message, message_len);
        *ok = (probe == ESP_OK);
        snprintf(code, code_len,
                 probe == ESP_OK ? "monitoring.diag.mqtt.tcp_ok" : "monitoring.diag.mqtt.tcp_failed");

        if (mqtt_tls) {
            // Append TLS note to the English fallback; the WebUI has its own
            // localized suffix and decides whether to append it based on the
            // tlsEnabled flag in the JSON response.
            size_t used = strlen(message);
            if (used < message_len) {
                snprintf(message + used, message_len - used, " (TLS enabled, cert validation not tested)");
            }
        }
        return ESP_OK;
    }

    if (strcmp(target, "prometheus") == 0) {
        if (!prom_enabled) {
            snprintf(code, code_len, "monitoring.diag.prometheus.disabled");
            snprintf(message, message_len, "Prometheus exporter is disabled");
            return ESP_OK;
        }

        *ok = prometheus_is_running();
        if (*ok) {
            snprintf(code, code_len, "monitoring.diag.prometheus.listening");
            snprintf(message, message_len, "Prometheus exporter listening on TCP port %u", prom_port);
        } else {
            snprintf(code, code_len, "monitoring.diag.prometheus.not_ready");
            snprintf(message, message_len, "Prometheus is enabled but listener is not ready");
        }
        if (port) *port = prom_port;
        return ESP_OK;
    }

    if (strcmp(target, "syslog") == 0) {
        if (!syslog_enabled) {
            snprintf(code, code_len, "monitoring.diag.syslog.disabled");
            snprintf(message, message_len, "Syslog forwarding is disabled");
            return ESP_OK;
        }

        if (host && host_len) {
            strncpy(host, syslog_server, host_len - 1);
            host[host_len - 1] = '\0';
        }
        if (port) *port = syslog_port;

        // Probe the syslog server with a TCP connect (UDP is fire-and-forget;
        // there is no handshake to test). For UDP we just verify DNS + a
        // best-effort UDP socket bind.
        esp_err_t probe = tcp_probe_endpoint(syslog_server, syslog_port, 3000, message, message_len);
        *ok = (probe == ESP_OK);
        snprintf(code, code_len,
                 probe == ESP_OK ? "monitoring.diag.syslog.tcp_ok" : "monitoring.diag.syslog.tcp_failed");
        return ESP_OK;
    }

    if (strcmp(target, "notify") == 0) {
        if (!notify_enabled) {
            snprintf(code, code_len, "monitoring.diag.notify.disabled");
            snprintf(message, message_len, "Notifications are disabled");
            return ESP_OK;
        }
        // Emit a test event on every enabled channel. The result is async;
        // we report "queued" and the user can check the metric counters or
        // the receiving end to confirm delivery.
        events_emit_test();
        *ok = true;
        snprintf(code, code_len, "monitoring.diag.notify.queued");
        snprintf(message, message_len,
                 "Test notification queued for channels bitmask 0x%02x",
                 notify_channels);
        return ESP_OK;
    }

    snprintf(code, code_len, "monitoring.diag.unsupported");
    snprintf(message, message_len, "Unknown diagnostic target");
    return ESP_ERR_NOT_SUPPORTED;
}
