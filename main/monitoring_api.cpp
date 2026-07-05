/*
 *  monitoring_api.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "monitoring_api.h"
#include "monitoring.h"
#include "validation.h"
#include "security_headers.h"
#include "esp_err.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <memory>
#include <new>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


// Helper functions implemented in webui.cpp
extern esp_err_t validate_auth(httpd_req_t *req);
extern int recv_full_body(httpd_req_t *req, char *buf, size_t buf_size);

static void copy_string_field(char *dest, size_t dest_size, const char *src)
{
    if (!dest || dest_size == 0) {
        return;
    }

    if (!src) {
        dest[0] = '\0';
        return;
    }

    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

static esp_err_t send_json_error(httpd_req_t *req, const char *message)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "{\"error\":\"%s\"}", message);
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

static esp_err_t send_monitoring_test_response(httpd_req_t *req, const char *target, bool ok,
                                               const char *code, const char *message,
                                               const char *host, uint16_t port, bool tls_enabled)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    cJSON_AddStringToObject(root, "target", target);
    cJSON_AddBoolToObject(root, "ok", ok);
    // `code` is the stable, machine-readable key the WebUI translates.
    // `message` is an English fallback kept for older frontends / curl users.
    if (code && code[0]) {
        cJSON_AddStringToObject(root, "code", code);
    } else {
        cJSON_AddNullToObject(root, "code");
    }
    cJSON_AddStringToObject(root, "message", message ? message : "");
    if (host && host[0]) {
        cJSON_AddStringToObject(root, "host", host);
    }
    if (port) {
        cJSON_AddNumberToObject(root, "port", port);
    }
    cJSON_AddBoolToObject(root, "tlsEnabled", tls_enabled);

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_string)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

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

    // monitoring_config_t is ~6.7 KB (three 2 KB PEM buffers) - far too large
    // for the 8 KB httpd task stack, so it must live on the heap.
    std::unique_ptr<monitoring_config_t> config_heap(new (std::nothrow) monitoring_config_t());
    if (!config_heap)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }
    monitoring_config_t &config = *config_heap;
    if (monitoring_get_config(&config) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to get config");
    }

    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    // MQTT config
    cJSON *mqtt = cJSON_CreateObject();
    if (!mqtt)
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

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
    // Command-topic security. Token is reported only as a boolean "set" flag
    // to avoid leaking the shared secret through the API. The frontend
    // sends "commandTokenClear=true" to remove it, or a new value to replace.
    cJSON_AddBoolToObject(mqtt, "commandEnabled", config.mqtt.command_enabled);
    cJSON_AddBoolToObject(mqtt, "commandTokenSet", strlen(config.mqtt.command_token) > 0);
    cJSON_AddItemToObject(root, "mqtt", mqtt);

    // CheckMK config
    cJSON *checkmk = cJSON_CreateObject();
    if (!checkmk)
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    cJSON_AddBoolToObject(checkmk, "enabled", config.checkmk.enabled);
    cJSON_AddNumberToObject(checkmk, "port", config.checkmk.port);
    cJSON_AddStringToObject(checkmk, "allowedHosts", config.checkmk.allowed_hosts);
    cJSON_AddItemToObject(root, "checkmk", checkmk);

    // Prometheus config (Phase A)
    cJSON *prom = cJSON_CreateObject();
    if (!prom)
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }
    cJSON_AddBoolToObject(prom, "enabled", config.prometheus.enabled);
    cJSON_AddNumberToObject(prom, "port", config.prometheus.port);
    cJSON_AddStringToObject(prom, "allowedHosts", config.prometheus.allowed_hosts);
    cJSON_AddItemToObject(root, "prometheus", prom);

    // Syslog config (Phase B)
    cJSON *syslog = cJSON_CreateObject();
    if (!syslog)
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }
    cJSON_AddBoolToObject(syslog, "enabled", config.syslog.enabled);
    cJSON_AddStringToObject(syslog, "server", config.syslog.server);
    cJSON_AddNumberToObject(syslog, "port", config.syslog.port);
    cJSON_AddNumberToObject(syslog, "transport", config.syslog.transport);
    cJSON_AddNumberToObject(syslog, "minSeverity", config.syslog.min_severity);
    cJSON_AddStringToObject(syslog, "hostname", config.syslog.hostname);
    cJSON_AddItemToObject(root, "syslog", syslog);

    // Notify config (Phase C/D) — secrets are echoed back only as "is set"
    // booleans, matching the MQTT password pattern.
    cJSON *notify = cJSON_CreateObject();
    if (!notify)
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }
    cJSON_AddBoolToObject(notify, "enabled", config.notify.enabled);
    cJSON_AddNumberToObject(notify, "channels", config.notify.channels);
    cJSON_AddStringToObject(notify, "webhookUrl", config.notify.webhook_url);
    cJSON_AddBoolToObject(notify, "webhookSecretSet", strlen(config.notify.webhook_secret) > 0);
    cJSON_AddStringToObject(notify, "telegramChatId", config.notify.telegram_chatid);
    cJSON_AddBoolToObject(notify, "telegramTokenSet", strlen(config.notify.telegram_token) > 0);
    cJSON_AddStringToObject(notify, "smtpServer", config.notify.smtp_server);
    cJSON_AddNumberToObject(notify, "smtpPort", config.notify.smtp_port);
    cJSON_AddNumberToObject(notify, "smtpTls", config.notify.smtp_tls);
    cJSON_AddStringToObject(notify, "smtpUser", config.notify.smtp_user);
    cJSON_AddStringToObject(notify, "smtpFrom", config.notify.smtp_from);
    cJSON_AddStringToObject(notify, "smtpTo", config.notify.smtp_to);
    cJSON_AddBoolToObject(notify, "smtpPasswordSet", strlen(config.notify.smtp_password) > 0);
    cJSON_AddNumberToObject(notify, "cooldownSeconds", config.notify.cooldown_seconds);
    cJSON_AddItemToObject(root, "notify", notify);

    char *json_string = cJSON_Print(root);
    cJSON_Delete(root);

    if (!json_string)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

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
    int ret = recv_full_body(req, content, 16384);
    if (ret <= 0)
    {
        free(content);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"error\":\"Invalid request\"}");
        return ESP_FAIL;
    }

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
    // (e.g., MQTT password is never sent back for security reasons).
    // Heap-allocated: the ~6.7 KB struct would overflow the httpd task stack.
    std::unique_ptr<monitoring_config_t> config_heap(new (std::nothrow) monitoring_config_t());
    if (!config_heap)
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }
    monitoring_config_t &config = *config_heap;
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
            copy_string_field(config.checkmk.allowed_hosts, sizeof(config.checkmk.allowed_hosts), allowedHosts->valuestring);
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
            copy_string_field(config.mqtt.server, sizeof(config.mqtt.server), server->valuestring);
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
            copy_string_field(config.mqtt.user, sizeof(config.mqtt.user), user->valuestring);
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
            copy_string_field(config.mqtt.password, sizeof(config.mqtt.password), password->valuestring);
        }

        cJSON *topicPrefix = cJSON_GetObjectItem(mqtt, "topicPrefix");
        if (topicPrefix != NULL && cJSON_IsString(topicPrefix))
        {
            if (!validateStringLength(topicPrefix->valuestring, sizeof(config.mqtt.topic_prefix) - 1))
            {
                cJSON_Delete(root);
                return send_json_error(req, "MQTT topic prefix too long");
            }
            copy_string_field(config.mqtt.topic_prefix, sizeof(config.mqtt.topic_prefix), topicPrefix->valuestring);
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
            copy_string_field(config.mqtt.ha_discovery_prefix, sizeof(config.mqtt.ha_discovery_prefix), haDiscoveryPrefix->valuestring);
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
            copy_string_field(config.mqtt.tls_ca_certs, sizeof(config.mqtt.tls_ca_certs), tlsCaCerts->valuestring);
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
            copy_string_field(config.mqtt.tls_certfile, sizeof(config.mqtt.tls_certfile), tlsCertfile->valuestring);
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
            copy_string_field(config.mqtt.tls_keyfile, sizeof(config.mqtt.tls_keyfile), tlsKeyfile->valuestring);
        }

        // ---- Phase A: command-topic security ----------------------------
        cJSON *commandEnabled = cJSON_GetObjectItem(mqtt, "commandEnabled");
        if (commandEnabled != NULL && cJSON_IsBool(commandEnabled))
        {
            config.mqtt.command_enabled = cJSON_IsTrue(commandEnabled);
        }

        cJSON *commandTokenClear = cJSON_GetObjectItem(mqtt, "commandTokenClear");
        if (commandTokenClear != NULL && cJSON_IsTrue(commandTokenClear))
        {
            config.mqtt.command_token[0] = '\0';
        }
        cJSON *commandToken = cJSON_GetObjectItem(mqtt, "commandToken");
        if (commandToken != NULL && cJSON_IsString(commandToken) && strlen(commandToken->valuestring) > 0)
        {
            // The command token ends up in plain-text MQTT payloads AND HA
            // discovery JSON. Restrict the alphabet so it cannot break out
            // of the JSON string or be confused with topic separators.
            if (!validateMqttCommandToken(commandToken->valuestring))
            {
                cJSON_Delete(root);
                return send_json_error(req, "MQTT command token invalid (8..63 chars, "
                                           "allowed: A-Z a-z 0-9 - _ .)");
            }
            copy_string_field(config.mqtt.command_token,
                              sizeof(config.mqtt.command_token),
                              commandToken->valuestring);
        }
    }

    // Parse Prometheus config (Phase A)
    cJSON *prom = cJSON_GetObjectItem(root, "prometheus");
    if (prom != NULL)
    {
        cJSON *pen = cJSON_GetObjectItem(prom, "enabled");
        if (pen && cJSON_IsBool(pen)) config.prometheus.enabled = cJSON_IsTrue(pen);

        cJSON *pport = cJSON_GetObjectItem(prom, "port");
        if (pport && cJSON_IsNumber(pport)) {
            if (!validatePort(pport->valueint)) {
                cJSON_Delete(root);
                return send_json_error(req, "Invalid Prometheus port");
            }
            config.prometheus.port = pport->valueint;
        }
        cJSON *phosts = cJSON_GetObjectItem(prom, "allowedHosts");
        if (phosts && cJSON_IsString(phosts)) {
            if (!validateStringLength(phosts->valuestring, sizeof(config.prometheus.allowed_hosts) - 1)) {
                cJSON_Delete(root);
                return send_json_error(req, "Prometheus allowed hosts too long");
            }
            copy_string_field(config.prometheus.allowed_hosts,
                              sizeof(config.prometheus.allowed_hosts),
                              phosts->valuestring);
        }
    }

    // Parse Syslog config (Phase B)
    cJSON *syslog = cJSON_GetObjectItem(root, "syslog");
    if (syslog != NULL)
    {
        cJSON *sen = cJSON_GetObjectItem(syslog, "enabled");
        if (sen && cJSON_IsBool(sen)) config.syslog.enabled = cJSON_IsTrue(sen);

        cJSON *ssrv = cJSON_GetObjectItem(syslog, "server");
        if (ssrv && cJSON_IsString(ssrv)) {
            if (config.syslog.enabled || strlen(ssrv->valuestring) > 0) {
                if (!validateServerAddress(ssrv->valuestring, sizeof(config.syslog.server) - 1)) {
                    cJSON_Delete(root);
                    return send_json_error(req, "Syslog server is empty or invalid");
                }
            }
            copy_string_field(config.syslog.server, sizeof(config.syslog.server), ssrv->valuestring);
        }
        cJSON *sport = cJSON_GetObjectItem(syslog, "port");
        if (sport && cJSON_IsNumber(sport)) {
            if (!validatePort(sport->valueint)) {
                cJSON_Delete(root);
                return send_json_error(req, "Invalid syslog port");
            }
            config.syslog.port = sport->valueint;
        }
        cJSON *sxp = cJSON_GetObjectItem(syslog, "transport");
        if (sxp && cJSON_IsNumber(sxp)) config.syslog.transport = (uint8_t)sxp->valueint;
        cJSON *ssev = cJSON_GetObjectItem(syslog, "minSeverity");
        if (ssev && cJSON_IsNumber(ssev)) config.syslog.min_severity = (uint8_t)ssev->valueint;
        cJSON *shost = cJSON_GetObjectItem(syslog, "hostname");
        if (shost && cJSON_IsString(shost)) {
            if (!validateStringLength(shost->valuestring, sizeof(config.syslog.hostname) - 1)) {
                cJSON_Delete(root);
                return send_json_error(req, "Syslog hostname override too long");
            }
            copy_string_field(config.syslog.hostname, sizeof(config.syslog.hostname), shost->valuestring);
        }
    }

    // Parse Notify config (Phase C/D)
    cJSON *notify = cJSON_GetObjectItem(root, "notify");
    if (notify != NULL)
    {
        cJSON *nen = cJSON_GetObjectItem(notify, "enabled");
        if (nen && cJSON_IsBool(nen)) config.notify.enabled = cJSON_IsTrue(nen);
        cJSON *nch = cJSON_GetObjectItem(notify, "channels");
        if (nch && cJSON_IsNumber(nch)) config.notify.channels = (uint8_t)nch->valueint;

        cJSON *nurl = cJSON_GetObjectItem(notify, "webhookUrl");
        if (nurl && cJSON_IsString(nurl)) {
            if (!validateStringLength(nurl->valuestring, sizeof(config.notify.webhook_url) - 1)) {
                cJSON_Delete(root);
                return send_json_error(req, "Webhook URL too long");
            }
            copy_string_field(config.notify.webhook_url, sizeof(config.notify.webhook_url), nurl->valuestring);
        }
        cJSON *nwsc = cJSON_GetObjectItem(notify, "webhookSecretClear");
        if (nwsc && cJSON_IsTrue(nwsc)) config.notify.webhook_secret[0] = '\0';
        cJSON *nws = cJSON_GetObjectItem(notify, "webhookSecret");
        if (nws && cJSON_IsString(nws) && strlen(nws->valuestring) > 0) {
            if (!validateStringLength(nws->valuestring, sizeof(config.notify.webhook_secret) - 1)) {
                cJSON_Delete(root);
                return send_json_error(req, "Webhook secret too long");
            }
            copy_string_field(config.notify.webhook_secret, sizeof(config.notify.webhook_secret), nws->valuestring);
        }
        cJSON *ntgtok = cJSON_GetObjectItem(notify, "telegramTokenClear");
        if (ntgtok && cJSON_IsTrue(ntgtok)) config.notify.telegram_token[0] = '\0';
        cJSON *ntg = cJSON_GetObjectItem(notify, "telegramToken");
        if (ntg && cJSON_IsString(ntg) && strlen(ntg->valuestring) > 0) {
            if (!validateStringLength(ntg->valuestring, sizeof(config.notify.telegram_token) - 1)) {
                cJSON_Delete(root);
                return send_json_error(req, "Telegram token too long");
            }
            copy_string_field(config.notify.telegram_token, sizeof(config.notify.telegram_token), ntg->valuestring);
        }
        cJSON *ntc = cJSON_GetObjectItem(notify, "telegramChatId");
        if (ntc && cJSON_IsString(ntc)) {
            if (!validateStringLength(ntc->valuestring, sizeof(config.notify.telegram_chatid) - 1)) {
                cJSON_Delete(root);
                return send_json_error(req, "Telegram chat id too long");
            }
            copy_string_field(config.notify.telegram_chatid, sizeof(config.notify.telegram_chatid), ntc->valuestring);
        }
        cJSON *nsrv = cJSON_GetObjectItem(notify, "smtpServer");
        if (nsrv && cJSON_IsString(nsrv)) {
            if (!validateStringLength(nsrv->valuestring, sizeof(config.notify.smtp_server) - 1)) {
                cJSON_Delete(root);
                return send_json_error(req, "SMTP server too long");
            }
            copy_string_field(config.notify.smtp_server, sizeof(config.notify.smtp_server), nsrv->valuestring);
        }
        cJSON *nsp = cJSON_GetObjectItem(notify, "smtpPort");
        if (nsp && cJSON_IsNumber(nsp)) {
            if (!validatePort(nsp->valueint)) {
                cJSON_Delete(root);
                return send_json_error(req, "Invalid SMTP port");
            }
            config.notify.smtp_port = nsp->valueint;
        }
        cJSON *nstls = cJSON_GetObjectItem(notify, "smtpTls");
        if (nstls && cJSON_IsNumber(nstls)) config.notify.smtp_tls = (uint8_t)nstls->valueint;
        cJSON *nsu = cJSON_GetObjectItem(notify, "smtpUser");
        if (nsu && cJSON_IsString(nsu)) {
            if (!validateStringLength(nsu->valuestring, sizeof(config.notify.smtp_user) - 1)) {
                cJSON_Delete(root);
                return send_json_error(req, "SMTP user too long");
            }
            copy_string_field(config.notify.smtp_user, sizeof(config.notify.smtp_user), nsu->valuestring);
        }
        cJSON *nspwclear = cJSON_GetObjectItem(notify, "smtpPasswordClear");
        if (nspwclear && cJSON_IsTrue(nspwclear)) config.notify.smtp_password[0] = '\0';
        cJSON *nspw = cJSON_GetObjectItem(notify, "smtpPassword");
        if (nspw && cJSON_IsString(nspw) && strlen(nspw->valuestring) > 0) {
            if (!validateStringLength(nspw->valuestring, sizeof(config.notify.smtp_password) - 1)) {
                cJSON_Delete(root);
                return send_json_error(req, "SMTP password too long");
            }
            copy_string_field(config.notify.smtp_password, sizeof(config.notify.smtp_password), nspw->valuestring);
        }
        cJSON *nsf = cJSON_GetObjectItem(notify, "smtpFrom");
        if (nsf && cJSON_IsString(nsf)) {
            if (!validateStringLength(nsf->valuestring, sizeof(config.notify.smtp_from) - 1)) {
                cJSON_Delete(root);
                return send_json_error(req, "SMTP from too long");
            }
            copy_string_field(config.notify.smtp_from, sizeof(config.notify.smtp_from), nsf->valuestring);
        }
        cJSON *nsto = cJSON_GetObjectItem(notify, "smtpTo");
        if (nsto && cJSON_IsString(nsto)) {
            if (!validateStringLength(nsto->valuestring, sizeof(config.notify.smtp_to) - 1)) {
                cJSON_Delete(root);
                return send_json_error(req, "SMTP to too long");
            }
            copy_string_field(config.notify.smtp_to, sizeof(config.notify.smtp_to), nsto->valuestring);
        }
        cJSON *ncd = cJSON_GetObjectItem(notify, "cooldownSeconds");
        if (ncd && cJSON_IsNumber(ncd)) config.notify.cooldown_seconds = ncd->valueint;
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

// The connectivity diagnostic blocks for seconds (getaddrinfo + up to 3 s
// TCP probe), so it runs in a short-lived worker task on an async copy of
// the request instead of stalling the single-threaded httpd task.
struct DiagnosticJob {
    httpd_req_t *req; // async copy of the request
    char target[16];
};

static std::atomic<bool> _diag_busy{false};

static void _monitoring_diag_task(void *arg)
{
    DiagnosticJob *job = (DiagnosticJob *)arg;
    bool ok = false;
    char code[80];
    char message[160];
    char host[128];
    uint16_t port = 0;
    bool tls_enabled = false;
    code[0] = '\0';
    message[0] = '\0';
    host[0] = '\0';

    esp_err_t result = monitoring_run_diagnostic(job->target, &ok,
                                                 code, sizeof(code),
                                                 message, sizeof(message),
                                                 host, sizeof(host),
                                                 &port, &tls_enabled);
    if (result == ESP_ERR_NOT_SUPPORTED) {
        send_json_error(job->req, "Unsupported diagnostic target");
    } else if (result != ESP_OK && message[0] == '\0') {
        send_json_error(job->req, "Diagnostic failed");
    } else {
        send_monitoring_test_response(job->req, job->target, ok,
                                      code, message, host, port, tls_enabled);
    }

    httpd_req_async_handler_complete(job->req);
    free(job);
    _diag_busy.store(false);
    vTaskDelete(NULL);
}

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

    // The diagnostic blocks for seconds (getaddrinfo + up to 3 s TCP probe)
    // and must not run in the single-threaded httpd task. Detach the request
    // and answer from a short-lived worker task.
    bool expected = false;
    if (!_diag_busy.compare_exchange_strong(expected, true))
    {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"error\":\"A diagnostic is already running, try again shortly\"}");
        return ESP_OK;
    }

    DiagnosticJob *job = (DiagnosticJob *)calloc(1, sizeof(DiagnosticJob));
    if (!job)
    {
        _diag_busy.store(false);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }
    snprintf(job->target, sizeof(job->target), "%s", target);

    if (httpd_req_async_handler_begin(req, &job->req) != ESP_OK)
    {
        free(job);
        _diag_busy.store(false);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    if (xTaskCreate(_monitoring_diag_task, "mon_diag", 5120, job, 5, NULL) != pdPASS)
    {
        httpd_resp_send_err(job->req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        httpd_req_async_handler_complete(job->req);
        free(job);
        _diag_busy.store(false);
    }
    return ESP_OK;
}

httpd_uri_t get_monitoring_test_handler = {
    .uri = "/api/monitoring/test",
    .method = HTTP_GET,
    .handler = get_monitoring_test_handler_func,
    .user_ctx = NULL
};
