/*
 *  nextcloud_client.cpp - Nextcloud WebDAV Backup Client Implementation
 *
 *  Part of HB-RF-ETH-ng firmware v2.1
 *  Copyright 2025 Xerolux
 */

#include "nextcloud_client.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <ctime>

static const char *TAG = "Nextcloud";

// Global configuration
static nextcloud_config_t g_config = {};
static bool g_initialized = false;
static TaskHandle_t g_backup_task_handle = NULL;

// Last backup status
static time_t g_last_backup_timestamp = 0;
static bool g_last_backup_success = false;

/**
 * Build WebDAV URL for file operations
 */
static void build_webdav_url(char *url, size_t url_size, const char *filename) {
    // Nextcloud WebDAV endpoint: /remote.php/dav/files/USERNAME/path
    snprintf(url, url_size, "%s/remote.php/dav/files/%s%s/%s",
             g_config.server_url,
             g_config.username,
             g_config.backup_path,
             filename ? filename : "");
}

/**
 * HTTP event handler for capturing response data
 */
typedef struct {
    char *buffer;
    size_t buffer_size;
    size_t data_len;
} http_download_context_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    http_download_context_t *ctx = (http_download_context_t *)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (ctx && evt->data_len > 0) {
                // Reallocate buffer if needed
                if (ctx->data_len + evt->data_len >= ctx->buffer_size) {
                    size_t new_size = ctx->buffer_size + evt->data_len + 1024;
                    char *new_buffer = (char *)realloc(ctx->buffer, new_size);
                    if (!new_buffer) {
                        ESP_LOGE(TAG, "Failed to allocate memory for download");
                        return ESP_FAIL;
                    }
                    ctx->buffer = new_buffer;
                    ctx->buffer_size = new_size;
                }

                memcpy(ctx->buffer + ctx->data_len, evt->data, evt->data_len);
                ctx->data_len += evt->data_len;
                ctx->buffer[ctx->data_len] = '\0';
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

/**
 * Initialize Nextcloud client
 */
esp_err_t nextcloud_init(const nextcloud_config_t *config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&g_config, config, sizeof(nextcloud_config_t));
    g_initialized = true;

    ESP_LOGI(TAG, "Nextcloud client initialized: %s", config->enabled ? "enabled" : "disabled");

    if (config->enabled) {
        return nextcloud_start_auto_backup();
    }

    return ESP_OK;
}

/**
 * Upload backup to Nextcloud via WebDAV PUT
 */
esp_err_t nextcloud_upload_backup(const char *json_data, size_t json_len, const char *filename) {
    if (!g_initialized || !g_config.enabled) {
        ESP_LOGW(TAG, "Nextcloud not initialized or disabled");
        return ESP_ERR_INVALID_STATE;
    }

    char url[512];
    build_webdav_url(url, sizeof(url), filename);

    ESP_LOGI(TAG, "Uploading backup to: %s", url);

    // Configure HTTP client for WebDAV PUT
    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_PUT;
    config.auth_type = HTTP_AUTH_TYPE_BASIC;
    config.username = g_config.username;
    config.password = g_config.password;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 30000;  // 30 second timeout
    config.buffer_size = 4096;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    // Set content type
    esp_http_client_set_header(client, "Content-Type", "application/json");

    // Write JSON data
    esp_err_t err = esp_http_client_open(client, json_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int written = esp_http_client_write(client, json_data, json_len);
    if (written < 0) {
        ESP_LOGE(TAG, "Failed to write data");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    // Get response
    (void)esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (status_code == 201 || status_code == 204) {
        ESP_LOGI(TAG, "Backup uploaded successfully (HTTP %d)", status_code);
        g_last_backup_timestamp = time(NULL);
        g_last_backup_success = true;
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Upload failed with HTTP %d", status_code);
        g_last_backup_success = false;
        return ESP_FAIL;
    }
}

/**
 * Download backup from Nextcloud via WebDAV GET
 */
esp_err_t nextcloud_download_backup(const char *filename, char **out_data, size_t *out_len) {
    if (!g_initialized || !filename || !out_data || !out_len) {
        return ESP_ERR_INVALID_ARG;
    }

    char url[512];
    build_webdav_url(url, sizeof(url), filename);

    ESP_LOGI(TAG, "Downloading backup from: %s", url);

    // Prepare download context
    http_download_context_t ctx = {};
    ctx.buffer_size = 4096;
    ctx.buffer = (char *)malloc(ctx.buffer_size);
    if (!ctx.buffer) {
        ESP_LOGE(TAG, "Failed to allocate initial buffer");
        return ESP_ERR_NO_MEM;
    }
    ctx.data_len = 0;

    // Configure HTTP client
    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_GET;
    config.auth_type = HTTP_AUTH_TYPE_BASIC;
    config.username = g_config.username;
    config.password = g_config.password;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.event_handler = http_event_handler;
    config.user_data = &ctx;
    config.timeout_ms = 30000;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(ctx.buffer);
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status_code != 200) {
        ESP_LOGE(TAG, "Download failed: %s (HTTP %d)", esp_err_to_name(err), status_code);
        free(ctx.buffer);
        return ESP_FAIL;
    }

    *out_data = ctx.buffer;
    *out_len = ctx.data_len;

    ESP_LOGI(TAG, "Backup downloaded successfully (%zu bytes)", ctx.data_len);
    return ESP_OK;
}

/**
 * Test connection to Nextcloud
 */
esp_err_t nextcloud_test_connection(void) {
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    char url[512];
    build_webdav_url(url, sizeof(url), NULL);  // Test base path

    ESP_LOGI(TAG, "Testing connection to: %s", url);

    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_PROPFIND;  // WebDAV directory listing
    config.auth_type = HTTP_AUTH_TYPE_BASIC;
    config.username = g_config.username;
    config.password = g_config.password;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 10000;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Depth", "0");

    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err == ESP_OK && (status_code == 207 || status_code == 200)) {
        ESP_LOGI(TAG, "Connection test successful (HTTP %d)", status_code);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Connection test failed: %s (HTTP %d)", esp_err_to_name(err), status_code);
        return ESP_FAIL;
    }
}

/**
 * Automatic backup task
 */
static void nextcloud_backup_task(void *pvParameters) {
    ESP_LOGI(TAG, "Automatic backup task started (interval: %lu hours)", g_config.backup_interval_hours);

    while (g_config.enabled) {
        // Wait for configured interval
        vTaskDelay(pdMS_TO_TICKS(g_config.backup_interval_hours * 3600 * 1000));

        if (!g_config.enabled) {
            break;
        }

        ESP_LOGI(TAG, "Starting automatic backup...");

        // This would integrate with existing backup system
        // For now, just log that backup would happen
        // TODO: Call actual backup generation function from webui.cpp

        ESP_LOGW(TAG, "Automatic backup logic not yet fully integrated");
    }

    ESP_LOGI(TAG, "Automatic backup task stopped");
    g_backup_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * Start automatic backup task
 */
esp_err_t nextcloud_start_auto_backup(void) {
    if (!g_initialized || !g_config.enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_backup_task_handle != NULL) {
        ESP_LOGW(TAG, "Backup task already running");
        return ESP_OK;
    }

    BaseType_t ret = xTaskCreate(nextcloud_backup_task, "nextcloud_backup",
                                  4096, NULL, 3, &g_backup_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create backup task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * Stop automatic backup task
 */
void nextcloud_stop_auto_backup(void) {
    if (g_backup_task_handle != NULL) {
        g_config.enabled = false;  // Signal task to stop
        vTaskDelay(pdMS_TO_TICKS(100));  // Give task time to exit
        if (g_backup_task_handle != NULL) {
            vTaskDelete(g_backup_task_handle);
            g_backup_task_handle = NULL;
        }
    }
}

/**
 * Get last backup status
 */
esp_err_t nextcloud_get_last_backup_status(time_t *out_timestamp, bool *out_success) {
    if (!out_timestamp || !out_success) {
        return ESP_ERR_INVALID_ARG;
    }

    if (g_last_backup_timestamp == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    *out_timestamp = g_last_backup_timestamp;
    *out_success = g_last_backup_success;
    return ESP_OK;
}

/**
 * List available backups (simplified - would need XML parsing for full PROPFIND)
 */
esp_err_t nextcloud_list_backups(char ***out_list, size_t *out_count) {
    // TODO: Implement WebDAV PROPFIND with XML parsing
    // This requires more complex XML handling
    ESP_LOGW(TAG, "List backups not yet implemented - requires XML parsing");
    return ESP_ERR_NOT_SUPPORTED;
}
