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
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "esp_app_format.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <atomic>
#include "ethernet.h"
#include "radiomoduledetector.h"
#include "systemclock.h"
#include "reset_info.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "updatecheck.h"

static const char *TAG = "MONITORING";
SemaphoreHandle_t g_net_fetch_mutex = NULL;
static StaticSemaphore_t net_fetch_mutex_buffer;

// True while an OTA firmware download is streaming. See net_fetch_set_ota_active().
static std::atomic<bool> g_ota_active{false};

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
static std::atomic<bool> update_in_progress{false};
static std::atomic<TaskHandle_t> checkmk_task_handle{NULL};
static std::atomic<int> checkmk_listen_sock{-1};
// Currently-connected client socket, or -1. Tracked atomically so that
// checkmk_stop() can close it before force-deleting the task. Without this,
// if vTaskDelete() strikes between accept() returning a fd and close() being
// called on it, the fd leaks. lwIP has a small fd table; over repeated
// monitoring config saves (which stop/start the agent) the table fills and
// socket() starts failing firmware-wide (WebUI, MQTT, syslog, OTA) —
// presenting exactly as "device becomes unreachable after some time".
static std::atomic<int> checkmk_client_sock{-1};

enum OtaPausedService : uint32_t {
    OTA_PAUSED_CHECKMK    = 1u << 0,
    OTA_PAUSED_PROMETHEUS = 1u << 1,
    OTA_PAUSED_SYSLOG     = 1u << 2,
    OTA_PAUSED_NOTIFY     = 1u << 3,
    OTA_PAUSED_MQTT       = 1u << 4,
};

// NVS keys
#define NVS_NAMESPACE "monitoring"
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
static UpdateCheck* g_updateCheck = NULL;
static Ethernet* g_ethernet = NULL;
static RadioModuleDetector* g_radioModuleDetector = NULL;
static SystemClock* g_systemClock = NULL;
static Settings* g_settings = NULL;

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

UpdateCheck* monitoring_get_updatecheck(void) {
    return g_updateCheck;
}

// CheckMK Agent Task
static void checkmk_agent_task(void *pvParameters)
{
    const checkmk_config_t *config = (const checkmk_config_t *)pvParameters;
    struct sockaddr_in server_addr;

    ESP_LOGI(TAG, "CheckMK Agent starting on port %d", config->port);

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    checkmk_listen_sock.store(listen_sock);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket");
        checkmk_running.store(false);
        checkmk_task_handle.store(NULL);
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(config->port);

    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Socket bind failed");
        close(listen_sock);
        checkmk_running.store(false);
        checkmk_listen_sock.store(-1);
        checkmk_task_handle.store(NULL);
        vTaskDelete(NULL);
        return;
    }

    if (listen(listen_sock, 5) < 0) {
        ESP_LOGE(TAG, "Socket listen failed");
        close(listen_sock);
        checkmk_running.store(false);
        checkmk_listen_sock.store(-1);
        checkmk_task_handle.store(NULL);
        vTaskDelete(NULL);
        return;
    }

    // Periodic timeout so the task can check checkmk_running even if no
    // client connects and even if shutdown() does not unblock accept() on
    // this lwIP version.
    struct timeval accept_tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(listen_sock, SOL_SOCKET, SO_RCVTIMEO, &accept_tv, sizeof(accept_tv));

    ESP_LOGI(TAG, "CheckMK Agent listening on port %d", config->port);

    while (checkmk_running.load()) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            // EAGAIN/EWOULDBLOCK = 1s timeout expired, just re-check the loop condition
            if (checkmk_running.load() && errno != EAGAIN && errno != EWOULDBLOCK) {
                ESP_LOGE(TAG, "Accept failed: errno %d", errno);
            }
            continue;
        }

        // Publish the fd so checkmk_stop() can close it before force-deleting
        // us. Cleared back to -1 once we close() the socket ourselves below.
        checkmk_client_sock.store(client_sock);

        char client_ip[16];
        inet_ntoa_r(client_addr.sin_addr, client_ip, sizeof(client_ip));
        ESP_LOGI(TAG, "CheckMK client connected from %s", client_ip);

        // Check if client IP is allowed (exact match per comma-separated entry)
        bool allowed = false;
        if (strlen(config->allowed_hosts) == 0 || strcmp(config->allowed_hosts, "*") == 0) {
            allowed = true;
        } else {
            // Parse comma-separated list and match each entry exactly
            char hosts_copy[sizeof(config->allowed_hosts)];
            strncpy(hosts_copy, config->allowed_hosts, sizeof(hosts_copy) - 1);
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
            checkmk_client_sock.store(-1);
            close(client_sock);
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

        checkmk_client_sock.store(-1);
        close(client_sock);
        ESP_LOGI(TAG, "CheckMK client disconnected");
    }

    if (checkmk_listen_sock.load() >= 0) {
        close(listen_sock);
        checkmk_listen_sock.store(-1);
    }
    ESP_LOGI(TAG, "CheckMK Agent stopped");
    checkmk_running.store(false);
    checkmk_task_handle.store(NULL);
    vTaskDelete(NULL);
}

// CheckMK Functions
esp_err_t checkmk_start(const checkmk_config_t *config)
{
    if (checkmk_running.load()) {
        ESP_LOGW(TAG, "CheckMK agent already running");
        return ESP_OK;
    }

    if (!config->enabled) {
        return ESP_OK;
    }

    checkmk_running.store(true);

    // Create CheckMK agent task - pass pointer to current_config
    // 8192 bytes: large output buffer (2048) + sockaddr/IP string operations
    // xTaskCreate requires a non-volatile TaskHandle_t*; assign to volatile global afterwards.
    TaskHandle_t cmk_handle = NULL;
    BaseType_t ret = xTaskCreate(checkmk_agent_task, "checkmk_agent", 8192,
                                  (void *)&current_config.checkmk, 5, &cmk_handle);
    checkmk_task_handle.store(cmk_handle);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create CheckMK agent task");
        checkmk_running.store(false);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t checkmk_stop(void)
{
    if (!checkmk_running.load()) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping CheckMK agent");
    checkmk_running.store(false);

    // shutdown() + close() to unblock accept(). shutdown() is more reliable
    // than close() alone for interrupting a blocking accept() call on lwIP.
    // The SO_RCVTIMEO set in the task ensures it wakes up within 1s regardless.
    int listen_sock = checkmk_listen_sock.exchange(-1);
    if (listen_sock >= 0) {
        shutdown(listen_sock, SHUT_RDWR);
        close(listen_sock);
    }

    // Wait up to 2.5s for the task to exit (accept timeout is 1s, so it
    // will detect checkmk_running==false within one timeout cycle).
    for (int i = 0; i < 25 && checkmk_task_handle.load() != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Use a local snapshot of the handle to avoid a race where the task
    // sets checkmk_task_handle=NULL between our NULL check and vTaskDelete().
    TaskHandle_t cmk_handle = checkmk_task_handle.load();
    if (cmk_handle != NULL) {
        ESP_LOGW(TAG, "CheckMK task did not exit cleanly, force deleting");
        // Before force-deleting, claim and close any in-flight client socket.
        // If the task is wedged in send()/recv() on client_sock right now,
        // vTaskDelete would leak that fd into the lwIP table permanently.
        int cs = checkmk_client_sock.exchange(-1);
        if (cs >= 0) {
            shutdown(cs, SHUT_RDWR);
            close(cs);
        }
        checkmk_task_handle.store(NULL);
        vTaskDelete(cmk_handle);
    }

    return ESP_OK;
}

// Save configuration to NVS
static esp_err_t save_config_to_nvs(const monitoring_config_t *config)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

#define NVS_CHECK(expr, label) \
    do { \
        if ((err = (expr)) != ESP_OK) { \
            ESP_LOGW(TAG, "NVS write error on %s: %s", label, esp_err_to_name(err)); \
        } \
    } while (0)

    // Save CheckMK config
    NVS_CHECK(nvs_set_u8(nvs_handle, NVS_CHECKMK_ENABLED, config->checkmk.enabled), "checkmk enabled");
    NVS_CHECK(nvs_set_u16(nvs_handle, NVS_CHECKMK_PORT, config->checkmk.port), "checkmk port");
    NVS_CHECK(nvs_set_str(nvs_handle, NVS_CHECKMK_HOSTS, config->checkmk.allowed_hosts), "checkmk hosts");

    // Save MQTT config
    NVS_CHECK(nvs_set_u8(nvs_handle, NVS_MQTT_ENABLED, config->mqtt.enabled), "mqtt enabled");
    NVS_CHECK(nvs_set_str(nvs_handle, NVS_MQTT_SERVER, config->mqtt.server), "mqtt server");
    NVS_CHECK(nvs_set_u16(nvs_handle, NVS_MQTT_PORT, config->mqtt.port), "mqtt port");
    NVS_CHECK(nvs_set_str(nvs_handle, NVS_MQTT_USER, config->mqtt.user), "mqtt user");
    NVS_CHECK(nvs_set_str(nvs_handle, NVS_MQTT_PASS, config->mqtt.password), "mqtt pass");
    NVS_CHECK(nvs_set_str(nvs_handle, NVS_MQTT_PREFIX, config->mqtt.topic_prefix), "mqtt prefix");
    NVS_CHECK(nvs_set_u8(nvs_handle, NVS_MQTT_HA_ENABLED, config->mqtt.ha_discovery_enabled), "mqtt ha");
    NVS_CHECK(nvs_set_str(nvs_handle, NVS_MQTT_HA_PREFIX, config->mqtt.ha_discovery_prefix), "mqtt ha prefix");

    NVS_CHECK(nvs_set_u8(nvs_handle, NVS_MQTT_TLS_EN, config->mqtt.tls_enable), "mqtt tls en");
    NVS_CHECK(nvs_set_u8(nvs_handle, NVS_MQTT_TLS_SKIP, config->mqtt.tls_skip_verify), "mqtt tls skip");
    NVS_CHECK(nvs_set_blob(nvs_handle, NVS_MQTT_TLS_CA,  config->mqtt.tls_ca_certs,  strlen(config->mqtt.tls_ca_certs) + 1), "mqtt tls ca");
    NVS_CHECK(nvs_set_blob(nvs_handle, NVS_MQTT_TLS_CRT, config->mqtt.tls_certfile,  strlen(config->mqtt.tls_certfile) + 1), "mqtt tls crt");
    NVS_CHECK(nvs_set_blob(nvs_handle, NVS_MQTT_TLS_KEY, config->mqtt.tls_keyfile,   strlen(config->mqtt.tls_keyfile) + 1), "mqtt tls key");

    // Command-topic security (Phase A). Default for command_enabled is true
    // so existing installations keep working after the upgrade. The token
    // is optional; if empty no token check is applied.
    NVS_CHECK(nvs_set_u8(nvs_handle, NVS_MQTT_CMD_EN, config->mqtt.command_enabled ? 1 : 0), "mqtt cmd en");
    NVS_CHECK(nvs_set_str(nvs_handle, NVS_MQTT_CMD_TOK, config->mqtt.command_token), "mqtt cmd tok");

    // Prometheus (Phase A)
    NVS_CHECK(nvs_set_u8(nvs_handle, NVS_PROM_ENABLED, config->prometheus.enabled ? 1 : 0), "prom en");
    NVS_CHECK(nvs_set_u16(nvs_handle, NVS_PROM_PORT, config->prometheus.port), "prom port");
    NVS_CHECK(nvs_set_str(nvs_handle, NVS_PROM_HOSTS, config->prometheus.allowed_hosts), "prom hosts");

    // Syslog (Phase B)
    NVS_CHECK(nvs_set_u8(nvs_handle, NVS_SYSLOG_ENABLED, config->syslog.enabled ? 1 : 0), "syslog en");
    NVS_CHECK(nvs_set_str(nvs_handle, NVS_SYSLOG_SERVER, config->syslog.server), "syslog srv");
    NVS_CHECK(nvs_set_u16(nvs_handle, NVS_SYSLOG_PORT, config->syslog.port), "syslog port");
    NVS_CHECK(nvs_set_u8(nvs_handle, NVS_SYSLOG_XPORT, config->syslog.transport), "syslog xport");
    NVS_CHECK(nvs_set_u8(nvs_handle, NVS_SYSLOG_SEV, config->syslog.min_severity), "syslog sev");
    NVS_CHECK(nvs_set_str(nvs_handle, NVS_SYSLOG_HOST, config->syslog.hostname), "syslog host");

    // Notifications (Phase C/D) — secrets stored as plain NVS strings; the
    // NVS partition is not encrypted by default on this build, matching how
    // the MQTT password is already handled.
    NVS_CHECK(nvs_set_u8(nvs_handle, NVS_NOTIFY_ENABLED, config->notify.enabled ? 1 : 0), "notify en");
    NVS_CHECK(nvs_set_u8(nvs_handle, NVS_NOTIFY_CHANS, config->notify.channels), "notify chans");
    NVS_CHECK(nvs_set_str(nvs_handle, NVS_NOTIFY_WHOOK, config->notify.webhook_url), "notify whook");
    NVS_CHECK(nvs_set_str(nvs_handle, NVS_NOTIFY_WSECRET, config->notify.webhook_secret), "notify wsecret");
    NVS_CHECK(nvs_set_str(nvs_handle, NVS_NOTIFY_TGTOKEN, config->notify.telegram_token), "notify tg token");
    NVS_CHECK(nvs_set_str(nvs_handle, NVS_NOTIFY_TGCHAT, config->notify.telegram_chatid), "notify tg chat");
    NVS_CHECK(nvs_set_str(nvs_handle, NVS_NOTIFY_SMTPSRV, config->notify.smtp_server), "notify smtp srv");
    NVS_CHECK(nvs_set_u16(nvs_handle, NVS_NOTIFY_SMTPPORT, config->notify.smtp_port), "notify smtp port");
    NVS_CHECK(nvs_set_u8(nvs_handle, NVS_NOTIFY_SMTPTLS, config->notify.smtp_tls), "notify smtp tls");
    NVS_CHECK(nvs_set_str(nvs_handle, NVS_NOTIFY_SMTPUSER, config->notify.smtp_user), "notify smtp user");
    NVS_CHECK(nvs_set_str(nvs_handle, NVS_NOTIFY_SMTPPW, config->notify.smtp_password), "notify smtp pw");
    NVS_CHECK(nvs_set_str(nvs_handle, NVS_NOTIFY_SMTPFROM, config->notify.smtp_from), "notify smtp from");
    NVS_CHECK(nvs_set_str(nvs_handle, NVS_NOTIFY_SMTPTO, config->notify.smtp_to), "notify smtp to");
    NVS_CHECK(nvs_set_u16(nvs_handle, NVS_NOTIFY_COOLDOWN, config->notify.cooldown_seconds), "notify cd");

#undef NVS_CHECK

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    return err;
}

// Load configuration from NVS
static esp_err_t load_config_from_nvs(monitoring_config_t *config)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No saved configuration found, using defaults");
        // Set defaults
        config->checkmk.enabled = false;
        config->checkmk.port = 6556;
        strncpy(config->checkmk.allowed_hosts, "*", sizeof(config->checkmk.allowed_hosts) - 1);
        config->checkmk.allowed_hosts[sizeof(config->checkmk.allowed_hosts) - 1] = '\0';

        config->mqtt.enabled = false;
        config->mqtt.port = 1883;
        strncpy(config->mqtt.topic_prefix, "hb-rf-eth-ng", sizeof(config->mqtt.topic_prefix) - 1);
        config->mqtt.topic_prefix[sizeof(config->mqtt.topic_prefix) - 1] = '\0';
        config->mqtt.ha_discovery_enabled = false;
        strncpy(config->mqtt.ha_discovery_prefix, "homeassistant", sizeof(config->mqtt.ha_discovery_prefix) - 1);
        config->mqtt.ha_discovery_prefix[sizeof(config->mqtt.ha_discovery_prefix) - 1] = '\0';

        config->mqtt.tls_enable = false;
        config->mqtt.tls_skip_verify = false;
        config->mqtt.tls_ca_certs[0] = '\0';
        config->mqtt.tls_certfile[0] = '\0';
        config->mqtt.tls_keyfile[0] = '\0';

        // Phase A defaults: commands enabled, no token. This preserves the
        // pre-Phase-A behaviour where any client with broker publish rights
        // could trigger a restart / OTA. Operators who care should set a
        // token or restrict the broker ACL.
        config->mqtt.command_enabled = true;
        config->mqtt.command_token[0] = '\0';

        // Phase A: Prometheus defaults — disabled, port 9100, allow all.
        config->prometheus.enabled = false;
        config->prometheus.port = 9100;
        strncpy(config->prometheus.allowed_hosts, "*", sizeof(config->prometheus.allowed_hosts) - 1);
        config->prometheus.allowed_hosts[sizeof(config->prometheus.allowed_hosts) - 1] = '\0';

        // Phase B: Syslog defaults — disabled, UDP/514, severity INFO (6).
        config->syslog.enabled = false;
        config->syslog.server[0] = '\0';
        config->syslog.port = 514;
        config->syslog.transport = 0;  // UDP
        config->syslog.min_severity = 6;
        config->syslog.hostname[0] = '\0';

        // Phase C/D: Notification defaults — disabled, no channels, 5 min cooldown.
        config->notify.enabled = false;
        config->notify.channels = 0;
        config->notify.webhook_url[0] = '\0';
        config->notify.webhook_secret[0] = '\0';
        config->notify.telegram_token[0] = '\0';
        config->notify.telegram_chatid[0] = '\0';
        config->notify.smtp_server[0] = '\0';
        config->notify.smtp_port = 587;
        config->notify.smtp_tls = 1;   // STARTTLS
        config->notify.smtp_user[0] = '\0';
        config->notify.smtp_password[0] = '\0';
        config->notify.smtp_from[0] = '\0';
        config->notify.smtp_to[0] = '\0';
        config->notify.cooldown_seconds = 300;

        return ESP_OK;
    }

    // Load CheckMK config
    uint8_t u8_val;
    uint16_t u16_val;
    size_t str_len;
    if (nvs_get_u8(nvs_handle, NVS_CHECKMK_ENABLED, &u8_val) == ESP_OK) {
        config->checkmk.enabled = u8_val;
    }

    if (nvs_get_u16(nvs_handle, NVS_CHECKMK_PORT, &u16_val) == ESP_OK) {
        config->checkmk.port = u16_val;
    }

    str_len = sizeof(config->checkmk.allowed_hosts);
    nvs_get_str(nvs_handle, NVS_CHECKMK_HOSTS, config->checkmk.allowed_hosts, &str_len);

    // Load MQTT config
    if (nvs_get_u8(nvs_handle, NVS_MQTT_ENABLED, &u8_val) == ESP_OK) {
        config->mqtt.enabled = u8_val;
    }

    str_len = sizeof(config->mqtt.server);
    if (nvs_get_str(nvs_handle, NVS_MQTT_SERVER, config->mqtt.server, &str_len) != ESP_OK) {
        config->mqtt.server[0] = 0;
    }

    if (nvs_get_u16(nvs_handle, NVS_MQTT_PORT, &u16_val) == ESP_OK) {
        config->mqtt.port = u16_val;
    } else {
        config->mqtt.port = 1883;
    }

    str_len = sizeof(config->mqtt.user);
    if (nvs_get_str(nvs_handle, NVS_MQTT_USER, config->mqtt.user, &str_len) != ESP_OK) {
        config->mqtt.user[0] = 0;
    }

    str_len = sizeof(config->mqtt.password);
    if (nvs_get_str(nvs_handle, NVS_MQTT_PASS, config->mqtt.password, &str_len) != ESP_OK) {
        config->mqtt.password[0] = 0;
    }

    str_len = sizeof(config->mqtt.topic_prefix);
    if (nvs_get_str(nvs_handle, NVS_MQTT_PREFIX, config->mqtt.topic_prefix, &str_len) != ESP_OK) {
        strncpy(config->mqtt.topic_prefix, "hb-rf-eth-ng", sizeof(config->mqtt.topic_prefix) - 1);
        config->mqtt.topic_prefix[sizeof(config->mqtt.topic_prefix) - 1] = '\0';
    } else if (strcmp(config->mqtt.topic_prefix, "hb-rf-eth") == 0) {
        // One-time migration: the old firmware default was "hb-rf-eth".
        // Update devices that still carry the legacy preset to the new
        // "hb-rf-eth-ng" default. Custom prefixes are left untouched.
        strncpy(config->mqtt.topic_prefix, "hb-rf-eth-ng", sizeof(config->mqtt.topic_prefix) - 1);
        config->mqtt.topic_prefix[sizeof(config->mqtt.topic_prefix) - 1] = '\0';
        nvs_set_str(nvs_handle, NVS_MQTT_PREFIX, config->mqtt.topic_prefix);
    }

    if (nvs_get_u8(nvs_handle, NVS_MQTT_HA_ENABLED, &u8_val) == ESP_OK) {
        config->mqtt.ha_discovery_enabled = u8_val;
    } else {
        config->mqtt.ha_discovery_enabled = false;
    }

    str_len = sizeof(config->mqtt.ha_discovery_prefix);
    if (nvs_get_str(nvs_handle, NVS_MQTT_HA_PREFIX, config->mqtt.ha_discovery_prefix, &str_len) != ESP_OK) {
        strncpy(config->mqtt.ha_discovery_prefix, "homeassistant", sizeof(config->mqtt.ha_discovery_prefix) - 1);
        config->mqtt.ha_discovery_prefix[sizeof(config->mqtt.ha_discovery_prefix) - 1] = '\0';
    }

    if (nvs_get_u8(nvs_handle, NVS_MQTT_TLS_EN, &u8_val) == ESP_OK) {
        config->mqtt.tls_enable = u8_val;
    } else {
        config->mqtt.tls_enable = false;
    }
    if (nvs_get_u8(nvs_handle, NVS_MQTT_TLS_SKIP, &u8_val) == ESP_OK) {
        config->mqtt.tls_skip_verify = u8_val;
    } else {
        config->mqtt.tls_skip_verify = false;
    }
    size_t blob_len = sizeof(config->mqtt.tls_ca_certs);
    if (nvs_get_blob(nvs_handle, NVS_MQTT_TLS_CA, config->mqtt.tls_ca_certs, &blob_len) != ESP_OK) {
        config->mqtt.tls_ca_certs[0] = '\0';
    }
    blob_len = sizeof(config->mqtt.tls_certfile);
    if (nvs_get_blob(nvs_handle, NVS_MQTT_TLS_CRT, config->mqtt.tls_certfile, &blob_len) != ESP_OK) {
        config->mqtt.tls_certfile[0] = '\0';
    }
    blob_len = sizeof(config->mqtt.tls_keyfile);
    if (nvs_get_blob(nvs_handle, NVS_MQTT_TLS_KEY, config->mqtt.tls_keyfile, &blob_len) != ESP_OK) {
        config->mqtt.tls_keyfile[0] = '\0';
    }

    // Phase A: command-topic security. command_enabled defaults to true for
    // upgrades from a pre-Phase-A build (no NVS key present yet) so that
    // existing MQTT integrations do not silently lose restart/update.
    if (nvs_get_u8(nvs_handle, NVS_MQTT_CMD_EN, &u8_val) == ESP_OK) {
        config->mqtt.command_enabled = (u8_val != 0);
    } else {
        config->mqtt.command_enabled = true;
    }
    str_len = sizeof(config->mqtt.command_token);
    if (nvs_get_str(nvs_handle, NVS_MQTT_CMD_TOK, config->mqtt.command_token, &str_len) != ESP_OK) {
        config->mqtt.command_token[0] = '\0';
    }

    // ---- Prometheus (Phase A) ----
    // Defaults for upgrades: disabled, port 9100, allow all.
    if (nvs_get_u8(nvs_handle, NVS_PROM_ENABLED, &u8_val) == ESP_OK) {
        config->prometheus.enabled = (u8_val != 0);
    } else {
        config->prometheus.enabled = false;
    }
    if (nvs_get_u16(nvs_handle, NVS_PROM_PORT, &u16_val) == ESP_OK) {
        config->prometheus.port = u16_val;
    } else {
        config->prometheus.port = 9100;
    }
    str_len = sizeof(config->prometheus.allowed_hosts);
    if (nvs_get_str(nvs_handle, NVS_PROM_HOSTS, config->prometheus.allowed_hosts, &str_len) != ESP_OK) {
        strncpy(config->prometheus.allowed_hosts, "*", sizeof(config->prometheus.allowed_hosts) - 1);
        config->prometheus.allowed_hosts[sizeof(config->prometheus.allowed_hosts) - 1] = '\0';
    }

    // ---- Syslog (Phase B) ----
    if (nvs_get_u8(nvs_handle, NVS_SYSLOG_ENABLED, &u8_val) == ESP_OK) {
        config->syslog.enabled = (u8_val != 0);
    } else {
        config->syslog.enabled = false;
    }
    str_len = sizeof(config->syslog.server);
    if (nvs_get_str(nvs_handle, NVS_SYSLOG_SERVER, config->syslog.server, &str_len) != ESP_OK) {
        config->syslog.server[0] = '\0';
    }
    if (nvs_get_u16(nvs_handle, NVS_SYSLOG_PORT, &u16_val) == ESP_OK) {
        config->syslog.port = u16_val;
    } else {
        config->syslog.port = 514;
    }
    if (nvs_get_u8(nvs_handle, NVS_SYSLOG_XPORT, &u8_val) == ESP_OK) {
        config->syslog.transport = u8_val;
    } else {
        config->syslog.transport = 0;
    }
    if (nvs_get_u8(nvs_handle, NVS_SYSLOG_SEV, &u8_val) == ESP_OK) {
        config->syslog.min_severity = u8_val;
    } else {
        config->syslog.min_severity = 6;
    }
    str_len = sizeof(config->syslog.hostname);
    if (nvs_get_str(nvs_handle, NVS_SYSLOG_HOST, config->syslog.hostname, &str_len) != ESP_OK) {
        config->syslog.hostname[0] = '\0';
    }

    // ---- Notifications (Phase C/D) ----
    if (nvs_get_u8(nvs_handle, NVS_NOTIFY_ENABLED, &u8_val) == ESP_OK) {
        config->notify.enabled = (u8_val != 0);
    } else {
        config->notify.enabled = false;
    }
    if (nvs_get_u8(nvs_handle, NVS_NOTIFY_CHANS, &u8_val) == ESP_OK) {
        config->notify.channels = u8_val;
    } else {
        config->notify.channels = 0;
    }
    str_len = sizeof(config->notify.webhook_url);
    nvs_get_str(nvs_handle, NVS_NOTIFY_WHOOK, config->notify.webhook_url, &str_len);
    if (config->notify.webhook_url[0] == 0 && str_len == 0) config->notify.webhook_url[0] = '\0';
    str_len = sizeof(config->notify.webhook_secret);
    nvs_get_str(nvs_handle, NVS_NOTIFY_WSECRET, config->notify.webhook_secret, &str_len);
    str_len = sizeof(config->notify.telegram_token);
    nvs_get_str(nvs_handle, NVS_NOTIFY_TGTOKEN, config->notify.telegram_token, &str_len);
    str_len = sizeof(config->notify.telegram_chatid);
    nvs_get_str(nvs_handle, NVS_NOTIFY_TGCHAT, config->notify.telegram_chatid, &str_len);
    str_len = sizeof(config->notify.smtp_server);
    nvs_get_str(nvs_handle, NVS_NOTIFY_SMTPSRV, config->notify.smtp_server, &str_len);
    if (nvs_get_u16(nvs_handle, NVS_NOTIFY_SMTPPORT, &u16_val) == ESP_OK) {
        config->notify.smtp_port = u16_val;
    } else {
        config->notify.smtp_port = 587;
    }
    if (nvs_get_u8(nvs_handle, NVS_NOTIFY_SMTPTLS, &u8_val) == ESP_OK) {
        config->notify.smtp_tls = u8_val;
    } else {
        config->notify.smtp_tls = 1;
    }
    str_len = sizeof(config->notify.smtp_user);
    nvs_get_str(nvs_handle, NVS_NOTIFY_SMTPUSER, config->notify.smtp_user, &str_len);
    str_len = sizeof(config->notify.smtp_password);
    nvs_get_str(nvs_handle, NVS_NOTIFY_SMTPPW, config->notify.smtp_password, &str_len);
    str_len = sizeof(config->notify.smtp_from);
    nvs_get_str(nvs_handle, NVS_NOTIFY_SMTPFROM, config->notify.smtp_from, &str_len);
    str_len = sizeof(config->notify.smtp_to);
    nvs_get_str(nvs_handle, NVS_NOTIFY_SMTPTO, config->notify.smtp_to, &str_len);
    if (nvs_get_u16(nvs_handle, NVS_NOTIFY_COOLDOWN, &u16_val) == ESP_OK) {
        config->notify.cooldown_seconds = u16_val;
    } else {
        config->notify.cooldown_seconds = 300;
    }

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

        // Skip checks during an active OTA: TLS handshakes, HTTP buffers and
        // flash writes routinely push free heap below the threshold, and a
        // restart here would interrupt and fail the firmware upgrade.
        if (g_updateCheck)
        {
            OtaSnapshot ota = g_updateCheck->getOtaState();
            if (ota.state == OTA_STATE_STARTING ||
                ota.state == OTA_STATE_DOWNLOADING ||
                ota.state == OTA_STATE_FLASHING)
            {
                low_heap_streak = 0;
                continue;
            }
        }

        size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        if (free_heap < HEAP_WATCHDOG_CRITICAL_BYTES)
        {
            low_heap_streak++;
            ESP_LOGW(TAG, "Low heap: %u bytes free (streak %d/%d)",
                     (unsigned)free_heap, low_heap_streak, HEAP_WATCHDOG_CONSECUTIVE_HITS);

            if (low_heap_streak >= HEAP_WATCHDOG_CONSECUTIVE_HITS)
            {
                size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
                size_t min_ever = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
                uint32_t secs = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000ULL);
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

// Initialize monitoring subsystem
esp_err_t monitoring_init(const monitoring_config_t *config, SysInfo* sysInfo, UpdateCheck* updateCheck)
{
    ESP_LOGI(TAG, "Initializing monitoring subsystem");

    // This lock protects the ESP32's limited heap from concurrent TLS
    // handshakes. Reserve it statically so low heap can never disable the
    // serialization exactly when it is needed most.
    g_net_fetch_mutex = xSemaphoreCreateMutexStatic(&net_fetch_mutex_buffer);
    if (g_net_fetch_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create net-fetch mutex");
        return ESP_ERR_NO_MEM;
    }

    config_mutex = xSemaphoreCreateMutex();
    if (config_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create config mutex");
        return ESP_ERR_NO_MEM;
    }

    g_sysInfo = sysInfo;
    g_updateCheck = updateCheck;

    // Initialize MQTT handler
    mqtt_handler_init();

    if (config == NULL) {
        // Load from NVS
        load_config_from_nvs(&current_config);
    } else {
        memcpy(&current_config, config, sizeof(monitoring_config_t));
        save_config_to_nvs(&current_config);
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

    // Start MQTT if enabled
    if (current_config.mqtt.enabled) {
        mqtt_handler_start(&current_config.mqtt);
    }

    // Heap watchdog runs regardless of monitoring config - it's a safety
    // net for the whole firmware, not a monitoring feature.
    BaseType_t wd_ret = xTaskCreate(heap_watchdog_task, "heap_watchdog", 4096, NULL, 2, NULL);
    if (wd_ret != pdPASS) {
        ESP_LOGW(TAG, "Failed to start heap watchdog task");
    }

    return ESP_OK;
}

// Update configuration
esp_err_t monitoring_update_config(const monitoring_config_t *config)
{
    if (config == NULL) return ESP_ERR_INVALID_ARG;
    if (config_mutex == NULL) return ESP_ERR_INVALID_STATE;

    // Take a snapshot of the current config under mutex to determine what changed.
    // Release the mutex before any blocking stop/start calls so GET requests
    // are never blocked for more than a memcpy duration.
    xSemaphoreTake(config_mutex, portMAX_DELAY);
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

    // Stop only what needs to change (these calls can block; no mutex held)
    if (checkmk_changed    && checkmk_was_enabled)    checkmk_stop();
    if (mqtt_changed       && mqtt_was_enabled)       mqtt_handler_stop();
    if (prometheus_changed && prometheus_was_enabled) prometheus_stop();
    if (syslog_changed     && syslog_was_enabled)     syslog_stop();
    if (notify_changed     && notify_was_enabled)     events_stop();

    // Commit the new config under mutex so concurrent GET requests see a
    // consistent snapshot (no partial memcpy visible)
    xSemaphoreTake(config_mutex, portMAX_DELAY);
    memcpy(&current_config, config, sizeof(monitoring_config_t));
    xSemaphoreGive(config_mutex);

    save_config_to_nvs(&current_config);

    // Restart only what changed and is now enabled
    if (checkmk_changed    && current_config.checkmk.enabled)    checkmk_start(&current_config.checkmk);
    if (prometheus_changed && current_config.prometheus.enabled) prometheus_start(&current_config.prometheus);
    if (syslog_changed     && current_config.syslog.enabled)     syslog_start(&current_config.syslog);
    if (notify_changed     && current_config.notify.enabled)     events_start(&current_config.notify);
    if (mqtt_changed       && current_config.mqtt.enabled)       mqtt_handler_start(&current_config.mqtt);

    return ESP_OK;
}

uint32_t monitoring_pause_for_ota(void)
{
    if (config_mutex == NULL) return 0;

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
        mqtt_handler_stop();
        paused |= OTA_PAUSED_MQTT;
    }
    if (notify_enabled) {
        events_stop();
        paused |= OTA_PAUSED_NOTIFY;
    }
    if (syslog_enabled) {
        syslog_stop();
        paused |= OTA_PAUSED_SYSLOG;
    }
    if (prometheus_enabled) {
        prometheus_stop();
        paused |= OTA_PAUSED_PROMETHEUS;
    }
    if (checkmk_enabled) {
        checkmk_stop();
        paused |= OTA_PAUSED_CHECKMK;
    }

    ESP_LOGI(TAG, "Monitoring paused for OTA (mask=0x%02x, free heap: %u KB)",
             (unsigned)paused,
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024));
    return paused;
}

void monitoring_resume_after_ota(uint32_t paused_mask)
{
    if (paused_mask == 0 || config_mutex == NULL) return;

    monitoring_config_t cfg;
    xSemaphoreTake(config_mutex, portMAX_DELAY);
    memcpy(&cfg, &current_config, sizeof(cfg));
    xSemaphoreGive(config_mutex);

    ESP_LOGI(TAG, "Resuming monitoring after failed OTA (mask=0x%02x)",
             (unsigned)paused_mask);

    if ((paused_mask & OTA_PAUSED_CHECKMK) && cfg.checkmk.enabled) {
        checkmk_start(&cfg.checkmk);
    }
    if ((paused_mask & OTA_PAUSED_PROMETHEUS) && cfg.prometheus.enabled) {
        prometheus_start(&cfg.prometheus);
    }
    if ((paused_mask & OTA_PAUSED_SYSLOG) && cfg.syslog.enabled) {
        syslog_start(&cfg.syslog);
    }
    if ((paused_mask & OTA_PAUSED_NOTIFY) && cfg.notify.enabled) {
        events_start(&cfg.notify);
    }
    if ((paused_mask & OTA_PAUSED_MQTT) && cfg.mqtt.enabled) {
        mqtt_handler_start(&cfg.mqtt);
    }
}

// Task that applies a pending config update asynchronously
static void apply_config_task(void *pvParameters)
{
    monitoring_config_t *config = (monitoring_config_t *)pvParameters;
    // Brief delay so the HTTP response is fully transmitted before we
    // stop/restart network services (MQTT, CheckMK)
    vTaskDelay(pdMS_TO_TICKS(300));
    monitoring_update_config(config);
    free(config);
    update_in_progress.store(false);
    vTaskDelete(NULL);
}

// Schedule configuration update asynchronously - returns immediately, update runs in background
esp_err_t monitoring_schedule_update_config(const monitoring_config_t *config)
{
    if (config == NULL) return ESP_ERR_INVALID_ARG;
    if (config_mutex == NULL) return ESP_ERR_INVALID_STATE;

    // Atomic compare-and-swap gate: only one update task at a time.
    // compare_exchange_strong guarantees no race between the check and the set,
    // even on dual-core ESP32 where volatile alone is insufficient.
    bool expected = false;
    if (!update_in_progress.compare_exchange_strong(expected, true)) {
        ESP_LOGW(TAG, "Config update already in progress, ignoring duplicate request");
        return ESP_ERR_INVALID_STATE;
    }

    monitoring_config_t *config_copy = (monitoring_config_t *)malloc(sizeof(monitoring_config_t));
    if (!config_copy) {
        ESP_LOGE(TAG, "Failed to allocate memory for async config update");
        update_in_progress.store(false);
        return ESP_ERR_NO_MEM;
    }
    memcpy(config_copy, config, sizeof(monitoring_config_t));

    // 8192 bytes: NVS write + MQTT client init/stop/start + CheckMK socket
    // operations require significantly more than 4096 bytes of stack.
    // Priority 3 (below httpd=5): ensures the httpd task can still serve HTTP
    // requests while the config update is in progress.
    BaseType_t ret = xTaskCreate(apply_config_task, "mon_update", 8192, config_copy, 3, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create config update task");
        free(config_copy);
        update_in_progress.store(false);
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
        if (!current_config.notify.enabled) {
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
                 current_config.notify.channels);
        return ESP_OK;
    }

    snprintf(code, code_len, "monitoring.diag.unsupported");
    snprintf(message, message_len, "Unknown diagnostic target");
    return ESP_ERR_NOT_SUPPORTED;
}
