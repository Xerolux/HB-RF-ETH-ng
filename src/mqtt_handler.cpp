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
    while (mqtt_running) {
        mqtt_handler_publish_status();
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
