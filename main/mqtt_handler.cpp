/*
 *  mqtt_handler.cpp is part of the HB-RF-ETH firmware v2.0
 *
 *  Copyright 2025 Xerolux
 *  MQTT support
 */

#include "mqtt_handler.h"
#include "monitoring.h"
#include "sysinfo.h"
#include "updatecheck.h"
#include "nvs_flash.h"
#include "reset_info.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include "lwip/ip4_addr.h"
#include "ethernet.h"
#include "radiomoduledetector.h"
#include "systemclock.h"
#include "semver.h"

#include <string.h>

static const char *TAG = "MQTT";

static esp_mqtt_client_handle_t client = NULL;
static volatile bool mqtt_running = false;
static volatile TaskHandle_t mqtt_publish_task_handle = NULL;
static mqtt_config_t current_mqtt_config;

// Latch set by mqtt_handler_trigger_status_publish() so the periodic task
// emits an immediate cycle out-of-band (after OTA state changes etc.).
static volatile bool mqtt_publish_request = false;

// Forward declarations
extern SysInfo* monitoring_get_sysinfo(void);
extern UpdateCheck* monitoring_get_updatecheck(void);
extern Ethernet* monitoring_get_ethernet(void);
extern RadioModuleDetector* monitoring_get_radiomodule(void);
extern SystemClock* monitoring_get_systemclock(void);

void mqtt_handler_publish_ha_discovery(void);
static void publish_ota_state(void);

// Helper: map OTA state enum to short string for status topic + HA.
static const char* ota_state_str(ota_state_t s)
{
    switch (s) {
    case OTA_STATE_CHECKING:    return "checking";
    case OTA_STATE_STARTING:    return "starting";
    case OTA_STATE_DOWNLOADING: return "downloading";
    case OTA_STATE_FLASHING:    return "flashing";
    case OTA_STATE_SUCCESS:     return "success";
    case OTA_STATE_FAILED:      return "failed";
    case OTA_STATE_IDLE:
    default:                    return "idle";
    }
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

// Forward declarations for system commands
extern "C" void esp_restart(void);

static void perform_factory_reset()
{
    ESP_LOGI(TAG, "Performing factory reset");

    ResetInfo::storeResetReason(RESET_REASON_FACTORY_RESET);

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("settings", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_erase_all(nvs_handle);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Settings NVS erased");
    } else {
        ESP_LOGE(TAG, "Failed to open NVS settings namespace: %s", esp_err_to_name(err));
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
}

// Validate the command payload against the optional shared-secret token.
//
// Semantics:
//   * If command_token is empty -> always accept (broker ACL must protect).
//   * Otherwise the payload (trimmed) must equal the configured token.
//
// HA integration: when a token is set, the HA discovery config publishes the
// token as payload_press / payload_install, so buttons "just work" in HA.
// This means the HA discovery topic contains the token in clear-text - lock
// down broker ACLs so only the device may publish to <ha_prefix>/#.
static bool command_token_ok(const char *payload, int payload_len)
{
    if (current_mqtt_config.command_token[0] == '\0') {
        return true;
    }
    if (payload == NULL || payload_len <= 0) {
        return false;
    }
    // Compare with length limit
    size_t expected = strlen(current_mqtt_config.command_token);
    if ((size_t)payload_len != expected) {
        return false;
    }
    return strncmp(payload, current_mqtt_config.command_token, expected) == 0;
}

static void handle_mqtt_command(const char* command, const char* payload, int payload_len)
{
    ESP_LOGI(TAG, "Received MQTT command: %s (payload %d bytes)", command, payload_len);

    if (!command_token_ok(payload, payload_len)) {
        ESP_LOGW(TAG, "Command %s rejected: missing/invalid token", command);
        char msg[96];
        snprintf(msg, sizeof(msg), "rejected cmd=%.48s reason=invalid_token", command);
        mqtt_handler_publish_event("event/command_rejected", msg);
        return;
    }

    if (strcmp(command, "restart") == 0) {
        ESP_LOGI(TAG, "Restart command received via MQTT");
        ResetInfo::storeResetReason(RESET_REASON_USER_RESTART);
        mqtt_handler_publish_event("event/restart", "requested");
        // Give the broker a moment to flush the publish before we tear down TCP.
        vTaskDelay(pdMS_TO_TICKS(300));
        esp_restart();
    } else if (strcmp(command, "factory_reset") == 0) {
        ESP_LOGI(TAG, "Factory reset command received via MQTT");
        mqtt_handler_publish_event("event/factory_reset", "requested");
        vTaskDelay(pdMS_TO_TICKS(300));
        perform_factory_reset();
        esp_restart();
    } else if (strcmp(command, "update") == 0) {
        ESP_LOGI(TAG, "Update command received via MQTT");
        UpdateCheck* updateCheck = monitoring_get_updatecheck();
        if (updateCheck) {
            // esp_https_ota needs more stack than the 6 KB MQTT task offers
            // (TLS handshake alone takes several KB) and would block the MQTT
            // keepalive loop for the whole download - run it in its own task.
            BaseType_t created = xTaskCreate([](void *p) {
                static_cast<UpdateCheck *>(p)->performOnlineUpdate();
                vTaskDelete(NULL);
            }, "mqtt_ota", 8192, updateCheck, 5, NULL);
            if (created != pdPASS) {
                ESP_LOGE(TAG, "Failed to create OTA update task");
                mqtt_handler_publish_event("event/update_failed", "task_create_failed");
            } else {
                mqtt_handler_publish_event("event/update_started", "requested");
            }
        } else {
            ESP_LOGW(TAG, "UpdateCheck not available");
            mqtt_handler_publish_event("event/update_failed", "updatecheck_unavailable");
        }
    } else if (strcmp(command, "check_update") == 0) {
        // Refresh release info from GitHub without flashing.
        ESP_LOGI(TAG, "Check-update command received via MQTT");
        UpdateCheck* updateCheck = monitoring_get_updatecheck();
        if (updateCheck) {
            xTaskCreate([](void *p) {
                static_cast<UpdateCheck *>(p)->refresh();
                vTaskDelete(NULL);
            }, "mqtt_chkupd", 8192, updateCheck, 4, NULL);
            mqtt_handler_publish_event("event/check_update", "requested");
        } else {
            mqtt_handler_publish_event("event/check_update", "updatecheck_unavailable");
        }
    } else {
        ESP_LOGW(TAG, "Unknown MQTT command: %s", command);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        // Birth message: announce we are online (retained so subscribers see
        // it immediately, even before the next status cycle). The matching
        // LWT below flips this to "offline" if the connection drops
        // unexpectedly (power loss, network outage, crash).
        if (current_mqtt_config.command_enabled || current_mqtt_config.ha_discovery_enabled) {
            // Subscribe to command topic whenever commands OR HA discovery
            // are enabled. Previously this was gated on ha_discovery only,
            // which blocked plain-MQTT users from triggering restart/update.
            char command_topic[128];
            snprintf(command_topic, sizeof(command_topic), "%s/command/#", current_mqtt_config.topic_prefix);
            esp_mqtt_client_subscribe(client, command_topic, 1);
            ESP_LOGI(TAG, "Subscribed to command topic: %s", command_topic);
        }
        // Publish initial status (includes online marker)
        mqtt_handler_publish_status();
        // Publish HA discovery config if enabled
        if (current_mqtt_config.ha_discovery_enabled) {
            mqtt_handler_publish_ha_discovery();
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        if (current_mqtt_config.command_enabled) {
            // Note: event->topic is NOT null-terminated per ESP-IDF MQTT API.
            char command_topic_prefix[128];
            snprintf(command_topic_prefix, sizeof(command_topic_prefix), "%s/command/", current_mqtt_config.topic_prefix);
            size_t prefix_len = strlen(command_topic_prefix);

            if ((size_t)event->topic_len > prefix_len &&
                strncmp(event->topic, command_topic_prefix, prefix_len) == 0) {
                char command[64];
                size_t cmd_len = (size_t)event->topic_len - prefix_len;
                if (cmd_len >= sizeof(command)) cmd_len = sizeof(command) - 1;
                memcpy(command, event->topic + prefix_len, cmd_len);
                command[cmd_len] = '\0';
                handle_mqtt_command(command, event->data, event->data_len);
            }
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        break;
    }
}

void mqtt_publish_task(void *pvParameters)
{
    // Track which OTA state we last published so we emit an out-of-cycle
    // status update the moment it changes (rather than waiting up to 60 s).
    ota_state_t last_ota_state = OTA_STATE_IDLE;
    int last_ota_progress = -1;
    int publish_cycle = 0;

    while (mqtt_running) {
        if (publish_cycle == 0) {
            ESP_LOGI(TAG, "mqtt_publish stack high water mark: %u bytes free",
                     (unsigned)uxTaskGetStackHighWaterMark(NULL));
        }
        publish_cycle++;
        mqtt_handler_publish_status();
        publish_ota_state();

        // Re-publish the moment the OTA state changes - users get instant
        // feedback when an update starts, finishes or fails.
        OtaSnapshot ota = monitoring_get_updatecheck()
            ? monitoring_get_updatecheck()->getOtaState() : OtaSnapshot{};
        if (ota.state != last_ota_state) {
            mqtt_handler_publish_status();
            publish_ota_state();
            // Emit a discrete event for state transitions that integrations
            // may want to trigger on (notification, automation, ...).
            if (ota.state == OTA_STATE_SUCCESS) {
                mqtt_handler_publish_event("event/update_finished", "success");
            } else if (ota.state == OTA_STATE_FAILED) {
                char buf[160];
                snprintf(buf, sizeof(buf), "failed: %s (code=0x%x)",
                         ota.error_text[0] ? ota.error_text : "unknown",
                         ota.error_code);
                mqtt_handler_publish_event("event/update_finished", buf);
            } else if (ota.state == OTA_STATE_DOWNLOADING && last_ota_state == OTA_STATE_STARTING) {
                mqtt_handler_publish_event("event/update_downloading", "started");
            }
            last_ota_state = ota.state;
        }
        // Emit progress events at most every ~5 % to avoid MQTT flooding.
        if (ota.state == OTA_STATE_DOWNLOADING &&
            ota.progress_pct >= 0 &&
            (ota.progress_pct - last_ota_progress >= 5 ||
             ota.progress_pct < last_ota_progress)) {
            publish_ota_state();
            last_ota_progress = ota.progress_pct;
        } else if (ota.state != OTA_STATE_DOWNLOADING) {
            last_ota_progress = -1;
        }

        // Sleep with quick wake-ups so we can react fast to trigger_publish
        // requests and so the OTA-state-changed detection runs every second
        // while an update is in flight.
        int delay_ms = (ota.state == OTA_STATE_IDLE) ? 5000 : 1000;
        for (int i = 0; i < 12 && mqtt_running; i++) {
            if (mqtt_publish_request) {
                mqtt_publish_request = false;
                break;  // run a fresh publish cycle immediately
            }
            int step = delay_ms / 12;
            if (step < 50) step = 50;
            vTaskDelay(pdMS_TO_TICKS(step));
        }
    }
    mqtt_publish_task_handle = NULL;
    vTaskDelete(NULL);
}

void mqtt_handler_trigger_status_publish(void)
{
    if (!mqtt_running) return;
    mqtt_publish_request = true;
}

void mqtt_handler_publish_event(const char *subtopic, const char *payload)
{
    if (!mqtt_running || client == NULL || !subtopic || !payload) {
        return;
    }
    char topic[160];
    snprintf(topic, sizeof(topic), "%s/%s", current_mqtt_config.topic_prefix, subtopic);
    // Non-retained, QoS 0: events are transient by definition.
    esp_mqtt_client_publish(client, topic, payload, 0, 0, 0);
}

// Publish one value under <prefix>/<subtopic> with retain=1, QoS=0.
// Defined as a macro so the compiler can inline the snprintf chains.
#define PUBLISH_STR(subtopic, value) \
    do { \
        snprintf(topic, sizeof(topic), "%s/%s", current_mqtt_config.topic_prefix, subtopic); \
        esp_mqtt_client_publish(client, topic, value, 0, 0, 1); \
    } while (0)

#define PUBLISH_INT(subtopic, value) \
    do { \
        snprintf(payload, sizeof(payload), "%d", (int)(value)); \
        PUBLISH_STR(subtopic, payload); \
    } while (0)

#define PUBLISH_UINT64(subtopic, value) \
    do { \
        snprintf(payload, sizeof(payload), "%llu", (unsigned long long)(value)); \
        PUBLISH_STR(subtopic, payload); \
    } while (0)

#define PUBLISH_DOUBLE(subtopic, value, prec) \
    do { \
        snprintf(payload, sizeof(payload), "%.*f", prec, value); \
        PUBLISH_STR(subtopic, payload); \
    } while (0)

void mqtt_handler_publish_status(void)
{
    if (!mqtt_running || client == NULL) {
        return;
    }

    SysInfo* sysInfo = monitoring_get_sysinfo();
    UpdateCheck* updateCheck = monitoring_get_updatecheck();
    Ethernet* eth = monitoring_get_ethernet();
    RadioModuleDetector* radio = monitoring_get_radiomodule();
    SystemClock* clk = monitoring_get_systemclock();

    if (sysInfo == NULL) {
        ESP_LOGW(TAG, "SysInfo not available");
        return;
    }

    char topic[160];
    char payload[96];

    // Birth/online marker. The matching LWT (set in mqtt_handler_start)
    // overwrites this with "offline" if the connection drops uncleanly.
    PUBLISH_STR("status/online", "online");

    // ---- Identity ---------------------------------------------------------
    PUBLISH_STR("status/serial", sysInfo->getSerialNumber());
    PUBLISH_STR("status/version", sysInfo->getCurrentVersion());
    PUBLISH_STR("status/board_revision", sysInfo->getBoardRevisionString());

    // ---- Update info ------------------------------------------------------
    if (updateCheck) {
        VersionSnapshot rel = updateCheck->getVersionSnapshot();
        const char* latest = rel.valid ? rel.version : "n/a";
        PUBLISH_STR("status/latest_version", latest);
        bool updateAvailable = rel.valid &&
            compareVersions(latest, sysInfo->getCurrentVersion()) > 0;
        PUBLISH_STR("status/update_available", updateAvailable ? "true" : "false");
    }

    // ---- System metrics ---------------------------------------------------
    PUBLISH_DOUBLE("status/cpu_usage", sysInfo->getCpuUsage(), 1);
    PUBLISH_DOUBLE("status/memory_usage", sysInfo->getMemoryUsage(), 1);
    PUBLISH_DOUBLE("status/supply_voltage", sysInfo->getSupplyVoltage(), 2);
    PUBLISH_DOUBLE("status/temperature", sysInfo->getTemperature(), 1);
    PUBLISH_UINT64("status/uptime", sysInfo->getUptimeSeconds());

    // Uptime formatted
    {
        uint64_t uptime_s = sysInfo->getUptimeSeconds();
        uint32_t days  = (uint32_t)(uptime_s / 86400); uptime_s %= 86400;
        uint32_t hours = (uint32_t)(uptime_s / 3600);  uptime_s %= 3600;
        uint32_t mins  = (uint32_t)(uptime_s / 60);
        snprintf(payload, sizeof(payload), "%lu d, %lu h, %lu m",
                 (unsigned long)days, (unsigned long)hours, (unsigned long)mins);
        PUBLISH_STR("status/uptime_text", payload);
    }

    // Heap details - useful for memory leak monitoring in HA graphs.
    {
        multi_heap_info_t info;
        heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);
        PUBLISH_UINT64("status/free_heap", info.total_free_bytes);
        PUBLISH_UINT64("status/min_free_heap", esp_get_minimum_free_heap_size());
    }

    // Reset reason (combines app-level stored reason + ESP hardware reason).
    if (sysInfo->getResetReason()) {
        PUBLISH_STR("status/last_reset_reason", sysInfo->getResetReason());
    }

    // ---- Ethernet ---------------------------------------------------------
    if (eth) {
        PUBLISH_STR("status/eth_connected", eth->isConnected() ? "true" : "false");
        PUBLISH_INT("status/eth_link_speed", eth->getLinkSpeedMbps());
        if (eth->getDuplexMode()) {
            PUBLISH_STR("status/eth_duplex", eth->getDuplexMode());
        }
        ip4_addr_t ip, nm, gw, dns1, dns2;
        eth->getNetworkSettings(&ip, &nm, &gw, &dns1, &dns2);
        snprintf(payload, sizeof(payload), IPSTR, IP2STR(&ip));
        PUBLISH_STR("status/ip_address", payload);
        snprintf(payload, sizeof(payload), IPSTR, IP2STR(&gw));
        PUBLISH_STR("status/gateway", payload);
    }

    // ---- Radio module -----------------------------------------------------
    if (radio) {
        const char* type = "none";
        switch (radio->getRadioModuleType()) {
            case RADIO_MODULE_HM_MOD_RPI_PCB: type = "HM-MOD-RPI-PCB"; break;
            case RADIO_MODULE_RPI_RF_MOD:     type = "RPI-RF-MOD";     break;
            case RADIO_MODULE_HMIP_RFUSB:     type = "HmIP-RFUSB";     break;
            case RADIO_MODULE_NONE:           type = "none";           break;
            default:                          type = "unknown";        break;
        }
        PUBLISH_STR("status/radio_module_type", type);
        if (radio->getSerial()) {
            PUBLISH_STR("status/radio_module_serial", radio->getSerial());
        }
        const uint8_t* fw = radio->getFirmwareVersion();
        if (fw && (fw[0] || fw[1] || fw[2])) {
            snprintf(payload, sizeof(payload), "%d.%d.%d", fw[0], fw[1], fw[2]);
            PUBLISH_STR("status/radio_module_firmware", payload);
        }
    }

    // ---- System clock / NTP sync state -----------------------------------
    if (clk) {
        struct timeval sync = clk->getLastSyncTime();
        bool synced = (sync.tv_sec > 0);
        PUBLISH_STR("status/ntp_synced", synced ? "true" : "false");
        if (synced) {
            PUBLISH_UINT64("status/last_ntp_sync", (unsigned long long)sync.tv_sec);
        } else {
            PUBLISH_STR("status/last_ntp_sync", "0");
        }
    }
}

// Publish the OTA state + progress under dedicated subtopics so HA can render
// a progress bar and automations can trigger on completion.
static void publish_ota_state(void)
{
    if (!mqtt_running || client == NULL) return;
    UpdateCheck* updateCheck = monitoring_get_updatecheck();
    if (!updateCheck) return;

    OtaSnapshot ota = updateCheck->getOtaState();
    char topic[160];
    char payload[64];

    PUBLISH_STR("status/ota_state", ota_state_str(ota.state));
    if (ota.progress_pct >= 0) {
        PUBLISH_INT("status/ota_progress", ota.progress_pct);
    } else {
        PUBLISH_STR("status/ota_progress", "-1");
    }
    if (ota.state == OTA_STATE_FAILED && ota.error_text[0]) {
        PUBLISH_STR("status/ota_error", ota.error_text);
    }
}

#undef PUBLISH_STR
#undef PUBLISH_INT
#undef PUBLISH_UINT64
#undef PUBLISH_DOUBLE

void mqtt_handler_publish_ha_discovery(void)
{
    if (!mqtt_running || client == NULL || !current_mqtt_config.ha_discovery_enabled) {
        return;
    }

    SysInfo* sysInfo = monitoring_get_sysinfo();
    if (sysInfo == NULL) {
        return;
    }

    ESP_LOGI(TAG, "Publishing Home Assistant discovery configs");

    // The token callers must send as payload_press / payload_install so HA
    // buttons work even when a command_token is configured. Empty token ->
    // plain "restart" etc. (legacy behaviour).
    const char* restart_payload  = current_mqtt_config.command_token[0] ? current_mqtt_config.command_token : "restart";
    const char* reset_payload    = current_mqtt_config.command_token[0] ? current_mqtt_config.command_token : "factory_reset";
    const char* update_payload   = current_mqtt_config.command_token[0] ? current_mqtt_config.command_token : "update";
    const char* chkupd_payload   = current_mqtt_config.command_token[0] ? current_mqtt_config.command_token : "check_update";

    // Device Info
    cJSON *device = cJSON_CreateObject();
    char identifiers[64];
    snprintf(identifiers, sizeof(identifiers), "hb-rf-eth-%s", sysInfo->getSerialNumber());
    cJSON_AddStringToObject(device, "identifiers", identifiers);
    cJSON_AddStringToObject(device, "name", "HB-RF-ETH-ng");
    cJSON_AddStringToObject(device, "model", "HB-RF-ETH-ng");
    cJSON_AddStringToObject(device, "manufacturer", "Xerolux");
    cJSON_AddStringToObject(device, "sw_version", sysInfo->getCurrentVersion());
    cJSON_AddStringToObject(device, "hw_version", sysInfo->getBoardRevisionString());

    // Helper: publish a sensor / binary_sensor / button / update config.
    auto publish_config = [&](const char* component, const char* object_id, const char* name,
                              const char* device_class, const char* state_class,
                              const char* unit_of_measurement, const char* value_template,
                              const char* entity_category = NULL, const char* icon = NULL) {

        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "name", name);

        char unique_id[128];
        snprintf(unique_id, sizeof(unique_id), "%s_%s", identifiers, object_id);
        cJSON_AddStringToObject(root, "unique_id", unique_id);

        char state_topic[160];
        snprintf(state_topic, sizeof(state_topic), "%s/status/%s", current_mqtt_config.topic_prefix, object_id);
        cJSON_AddStringToObject(root, "state_topic", state_topic);

        if (device_class) cJSON_AddStringToObject(root, "device_class", device_class);
        if (state_class) cJSON_AddStringToObject(root, "state_class", state_class);
        if (unit_of_measurement) cJSON_AddStringToObject(root, "unit_of_measurement", unit_of_measurement);
        if (value_template) cJSON_AddStringToObject(root, "value_template", value_template);
        if (entity_category) cJSON_AddStringToObject(root, "entity_category", entity_category);
        if (icon) cJSON_AddStringToObject(root, "icon", icon);

        cJSON_AddItemToObject(root, "device", cJSON_Duplicate(device, 1));

        char *json_str = cJSON_PrintUnformatted(root);
        char topic[256];
        snprintf(topic, sizeof(topic), "%s/%s/hb-rf-eth-%s/%s/config",
                 current_mqtt_config.ha_discovery_prefix, component, sysInfo->getSerialNumber(), object_id);
        esp_mqtt_client_publish(client, topic, json_str, 0, 1, 1);
        free(json_str);
        cJSON_Delete(root);
    };

    // ---- Sensors: system metrics ----------------------------------------
    publish_config("sensor", "cpu_usage", "CPU Usage", NULL, "measurement", "%", NULL, "diagnostic", "mdi:cpu-64-bit");
    publish_config("sensor", "memory_usage", "Memory Usage", NULL, "measurement", "%", NULL, "diagnostic", "mdi:memory");
    publish_config("sensor", "free_heap", "Free Heap", "data_size", "measurement", "B", NULL, "diagnostic", "mdi:memory");
    publish_config("sensor", "supply_voltage", "Supply Voltage", "voltage", "measurement", "V", NULL, "diagnostic", NULL);
    publish_config("sensor", "temperature", "Temperature", "temperature", "measurement", "°C", NULL, "diagnostic", NULL);
    publish_config("sensor", "uptime", "Uptime", "duration", "total_increasing", "s", NULL, "diagnostic", "mdi:clock-outline");
    publish_config("sensor", "uptime_text", "Uptime (Text)", NULL, NULL, NULL, NULL, "diagnostic", "mdi:clock-outline");
    publish_config("sensor", "version", "Current Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:package-variant");
    publish_config("sensor", "latest_version", "Latest Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:package-up");
    publish_config("sensor", "board_revision", "Board Revision", NULL, NULL, NULL, NULL, "diagnostic", "mdi:expansion-card");

    // ---- Sensors: network -----------------------------------------------
    publish_config("binary_sensor", "online", "Online", "connectivity", NULL, NULL,
                   "{{ value_json }}", "diagnostic", "mdi:lan-connect");
    publish_config("binary_sensor", "eth_connected", "Ethernet Link", "connectivity", NULL, NULL,
                   "{{ value_json }}", "diagnostic", "mdi:ethernet");
    publish_config("sensor", "eth_link_speed", "Ethernet Speed", "data_rate", "measurement", "Mbit/s", NULL, "diagnostic", "mdi:speedometer");
    publish_config("sensor", "ip_address", "IP Address", NULL, NULL, NULL, NULL, "diagnostic", "mdi:ip");

    // ---- Sensors: radio module ------------------------------------------
    publish_config("sensor", "radio_module_type", "Radio Module", NULL, NULL, NULL, NULL, "diagnostic", "mdi:radio-tower");
    publish_config("sensor", "radio_module_serial", "Radio Serial", NULL, NULL, NULL, NULL, "diagnostic", "mdi:barcode");
    publish_config("sensor", "radio_module_firmware", "Radio Firmware", NULL, NULL, NULL, NULL, "diagnostic", "mdi:chip");

    // ---- Sensors: time / NTP --------------------------------------------
    publish_config("binary_sensor", "ntp_synced", "NTP Synced", NULL, NULL, NULL,
                   "{{ value_json }}", "diagnostic", "mdi:clock-check");

    // ---- Sensors: OTA ----------------------------------------------------
    publish_config("sensor", "ota_progress", "OTA Progress", NULL, "measurement", "%", NULL, "diagnostic", "mdi:progress-download");

    // ---- Update Available (binary) --------------------------------------
    {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "name", "Update Available");
        char unique_id[128];
        snprintf(unique_id, sizeof(unique_id), "%s_update_available", identifiers);
        cJSON_AddStringToObject(root, "unique_id", unique_id);

        char state_topic[160];
        snprintf(state_topic, sizeof(state_topic), "%s/status/update_available", current_mqtt_config.topic_prefix);
        cJSON_AddStringToObject(root, "state_topic", state_topic);

        cJSON_AddStringToObject(root, "device_class", "update");
        cJSON_AddStringToObject(root, "entity_category", "diagnostic");
        cJSON_AddStringToObject(root, "payload_on", "true");
        cJSON_AddStringToObject(root, "payload_off", "false");

        cJSON_AddItemToObject(root, "device", cJSON_Duplicate(device, 1));

        char *json_str = cJSON_PrintUnformatted(root);
        char topic[256];
        snprintf(topic, sizeof(topic), "%s/binary_sensor/hb-rf-eth-%s/update_available/config",
                 current_mqtt_config.ha_discovery_prefix, sysInfo->getSerialNumber());
        esp_mqtt_client_publish(client, topic, json_str, 0, 1, 1);
        free(json_str);
        cJSON_Delete(root);
    }

    // ---- Buttons ---------------------------------------------------------
    auto publish_button = [&](const char* object_id, const char* name,
                              const char* command, const char* payload_str,
                              const char* device_class = "restart",
                              const char* icon = nullptr) {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "name", name);
        char unique_id[128];
        snprintf(unique_id, sizeof(unique_id), "%s_%s", identifiers, object_id);
        cJSON_AddStringToObject(root, "unique_id", unique_id);

        char command_topic[160];
        snprintf(command_topic, sizeof(command_topic), "%s/command/%s",
                 current_mqtt_config.topic_prefix, command);
        cJSON_AddStringToObject(root, "command_topic", command_topic);
        cJSON_AddStringToObject(root, "payload_press", payload_str);

        cJSON_AddStringToObject(root, "entity_category", "config");
        if (device_class) cJSON_AddStringToObject(root, "device_class", device_class);
        if (icon) cJSON_AddStringToObject(root, "icon", icon);

        cJSON_AddItemToObject(root, "device", cJSON_Duplicate(device, 1));

        char *json_str = cJSON_PrintUnformatted(root);
        char topic[256];
        snprintf(topic, sizeof(topic), "%s/button/hb-rf-eth-%s/%s/config",
                 current_mqtt_config.ha_discovery_prefix, sysInfo->getSerialNumber(), object_id);
        esp_mqtt_client_publish(client, topic, json_str, 0, 1, 1);
        free(json_str);
        cJSON_Delete(root);
    };

    // When commands are disabled, the device ignores every payload - so we
    // must NOT publish buttons that look clickable. Hide them by skipping.
    if (current_mqtt_config.command_enabled) {
        publish_button("restart", "Restart", "restart", restart_payload, "restart", "mdi:restart");
        publish_button("factory_reset", "Factory Reset", "factory_reset", reset_payload, "restart", "mdi:lock-reset");
        publish_button("check_update", "Check for Update", "check_update", chkupd_payload, "update", "mdi:refresh");
    }

    // ---- HA Update entity -----------------------------------------------
    // Combines "current vs latest version" with an Install button that
    // triggers OTA via MQTT. Uses latest_version as state so the entity shows
    // the version number directly. enabled_by_default follows command_enabled.
    {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "name", "Firmware Update");
        char unique_id[128];
        snprintf(unique_id, sizeof(unique_id), "%s_firmware_update", identifiers);
        cJSON_AddStringToObject(root, "unique_id", unique_id);

        char state_topic[160];
        snprintf(state_topic, sizeof(state_topic), "%s/status/latest_version", current_mqtt_config.topic_prefix);
        cJSON_AddStringToObject(root, "state_topic", state_topic);

        char command_topic[160];
        snprintf(command_topic, sizeof(command_topic), "%s/command/update", current_mqtt_config.topic_prefix);
        cJSON_AddStringToObject(root, "command_topic", command_topic);
        cJSON_AddStringToObject(root, "payload_install", update_payload);

        cJSON_AddStringToObject(root, "entity_category", "config");
        cJSON_AddStringToObject(root, "device_class", "firmware");
        cJSON_AddStringToObject(root, "value_template", "{{ value_json }}");
        cJSON_AddStringToObject(root, "latest_version_template", "{{ value_json }}");
        if (!current_mqtt_config.command_enabled) {
            cJSON_AddBoolToObject(root, "enabled_by_default", false);
        }

        cJSON_AddItemToObject(root, "device", cJSON_Duplicate(device, 1));

        char *json_str = cJSON_PrintUnformatted(root);
        char topic[256];
        snprintf(topic, sizeof(topic), "%s/update/hb-rf-eth-%s/firmware_update/config",
                 current_mqtt_config.ha_discovery_prefix, sysInfo->getSerialNumber());
        esp_mqtt_client_publish(client, topic, json_str, 0, 1, 1);
        free(json_str);
        cJSON_Delete(root);
    }

    cJSON_Delete(device);
}

esp_err_t mqtt_handler_init(void)
{
    return ESP_OK;
}

esp_err_t mqtt_handler_start(const mqtt_config_t *config)
{
    if (mqtt_running) {
        ESP_LOGW(TAG, "MQTT already running");
        return ESP_OK;
    }

    if (!config->enabled) {
        return ESP_OK;
    }

    if (strlen(config->server) == 0) {
        ESP_LOGE(TAG, "MQTT Server address is empty");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting MQTT client connecting to %s:%d", config->server, config->port);

    memcpy(&current_mqtt_config, config, sizeof(mqtt_config_t));

    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = NULL;
    mqtt_cfg.broker.address.hostname = current_mqtt_config.server;
    mqtt_cfg.broker.address.port = current_mqtt_config.port;
    mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;

    if (current_mqtt_config.tls_enable) {
        mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_SSL;

        if (current_mqtt_config.tls_skip_verify) {
            mqtt_cfg.broker.verification.skip_cert_common_name_check = true;
        } else {
            if (strlen(current_mqtt_config.tls_ca_certs) > 0) {
                mqtt_cfg.broker.verification.certificate = current_mqtt_config.tls_ca_certs;
            } else {
                mqtt_cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
            }
        }

        if (strlen(current_mqtt_config.tls_certfile) > 0 && strlen(current_mqtt_config.tls_keyfile) > 0) {
            mqtt_cfg.credentials.authentication.certificate = current_mqtt_config.tls_certfile;
            mqtt_cfg.credentials.authentication.key         = current_mqtt_config.tls_keyfile;
        }
    }

    mqtt_cfg.network.timeout_ms = 2000;
    mqtt_cfg.network.reconnect_timeout_ms = 30000;

    if (strlen(current_mqtt_config.user) > 0) {
        mqtt_cfg.credentials.username = current_mqtt_config.user;
    }
    if (strlen(current_mqtt_config.password) > 0) {
        mqtt_cfg.credentials.authentication.password = current_mqtt_config.password;
    }

    // Last Will & Testament: broker publishes "offline" on the status/online
    // topic if the client drops without sending DISCONNECT. Retained so
    // subscribers see the device as offline even after a HA restart.
    {
        char lwt_topic[160];
        snprintf(lwt_topic, sizeof(lwt_topic), "%s/status/online", current_mqtt_config.topic_prefix);
        mqtt_cfg.session.last_will.topic = lwt_topic;
        mqtt_cfg.session.last_will.msg = "offline";
        mqtt_cfg.session.last_will.msg_len = 7;
        mqtt_cfg.session.last_will.qos = 1;
        mqtt_cfg.session.last_will.retain = 1;
    }

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqtt_event_handler, NULL);

    esp_err_t err = esp_mqtt_client_start(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        return err;
    }

    mqtt_running = true;

    // ReleaseInfo body is 4 KB – the publish task copies it via
    // getReleaseInfo(), plus IP/GW formatting. 10 KB gives safe headroom.
    TaskHandle_t pub_handle = NULL;
    xTaskCreate(mqtt_publish_task, "mqtt_publish", 10240, NULL, 4, &pub_handle);
    mqtt_publish_task_handle = pub_handle;

    return ESP_OK;
}

esp_err_t mqtt_handler_stop(void)
{
    if (!mqtt_running) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping MQTT client");
    mqtt_running = false;

    if (client) {
        esp_mqtt_client_stop(client);
    }

    for (int i = 0; i < 30 && mqtt_publish_task_handle != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    TaskHandle_t pub_handle = (TaskHandle_t)mqtt_publish_task_handle;
    if (pub_handle != NULL) {
        ESP_LOGW(TAG, "MQTT publish task did not exit cleanly, force deleting");
        mqtt_publish_task_handle = NULL;
        vTaskDelete(pub_handle);
    }

    if (client) {
        esp_mqtt_client_destroy(client);
        client = NULL;
    }

    return ESP_OK;
}
