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
#include "esp_log.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"

static const char *TAG = "MQTT";

static esp_mqtt_client_handle_t client = NULL;
static bool mqtt_running = false;
static TaskHandle_t mqtt_publish_task_handle = NULL;
static mqtt_config_t current_mqtt_config;

// Forward declarations
extern SysInfo* monitoring_get_sysinfo(void);
extern UpdateCheck* monitoring_get_updatecheck(void);

void mqtt_handler_publish_ha_discovery(void);

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        // Publish initial status
        mqtt_handler_publish_status();
        // Publish HA discovery config if enabled
        if (current_mqtt_config.ha_discovery_enabled) {
            mqtt_handler_publish_ha_discovery();
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
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
    static UBaseType_t stack_watermark_min = 2048;  // Start with allocated size
    static uint32_t iteration_count = 0;

    while (mqtt_running) {
        mqtt_handler_publish_status();

        // Monitor stack watermark every ~5 minutes (5 iterations * 60s)
        iteration_count++;
        if (iteration_count % 5 == 0) {
            UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
            if (watermark < stack_watermark_min) {
                stack_watermark_min = watermark;
                ESP_LOGI(TAG, "mqtt_publish stack watermark: %u bytes free (allocated: 2048, usage: %.1f%%)",
                         watermark * sizeof(StackType_t),
                         100.0 * (2048 - watermark * sizeof(StackType_t)) / 2048.0);
            }
        }

        // Publish every 60 seconds
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void mqtt_handler_publish_status(void)
{
    if (!mqtt_running || client == NULL) {
        return;
    }

    SysInfo* sysInfo = monitoring_get_sysinfo();
    UpdateCheck* updateCheck = monitoring_get_updatecheck();

    if (sysInfo == NULL) {
        ESP_LOGW(TAG, "SysInfo not available");
        return;
    }

    char topic[128];
    char payload[64];

    // Helper macro to publish
    #define PUBLISH(subtopic, value) \
        snprintf(topic, sizeof(topic), "%s/%s", current_mqtt_config.topic_prefix, subtopic); \
        esp_mqtt_client_publish(client, topic, value, 0, 1, 1)

    // Helper macro to publish double
    #define PUBLISH_DOUBLE(subtopic, value, prec) \
        snprintf(payload, sizeof(payload), "%.*f", prec, value); \
        PUBLISH(subtopic, payload)

    // Helper macro to publish int
    #define PUBLISH_INT(subtopic, value) \
        snprintf(payload, sizeof(payload), "%d", value); \
        PUBLISH(subtopic, payload)

    // Helper macro to publish string
    #define PUBLISH_STR(subtopic, value) \
        PUBLISH(subtopic, value)

    // Status Page Topics
    PUBLISH_STR("status/serial", sysInfo->getSerialNumber());
    PUBLISH_STR("status/version", sysInfo->getCurrentVersion());

    if (updateCheck) {
        const char* latest = updateCheck->getLatestVersion();
        PUBLISH_STR("status/latest_version", latest);

        bool updateAvailable = (strcmp(sysInfo->getCurrentVersion(), latest) != 0 && strcmp(latest, "n/a") != 0);
        PUBLISH_STR("status/update_available", updateAvailable ? "true" : "false");
    }

    PUBLISH_DOUBLE("status/cpu_usage", sysInfo->getCpuUsage(), 1);
    PUBLISH_DOUBLE("status/memory_usage", sysInfo->getMemoryUsage(), 1);
    PUBLISH_DOUBLE("status/supply_voltage", sysInfo->getSupplyVoltage(), 2);
    PUBLISH_DOUBLE("status/temperature", sysInfo->getTemperature(), 1);
    PUBLISH_INT("status/uptime", (int)sysInfo->getUptimeSeconds());
    PUBLISH_STR("status/board_revision", sysInfo->getBoardRevisionString());

    // Uptime formatted
    uint32_t days, hours, minutes;
    uint64_t uptime_s = sysInfo->getUptimeSeconds();
    days = uptime_s / 86400;
    uptime_s %= 86400;
    hours = uptime_s / 3600;
    uptime_s %= 3600;
    minutes = uptime_s / 60;

    snprintf(payload, sizeof(payload), "%lu d, %lu h, %lu m", (unsigned long)days, (unsigned long)hours, (unsigned long)minutes);
    PUBLISH_STR("status/uptime_text", payload);

}

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

    // Device Info
    cJSON *device = cJSON_CreateObject();
    char identifiers[64];
    snprintf(identifiers, sizeof(identifiers), "hb-rf-eth-%s", sysInfo->getSerialNumber());
    cJSON_AddStringToObject(device, "identifiers", identifiers);
    cJSON_AddStringToObject(device, "name", "HB-RF-ETH-ng");
    cJSON_AddStringToObject(device, "model", "HB-RF-ETH-ng");
    cJSON_AddStringToObject(device, "manufacturer", "Xerolux");
    cJSON_AddStringToObject(device, "sw_version", sysInfo->getCurrentVersion());
    // Use board revision as hardware version
    cJSON_AddStringToObject(device, "hw_version", sysInfo->getBoardRevisionString());
    // configuration_url
    // Since we don't know the IP/hostname easily here without including settings/ethernet,
    // we skip config_url or use a generic one if possible.
    // Actually we can leave it out.

    // Helper lambda to publish discovery config
    auto publish_config = [&](const char* component, const char* object_id, const char* name,
                              const char* device_class, const char* state_class,
                              const char* unit_of_measurement, const char* value_template,
                              const char* entity_category = NULL, const char* icon = NULL) {

        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "name", name);

        char unique_id[128];
        snprintf(unique_id, sizeof(unique_id), "%s_%s", identifiers, object_id);
        cJSON_AddStringToObject(root, "unique_id", unique_id);

        char state_topic[128];
        snprintf(state_topic, sizeof(state_topic), "%s/status/%s", current_mqtt_config.topic_prefix, object_id);

        // Adjust state topic if value_template is generic (not matching specific status topic)
        // Here we assume object_id matches the subtopic in status/... unless we need logic
        // But our status topics are flat: prefix/status/cpu_usage

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

    // CPU Usage
    publish_config("sensor", "cpu_usage", "CPU Usage", NULL, "measurement", "%", NULL, "diagnostic", "mdi:cpu-64-bit");

    // Memory Usage
    publish_config("sensor", "memory_usage", "Memory Usage", NULL, "measurement", "%", NULL, "diagnostic", "mdi:memory");

    // Supply Voltage
    publish_config("sensor", "supply_voltage", "Supply Voltage", "voltage", "measurement", "V", NULL, "diagnostic", NULL);

    // Temperature
    publish_config("sensor", "temperature", "Temperature", "temperature", "measurement", "°C", NULL, "diagnostic", NULL);

    // Uptime (seconds)
    publish_config("sensor", "uptime", "Uptime", "duration", "total_increasing", "s", NULL, "diagnostic", "mdi:clock-outline");

    // Uptime (text)
    publish_config("sensor", "uptime_text", "Uptime (Text)", NULL, NULL, NULL, NULL, "diagnostic", "mdi:clock-outline");

    // Update Available
    // Binary sensor for update available
    // For binary sensor, we need payload_on="true", payload_off="false"
    {
         cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "name", "Update Available");
        char unique_id[128];
        snprintf(unique_id, sizeof(unique_id), "%s_update_available", identifiers);
        cJSON_AddStringToObject(root, "unique_id", unique_id);

        char state_topic[128];
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

    // Current Version
    publish_config("sensor", "version", "Current Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:package-variant");

    // Latest Version
    publish_config("sensor", "latest_version", "Latest Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:package-up");

    // Board Revision
    publish_config("sensor", "board_revision", "Board Revision", NULL, NULL, NULL, NULL, "diagnostic", "mdi:expansion-card");


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
    mqtt_cfg.broker.address.uri = NULL; // We construct URI or use host/port
    mqtt_cfg.broker.address.hostname = current_mqtt_config.server;
    mqtt_cfg.broker.address.port = current_mqtt_config.port;
    mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;

    if (strlen(current_mqtt_config.user) > 0) {
        mqtt_cfg.credentials.username = current_mqtt_config.user;
    }
    if (strlen(current_mqtt_config.password) > 0) {
        mqtt_cfg.credentials.authentication.password = current_mqtt_config.password;
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

    // Start publishing task
    // Reduced from 4096 to 2048 bytes - simple periodic task with small stack buffers
    // Only calls mqtt_handler_publish_status() every 60 seconds
    xTaskCreate(mqtt_publish_task, "mqtt_publish", 2048, NULL, 5, &mqtt_publish_task_handle);

    return ESP_OK;
}

esp_err_t mqtt_handler_stop(void)
{
    if (!mqtt_running) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping MQTT client");
    mqtt_running = false;

    if (mqtt_publish_task_handle) {
        vTaskDelete(mqtt_publish_task_handle);
        mqtt_publish_task_handle = NULL;
    }

    if (client) {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client = NULL;
    }

    return ESP_OK;
}
