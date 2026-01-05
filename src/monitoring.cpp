/*
 *  monitoring.cpp is part of the HB-RF-ETH firmware v2.0
 *
 *  Copyright 2025 Xerolux
 *  SNMP and CheckMK monitoring support
 */

#include "monitoring.h"
#include "mqtt_handler.h"
#include "nextcloud_client.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "esp_app_format.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

// SNMP support (optional, requires CONFIG_LWIP_SNMP=y)
#if CONFIG_LWIP_SNMP
#include "lwip/apps/snmp.h"
#include "lwip/apps/snmp_mib2.h"
#include "lwip/apps/snmp_snmpv2_framework.h"
#include "lwip/apps/snmp_snmpv2_usm.h"
#endif

static const char *TAG = "MONITORING";

// Buffer size constants
#define CHECKMK_OUTPUT_BUFFER_SIZE 2048
#define CHECKMK_ALLOWED_HOSTS_SIZE 256

static monitoring_config_t current_config = {};
static bool snmp_running = false;
static bool checkmk_running = false;
static TaskHandle_t checkmk_task_handle = NULL;

// NVS keys
#define NVS_NAMESPACE "monitoring"
#define NVS_SNMP_ENABLED "snmp_en"
#define NVS_SNMP_COMMUNITY "snmp_comm"
#define NVS_SNMP_LOCATION "snmp_loc"
#define NVS_SNMP_CONTACT "snmp_cont"
#define NVS_SNMP_PORT "snmp_port"
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
#define NVS_NC_ENABLED "nc_en"
#define NVS_NC_SERVER "nc_srv"
#define NVS_NC_USER "nc_usr"
#define NVS_NC_PASS "nc_pw"
#define NVS_NC_PATH "nc_path"
#define NVS_NC_INTERVAL "nc_int"
#define NVS_NC_KEEP_LOCAL "nc_keep"

// Global pointers
static SysInfo* g_sysInfo = NULL;
static UpdateCheck* g_updateCheck = NULL;

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

static bool is_ip_allowed(const char* allowed_hosts, const char* client_ip) {
    if (strlen(allowed_hosts) == 0 || strcmp(allowed_hosts, "*") == 0) {
        return true;
    }

    char* hosts_copy = strdup(allowed_hosts);
    if (!hosts_copy) return false; // Allocation failure -> deny

    bool match = false;
    // Delimiters: comma, semicolon, space
    char* token = strtok(hosts_copy, ",; ");
    while (token != NULL) {
        if (strcmp(token, client_ip) == 0) {
            match = true;
            break;
        }
        token = strtok(NULL, ",; ");
    }

    free(hosts_copy);
    return match;
}

// CheckMK Agent Task
static void checkmk_agent_task(void *pvParameters)
{
    const checkmk_config_t *config = (const checkmk_config_t *)pvParameters;
    int listen_sock = -1;
    struct sockaddr_in server_addr;

    ESP_LOGI(TAG, "CheckMK Agent starting on port %d", config->port);

    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket");
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
        vTaskDelete(NULL);
        return;
    }

    if (listen(listen_sock, 5) < 0) {
        ESP_LOGE(TAG, "Socket listen failed");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    // Set accept timeout to allow clean shutdown when checkmk_running becomes false
    struct timeval accept_timeout;
    accept_timeout.tv_sec = 1;
    accept_timeout.tv_usec = 0;
    setsockopt(listen_sock, SOL_SOCKET, SO_RCVTIMEO, &accept_timeout, sizeof(accept_timeout));

    ESP_LOGI(TAG, "CheckMK Agent listening on port %d", config->port);

    // Allocate output buffer on heap to avoid stack overflow (2KB on 4KB stack is risky)
    char *output = (char *)malloc(CHECKMK_OUTPUT_BUFFER_SIZE);
    if (output == NULL) {
        ESP_LOGE(TAG, "Failed to allocate output buffer");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    while (checkmk_running) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            // Timeout is expected when no client connects - just continue
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            if (checkmk_running) {
                ESP_LOGE(TAG, "Accept failed: errno %d", errno);
            }
            continue;
        }

        char client_ip[16];
        inet_ntoa_r(client_addr.sin_addr, client_ip, sizeof(client_ip));
        ESP_LOGI(TAG, "CheckMK client connected from %s", client_ip);

        // Set socket timeouts to prevent DoS via connection stalling
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        setsockopt(client_sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        // Check if client IP is allowed
        if (!is_ip_allowed(config->allowed_hosts, client_ip)) {
            ESP_LOGW(TAG, "Client %s not in allowed hosts list", client_ip);
            close(client_sock);
            continue;
        }

        // Send CheckMK agent output
        size_t len = 0;

        // Helper for safe appending to buffer
        auto safe_append = [&](const char* format, ...) {
            if (len >= CHECKMK_OUTPUT_BUFFER_SIZE - 1) return; // Buffer full

            va_list args;
            va_start(args, format);
            int ret = vsnprintf(output + len, CHECKMK_OUTPUT_BUFFER_SIZE - len, format, args);
            va_end(args);

            if (ret > 0) {
                if (len + ret < CHECKMK_OUTPUT_BUFFER_SIZE) {
                    len += ret;
                } else {
                    // Truncated - saturate to max
                    len = CHECKMK_OUTPUT_BUFFER_SIZE - 1;
                    output[len] = '\0';
                }
            }
        };

        // Version section
        safe_append("<<<check_mk>>>\n");
        safe_append("Version: HB-RF-ETH-%s\n", get_firmware_version());
        safe_append("AgentOS: ESP-IDF\n");

        // Uptime section
        uint32_t days, hours, minutes;
        get_system_uptime(&days, &hours, &minutes);
        safe_append("<<<uptime>>>\n");
        safe_append("%lu\n", (unsigned long)(days * 86400 + hours * 3600 + minutes * 60));

        // Memory section
        safe_append("<<<mem>>>\n");
        safe_append("MemTotal: %lu kB\n",
                       (unsigned long)(heap_caps_get_total_size(MALLOC_CAP_DEFAULT) / 1024));
        safe_append("MemFree: %lu kB\n",
                       (unsigned long)(heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024));

        // CPU section
        safe_append("<<<cpu>>>\n");
        safe_append("esp32 0 0 0\n");

        // Send data
        send(client_sock, output, len, 0);

        close(client_sock);
        ESP_LOGI(TAG, "CheckMK client disconnected");
    }

    free(output);
    close(listen_sock);
    ESP_LOGI(TAG, "CheckMK Agent stopped");
    vTaskDelete(NULL);
}

// SNMP Functions
esp_err_t snmp_start(const snmp_config_t *config)
{
#if CONFIG_LWIP_SNMP
    if (snmp_running) {
        ESP_LOGW(TAG, "SNMP already running");
        return ESP_OK;
    }

    if (!config->enabled) {
        return ESP_OK;
    }

    ESP_LOGW(TAG, "SNMP agent requested on port %d - Feature disabled (requires CONFIG_LWIP_SNMP=y)", config->port);
    ESP_LOGW(TAG, "SNMP code available but not compiled. Enable via: pio run -t menuconfig -> Component config -> LWIP -> Enable SNMP");
    return ESP_ERR_NOT_SUPPORTED;
#else
    ESP_LOGW(TAG, "SNMP not enabled in build configuration");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t snmp_stop(void)
{
    if (!snmp_running) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping SNMP agent");
    // Note: lwIP SNMP doesn't have a clean shutdown function
    snmp_running = false;

    return ESP_OK;
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

    // Copy config to current_config to avoid dangling pointer
    memcpy(&current_config.checkmk, config, sizeof(checkmk_config_t));

    // Create CheckMK agent task - pass pointer to current_config
    BaseType_t ret = xTaskCreate(checkmk_agent_task, "checkmk_agent", 4096,
                                  (void *)&current_config.checkmk, 5, &checkmk_task_handle);

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

    if (checkmk_task_handle != NULL) {
        vTaskDelete(checkmk_task_handle);
        checkmk_task_handle = NULL;
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

    // Save SNMP config
    nvs_set_u8(nvs_handle, NVS_SNMP_ENABLED, config->snmp.enabled);
    nvs_set_str(nvs_handle, NVS_SNMP_COMMUNITY, config->snmp.community);
    nvs_set_str(nvs_handle, NVS_SNMP_LOCATION, config->snmp.location);
    nvs_set_str(nvs_handle, NVS_SNMP_CONTACT, config->snmp.contact);
    nvs_set_u16(nvs_handle, NVS_SNMP_PORT, config->snmp.port);

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

    // Save Nextcloud config
    nvs_set_u8(nvs_handle, NVS_NC_ENABLED, config->nextcloud.enabled);
    nvs_set_str(nvs_handle, NVS_NC_SERVER, config->nextcloud.server_url);
    nvs_set_str(nvs_handle, NVS_NC_USER, config->nextcloud.username);
    nvs_set_str(nvs_handle, NVS_NC_PASS, config->nextcloud.password);
    nvs_set_str(nvs_handle, NVS_NC_PATH, config->nextcloud.backup_path);
    nvs_set_u32(nvs_handle, NVS_NC_INTERVAL, config->nextcloud.backup_interval_hours);
    nvs_set_u8(nvs_handle, NVS_NC_KEEP_LOCAL, config->nextcloud.keep_local_backup);

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
        config->snmp.enabled = false;  // Disabled by default for security
        // Empty community string forces user to set a custom value
        config->snmp.community[0] = '\0';
        config->snmp.location[0] = '\0';
        config->snmp.contact[0] = '\0';
        config->snmp.port = 161;

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

        config->nextcloud.enabled = false;
        config->nextcloud.server_url[0] = '\0';
        config->nextcloud.username[0] = '\0';
        config->nextcloud.password[0] = '\0';
        strncpy(config->nextcloud.backup_path, "/HB-RF-ETH/backups", sizeof(config->nextcloud.backup_path) - 1);
        config->nextcloud.backup_path[sizeof(config->nextcloud.backup_path) - 1] = '\0';
        config->nextcloud.backup_interval_hours = 24;  // Daily by default
        config->nextcloud.keep_local_backup = true;

        return ESP_OK;
    }

    // Load SNMP config
    uint8_t u8_val;
    uint16_t u16_val;
    size_t str_len;

    if (nvs_get_u8(nvs_handle, NVS_SNMP_ENABLED, &u8_val) == ESP_OK) {
        config->snmp.enabled = u8_val;
    }

    str_len = sizeof(config->snmp.community);
    nvs_get_str(nvs_handle, NVS_SNMP_COMMUNITY, config->snmp.community, &str_len);

    str_len = sizeof(config->snmp.location);
    nvs_get_str(nvs_handle, NVS_SNMP_LOCATION, config->snmp.location, &str_len);

    str_len = sizeof(config->snmp.contact);
    nvs_get_str(nvs_handle, NVS_SNMP_CONTACT, config->snmp.contact, &str_len);

    if (nvs_get_u16(nvs_handle, NVS_SNMP_PORT, &u16_val) == ESP_OK) {
        config->snmp.port = u16_val;
    }

    // Load CheckMK config
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

    // Load Nextcloud config
    if (nvs_get_u8(nvs_handle, NVS_NC_ENABLED, &u8_val) == ESP_OK) {
        config->nextcloud.enabled = u8_val;
    } else {
        config->nextcloud.enabled = false;
    }

    str_len = sizeof(config->nextcloud.server_url);
    if (nvs_get_str(nvs_handle, NVS_NC_SERVER, config->nextcloud.server_url, &str_len) != ESP_OK) {
        config->nextcloud.server_url[0] = '\0';
    }

    str_len = sizeof(config->nextcloud.username);
    if (nvs_get_str(nvs_handle, NVS_NC_USER, config->nextcloud.username, &str_len) != ESP_OK) {
        config->nextcloud.username[0] = '\0';
    }

    str_len = sizeof(config->nextcloud.password);
    if (nvs_get_str(nvs_handle, NVS_NC_PASS, config->nextcloud.password, &str_len) != ESP_OK) {
        config->nextcloud.password[0] = '\0';
    }

    str_len = sizeof(config->nextcloud.backup_path);
    if (nvs_get_str(nvs_handle, NVS_NC_PATH, config->nextcloud.backup_path, &str_len) != ESP_OK) {
        strncpy(config->nextcloud.backup_path, "/HB-RF-ETH/backups", sizeof(config->nextcloud.backup_path) - 1);
        config->nextcloud.backup_path[sizeof(config->nextcloud.backup_path) - 1] = '\0';
    }

    uint32_t u32_val;
    if (nvs_get_u32(nvs_handle, NVS_NC_INTERVAL, &u32_val) == ESP_OK) {
        config->nextcloud.backup_interval_hours = u32_val;
    } else {
        config->nextcloud.backup_interval_hours = 24;
    }

    if (nvs_get_u8(nvs_handle, NVS_NC_KEEP_LOCAL, &u8_val) == ESP_OK) {
        config->nextcloud.keep_local_backup = u8_val;
    } else {
        config->nextcloud.keep_local_backup = true;
    }

    nvs_close(nvs_handle);
    return ESP_OK;
}

// Initialize monitoring subsystem
esp_err_t monitoring_init(const monitoring_config_t *config, SysInfo* sysInfo, UpdateCheck* updateCheck)
{
    ESP_LOGI(TAG, "Initializing monitoring subsystem");

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

    // Start SNMP if enabled
    if (current_config.snmp.enabled) {
        snmp_start(&current_config.snmp);
    }

    // Start CheckMK if enabled
    if (current_config.checkmk.enabled) {
        checkmk_start(&current_config.checkmk);
    }

    // Start MQTT if enabled
    if (current_config.mqtt.enabled) {
        mqtt_handler_start(&current_config.mqtt);
    }

    // Initialize Nextcloud client if enabled
    if (current_config.nextcloud.enabled) {
        esp_err_t ret = nextcloud_init(&current_config.nextcloud);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize Nextcloud client: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Nextcloud client initialized");
            // Start auto-backup if configured
            if (current_config.nextcloud.backup_interval_hours > 0) {
                nextcloud_start_auto_backup();
            }
        }
    }

    return ESP_OK;
}

// Update configuration
esp_err_t monitoring_update_config(const monitoring_config_t *config)
{
    // Stop current services
    if (current_config.snmp.enabled) {
        snmp_stop();
    }
    if (current_config.checkmk.enabled) {
        checkmk_stop();
    }
    if (current_config.mqtt.enabled) {
        mqtt_handler_stop();
    }
    if (current_config.nextcloud.enabled) {
        nextcloud_stop_auto_backup();
    }

    // Update config
    memcpy(&current_config, config, sizeof(monitoring_config_t));
    save_config_to_nvs(&current_config);

    // Restart services with new config
    if (current_config.snmp.enabled) {
        snmp_start(&current_config.snmp);
    }
    if (current_config.checkmk.enabled) {
        checkmk_start(&current_config.checkmk);
    }
    if (current_config.mqtt.enabled) {
        mqtt_handler_start(&current_config.mqtt);
    }
    if (current_config.nextcloud.enabled) {
        esp_err_t ret = nextcloud_init(&current_config.nextcloud);
        if (ret == ESP_OK && current_config.nextcloud.backup_interval_hours > 0) {
            nextcloud_start_auto_backup();
        }
    }

    return ESP_OK;
}

// Get current configuration
esp_err_t monitoring_get_config(monitoring_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(config, &current_config, sizeof(monitoring_config_t));
    return ESP_OK;
}
