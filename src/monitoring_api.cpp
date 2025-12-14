/*
 *  monitoring_api.cpp is part of the HB-RF-ETH firmware v2.0
 *
 *  Copyright 2025 Xerolux
 *  API endpoints for monitoring configuration
 */

#include "monitoring_api.h"
#include "monitoring.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

// static const char *TAG = "MONITORING_API";

// Helper function to validate authentication (extern from webui.cpp)
extern esp_err_t validate_auth(httpd_req_t *req);

// GET /api/monitoring - Get monitoring configuration
esp_err_t get_monitoring_handler_func(httpd_req_t *req)
{
    if (validate_auth(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }

    monitoring_config_t config;
    if (monitoring_get_config(&config) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to get config");
    }

    cJSON *root = cJSON_CreateObject();

    // SNMP config
    cJSON *snmp = cJSON_CreateObject();
    cJSON_AddBoolToObject(snmp, "enabled", config.snmp.enabled);
    cJSON_AddStringToObject(snmp, "community", config.snmp.community);
    cJSON_AddStringToObject(snmp, "location", config.snmp.location);
    cJSON_AddStringToObject(snmp, "contact", config.snmp.contact);
    cJSON_AddNumberToObject(snmp, "port", config.snmp.port);
    cJSON_AddItemToObject(root, "snmp", snmp);

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
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, json_string);

    free(json_string);
    return ESP_OK;
}

// POST /api/monitoring - Update monitoring configuration
esp_err_t post_monitoring_handler_func(httpd_req_t *req)
{
    if (validate_auth(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }

    char content[1024];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
    }
    content[ret] = '\0';

    cJSON *root = cJSON_Parse(content);
    if (root == NULL) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    monitoring_config_t config = {};

    // Parse SNMP config
    cJSON *snmp = cJSON_GetObjectItem(root, "snmp");
    if (snmp != NULL) {
        cJSON *enabled = cJSON_GetObjectItem(snmp, "enabled");
        if (enabled != NULL && cJSON_IsBool(enabled)) {
            config.snmp.enabled = cJSON_IsTrue(enabled);
        }

        cJSON *community = cJSON_GetObjectItem(snmp, "community");
        if (community != NULL && cJSON_IsString(community)) {
            strncpy(config.snmp.community, community->valuestring, sizeof(config.snmp.community) - 1);
        }

        cJSON *location = cJSON_GetObjectItem(snmp, "location");
        if (location != NULL && cJSON_IsString(location)) {
            strncpy(config.snmp.location, location->valuestring, sizeof(config.snmp.location) - 1);
        }

        cJSON *contact = cJSON_GetObjectItem(snmp, "contact");
        if (contact != NULL && cJSON_IsString(contact)) {
            strncpy(config.snmp.contact, contact->valuestring, sizeof(config.snmp.contact) - 1);
        }

        cJSON *port = cJSON_GetObjectItem(snmp, "port");
        if (port != NULL && cJSON_IsNumber(port)) {
            config.snmp.port = port->valueint;
        } else {
            config.snmp.port = 161;  // default
        }
    }

    // Parse CheckMK config
    cJSON *checkmk = cJSON_GetObjectItem(root, "checkmk");
    if (checkmk != NULL) {
        cJSON *enabled = cJSON_GetObjectItem(checkmk, "enabled");
        if (enabled != NULL && cJSON_IsBool(enabled)) {
            config.checkmk.enabled = cJSON_IsTrue(enabled);
        }

        cJSON *port = cJSON_GetObjectItem(checkmk, "port");
        if (port != NULL && cJSON_IsNumber(port)) {
            config.checkmk.port = port->valueint;
        } else {
            config.checkmk.port = 6556;  // default
        }

        cJSON *allowedHosts = cJSON_GetObjectItem(checkmk, "allowedHosts");
        if (allowedHosts != NULL && cJSON_IsString(allowedHosts)) {
            strncpy(config.checkmk.allowed_hosts, allowedHosts->valuestring, sizeof(config.checkmk.allowed_hosts) - 1);
        }
    }

    // Parse MQTT config
    cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
    if (mqtt != NULL) {
        cJSON *enabled = cJSON_GetObjectItem(mqtt, "enabled");
        if (enabled != NULL && cJSON_IsBool(enabled)) {
            config.mqtt.enabled = cJSON_IsTrue(enabled);
        }

        cJSON *server = cJSON_GetObjectItem(mqtt, "server");
        if (server != NULL && cJSON_IsString(server)) {
            strncpy(config.mqtt.server, server->valuestring, sizeof(config.mqtt.server) - 1);
        }

        cJSON *port = cJSON_GetObjectItem(mqtt, "port");
        if (port != NULL && cJSON_IsNumber(port)) {
            config.mqtt.port = port->valueint;
        }

        cJSON *user = cJSON_GetObjectItem(mqtt, "user");
        if (user != NULL && cJSON_IsString(user)) {
            strncpy(config.mqtt.user, user->valuestring, sizeof(config.mqtt.user) - 1);
        }

        cJSON *password = cJSON_GetObjectItem(mqtt, "password");
        if (password != NULL && cJSON_IsString(password) && strlen(password->valuestring) > 0) {
            // Only update password if provided
            strncpy(config.mqtt.password, password->valuestring, sizeof(config.mqtt.password) - 1);
        }

        cJSON *topicPrefix = cJSON_GetObjectItem(mqtt, "topicPrefix");
        if (topicPrefix != NULL && cJSON_IsString(topicPrefix)) {
            strncpy(config.mqtt.topic_prefix, topicPrefix->valuestring, sizeof(config.mqtt.topic_prefix) - 1);
        }

        cJSON *haDiscoveryEnabled = cJSON_GetObjectItem(mqtt, "haDiscoveryEnabled");
        if (haDiscoveryEnabled != NULL && cJSON_IsBool(haDiscoveryEnabled)) {
            config.mqtt.ha_discovery_enabled = cJSON_IsTrue(haDiscoveryEnabled);
        }

        cJSON *haDiscoveryPrefix = cJSON_GetObjectItem(mqtt, "haDiscoveryPrefix");
        if (haDiscoveryPrefix != NULL && cJSON_IsString(haDiscoveryPrefix)) {
            strncpy(config.mqtt.ha_discovery_prefix, haDiscoveryPrefix->valuestring,
                    sizeof(config.mqtt.ha_discovery_prefix) - 1);
        }
    }

    cJSON_Delete(root);

    // Update configuration
    if (monitoring_update_config(&config) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to update config");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, "{\"success\":true}");

    return ESP_OK;
}

httpd_uri_t get_monitoring_handler = {
    .uri = "/api/monitoring", .method = HTTP_GET, .handler = get_monitoring_handler_func, .user_ctx = NULL};

httpd_uri_t post_monitoring_handler = {
    .uri = "/api/monitoring", .method = HTTP_POST, .handler = post_monitoring_handler_func, .user_ctx = NULL};
