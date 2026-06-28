/*
 *  monitoring.cpp is part of the HB-RF-ETH firmware v2.0
 *
 *  Copyright 2025 Xerolux
 *  CheckMK and MQTT monitoring support
 */

#include "monitoring.h"
#include "mqtt_handler.h"
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

static const char *TAG = "MONITORING";
SemaphoreHandle_t g_net_fetch_mutex = NULL;

static monitoring_config_t current_config = {};
static SemaphoreHandle_t config_mutex = NULL;
static volatile bool checkmk_running = false;
static std::atomic<bool> update_in_progress{false};
static volatile TaskHandle_t checkmk_task_handle = NULL;
static volatile int checkmk_listen_sock = -1;

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

// Global pointers
static SysInfo* g_sysInfo = NULL;
static UpdateCheck* g_updateCheck = NULL;
static Ethernet* g_ethernet = NULL;
static RadioModuleDetector* g_radioModuleDetector = NULL;
static SystemClock* g_systemClock = NULL;

// Provider accessors for mqtt_handler.cpp
Ethernet* monitoring_get_ethernet(void) { return g_ethernet; }
RadioModuleDetector* monitoring_get_radiomodule(void) { return g_radioModuleDetector; }
SystemClock* monitoring_get_systemclock(void) { return g_systemClock; }

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

    checkmk_listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (checkmk_listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket");
        checkmk_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(checkmk_listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(config->port);

    if (bind(checkmk_listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Socket bind failed");
        close(checkmk_listen_sock);
        checkmk_listen_sock = -1;
        checkmk_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    if (listen(checkmk_listen_sock, 5) < 0) {
        ESP_LOGE(TAG, "Socket listen failed");
        close(checkmk_listen_sock);
        checkmk_listen_sock = -1;
        checkmk_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    // Periodic timeout so the task can check checkmk_running even if no
    // client connects and even if shutdown() does not unblock accept() on
    // this lwIP version.
    struct timeval accept_tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(checkmk_listen_sock, SOL_SOCKET, SO_RCVTIMEO, &accept_tv, sizeof(accept_tv));

    ESP_LOGI(TAG, "CheckMK Agent listening on port %d", config->port);

    while (checkmk_running) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_sock = accept(checkmk_listen_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            // EAGAIN/EWOULDBLOCK = 1s timeout expired, just re-check the loop condition
            if (checkmk_running && errno != EAGAIN && errno != EWOULDBLOCK) {
                ESP_LOGE(TAG, "Accept failed: errno %d", errno);
            }
            continue;
        }

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
                char *end = token + strlen(token) - 1;
                while (end > token && *end == ' ') { *end = '\0'; end--; }
                if (strcmp(token, client_ip) == 0) {
                    allowed = true;
                    break;
                }
                token = strtok_r(NULL, ",", &saveptr);
            }
        }

        if (!allowed) {
            ESP_LOGW(TAG, "Client %s not in allowed hosts list", client_ip);
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

        // Send data
        send(client_sock, output, len, 0);

        close(client_sock);
        ESP_LOGI(TAG, "CheckMK client disconnected");
    }

    if (checkmk_listen_sock >= 0) {
        close(checkmk_listen_sock);
        checkmk_listen_sock = -1;
    }
    ESP_LOGI(TAG, "CheckMK Agent stopped");
    checkmk_task_handle = NULL;
    vTaskDelete(NULL);
}

// CheckMK Functions
esp_err_t checkmk_start(const checkmk_config_t *config)
{
    if (checkmk_running) {
        ESP_LOGW(TAG, "CheckMK agent already running");
        return ESP_OK;
    }

    if (!config->enabled) {
        return ESP_OK;
    }

    checkmk_running = true;

    // Create CheckMK agent task - pass pointer to current_config
    // 8192 bytes: large output buffer (2048) + sockaddr/IP string operations
    // xTaskCreate requires a non-volatile TaskHandle_t*; assign to volatile global afterwards.
    TaskHandle_t cmk_handle = NULL;
    BaseType_t ret = xTaskCreate(checkmk_agent_task, "checkmk_agent", 8192,
                                  (void *)&current_config.checkmk, 5, &cmk_handle);
    checkmk_task_handle = cmk_handle;

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create CheckMK agent task");
        checkmk_running = false;
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t checkmk_stop(void)
{
    if (!checkmk_running) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping CheckMK agent");
    checkmk_running = false;

    // shutdown() + close() to unblock accept(). shutdown() is more reliable
    // than close() alone for interrupting a blocking accept() call on lwIP.
    // The SO_RCVTIMEO set in the task ensures it wakes up within 1s regardless.
    if (checkmk_listen_sock >= 0) {
        shutdown(checkmk_listen_sock, SHUT_RDWR);
        close(checkmk_listen_sock);
        checkmk_listen_sock = -1;
    }

    // Wait up to 2.5s for the task to exit (accept timeout is 1s, so it
    // will detect checkmk_running==false within one timeout cycle).
    for (int i = 0; i < 25 && checkmk_task_handle != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Use a local snapshot of the handle to avoid a race where the task
    // sets checkmk_task_handle=NULL between our NULL check and vTaskDelete().
    TaskHandle_t cmk_handle = checkmk_task_handle;
    if (cmk_handle != NULL) {
        ESP_LOGW(TAG, "CheckMK task did not exit cleanly, force deleting");
        checkmk_task_handle = NULL;
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

    // Save CheckMK config
    nvs_set_u8(nvs_handle, NVS_CHECKMK_ENABLED, config->checkmk.enabled);
    nvs_set_u16(nvs_handle, NVS_CHECKMK_PORT, config->checkmk.port);
    nvs_set_str(nvs_handle, NVS_CHECKMK_HOSTS, config->checkmk.allowed_hosts);

    // Save MQTT config
    nvs_set_u8(nvs_handle, NVS_MQTT_ENABLED, config->mqtt.enabled);
    nvs_set_str(nvs_handle, NVS_MQTT_SERVER, config->mqtt.server);
    nvs_set_u16(nvs_handle, NVS_MQTT_PORT, config->mqtt.port);
    nvs_set_str(nvs_handle, NVS_MQTT_USER, config->mqtt.user);
    nvs_set_str(nvs_handle, NVS_MQTT_PASS, config->mqtt.password);
    nvs_set_str(nvs_handle, NVS_MQTT_PREFIX, config->mqtt.topic_prefix);
    nvs_set_u8(nvs_handle, NVS_MQTT_HA_ENABLED, config->mqtt.ha_discovery_enabled);
    nvs_set_str(nvs_handle, NVS_MQTT_HA_PREFIX, config->mqtt.ha_discovery_prefix);

    nvs_set_u8(nvs_handle, NVS_MQTT_TLS_EN, config->mqtt.tls_enable);
    nvs_set_u8(nvs_handle, NVS_MQTT_TLS_SKIP, config->mqtt.tls_skip_verify);
    nvs_set_blob(nvs_handle, NVS_MQTT_TLS_CA,  config->mqtt.tls_ca_certs,  strlen(config->mqtt.tls_ca_certs) + 1);
    nvs_set_blob(nvs_handle, NVS_MQTT_TLS_CRT, config->mqtt.tls_certfile,  strlen(config->mqtt.tls_certfile) + 1);
    nvs_set_blob(nvs_handle, NVS_MQTT_TLS_KEY, config->mqtt.tls_keyfile,   strlen(config->mqtt.tls_keyfile) + 1);

    // Command-topic security (Phase A). Default for command_enabled is true
    // so existing installations keep working after the upgrade. The token
    // is optional; if empty no token check is applied.
    nvs_set_u8(nvs_handle, NVS_MQTT_CMD_EN, config->mqtt.command_enabled ? 1 : 0);
    nvs_set_str(nvs_handle, NVS_MQTT_CMD_TOK, config->mqtt.command_token);

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
        strncpy(config->mqtt.topic_prefix, "hb-rf-eth", sizeof(config->mqtt.topic_prefix) - 1);
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
        strncpy(config->mqtt.topic_prefix, "hb-rf-eth", sizeof(config->mqtt.topic_prefix) - 1);
        config->mqtt.topic_prefix[sizeof(config->mqtt.topic_prefix) - 1] = '\0';
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

    nvs_close(nvs_handle);
    return ESP_OK;
}

// Initialize monitoring subsystem
esp_err_t monitoring_init(const monitoring_config_t *config, SysInfo* sysInfo, UpdateCheck* updateCheck)
{
    ESP_LOGI(TAG, "Initializing monitoring subsystem");

    config_mutex = xSemaphoreCreateMutex();
    if (config_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create config mutex");
        return ESP_ERR_NO_MEM;
    }

    g_net_fetch_mutex = xSemaphoreCreateMutex();
    if (g_net_fetch_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create net-fetch mutex");
        // Non-critical: continue without serialisation; the changelog-proxy
        // retry will handle the transient double-TLS OOM.
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

    // Start MQTT if enabled
    if (current_config.mqtt.enabled) {
        mqtt_handler_start(&current_config.mqtt);
    }

    return ESP_OK;
}

// Update configuration
esp_err_t monitoring_update_config(const monitoring_config_t *config)
{
    // Take a snapshot of the current config under mutex to determine what changed.
    // Release the mutex before any blocking stop/start calls so GET requests
    // are never blocked for more than a memcpy duration.
    xSemaphoreTake(config_mutex, portMAX_DELAY);
    bool checkmk_changed = (memcmp(&current_config.checkmk, &config->checkmk, sizeof(checkmk_config_t)) != 0);
    bool mqtt_changed    = (memcmp(&current_config.mqtt,    &config->mqtt,    sizeof(mqtt_config_t))    != 0);
    bool checkmk_was_enabled = current_config.checkmk.enabled;
    bool mqtt_was_enabled    = current_config.mqtt.enabled;
    xSemaphoreGive(config_mutex);

    // Stop only what needs to change (these calls can block; no mutex held)
    if (checkmk_changed && checkmk_was_enabled) checkmk_stop();
    if (mqtt_changed && mqtt_was_enabled)    mqtt_handler_stop();

    // Commit the new config under mutex so concurrent GET requests see a
    // consistent snapshot (no partial memcpy visible)
    xSemaphoreTake(config_mutex, portMAX_DELAY);
    memcpy(&current_config, config, sizeof(monitoring_config_t));
    xSemaphoreGive(config_mutex);

    save_config_to_nvs(&current_config);

    // Restart only what changed and is now enabled
    if (checkmk_changed && current_config.checkmk.enabled) checkmk_start(&current_config.checkmk);
    if (mqtt_changed && current_config.mqtt.enabled)    mqtt_handler_start(&current_config.mqtt);

    return ESP_OK;
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

esp_err_t monitoring_run_diagnostic(const char *target, bool *ok, char *message, size_t message_len)
{
    if (target == NULL || ok == NULL || message == NULL || message_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    *ok = false;
    message[0] = '\0';

    // Snapshot the needed fields under the config mutex - apply_config_task
    // may memcpy the whole config struct concurrently, and tcp_probe_endpoint
    // below blocks for seconds, so don't read current_config directly.
    bool checkmk_enabled, mqtt_enabled, mqtt_tls;
    uint16_t checkmk_port, mqtt_port;
    char mqtt_server[sizeof(current_config.mqtt.server)];

    if (config_mutex)
        xSemaphoreTake(config_mutex, portMAX_DELAY);
    checkmk_enabled = current_config.checkmk.enabled;
    checkmk_port = current_config.checkmk.port;
    mqtt_enabled = current_config.mqtt.enabled;
    mqtt_port = current_config.mqtt.port;
    mqtt_tls = current_config.mqtt.tls_enable;
    strncpy(mqtt_server, current_config.mqtt.server, sizeof(mqtt_server) - 1);
    mqtt_server[sizeof(mqtt_server) - 1] = '\0';
    if (config_mutex)
        xSemaphoreGive(config_mutex);

    if (strcmp(target, "checkmk") == 0) {
        if (!checkmk_enabled) {
            snprintf(message, message_len, "CheckMK is disabled");
            return ESP_OK;
        }

        *ok = checkmk_running && checkmk_listen_sock >= 0;
        snprintf(message, message_len, *ok
            ? "CheckMK agent listening on TCP port %u"
            : "CheckMK is enabled but listener is not ready",
            checkmk_port);
        return ESP_OK;
    }

    if (strcmp(target, "mqtt") == 0) {
        if (!mqtt_enabled) {
            snprintf(message, message_len, "MQTT is disabled");
            return ESP_OK;
        }

        esp_err_t probe = tcp_probe_endpoint(mqtt_server, mqtt_port, 3000, message, message_len);
        *ok = (probe == ESP_OK);
        if (mqtt_tls) {
            // Append TLS note; TCP probe only, TLS handshake is not tested here.
            size_t used = strlen(message);
            if (used < message_len) {
                snprintf(message + used, message_len - used, " (TLS enabled, cert validation not tested)");
            }
        }
        return ESP_OK;
    }

    snprintf(message, message_len, "Unknown diagnostic target");
    return ESP_ERR_NOT_SUPPORTED;
}
