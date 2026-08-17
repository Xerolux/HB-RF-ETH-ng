/*
 *  webui_backup.cpp is part of the HB-RF-ETH firmware v2.0
 *
 *  Original work Copyright 2022 Alexander Reinert
 *  https://github.com/alexreinert/HB-RF-ETH
 *
 *  Modified work Copyright 2025 Xerolux
 *  Modernized fork - Updated to ESP-IDF 6.0 and modern toolchains
 *
 *  The HB-RF-ETH firmware is licensed under a
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 *
 *  You should have received a copy of the license along with this
 *  work.  If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <sys/param.h>
#include <atomic>
#include <memory>
#include <new>
#include "webui.h"
#include "webui_internal.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "esp_ota_ops.h"
#include "monitoring.h"
#include "monitoring_api.h"
#include "security_headers.h"
#include "secure_utils.h"
#include "log_manager.h"
#include "reset_info.h"
#include "nvs_storage_lock.h"
#include "system_reset.h"
#include "crash_blackbox.h"
#include "events.h"
#include "theme_api.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "validation.h"
#include "settings.h"
#include "led.h"

// Settings backup export and restore, extracted from webui.cpp.
//
// This is the single largest self-contained group of handlers in the WebUI:
// it serializes every persisted subsystem (device settings, monitoring, theme)
// into one JSON document and parses that document back with per-field
// validation. It shares only the request helpers in webui_internal.h with the
// rest of the WebUI, which is what made it separable without behaviour risk.

static const char *TAG = "WebUI.backup";

static bool backup_get_bool(const cJSON *object, const char *key, bool *value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsBool(item) || !value) return false;
    *value = cJSON_IsTrue(item);
    return true;
}

static bool backup_get_uint(const cJSON *object, const char *key,
                            unsigned maximum, unsigned *value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsNumber(item) || !value ||
        item->valuedouble < 0 || item->valuedouble > maximum ||
        item->valuedouble != item->valueint) {
        return false;
    }
    *value = static_cast<unsigned>(item->valueint);
    return true;
}

static bool backup_get_string(const cJSON *object, const char *key,
                              char *value, size_t value_size)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsString(item) || !item->valuestring ||
        !value || value_size == 0 ||
        strlen(item->valuestring) >= value_size) {
        return false;
    }
    snprintf(value, value_size, "%s", item->valuestring);
    return true;
}

static esp_err_t add_monitoring_backup(cJSON *root)
{
    monitoring_config_t *config =
        new (std::nothrow) monitoring_config_t();
    if (!config) return ESP_ERR_NO_MEM;
    if (monitoring_get_config(config) != ESP_OK) {
        delete config;
        return ESP_ERR_INVALID_STATE;
    }

    cJSON *monitoring = cJSON_AddObjectToObject(root, "monitoring");
    cJSON *checkmk = monitoring
        ? cJSON_AddObjectToObject(monitoring, "checkmk") : NULL;
    cJSON *mqtt = monitoring
        ? cJSON_AddObjectToObject(monitoring, "mqtt") : NULL;
    cJSON *prometheus = monitoring
        ? cJSON_AddObjectToObject(monitoring, "prometheus") : NULL;
    cJSON *syslog = monitoring
        ? cJSON_AddObjectToObject(monitoring, "syslog") : NULL;
    cJSON *notify = monitoring
        ? cJSON_AddObjectToObject(monitoring, "notify") : NULL;
    if (!monitoring || !checkmk || !mqtt || !prometheus ||
        !syslog || !notify) {
        delete config;
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddBoolToObject(checkmk, "enabled", config->checkmk.enabled);
    cJSON_AddNumberToObject(checkmk, "port", config->checkmk.port);
    cJSON_AddStringToObject(checkmk, "allowedHosts",
                            config->checkmk.allowed_hosts);

    cJSON_AddBoolToObject(mqtt, "enabled", config->mqtt.enabled);
    cJSON_AddStringToObject(mqtt, "server", config->mqtt.server);
    cJSON_AddNumberToObject(mqtt, "port", config->mqtt.port);
    cJSON_AddStringToObject(mqtt, "user", config->mqtt.user);
    cJSON_AddStringToObject(mqtt, "password", config->mqtt.password);
    cJSON_AddStringToObject(mqtt, "topicPrefix", config->mqtt.topic_prefix);
    cJSON_AddBoolToObject(mqtt, "haDiscoveryEnabled",
                          config->mqtt.ha_discovery_enabled);
    cJSON_AddStringToObject(mqtt, "haDiscoveryPrefix",
                            config->mqtt.ha_discovery_prefix);
    cJSON_AddBoolToObject(mqtt, "tlsEnable", config->mqtt.tls_enable);
    cJSON_AddBoolToObject(mqtt, "tlsSkipVerify",
                          config->mqtt.tls_skip_verify);
    cJSON_AddStringToObject(mqtt, "tlsCaCerts",
                            config->mqtt.tls_ca_certs);
    cJSON_AddStringToObject(mqtt, "tlsCertfile",
                            config->mqtt.tls_certfile);
    cJSON_AddStringToObject(mqtt, "tlsKeyfile",
                            config->mqtt.tls_keyfile);
    cJSON_AddBoolToObject(mqtt, "commandEnabled",
                          config->mqtt.command_enabled);
    cJSON_AddStringToObject(mqtt, "commandToken",
                            config->mqtt.command_token);

    cJSON_AddBoolToObject(prometheus, "enabled",
                          config->prometheus.enabled);
    cJSON_AddNumberToObject(prometheus, "port", config->prometheus.port);
    cJSON_AddStringToObject(prometheus, "allowedHosts",
                            config->prometheus.allowed_hosts);

    cJSON_AddBoolToObject(syslog, "enabled", config->syslog.enabled);
    cJSON_AddStringToObject(syslog, "server", config->syslog.server);
    cJSON_AddNumberToObject(syslog, "port", config->syslog.port);
    cJSON_AddNumberToObject(syslog, "transport",
                            config->syslog.transport);
    cJSON_AddNumberToObject(syslog, "minSeverity",
                            config->syslog.min_severity);
    cJSON_AddStringToObject(syslog, "hostname",
                            config->syslog.hostname);

    cJSON_AddBoolToObject(notify, "enabled", config->notify.enabled);
    cJSON_AddNumberToObject(notify, "channels", config->notify.channels);
    cJSON_AddStringToObject(notify, "webhookUrl",
                            config->notify.webhook_url);
    cJSON_AddStringToObject(notify, "webhookSecret",
                            config->notify.webhook_secret);
    cJSON_AddStringToObject(notify, "telegramToken",
                            config->notify.telegram_token);
    cJSON_AddStringToObject(notify, "telegramChatId",
                            config->notify.telegram_chatid);
    cJSON_AddStringToObject(notify, "smtpServer",
                            config->notify.smtp_server);
    cJSON_AddNumberToObject(notify, "smtpPort",
                            config->notify.smtp_port);
    cJSON_AddNumberToObject(notify, "smtpTls",
                            config->notify.smtp_tls);
    cJSON_AddStringToObject(notify, "smtpUser",
                            config->notify.smtp_user);
    cJSON_AddStringToObject(notify, "smtpPassword",
                            config->notify.smtp_password);
    cJSON_AddStringToObject(notify, "smtpFrom",
                            config->notify.smtp_from);
    cJSON_AddStringToObject(notify, "smtpTo",
                            config->notify.smtp_to);
    cJSON_AddNumberToObject(notify, "cooldownSeconds",
                            config->notify.cooldown_seconds);
    cJSON_AddNumberToObject(notify, "eventMask", config->notify.event_mask);

    delete config;
    return ESP_OK;
}

static esp_err_t parse_monitoring_backup(
    const cJSON *root, monitoring_config_t **restored_config)
{
    if (!restored_config) return ESP_ERR_INVALID_ARG;
    *restored_config = NULL;

    const cJSON *monitoring =
        cJSON_GetObjectItemCaseSensitive(root, "monitoring");
    if (!monitoring) return ESP_OK; // Backwards-compatible legacy backup.
    if (!cJSON_IsObject(monitoring)) return ESP_ERR_INVALID_ARG;

    const cJSON *checkmk =
        cJSON_GetObjectItemCaseSensitive(monitoring, "checkmk");
    const cJSON *mqtt =
        cJSON_GetObjectItemCaseSensitive(monitoring, "mqtt");
    const cJSON *prometheus =
        cJSON_GetObjectItemCaseSensitive(monitoring, "prometheus");
    const cJSON *syslog =
        cJSON_GetObjectItemCaseSensitive(monitoring, "syslog");
    const cJSON *notify =
        cJSON_GetObjectItemCaseSensitive(monitoring, "notify");
    if (!cJSON_IsObject(checkmk) || !cJSON_IsObject(mqtt) ||
        !cJSON_IsObject(prometheus) || !cJSON_IsObject(syslog) ||
        !cJSON_IsObject(notify)) {
        return ESP_ERR_INVALID_ARG;
    }

    monitoring_config_t *config =
        new (std::nothrow) monitoring_config_t();
    if (!config) return ESP_ERR_NO_MEM;
    memset(config, 0, sizeof(*config));
    unsigned number = 0;

    bool valid =
        backup_get_bool(checkmk, "enabled", &config->checkmk.enabled) &&
        backup_get_uint(checkmk, "port", UINT16_MAX, &number);
    if (valid) config->checkmk.port = static_cast<uint16_t>(number);
    valid = valid && validatePort(config->checkmk.port) &&
        backup_get_string(checkmk, "allowedHosts",
                          config->checkmk.allowed_hosts,
                          sizeof(config->checkmk.allowed_hosts));

    valid = valid &&
        backup_get_bool(mqtt, "enabled", &config->mqtt.enabled) &&
        backup_get_string(mqtt, "server", config->mqtt.server,
                          sizeof(config->mqtt.server)) &&
        backup_get_uint(mqtt, "port", UINT16_MAX, &number);
    if (valid) config->mqtt.port = static_cast<uint16_t>(number);
    valid = valid && validatePort(config->mqtt.port) &&
        backup_get_string(mqtt, "user", config->mqtt.user,
                          sizeof(config->mqtt.user)) &&
        backup_get_string(mqtt, "password", config->mqtt.password,
                          sizeof(config->mqtt.password)) &&
        backup_get_string(mqtt, "topicPrefix", config->mqtt.topic_prefix,
                          sizeof(config->mqtt.topic_prefix)) &&
        backup_get_bool(mqtt, "haDiscoveryEnabled",
                        &config->mqtt.ha_discovery_enabled) &&
        backup_get_string(mqtt, "haDiscoveryPrefix",
                          config->mqtt.ha_discovery_prefix,
                          sizeof(config->mqtt.ha_discovery_prefix)) &&
        backup_get_bool(mqtt, "tlsEnable", &config->mqtt.tls_enable) &&
        backup_get_bool(mqtt, "tlsSkipVerify",
                        &config->mqtt.tls_skip_verify) &&
        backup_get_string(mqtt, "tlsCaCerts",
                          config->mqtt.tls_ca_certs,
                          sizeof(config->mqtt.tls_ca_certs)) &&
        backup_get_string(mqtt, "tlsCertfile",
                          config->mqtt.tls_certfile,
                          sizeof(config->mqtt.tls_certfile)) &&
        backup_get_string(mqtt, "tlsKeyfile",
                          config->mqtt.tls_keyfile,
                          sizeof(config->mqtt.tls_keyfile)) &&
        backup_get_bool(mqtt, "commandEnabled",
                        &config->mqtt.command_enabled) &&
        backup_get_string(mqtt, "commandToken",
                          config->mqtt.command_token,
                          sizeof(config->mqtt.command_token));
    if (valid && (config->mqtt.enabled || config->mqtt.server[0] != '\0')) {
        valid = validateServerAddress(config->mqtt.server,
                                      sizeof(config->mqtt.server) - 1);
    }
    if (valid && config->mqtt.command_token[0] != '\0') {
        valid = validateMqttCommandToken(config->mqtt.command_token);
    }

    valid = valid &&
        backup_get_bool(prometheus, "enabled",
                        &config->prometheus.enabled) &&
        backup_get_uint(prometheus, "port", UINT16_MAX, &number);
    if (valid) config->prometheus.port = static_cast<uint16_t>(number);
    valid = valid && validatePort(config->prometheus.port) &&
        backup_get_string(prometheus, "allowedHosts",
                          config->prometheus.allowed_hosts,
                          sizeof(config->prometheus.allowed_hosts));

    valid = valid &&
        backup_get_bool(syslog, "enabled", &config->syslog.enabled) &&
        backup_get_string(syslog, "server", config->syslog.server,
                          sizeof(config->syslog.server)) &&
        backup_get_uint(syslog, "port", UINT16_MAX, &number);
    if (valid) config->syslog.port = static_cast<uint16_t>(number);
    valid = valid && validatePort(config->syslog.port) &&
        backup_get_uint(syslog, "transport", 2, &number);
    if (valid) config->syslog.transport = static_cast<uint8_t>(number);
    valid = valid &&
        backup_get_uint(syslog, "minSeverity", 7, &number);
    if (valid) config->syslog.min_severity = static_cast<uint8_t>(number);
    valid = valid &&
        backup_get_string(syslog, "hostname", config->syslog.hostname,
                          sizeof(config->syslog.hostname));
    if (valid && (config->syslog.enabled || config->syslog.server[0] != '\0')) {
        valid = validateServerAddress(config->syslog.server,
                                      sizeof(config->syslog.server) - 1);
    }

    valid = valid &&
        backup_get_bool(notify, "enabled", &config->notify.enabled) &&
        backup_get_uint(notify, "channels", 7, &number);
    if (valid) config->notify.channels = static_cast<uint8_t>(number);
    valid = valid &&
        backup_get_string(notify, "webhookUrl",
                          config->notify.webhook_url,
                          sizeof(config->notify.webhook_url)) &&
        backup_get_string(notify, "webhookSecret",
                          config->notify.webhook_secret,
                          sizeof(config->notify.webhook_secret)) &&
        backup_get_string(notify, "telegramToken",
                          config->notify.telegram_token,
                          sizeof(config->notify.telegram_token)) &&
        backup_get_string(notify, "telegramChatId",
                          config->notify.telegram_chatid,
                          sizeof(config->notify.telegram_chatid)) &&
        backup_get_string(notify, "smtpServer",
                          config->notify.smtp_server,
                          sizeof(config->notify.smtp_server)) &&
        backup_get_uint(notify, "smtpPort", UINT16_MAX, &number);
    if (valid) config->notify.smtp_port = static_cast<uint16_t>(number);
    valid = valid && validatePort(config->notify.smtp_port) &&
        backup_get_uint(notify, "smtpTls", 2, &number);
    if (valid) config->notify.smtp_tls = static_cast<uint8_t>(number);
    valid = valid &&
        backup_get_string(notify, "smtpUser", config->notify.smtp_user,
                          sizeof(config->notify.smtp_user)) &&
        backup_get_string(notify, "smtpPassword",
                          config->notify.smtp_password,
                          sizeof(config->notify.smtp_password)) &&
        backup_get_string(notify, "smtpFrom", config->notify.smtp_from,
                          sizeof(config->notify.smtp_from)) &&
        backup_get_string(notify, "smtpTo", config->notify.smtp_to,
                          sizeof(config->notify.smtp_to)) &&
        backup_get_uint(notify, "cooldownSeconds", UINT16_MAX, &number);
    if (valid) {
        config->notify.cooldown_seconds = static_cast<uint16_t>(number);
    }
    // Optional on purpose: backups written before the event selection existed
    // have no such key, and requiring it would make every one of them fail to
    // restore the whole notify block. A missing key keeps the default, which
    // is the same "everything" behaviour those backups were taken under.
    unsigned event_mask = 0;
    if (backup_get_uint(notify, "eventMask", NOTIFY_EVENT_ALL, &event_mask)) {
        config->notify.event_mask = static_cast<uint16_t>(event_mask);
    }
    // Same CRLF/control-char rejection as the normal POST /api/monitoring
    // path (monitoring_api.cpp): these three fields are interpolated
    // verbatim into raw SMTP protocol lines / an HTTP header, so a crafted
    // or corrupted backup file must not be able to reintroduce header/command
    // injection through the restore path.
    if (valid && config->notify.webhook_secret[0] != '\0') {
        valid = validateNoControlChars(config->notify.webhook_secret);
    }
    if (valid && config->notify.smtp_from[0] != '\0') {
        valid = validateNoControlChars(config->notify.smtp_from);
    }
    if (valid && config->notify.smtp_to[0] != '\0') {
        valid = validateNoControlChars(config->notify.smtp_to);
    }

    if (!valid) {
        delete config;
        return ESP_ERR_INVALID_ARG;
    }
    *restored_config = config;
    return ESP_OK;
}

static esp_err_t add_theme_backup(cJSON *root)
{
    char scheme[8] = {};
    char color[8] = {};
    esp_err_t result = theme_api_get_config(
        scheme, sizeof(scheme), color, sizeof(color));
    if (result != ESP_OK) return result;
    cJSON *theme = cJSON_AddObjectToObject(root, "theme");
    if (!theme) return ESP_ERR_NO_MEM;
    cJSON_AddStringToObject(theme, "colorScheme", scheme);
    cJSON_AddStringToObject(theme, "primaryColor", color);
    return ESP_OK;
}

static esp_err_t parse_theme_backup(const cJSON *root,
                                    char scheme[8], char color[8],
                                    bool *has_theme)
{
    if (!scheme || !color || !has_theme) return ESP_ERR_INVALID_ARG;
    *has_theme = false;
    const cJSON *theme = cJSON_GetObjectItemCaseSensitive(root, "theme");
    if (!theme) return ESP_OK; // Backwards-compatible legacy backup.
    if (!cJSON_IsObject(theme) ||
        !backup_get_string(theme, "colorScheme", scheme, 8) ||
        !backup_get_string(theme, "primaryColor", color, 8)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (strcmp(scheme, "system") != 0 &&
        strcmp(scheme, "light") != 0 &&
        strcmp(scheme, "dark") != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(color) != 7 || color[0] != '#') return ESP_ERR_INVALID_ARG;
    for (size_t index = 1; index < 7; ++index) {
        if (!isxdigit(static_cast<unsigned char>(color[index]))) {
            return ESP_ERR_INVALID_ARG;
        }
    }
    *has_theme = true;
    return ESP_OK;
}

esp_err_t get_backup_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(
        req, "Content-Disposition",
        "attachment; filename=\"hb-rf-eth-ng-backup.json\"");
    httpd_resp_set_hdr(req, "Cache-Control",
                       "no-store, no-cache, must-revalidate, max-age=0");

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Out of memory");
    }

    cJSON_AddStringToObject(root, "_format", "hb-rf-eth-ng-backup");
    cJSON_AddNumberToObject(root, "_version", 2);

    // A complete, text-editable JSON backup necessarily contains secrets in
    // plaintext. Keep this warning inside the file so it remains visible even
    // when the backup is copied or opened outside the WebUI.
    cJSON *security = cJSON_AddObjectToObject(root, "_security");
    if (!security) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Out of memory");
    }
    cJSON_AddBoolToObject(security, "containsPlaintextSecrets", true);
    cJSON_AddStringToObject(
        security, "warning",
        "This unencrypted backup contains passwords, tokens, certificates and "
        "private keys in plaintext. Store it like a password and never publish it.");

    // Unknown metadata keys are intentionally ignored by the restore parser.
    // This makes the file useful as a manually edited multi-device template
    // without weakening validation of the actual configuration fields.
    cJSON *portability = cJSON_AddObjectToObject(root, "_portability");
    if (!portability) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Out of memory");
    }
    cJSON_AddBoolToObject(portability, "editable", true);
    cJSON_AddStringToObject(
        portability, "warning",
        "Before importing on another device, review hostname, static IP, "
        "administrator password and device-specific MQTT values.");

    add_settings(root);

    // Merge settings object into root if add_settings creates a sub-object
    // NOTE: add_settings creates a "settings" object inside root.
    // If we want a flat structure or specific structure for restore, we need to match post_settings_json_handler expectations.
    // post_settings_json_handler expects a flat JSON object with keys like "adminUsername", "adminPassword", "hostname", etc.
    // But add_settings creates { "settings": { "hostname": ... } }

    // We need to flatten it.
    cJSON *settingsObj = cJSON_GetObjectItem(root, "settings");
    if (settingsObj) {
        cJSON *child = settingsObj->child;
        while (child) {
            cJSON_AddItemToObject(root, child->string, cJSON_Duplicate(child, 1));
            child = child->next;
        }
        cJSON_DeleteItemFromObject(root, "settings");
    }

    // A complete backup intentionally contains credentials and private keys.
    // It is available only through the authenticated endpoint and marked
    // no-store, but the downloaded file itself must be protected by the user.
    cJSON_AddStringToObject(root, "adminPassword",
                            webui_settings()->getAdminPassword());
    cJSON_AddBoolToObject(root, "passwordChanged",
                          webui_settings()->getPasswordChanged());

    if (add_theme_backup(root) != ESP_OK ||
        add_monitoring_backup(root) != ESP_OK) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Could not create complete backup");
    }

    const char *json = cJSON_PrintUnformatted(root);
    if (json) {
        httpd_resp_sendstr(req, json);
        free((void *)json);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON alloc failed");
    }
    cJSON_Delete(root);

    return ESP_OK;
}

httpd_uri_t get_backup_handler = {
    .uri = "/api/backup",
    .method = HTTP_GET,
    .handler = get_backup_handler_func,
    .user_ctx = NULL};

// Serializes restore/factory-reset mutations with monitoring configuration,
// manual firmware writes and MQTT restarts.  Every early return releases the
// reservation automatically; a successful reserved restart never returns.
esp_err_t post_restore_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    ScopedOperationReservation operation;
    if (!operation) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(
            req, "Another configuration or restart operation is active");
    }

    // Complete backups include MQTT TLS material and notification credentials,
    // so they can legitimately be larger than the old settings-only 8 KiB
    // payload.
    constexpr size_t maxRestoreBodySize = 24576;
    if (req->content_len == 0)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data received");
    }
    if (req->content_len >= maxRestoreBodySize)
    {
        return httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Request body too large");
    }

    const size_t bufferSize = req->content_len + 1;
    char *buffer = (char*)malloc(bufferSize);
    if (!buffer) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
    }

    int len = recv_full_body(req, buffer, bufferSize);

    if (len < 0)
    {
        free(buffer);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive request body");
    }

    cJSON *root = cJSON_Parse(buffer);
    free(buffer);

    if (!root) {
         return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    monitoring_config_t *parsed_monitoring = NULL;
    const esp_err_t monitoring_parse_result =
        parse_monitoring_backup(root, &parsed_monitoring);
    std::unique_ptr<monitoring_config_t> restored_monitoring(
        parsed_monitoring);
    char restored_theme_scheme[8] = {};
    char restored_theme_color[8] = {};
    bool has_restored_theme = false;
    const esp_err_t theme_parse_result =
        parse_theme_backup(root, restored_theme_scheme,
                           restored_theme_color, &has_restored_theme);
    if (monitoring_parse_result != ESP_OK ||
        theme_parse_result != ESP_OK) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request",
                               "invalid_backup", "backup");
    }

    char *adminUsername = cJSON_GetStringValue(cJSON_GetObjectItem(root, "adminUsername"));
    char *adminPassword = cJSON_GetStringValue(cJSON_GetObjectItem(root, "adminPassword"));
    char *currentPassword = cJSON_GetStringValue(
        cJSON_GetObjectItem(root, "currentPassword"));
    cJSON *passwordChangedItem =
        cJSON_GetObjectItem(root, "passwordChanged");

    char *hostname = cJSON_GetStringValue(cJSON_GetObjectItem(root, "hostname"));
    // Presence guard mirrors the IPv6 fields below: without it,
    // cJSON_GetBoolValue silently returns false on a missing key, and a
    // partial payload reaching setNetworkSettings() would turn DHCP off.
    // buildSettingsPayload() always sends useDHCP so this rarely matters,
    // but the guard makes the contract explicit and future-proof.
    bool useDHCP = cJSON_GetObjectItem(root, "useDHCP")
                       ? cJSON_GetBoolValue(cJSON_GetObjectItem(root, "useDHCP"))
                       : webui_settings()->getUseDHCP();
    ip4_addr_t localIP = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "localIP"));
    ip4_addr_t netmask = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "netmask"));
    ip4_addr_t gateway = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "gateway"));
    ip4_addr_t dns1 = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "dns1"));
    ip4_addr_t dns2 = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "dns2"));

    timesource_t timesource = (timesource_t)cJSON_GetIntValueSafe(
        cJSON_GetObjectItem(root, "timesource"), (int)webui_settings()->getTimesource());

    int dcfOffset = cJSON_GetIntValueSafe(
        cJSON_GetObjectItem(root, "dcfOffset"), webui_settings()->getDcfOffset());

    int gpsBaudrate = cJSON_GetIntValueSafe(
        cJSON_GetObjectItem(root, "gpsBaudrate"), webui_settings()->getGpsBaudrate());

    char *ntpServer = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ntpServer"));
    int ledBrightness = cJSON_GetIntValueSafe(
        cJSON_GetObjectItem(root, "ledBrightness"), webui_settings()->getLEDBrightness());
    cJSON *ledPrograms = cJSON_GetObjectItem(root, "ledPrograms");
    bool enableIPv6 = cJSON_GetBoolValue(
        cJSON_GetObjectItem(root, "enableIPv6"));
    char *ipv6Mode = cJSON_GetStringValue(
        cJSON_GetObjectItem(root, "ipv6Mode"));
    char *ipv6Address = cJSON_GetStringValue(
        cJSON_GetObjectItem(root, "ipv6Address"));
    int ipv6PrefixLength = cJSON_GetIntValueSafe(
        cJSON_GetObjectItem(root, "ipv6PrefixLength"), 64);
    char *ipv6Gateway = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Gateway"));
    char *ipv6Dns1 = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Dns1"));
    char *ipv6Dns2 = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Dns2"));
    char *ccuIP = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ccuIP"));
    cJSON *systemLogEnabledItem =
        cJSON_GetObjectItem(root, "systemLogEnabled");
    cJSON *flashPauseItem = cJSON_GetObjectItem(root, "flashPause");
    cJSON *testDesignItem = cJSON_GetObjectItem(root, "testDesignEnabled");

    // Validate the entire backup and prove NVS capacity before changing any
    // live or persistent setting. A rejected restore must leave the running
    // configuration untouched.
    const bool has_password_changed_flag =
        passwordChangedItem && cJSON_IsBool(passwordChangedItem);
    if (adminUsername && adminUsername[0] != '\0' &&
        !valid_admin_username(adminUsername)) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request", "invalid_username",
                               "adminUsername");
    }
    bool admin_password_change_requested = false;
    if (adminPassword && adminPassword[0] != '\0') {
        const size_t password_len = strlen(adminPassword);
        if (password_len >= 33) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_password",
                                   "adminPassword");
        }
        admin_password_change_requested =
            secure_strcmp(adminPassword,
                          webui_settings()->getAdminPassword()) != 0;
        // A same-password backup is configuration-only. For an actual
        // credential replacement, legacy backups with an explicit flag may
        // retain their historic password while new-format input must satisfy
        // the current password policy.
        const bool password_valid = !admin_password_change_requested ||
            (has_password_changed_flag
                ? password_len > 0
                : validateAdminPassword(adminPassword));
        if (!password_valid) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_password",
                                   "adminPassword");
        }
    }
    if (admin_password_change_requested) {
        if (currentPassword == NULL || currentPassword[0] == '\0') {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request",
                                   "current_password_required",
                                   "currentPassword");
        }
        if (secure_strcmp(currentPassword,
                          webui_settings()->getAdminPassword()) != 0) {
            cJSON_Delete(root);
            return send_json_error(req, "403 Forbidden",
                                   "current_password_incorrect",
                                   "currentPassword");
        }
    }
    if (hostname && !validateHostname(hostname)) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request", "invalid_hostname",
                               "hostname");
    }
    if (hostname && !useDHCP) {
        const bool local_ok = parse_ipv4_json(
            cJSON_GetObjectItem(root, "localIP"), false, &localIP);
        const bool netmask_ok = parse_ipv4_json(
            cJSON_GetObjectItem(root, "netmask"), false, &netmask);
        const bool gateway_ok = parse_ipv4_json(
            cJSON_GetObjectItem(root, "gateway"), false, &gateway);
        if (!local_ok || localIP.addr == IPADDR_ANY ||
            !validateIPAddress(localIP) ||
            !netmask_ok || netmask.addr == IPADDR_ANY ||
            !validateNetmask(netmask) ||
            !gateway_ok || !validateIPAddress(gateway)) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_network",
                                   "network");
        }
    }
    if (hostname) {
        const bool dns1_ok = parse_ipv4_json(
            cJSON_GetObjectItem(root, "dns1"), true, &dns1);
        const bool dns2_ok = parse_ipv4_json(
            cJSON_GetObjectItem(root, "dns2"), true, &dns2);
        if (!dns1_ok || !dns2_ok ||
            (dns1.addr != IPADDR_ANY && !validateIPAddress(dns1)) ||
            (dns2.addr != IPADDR_ANY && !validateIPAddress(dns2))) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_network",
                                   "dns");
        }
    }
    if (timesource < TIMESOURCE_NTP || timesource > TIMESOURCE_GPS ||
        !validateDcfOffset(dcfOffset) ||
        !validateGpsBaudrate(gpsBaudrate) ||
        !validateLEDBrightness(ledBrightness) ||
        (ntpServer && !validateNtpServer(ntpServer)) ||
        (ccuIP && ccuIP[0] != '\0' && !validateCcuAddress(ccuIP))) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request", "invalid_backup",
                               "settings");
    }

    const bool has_ipv6 =
        cJSON_GetObjectItem(root, "enableIPv6") ||
        cJSON_GetObjectItem(root, "ipv6Mode") ||
        cJSON_GetObjectItem(root, "ipv6Address") ||
        cJSON_GetObjectItem(root, "ipv6PrefixLength") ||
        cJSON_GetObjectItem(root, "ipv6Gateway") ||
        cJSON_GetObjectItem(root, "ipv6Dns1") ||
        cJSON_GetObjectItem(root, "ipv6Dns2");
    if (has_ipv6) {
        const bool effective_enabled = cJSON_GetObjectItem(root, "enableIPv6")
            ? enableIPv6 : webui_settings()->getEnableIPv6();
        const char *effective_mode =
            ipv6Mode ? ipv6Mode : webui_settings()->getIPv6Mode();
        const char *effective_address =
            ipv6Address ? ipv6Address : webui_settings()->getIPv6Address();
        if (!effective_mode ||
            (strcmp(effective_mode, "auto") != 0 &&
             strcmp(effective_mode, "static") != 0 &&
             strcmp(effective_mode, "disabled") != 0) ||
            ipv6PrefixLength < 1 || ipv6PrefixLength > 128 ||
            (effective_enabled && strcmp(effective_mode, "static") == 0 &&
             (!effective_address || effective_address[0] == '\0' ||
              !validateIPv6Address(effective_address)))) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_ipv6",
                                   "ipv6");
        }
        const char *optional_ipv6[] = {
            ipv6Gateway, ipv6Dns1, ipv6Dns2
        };
        for (const char *value : optional_ipv6) {
            if (value && value[0] != '\0' && !validateIPv6Address(value)) {
                cJSON_Delete(root);
                return send_json_error(req, "400 Bad Request", "invalid_ipv6",
                                       "ipv6");
            }
        }
    }

    static const char *LED_PROGRAM_KEYS[] = {
        "idle", "ccu_disconnected", "ccu_connected", "update_available",
        "error", "booting", "update_in_progress"
    };
    if (ledPrograms && !cJSON_IsObject(ledPrograms)) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request", "invalid_backup",
                               "ledPrograms");
    }
    if (ledPrograms) {
        for (const char *key : LED_PROGRAM_KEYS) {
            cJSON *item = cJSON_GetObjectItem(ledPrograms, key);
            if (item && (!cJSON_IsNumber(item) ||
                         item->valuedouble != item->valueint ||
                         item->valueint < 0 || item->valueint > 10)) {
                cJSON_Delete(root);
                return send_json_error(req, "400 Bad Request",
                                       "invalid_backup", "ledPrograms");
            }
        }
    }

    std::unique_ptr<settings_snapshot_t> previous_settings(
        new (std::nothrow) settings_snapshot_t{});
    std::unique_ptr<monitoring_config_t> previous_monitoring;
    if (!previous_settings) {
        cJSON_Delete(root);
        return send_json_error(req, "500 Internal Server Error",
                               "restore_snapshot_allocation", "settings");
    }
    webui_settings()->snapshot(previous_settings.get());
    if (restored_monitoring) {
        previous_monitoring.reset(
            new (std::nothrow) monitoring_config_t{});
        if (!previous_monitoring ||
            monitoring_get_config(previous_monitoring.get()) != ESP_OK) {
            cJSON_Delete(root);
            return send_json_error(req, "500 Internal Server Error",
                                   "restore_snapshot_allocation",
                                   "monitoring");
        }
    }

    // Keep every app-owned writer out of the shared 16 KiB NVS partition
    // from capacity preflight through the final commit. Nested helpers take
    // this recursive lock as well.
    NvsStorageLock restore_storage(portMAX_DELAY, "webui.restore");
    if (!restore_storage) {
        cJSON_Delete(root);
        return send_json_error(req, "503 Service Unavailable",
                               "restore_storage_busy", "backup");
    }

    if (restored_monitoring &&
        monitoring_validate_config_storage(restored_monitoring.get()) !=
            ESP_OK) {
        cJSON_Delete(root);
        return send_json_error(req, "507 Insufficient Storage",
                               "restore_monitoring_capacity", "monitoring");
    }

    // Snapshot the rollbackable theme before changing any persistent value.
    char previous_theme_scheme[8] = {};
    char previous_theme_color[8] = {};
    if (has_restored_theme) {
        if (theme_api_get_config(previous_theme_scheme,
                                 sizeof(previous_theme_scheme),
                                 previous_theme_color,
                                 sizeof(previous_theme_color)) != ESP_OK) {
            cJSON_Delete(root);
            return send_json_error(req, "500 Internal Server Error",
                                   "restore_theme_snapshot_failed", "theme");
        }
    }

    // Rotate before accepting the replacement password in RAM and before the
    // restore transaction captures its auth rollback image. The backup will
    // therefore contain the old durable password paired with the new token.
    if (admin_password_change_requested) {
        const esp_err_t token_result = rotate_admin_token();
        if (token_result != ESP_OK) {
            ESP_LOGE(TAG, "Restore password token rotation failed: %s",
                     esp_err_to_name(token_result));
            cJSON_Delete(root);
            return send_json_error(req, "500 Internal Server Error",
                                   "restore_token_rotation_failed",
                                   "adminPassword");
        }
    }

    // Apply the already-validated Settings candidate in RAM, then require a
    // successful NVS commit before touching the other namespaces.
    if (adminUsername && adminUsername[0] != '\0') {
        (void)webui_settings()->setAdminUsername(adminUsername);
    }
    if (admin_password_change_requested) {
        if (has_password_changed_flag) {
            (void)webui_settings()->restoreAdminPassword(
                adminPassword, cJSON_IsTrue(passwordChangedItem));
        } else {
            (void)webui_settings()->setAdminPassword(adminPassword);
        }
    }
    if (hostname) {
        (void)webui_settings()->setNetworkSettings(
            hostname, useDHCP, localIP, netmask, gateway, dns1, dns2);
    }
    webui_settings()->setTimesource(timesource);
    webui_settings()->setDcfOffset(dcfOffset);
    webui_settings()->setGpsBaudrate(gpsBaudrate);
    if (ntpServer) webui_settings()->setNtpServer(ntpServer);
    webui_settings()->setLEDBrightness(ledBrightness);

    if (ledPrograms) {
        for (int index = 0; index < 7; ++index) {
            cJSON *item =
                cJSON_GetObjectItem(ledPrograms, LED_PROGRAM_KEYS[index]);
            if (item) {
                webui_settings()->setLedProgram(index, item->valueint);
            }
        }
    }

    if (has_ipv6) {
        webui_settings()->setIPv6Settings(
            cJSON_GetObjectItem(root, "enableIPv6")
                ? enableIPv6 : webui_settings()->getEnableIPv6(),
            ipv6Mode ? ipv6Mode : webui_settings()->getIPv6Mode(),
            ipv6Address ? ipv6Address : webui_settings()->getIPv6Address(),
            cJSON_GetObjectItem(root, "ipv6PrefixLength")
                ? ipv6PrefixLength : webui_settings()->getIPv6PrefixLength(),
            ipv6Gateway ? ipv6Gateway : webui_settings()->getIPv6Gateway(),
            ipv6Dns1 ? ipv6Dns1 : webui_settings()->getIPv6Dns1(),
            ipv6Dns2 ? ipv6Dns2 : webui_settings()->getIPv6Dns2());
    }
    if (ccuIP) webui_settings()->setCCUIP(ccuIP);
    if (systemLogEnabledItem && cJSON_IsBool(systemLogEnabledItem)) {
        webui_settings()->setSystemLogEnabled(cJSON_IsTrue(systemLogEnabledItem));
    }
    if (flashPauseItem && cJSON_IsBool(flashPauseItem)) {
        webui_settings()->setFlashPause(cJSON_IsTrue(flashPauseItem));
    }
    if (testDesignItem && cJSON_IsBool(testDesignItem)) {
        webui_settings()->setTestDesignEnabled(cJSON_IsTrue(testDesignItem));
    }

    const esp_err_t settings_capacity_result =
        webui_settings()->validateStorageCapacity();
    if (settings_capacity_result != ESP_OK) {
        webui_settings()->restoreSnapshot(previous_settings.get());
        cJSON_Delete(root);
        if (settings_capacity_result == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
            return send_json_error(req, "507 Insufficient Storage",
                                   "restore_settings_capacity", "settings");
        }
        return send_json_error(req, "500 Internal Server Error",
                               "restore_settings_validation_failed",
                               "settings");
    }

    // Arm one durable marker before the first persistent Settings write. It
    // covers Settings, theme and monitoring as a single restore generation.
    // restore_storage remains held until the marker is cleared after all
    // writes, or after a fully successful rollback.
    const esp_err_t restore_begin_result =
        Settings::beginRestoreTransaction();
    if (restore_begin_result != ESP_OK) {
        webui_settings()->restoreSnapshot(previous_settings.get());
        cJSON_Delete(root);
        if (restore_begin_result == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
            return send_json_error(req, "507 Insufficient Storage",
                                   "restore_storage_capacity", "backup");
        }
        return send_json_error(req, "500 Internal Server Error",
                               "restore_transaction_begin_failed", "backup");
    }

    auto finish_rollback_transaction = [](
        const char *context, esp_err_t settings_rollback,
        esp_err_t monitoring_rollback, esp_err_t theme_rollback) -> bool {
        if (settings_rollback != ESP_OK || monitoring_rollback != ESP_OK ||
            theme_rollback != ESP_OK) {
            ESP_LOGE(TAG,
                     "%s; restore marker retained: settings=%s monitoring=%s theme=%s",
                     context, esp_err_to_name(settings_rollback),
                     esp_err_to_name(monitoring_rollback),
                     esp_err_to_name(theme_rollback));
            return false;
        }
        const esp_err_t finish_result =
            Settings::finishRestoreTransaction();
        if (finish_result != ESP_OK) {
            ESP_LOGE(TAG, "%s; could not clear restore marker: %s",
                     context, esp_err_to_name(finish_result));
            return false;
        }
        return true;
    };

    const esp_err_t settings_save_result = webui_settings()->save();
    if (settings_save_result != ESP_OK) {
        webui_settings()->restoreSnapshot(previous_settings.get());
        const esp_err_t rollback_result = webui_settings()->save();
        const bool rollback_complete = finish_rollback_transaction(
            "Could not roll back Settings after failed restore",
            rollback_result, ESP_OK, ESP_OK);
        cJSON_Delete(root);
        if (!rollback_complete) {
            return send_json_error(req, "500 Internal Server Error",
                                   "restore_rollback_failed", "backup");
        }
        if (settings_save_result == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
            return send_json_error(req, "507 Insufficient Storage",
                                   "restore_settings_failed", "settings");
        }
        return send_json_error(req, "500 Internal Server Error",
                               "restore_settings_failed", "settings");
    }

    bool theme_written = false;
    if (has_restored_theme) {
        const esp_err_t theme_result = theme_api_set_config(
            restored_theme_scheme, restored_theme_color);
        if (theme_result != ESP_OK) {
            const esp_err_t theme_rollback = theme_api_set_config(
                previous_theme_scheme, previous_theme_color);
            webui_settings()->restoreSnapshot(previous_settings.get());
            const esp_err_t rollback_result = webui_settings()->save();
            const bool rollback_complete = finish_rollback_transaction(
                "Could not fully roll back theme restore failure",
                rollback_result, ESP_OK, theme_rollback);
            cJSON_Delete(root);
            if (!rollback_complete) {
                return send_json_error(req, "500 Internal Server Error",
                                       "restore_rollback_failed", "backup");
            }
            if (theme_result == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
                return send_json_error(req, "507 Insufficient Storage",
                                       "restore_theme_failed", "theme");
            }
            return send_json_error(req, "500 Internal Server Error",
                                   "restore_theme_failed", "theme");
        }
        theme_written = true;
    }

    if (restored_monitoring) {
        // Settings and theme share the same physical NVS partition with
        // monitoring. Repeat the earlier preflight immediately before the
        // monitoring transaction, while restore_storage still excludes every
        // other app-owned writer. This catches capacity consumed by the two
        // preceding commits without ever erasing the known-good monitoring
        // generation.
        const esp_err_t final_monitoring_capacity =
            monitoring_validate_config_storage(restored_monitoring.get());
        if (final_monitoring_capacity != ESP_OK) {
            webui_settings()->restoreSnapshot(previous_settings.get());
            const esp_err_t settings_rollback = webui_settings()->save();
            esp_err_t theme_rollback = ESP_OK;
            if (theme_written) {
                theme_rollback = theme_api_set_config(
                    previous_theme_scheme, previous_theme_color);
            }
            const bool rollback_complete = finish_rollback_transaction(
                "Restore capacity rollback incomplete", settings_rollback,
                ESP_OK, theme_rollback);
            cJSON_Delete(root);
            if (!rollback_complete) {
                return send_json_error(req, "500 Internal Server Error",
                                       "restore_rollback_failed", "backup");
            }

            if (final_monitoring_capacity ==
                ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
                return send_json_error(req, "507 Insufficient Storage",
                                       "restore_monitoring_capacity",
                                       "monitoring");
            }
            ESP_LOGE(TAG, "Final monitoring capacity validation failed: %s",
                     esp_err_to_name(final_monitoring_capacity));
            return send_json_error(req, "500 Internal Server Error",
                                   "restore_monitoring_validation_failed",
                                   "monitoring");
        }

        const esp_err_t monitoring_result =
            monitoring_save_config_for_restore(restored_monitoring.get());
        if (monitoring_result != ESP_OK) {
            webui_settings()->restoreSnapshot(previous_settings.get());
            const esp_err_t settings_rollback = webui_settings()->save();
            const esp_err_t monitoring_rollback =
                monitoring_save_config_for_restore(previous_monitoring.get());
            esp_err_t theme_rollback = ESP_OK;
            if (theme_written) {
                theme_rollback = theme_api_set_config(
                    previous_theme_scheme, previous_theme_color);
            }
            const bool rollback_complete = finish_rollback_transaction(
                "Restore rollback incomplete", settings_rollback,
                monitoring_rollback, theme_rollback);
            cJSON_Delete(root);
            if (!rollback_complete) {
                return send_json_error(req, "500 Internal Server Error",
                                       "restore_rollback_failed", "backup");
            }
            if (monitoring_result == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
                return send_json_error(req, "507 Insufficient Storage",
                                       "restore_monitoring_capacity",
                                       "monitoring");
            }
            return send_json_error(req, "500 Internal Server Error",
                                   "restore_monitoring_failed", "monitoring");
        }
    }

    const esp_err_t restore_finish_result =
        Settings::finishRestoreTransaction();
    if (restore_finish_result != ESP_OK) {
        webui_settings()->restoreSnapshot(previous_settings.get());
        const esp_err_t settings_rollback = webui_settings()->save();
        esp_err_t monitoring_rollback = ESP_OK;
        if (restored_monitoring) {
            monitoring_rollback =
                monitoring_save_config_for_restore(previous_monitoring.get());
        }
        esp_err_t theme_rollback = ESP_OK;
        if (theme_written) {
            theme_rollback = theme_api_set_config(
                previous_theme_scheme, previous_theme_color);
        }
        const bool rollback_complete = finish_rollback_transaction(
            "Restore marker finalization failed", settings_rollback,
            monitoring_rollback, theme_rollback);
        cJSON_Delete(root);
        if (!rollback_complete) {
            return send_json_error(req, "500 Internal Server Error",
                                   "restore_rollback_failed", "backup");
        }
        return send_json_error(req, "500 Internal Server Error",
                               "restore_transaction_finish_failed", "backup");
    }

    // Only expose non-persistent side effects after every namespace committed.
    if (cJSON_GetObjectItem(root, "ledBrightness") != NULL) {
        LED::setBrightness(ledBrightness);
    }
    if (flashPauseItem && cJSON_IsBool(flashPauseItem)) {
        set_flash_pause_enabled(cJSON_IsTrue(flashPauseItem));
    }

    // Do not carry the storage lock into the restart cleanup: a late MQTT
    // callback may need to write its one-shot migration marker before the
    // cooperative MQTT stop can finish.
    restore_storage.release();

    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\":true}");

    // Restart
    vTaskDelay(pdMS_TO_TICKS(1000));
    refresh_restart_sync_from_settings();
    full_system_restart_with_reserved_operation();

    return ESP_OK;
}

httpd_uri_t post_restore_handler = {
    .uri = "/api/restore",
    .method = HTTP_POST,
    .handler = post_restore_handler_func,
    .user_ctx = NULL};
