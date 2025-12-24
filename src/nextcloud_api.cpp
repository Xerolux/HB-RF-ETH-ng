/*
 *  nextcloud_api.cpp is part of the HB-RF-ETH firmware v2.0
 *
 *  Copyright 2025 Xerolux
 *  API endpoints for Nextcloud backup configuration
 */

#include "nextcloud_api.h"
#include "nextcloud_client.h"
#include "monitoring.h"
#include "security_headers.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <time.h>

static const char *TAG = "NEXTCLOUD_API";

// Helper function to validate authentication (extern from webui.cpp)
extern esp_err_t validate_auth(httpd_req_t *req);

// GET /api/nextcloud - Get Nextcloud configuration
esp_err_t get_nextcloud_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }

    monitoring_config_t config;
    if (monitoring_get_config(&config) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to get config");
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "enabled", config.nextcloud.enabled);
    cJSON_AddStringToObject(root, "serverUrl", config.nextcloud.server_url);
    cJSON_AddStringToObject(root, "username", config.nextcloud.username);
    // Do not send password back for security
    cJSON_AddStringToObject(root, "password", "");
    cJSON_AddStringToObject(root, "backupPath", config.nextcloud.backup_path);
    cJSON_AddNumberToObject(root, "backupIntervalHours", config.nextcloud.backup_interval_hours);
    cJSON_AddBoolToObject(root, "keepLocalBackup", config.nextcloud.keep_local_backup);

    // Add last backup status
    time_t last_timestamp;
    bool last_success;
    if (nextcloud_get_last_backup_status(&last_timestamp, &last_success) == ESP_OK)
    {
        cJSON_AddNumberToObject(root, "lastBackupTimestamp", (double)last_timestamp);
        cJSON_AddBoolToObject(root, "lastBackupSuccess", last_success);
    }
    else
    {
        cJSON_AddNumberToObject(root, "lastBackupTimestamp", 0);
        cJSON_AddBoolToObject(root, "lastBackupSuccess", false);
    }

    char *json_string = cJSON_Print(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, json_string);

    free(json_string);
    return ESP_OK;
}

// POST /api/nextcloud - Update Nextcloud configuration
esp_err_t post_nextcloud_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }

    char content[1024];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
    }
    content[ret] = '\0';

    cJSON *root = cJSON_Parse(content);
    if (root == NULL)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    // Get current config to preserve values not being updated
    monitoring_config_t config;
    if (monitoring_get_config(&config) != ESP_OK)
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to get config");
    }

    // Parse Nextcloud config
    cJSON *enabled = cJSON_GetObjectItem(root, "enabled");
    if (enabled != NULL && cJSON_IsBool(enabled))
    {
        config.nextcloud.enabled = cJSON_IsTrue(enabled);
    }

    cJSON *serverUrl = cJSON_GetObjectItem(root, "serverUrl");
    if (serverUrl != NULL && cJSON_IsString(serverUrl))
    {
        strncpy(config.nextcloud.server_url, serverUrl->valuestring, sizeof(config.nextcloud.server_url) - 1);
        config.nextcloud.server_url[sizeof(config.nextcloud.server_url) - 1] = '\0';
    }

    cJSON *username = cJSON_GetObjectItem(root, "username");
    if (username != NULL && cJSON_IsString(username))
    {
        strncpy(config.nextcloud.username, username->valuestring, sizeof(config.nextcloud.username) - 1);
        config.nextcloud.username[sizeof(config.nextcloud.username) - 1] = '\0';
    }

    cJSON *password = cJSON_GetObjectItem(root, "password");
    if (password != NULL && cJSON_IsString(password) && strlen(password->valuestring) > 0)
    {
        // Only update password if provided (not empty)
        strncpy(config.nextcloud.password, password->valuestring, sizeof(config.nextcloud.password) - 1);
        config.nextcloud.password[sizeof(config.nextcloud.password) - 1] = '\0';
    }

    cJSON *backupPath = cJSON_GetObjectItem(root, "backupPath");
    if (backupPath != NULL && cJSON_IsString(backupPath))
    {
        strncpy(config.nextcloud.backup_path, backupPath->valuestring, sizeof(config.nextcloud.backup_path) - 1);
        config.nextcloud.backup_path[sizeof(config.nextcloud.backup_path) - 1] = '\0';
    }

    cJSON *backupIntervalHours = cJSON_GetObjectItem(root, "backupIntervalHours");
    if (backupIntervalHours != NULL && cJSON_IsNumber(backupIntervalHours))
    {
        config.nextcloud.backup_interval_hours = (uint32_t)backupIntervalHours->valueint;
    }

    cJSON *keepLocalBackup = cJSON_GetObjectItem(root, "keepLocalBackup");
    if (keepLocalBackup != NULL && cJSON_IsBool(keepLocalBackup))
    {
        config.nextcloud.keep_local_backup = cJSON_IsTrue(keepLocalBackup);
    }

    cJSON_Delete(root);

    // Update configuration
    if (monitoring_update_config(&config) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to update config");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, "{\"success\":true}");

    return ESP_OK;
}

// POST /api/nextcloud/test - Test Nextcloud connection
esp_err_t post_nextcloud_test_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }

    ESP_LOGI(TAG, "Testing Nextcloud connection...");

    esp_err_t ret = nextcloud_test_connection();

    cJSON *root = cJSON_CreateObject();
    if (ret == ESP_OK)
    {
        cJSON_AddBoolToObject(root, "success", true);
        cJSON_AddStringToObject(root, "message", "Connection successful");
        ESP_LOGI(TAG, "Nextcloud connection test: SUCCESS");
    }
    else
    {
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "message", "Connection failed");
        cJSON_AddStringToObject(root, "error", esp_err_to_name(ret));
        ESP_LOGW(TAG, "Nextcloud connection test: FAILED (%s)", esp_err_to_name(ret));
    }

    char *json_string = cJSON_Print(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, json_string);

    free(json_string);
    return ESP_OK;
}

// POST /api/nextcloud/upload - Manual backup upload
esp_err_t post_nextcloud_upload_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }

    // Get backup data from request body
    size_t content_len = req->content_len;
    if (content_len == 0 || content_len > 32768) // Max 32KB backup
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid backup size");
    }

    char *backup_data = (char *)malloc(content_len + 1);
    if (!backup_data)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    int ret = httpd_req_recv(req, backup_data, content_len);
    if (ret <= 0)
    {
        free(backup_data);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
    }
    backup_data[ret] = '\0';

    // Generate filename with timestamp
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char filename[64];
    strftime(filename, sizeof(filename), "backup_%Y%m%d_%H%M%S.json", &timeinfo);

    ESP_LOGI(TAG, "Uploading manual backup to Nextcloud: %s", filename);

    esp_err_t upload_ret = nextcloud_upload_backup(backup_data, ret, filename);
    free(backup_data);

    cJSON *root = cJSON_CreateObject();
    if (upload_ret == ESP_OK)
    {
        cJSON_AddBoolToObject(root, "success", true);
        cJSON_AddStringToObject(root, "message", "Backup uploaded successfully");
        cJSON_AddStringToObject(root, "filename", filename);
        ESP_LOGI(TAG, "Manual backup upload: SUCCESS");
    }
    else
    {
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "message", "Upload failed");
        cJSON_AddStringToObject(root, "error", esp_err_to_name(upload_ret));
        ESP_LOGW(TAG, "Manual backup upload: FAILED (%s)", esp_err_to_name(upload_ret));
    }

    char *json_string = cJSON_Print(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, json_string);

    free(json_string);
    return ESP_OK;
}

// Handler structures
httpd_uri_t get_nextcloud_handler = {
    .uri = "/api/nextcloud",
    .method = HTTP_GET,
    .handler = get_nextcloud_handler_func,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL};

httpd_uri_t post_nextcloud_handler = {
    .uri = "/api/nextcloud",
    .method = HTTP_POST,
    .handler = post_nextcloud_handler_func,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL};

httpd_uri_t post_nextcloud_test_handler = {
    .uri = "/api/nextcloud/test",
    .method = HTTP_POST,
    .handler = post_nextcloud_test_handler_func,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL};

httpd_uri_t post_nextcloud_upload_handler = {
    .uri = "/api/nextcloud/upload",
    .method = HTTP_POST,
    .handler = post_nextcloud_upload_handler_func,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL};
