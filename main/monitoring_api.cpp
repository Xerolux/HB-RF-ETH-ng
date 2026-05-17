/*
 *  monitoring_api.cpp is part of the HB-RF-ETH firmware v2.0
 *
 *  Copyright 2025 Xerolux
 *  API endpoints for monitoring configuration
 */

#include "monitoring_api.h"
#include "monitoring.h"
#include "validation.h"
#include "security_headers.h"
#include "esp_err.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>


// Helper function to validate authentication (extern from webui.cpp)
extern esp_err_t validate_auth(httpd_req_t *req);

static esp_err_t send_json_error(httpd_req_t *req, const char *message)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "{\"error\":\"%s\"}", message);
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

static esp_err_t send_monitoring_test_response(httpd_req_t *req, const char *target, bool ok, const char *message)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "target", target);
    cJSON_AddBoolToObject(root, "ok", ok);
    cJSON_AddStringToObject(root, "message", message);

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_string);
    free(json_string);
    return ESP_OK;
}

// GET /api/monitoring - Get monitoring configuration
esp_err_t get_monitoring_handler_func(httpd_req_t *req)
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

    // MQTT config
    cJSON *mqtt = cJSON_CreateObject();
    cJSON_AddBoolToObject(mqtt, "enabled", config.mqtt.enabled);
    cJSON_AddStringToObject(mqtt, "server", config.mqtt.server);
    cJSON_AddNumberToObject(mqtt, "port", config.mqtt.port);
    cJSON_AddStringToObject(mqtt, "user", config.mqtt.user);
    // Do not send password back for security, or send empty/dummy?
    // Usually we send empty if set, or just existing.
    // The frontend handles empty password as "don't change".
    // For now, let's just send empty string if set to avoid exposing it.
    cJSON_AddStringToObject(mqtt, "password", "");
    cJSON_AddStringToObject(mqtt, "topicPrefix", config.mqtt.topic_prefix);
    cJSON_AddBoolToObject(mqtt, "haDiscoveryEnabled", config.mqtt.ha_discovery_enabled);
    cJSON_AddStringToObject(mqtt, "haDiscoveryPrefix", config.mqtt.ha_discovery_prefix);
    cJSON_AddBoolToObject(mqtt, "tlsEnable", config.mqtt.tls_enable);
    cJSON_AddBoolToObject(mqtt, "tlsSkipVerify", config.mqtt.tls_skip_verify);
    cJSON_AddBoolToObject(mqtt, "tlsCaCertsSet",   strlen(config.mqtt.tls_ca_certs)  > 0);
    cJSON_AddBoolToObject(mqtt, "tlsCertfileSet",  strlen(config.mqtt.tls_certfile)  > 0);
    cJSON_AddBoolToObject(mqtt, "tlsKeyfileSet",   strlen(config.mqtt.tls_keyfile)   > 0);
    cJSON_AddItemToObject(root, "mqtt", mqtt);

    // CheckMK config
    cJSON *checkmk = cJSON_CreateObject();
    cJSON_AddBoolToObject(checkmk, "enabled", config.checkmk.enabled);
    cJSON_AddNumberToObject(checkmk, "port", config.checkmk.port);
    cJSON_AddStringToObject(checkmk, "allowedHosts", config.checkmk.allowed_hosts);
    cJSON_AddItemToObject(root, "checkmk", checkmk);

    char *json_string = cJSON_Print(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    /* CORS header removed - same-origin requests only */
    httpd_resp_sendstr(req, json_string);

    free(json_string);
    return ESP_OK;
}

// POST /api/monitoring - Update monitoring configuration
esp_err_t post_monitoring_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }

    // Heap-allocate to keep stack usage within httpd task limits
    char *content = (char *)malloc(16384);
    if (!content)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }
    int ret = httpd_req_recv(req, content, 16383);
    if (ret <= 0)
    {
        free(content);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"error\":\"Invalid request\"}");
        return ESP_FAIL;
    }
    content[ret] = '\0';

    cJSON *root = cJSON_Parse(content);
    free(content);
    if (root == NULL)
    {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"error\":\"Invalid JSON\"}");
        return ESP_OK;
    }

    // Load current config first to preserve fields not sent by frontend
    // (e.g., MQTT password is never sent back for security reasons)
    monitoring_config_t config = {};
    monitoring_get_config(&config);

    // Parse CheckMK config
    cJSON *checkmk = cJSON_GetObjectItem(root, "checkmk");
    if (checkmk != NULL)
    {
        cJSON *enabled = cJSON_GetObjectItem(checkmk, "enabled");
        if (enabled != NULL && cJSON_IsBool(enabled))
        {
            config.checkmk.enabled = cJSON_IsTrue(enabled);
        }

        cJSON *port = cJSON_GetObjectItem(checkmk, "port");
        if (port != NULL && cJSON_IsNumber(port))
        {
            if (!validatePort(port->valueint))
            {
                cJSON_Delete(root);
                return send_json_error(req, "Invalid CheckMK port");
            }
            config.checkmk.port = port->valueint;
        }

        cJSON *allowedHosts = cJSON_GetObjectItem(checkmk, "allowedHosts");
        if (allowedHosts != NULL && cJSON_IsString(allowedHosts))
        {
            if (!validateStringLength(allowedHosts->valuestring, sizeof(config.checkmk.allowed_hosts) - 1))
            {
                cJSON_Delete(root);
                return send_json_error(req, "CheckMK allowed hosts string too long");
            }
            strncpy(config.checkmk.allowed_hosts, allowedHosts->valuestring, sizeof(config.checkmk.allowed_hosts) - 1);
        }
    }

    // Parse MQTT config
    cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
    if (mqtt != NULL)
    {
        cJSON *enabled = cJSON_GetObjectItem(mqtt, "enabled");
        if (enabled != NULL && cJSON_IsBool(enabled))
        {
            config.mqtt.enabled = cJSON_IsTrue(enabled);
        }

        cJSON *server = cJSON_GetObjectItem(mqtt, "server");
        if (server != NULL && cJSON_IsString(server))
        {
            if (config.mqtt.enabled || strlen(server->valuestring) > 0)
            {
                if (!validateServerAddress(server->valuestring, sizeof(config.mqtt.server) - 1))
                {
                    cJSON_Delete(root);
                    return send_json_error(req, "MQTT server address is empty or invalid");
                }
            }
            strncpy(config.mqtt.server, server->valuestring, sizeof(config.mqtt.server) - 1);
        }

        cJSON *port = cJSON_GetObjectItem(mqtt, "port");
        if (port != NULL && cJSON_IsNumber(port))
        {
            if (!validatePort(port->valueint))
            {
                cJSON_Delete(root);
                return send_json_error(req, "Invalid MQTT port");
            }
            config.mqtt.port = port->valueint;
        }

        cJSON *user = cJSON_GetObjectItem(mqtt, "user");
        if (user != NULL && cJSON_IsString(user))
        {
            if (!validateStringLength(user->valuestring, sizeof(config.mqtt.user) - 1))
            {
                cJSON_Delete(root);
                return send_json_error(req, "MQTT user string too long");
            }
            strncpy(config.mqtt.user, user->valuestring, sizeof(config.mqtt.user) - 1);
        }

        cJSON *password = cJSON_GetObjectItem(mqtt, "password");
        if (password != NULL && cJSON_IsString(password) && strlen(password->valuestring) > 0)
        {
            if (!validateStringLength(password->valuestring, sizeof(config.mqtt.password) - 1))
            {
                cJSON_Delete(root);
                return send_json_error(req, "MQTT password too long");
            }
            // Only update password if provided
            strncpy(config.mqtt.password, password->valuestring, sizeof(config.mqtt.password) - 1);
        }

        cJSON *topicPrefix = cJSON_GetObjectItem(mqtt, "topicPrefix");
        if (topicPrefix != NULL && cJSON_IsString(topicPrefix))
        {
            if (!validateStringLength(topicPrefix->valuestring, sizeof(config.mqtt.topic_prefix) - 1))
            {
                cJSON_Delete(root);
                return send_json_error(req, "MQTT topic prefix too long");
            }
            strncpy(config.mqtt.topic_prefix, topicPrefix->valuestring, sizeof(config.mqtt.topic_prefix) - 1);
        }

        cJSON *haDiscoveryEnabled = cJSON_GetObjectItem(mqtt, "haDiscoveryEnabled");
        if (haDiscoveryEnabled != NULL && cJSON_IsBool(haDiscoveryEnabled))
        {
            config.mqtt.ha_discovery_enabled = cJSON_IsTrue(haDiscoveryEnabled);
        }

        cJSON *haDiscoveryPrefix = cJSON_GetObjectItem(mqtt, "haDiscoveryPrefix");
        if (haDiscoveryPrefix != NULL && cJSON_IsString(haDiscoveryPrefix))
        {
            if (!validateStringLength(haDiscoveryPrefix->valuestring, sizeof(config.mqtt.ha_discovery_prefix) - 1))
            {
                cJSON_Delete(root);
                return send_json_error(req, "MQTT HA discovery prefix too long");
            }
            strncpy(config.mqtt.ha_discovery_prefix, haDiscoveryPrefix->valuestring, sizeof(config.mqtt.ha_discovery_prefix) - 1);
        }

        cJSON *tlsEnable = cJSON_GetObjectItem(mqtt, "tlsEnable");
        if (tlsEnable != NULL && cJSON_IsBool(tlsEnable))
        {
            config.mqtt.tls_enable = cJSON_IsTrue(tlsEnable);
        }

        cJSON *tlsSkipVerify = cJSON_GetObjectItem(mqtt, "tlsSkipVerify");
        if (tlsSkipVerify != NULL && cJSON_IsBool(tlsSkipVerify))
        {
            config.mqtt.tls_skip_verify = cJSON_IsTrue(tlsSkipVerify);
        }

        cJSON *tlsCaCertsClear = cJSON_GetObjectItem(mqtt, "tlsCaCertsClear");
        if (tlsCaCertsClear != NULL && cJSON_IsTrue(tlsCaCertsClear))
        {
            config.mqtt.tls_ca_certs[0] = '\0';
        }
        cJSON *tlsCaCerts = cJSON_GetObjectItem(mqtt, "tlsCaCerts");
        if (tlsCaCerts != NULL && cJSON_IsString(tlsCaCerts) && strlen(tlsCaCerts->valuestring) > 0)
        {
            if (!validateStringLength(tlsCaCerts->valuestring, sizeof(config.mqtt.tls_ca_certs) - 1))
            {
                cJSON_Delete(root);
                return send_json_error(req, "MQTT TLS CA certs too long");
            }
            strncpy(config.mqtt.tls_ca_certs, tlsCaCerts->valuestring, sizeof(config.mqtt.tls_ca_certs) - 1);
            config.mqtt.tls_ca_certs[sizeof(config.mqtt.tls_ca_certs) - 1] = '\0';
        }

        cJSON *tlsCertfileClear = cJSON_GetObjectItem(mqtt, "tlsCertfileClear");
        if (tlsCertfileClear != NULL && cJSON_IsTrue(tlsCertfileClear))
        {
            config.mqtt.tls_certfile[0] = '\0';
        }
        cJSON *tlsCertfile = cJSON_GetObjectItem(mqtt, "tlsCertfile");
        if (tlsCertfile != NULL && cJSON_IsString(tlsCertfile) && strlen(tlsCertfile->valuestring) > 0)
        {
            if (!validateStringLength(tlsCertfile->valuestring, sizeof(config.mqtt.tls_certfile) - 1))
            {
                cJSON_Delete(root);
                return send_json_error(req, "MQTT TLS client cert too long");
            }
            strncpy(config.mqtt.tls_certfile, tlsCertfile->valuestring, sizeof(config.mqtt.tls_certfile) - 1);
            config.mqtt.tls_certfile[sizeof(config.mqtt.tls_certfile) - 1] = '\0';
        }

        cJSON *tlsKeyfileClear = cJSON_GetObjectItem(mqtt, "tlsKeyfileClear");
        if (tlsKeyfileClear != NULL && cJSON_IsTrue(tlsKeyfileClear))
        {
            config.mqtt.tls_keyfile[0] = '\0';
        }
        cJSON *tlsKeyfile = cJSON_GetObjectItem(mqtt, "tlsKeyfile");
        if (tlsKeyfile != NULL && cJSON_IsString(tlsKeyfile) && strlen(tlsKeyfile->valuestring) > 0)
        {
            if (!validateStringLength(tlsKeyfile->valuestring, sizeof(config.mqtt.tls_keyfile) - 1))
            {
                cJSON_Delete(root);
                return send_json_error(req, "MQTT TLS client key too long");
            }
            strncpy(config.mqtt.tls_keyfile, tlsKeyfile->valuestring, sizeof(config.mqtt.tls_keyfile) - 1);
            config.mqtt.tls_keyfile[sizeof(config.mqtt.tls_keyfile) - 1] = '\0';
        }
    }

    cJSON_Delete(root);

    // Schedule the config update asynchronously so the HTTP handler returns immediately.
    // Stopping/restarting MQTT and CheckMK can take several seconds; doing it synchronously
    // would block the httpd task, stall the HTTP response, and risk a watchdog reset.
    esp_err_t schedule_err = monitoring_schedule_update_config(&config);
    if (schedule_err == ESP_ERR_INVALID_STATE)
    {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"error\":\"Config update already in progress, please wait\"}");
        return ESP_OK;
    }
    if (schedule_err != ESP_OK)
    {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"error\":\"Failed to schedule config update\"}");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    /* CORS header removed - same-origin requests only */
    httpd_resp_sendstr(req, "{\"success\":true}");

    return ESP_OK;
}

httpd_uri_t get_monitoring_handler = {
    .uri = "/api/monitoring",
    .method = HTTP_GET,
    .handler = get_monitoring_handler_func,
    .user_ctx = NULL
};

httpd_uri_t post_monitoring_handler = {
    .uri = "/api/monitoring",
    .method = HTTP_POST,
    .handler = post_monitoring_handler_func,
    .user_ctx = NULL
};

esp_err_t get_monitoring_test_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }

    char query[64];
    char target[16];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "target", target, sizeof(target)) != ESP_OK)
    {
        return send_json_error(req, "Missing diagnostic target");
    }

    bool ok = false;
    char message[160];
    esp_err_t result = monitoring_run_diagnostic(target, &ok, message, sizeof(message));
    if (result == ESP_ERR_NOT_SUPPORTED) {
        return send_json_error(req, "Unsupported diagnostic target");
    }
    if (result != ESP_OK && message[0] == '\0') {
        return send_json_error(req, "Diagnostic failed");
    }

    return send_monitoring_test_response(req, target, ok, message);
}

httpd_uri_t get_monitoring_test_handler = {
    .uri = "/api/monitoring/test",
    .method = HTTP_GET,
    .handler = get_monitoring_test_handler_func,
    .user_ctx = NULL
};
