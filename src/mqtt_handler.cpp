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

// Forward declarations for system commands
extern "C" void esp_restart(void);
extern "C" esp_err_t settings_factory_reset(void);

static void handle_mqtt_command(const char* command)
{
    ESP_LOGI(TAG, "Received MQTT command: %s", command);

    if (strcmp(command, "restart") == 0) {
        ESP_LOGI(TAG, "Restart command received via MQTT");
        esp_restart();
    } else if (strcmp(command, "factory_reset") == 0) {
        ESP_LOGI(TAG, "Factory reset command received via MQTT");
        settings_factory_reset();
        esp_restart();
    } else if (strcmp(command, "update") == 0) {
        ESP_LOGI(TAG, "Update command received via MQTT");
        UpdateCheck* updateCheck = monitoring_get_updatecheck();
        if (updateCheck) {
            updateCheck->performOnlineUpdate();
        } else {
            ESP_LOGW(TAG, "UpdateCheck not available");
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
        // Subscribe to command topic if HA discovery is enabled
        if (current_mqtt_config.ha_discovery_enabled) {
            char command_topic[128];
            snprintf(command_topic, sizeof(command_topic), "%s/command/#", current_mqtt_config.topic_prefix);
            esp_mqtt_client_subscribe(client, command_topic, 1);
            ESP_LOGI(TAG, "Subscribed to command topic: %s", command_topic);
        }
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
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        // Handle incoming commands
        if (current_mqtt_config.ha_discovery_enabled) {
            char command_topic_prefix[128];
            snprintf(command_topic_prefix, sizeof(command_topic_prefix), "%s/command/", current_mqtt_config.topic_prefix);

            if (strncmp(event->topic, command_topic_prefix, strlen(command_topic_prefix)) == 0) {
                const char* command = event->topic + strlen(command_topic_prefix);
                handle_mqtt_command(command);
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
    while (mqtt_running) {
        mqtt_handler_publish_status();
        // Publish every 60 seconds, but check mqtt_running more often to exit promptly
        for (int i = 0; i < 60 && mqtt_running; i++) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    mqtt_publish_task_handle = NULL;
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

    // HA Buttons - Restart Button
    {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "name", "Restart");
        char unique_id[128];
        snprintf(unique_id, sizeof(unique_id), "%s_restart", identifiers);
        cJSON_AddStringToObject(root, "unique_id", unique_id);

        char command_topic[128];
        snprintf(command_topic, sizeof(command_topic), "%s/command/restart", current_mqtt_config.topic_prefix);
        cJSON_AddStringToObject(root, "command_topic", command_topic);

        cJSON_AddStringToObject(root, "entity_category", "config");
        cJSON_AddStringToObject(root, "device_class", "restart");
        cJSON_AddStringToObject(root, "payload_press", "restart");

        cJSON_AddItemToObject(root, "device", cJSON_Duplicate(device, 1));

        char *json_str = cJSON_PrintUnformatted(root);
        char topic[256];
        snprintf(topic, sizeof(topic), "%s/button/hb-rf-eth-%s/restart/config",
                 current_mqtt_config.ha_discovery_prefix, sysInfo->getSerialNumber());

        esp_mqtt_client_publish(client, topic, json_str, 0, 1, 1);
        free(json_str);
        cJSON_Delete(root);
    }

    // HA Buttons - Factory Reset Button
    {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "name", "Factory Reset");
        char unique_id[128];
        snprintf(unique_id, sizeof(unique_id), "%s_factory_reset", identifiers);
        cJSON_AddStringToObject(root, "unique_id", unique_id);

        char command_topic[128];
        snprintf(command_topic, sizeof(command_topic), "%s/command/factory_reset", current_mqtt_config.topic_prefix);
        cJSON_AddStringToObject(root, "command_topic", command_topic);

        cJSON_AddStringToObject(root, "entity_category", "config");
        cJSON_AddStringToObject(root, "device_class", "restart");
        cJSON_AddStringToObject(root, "payload_press", "factory_reset");

        cJSON_AddItemToObject(root, "device", cJSON_Duplicate(device, 1));

        char *json_str = cJSON_PrintUnformatted(root);
        char topic[256];
        snprintf(topic, sizeof(topic), "%s/button/hb-rf-eth-%s/factory_reset/config",
                 current_mqtt_config.ha_discovery_prefix, sysInfo->getSerialNumber());

        esp_mqtt_client_publish(client, topic, json_str, 0, 1, 1);
        free(json_str);
        cJSON_Delete(root);
    }

    // HA Update Entity - uses update entity type for firmware updates
    {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "name", "Firmware Update");
        char unique_id[128];
        snprintf(unique_id, sizeof(unique_id), "%s_firmware_update", identifiers);
        cJSON_AddStringToObject(root, "unique_id", unique_id);

        char command_topic[128];
        snprintf(command_topic, sizeof(command_topic), "%s/command/update", current_mqtt_config.topic_prefix);
        cJSON_AddStringToObject(root, "command_topic", command_topic);

        char state_topic[128];
        snprintf(state_topic, sizeof(state_topic), "%s/status/latest_version", current_mqtt_config.topic_prefix);
        cJSON_AddStringToObject(root, "state_topic", state_topic);

        cJSON_AddStringToObject(root, "entity_category", "config");
        cJSON_AddStringToObject(root, "device_class", "firmware");
        cJSON_AddStringToObject(root, "payload_install", "update");

        // Add latest version as state
        cJSON_AddStringToObject(root, "value_template", "{{ value_json }}");

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
    xTaskCreate(mqtt_publish_task, "mqtt_publish", 4096, NULL, 5, &mqtt_publish_task_handle);

    return ESP_OK;
}

esp_err_t mqtt_handler_stop(void)
{
    if (!mqtt_running) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping MQTT client");
    mqtt_running = false;

    // Wait for publish task to exit on its own (max 2 seconds)
    for (int i = 0; i < 20 && mqtt_publish_task_handle != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Force-delete only if it didn't exit in time
    if (mqtt_publish_task_handle != NULL) {
        ESP_LOGW(TAG, "MQTT publish task did not exit cleanly, force deleting");
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
