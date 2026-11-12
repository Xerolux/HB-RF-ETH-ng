/*
 *  mqtt_commands.cpp is part of the HB-RF-ETH firmware v2.0
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

// Stage-1 "Home Assistant full control" command surface. See mqtt_commands.h
// for the protocol contract and the security policy. Everything reachable
// here goes through the same validation and persistence paths as the REST
// API - MQTT is an additional transport, not a parallel implementation with
// its own rules.

#include "mqtt_commands.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mqtt_handler.h"
#include "monitoring.h"
#include "settings.h"
#include "validation.h"
#include "led.h"
#include "log_manager.h"
#include "sysinfo.h"
#include "update_check.h"
#include "update_install.h"
#include "update_schedule_policy.h"
#include "webui_storage.h"
#include "webui.h"
#include "rawuartudplistener.h"
#include "radiomoduleconnector.h"

static const char *TAG = "MQTTCMD";

// Provider getters implemented in monitoring.cpp; the same extern pattern
// used by mqtt_handler.cpp (no shared header exports them).
extern SysInfo *monitoring_get_sysinfo(void);

// State-topic publishers used by the command handlers before their
// definitions (see "Retained state topics" below).
void mqtt_commands_publish_settings_states(const char *topic_prefix);
void mqtt_commands_publish_monitoring_states(const char *topic_prefix);

// Longest accepted JSON command payload. The command protocol never carries
// secrets beyond the token (65) plus one bounded value (192), so anything
// larger is rejected before cJSON allocates intermediate storage.
#define MQTT_COMMAND_PAYLOAD_MAX 512

// Longest value string extracted from the JSON "value" field.
#define MQTT_COMMAND_VALUE_MAX 192

// ---------------------------------------------------------------------------
// Small shared helpers
// ---------------------------------------------------------------------------

static void reject(const char *command, const char *reason)
{
    char msg[112];
    snprintf(msg, sizeof(msg), "rejected cmd=%.48s reason=%s", command, reason);
    mqtt_handler_publish_event("event/command_rejected", msg);
}

static bool str_to_long(const char *s, long *out)
{
    if (s == NULL || *s == '\0') {
        return false;
    }
    char *end = NULL;
    const long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') {
        return false;
    }
    *out = v;
    return true;
}

static bool str_to_bool(const char *s, bool *out)
{
    if (s == NULL) {
        return false;
    }
    if (!strcmp(s, "true") || !strcmp(s, "on") || !strcmp(s, "1") ||
        !strcmp(s, "ON") || !strcmp(s, "TRUE")) {
        *out = true;
        return true;
    }
    if (!strcmp(s, "false") || !strcmp(s, "off") || !strcmp(s, "0") ||
        !strcmp(s, "OFF") || !strcmp(s, "FALSE")) {
        *out = false;
        return true;
    }
    return false;
}

// Legacy byte-compare used for plain-token (button style) payloads. Same
// semantics as the restart command in mqtt_handler.cpp: without a
// configured token every payload is accepted (broker ACLs protect).
static bool plain_token_matches(const mqtt_config_t *cfg, const char *payload,
                                int payload_len)
{
    if (cfg->command_token[0] == '\0') {
        return true;
    }
    if (payload == NULL || payload_len <= 0) {
        return false;
    }
    const size_t expected = strlen(cfg->command_token);
    if ((size_t)payload_len != expected) {
        return false;
    }
    return strncmp(payload, cfg->command_token, expected) == 0;
}

struct command_payload_t {
    char value[MQTT_COMMAND_VALUE_MAX];
    bool has_value;
};

/**
 * @brief Authenticate and decode one command payload.
 *
 * Accepted forms:
 *   * JSON object {"token":"...","value":...} - the token field must match
 *     when a token is configured. Required form for value commands.
 *   * The bare token string - allowed for value-less action commands so HA
 *     buttons (payload_press) and update entities (payload_install) work.
 *
 * When no token is configured both forms are accepted and the token field
 * is not checked (broker ACL + TLS provide the protection, same model as
 * the restart command).
 */
static bool command_payload_decode(const mqtt_config_t *cfg, const char *command,
                                   const char *payload, int payload_len,
                                   bool require_value,
                                   command_payload_t *out)
{
    memset(out, 0, sizeof(*out));

    if (payload == NULL || payload_len <= 0 ||
        (size_t)payload_len > MQTT_COMMAND_PAYLOAD_MAX) {
        reject(command, "invalid_payload");
        return false;
    }

    // The payload is not NUL-terminated (ESP-MQTT event data).
    if (payload[0] == '{') {
        cJSON *root = cJSON_ParseWithLength(payload, (size_t)payload_len);
        if (root == NULL || !cJSON_IsObject(root)) {
            cJSON_Delete(root);
            reject(command, "invalid_json");
            return false;
        }

        const cJSON *token = cJSON_GetObjectItem(root, "token");
        if (cfg->command_token[0] != '\0') {
            const cJSON *found = token && cJSON_IsString(token) ? token : NULL;
            if (found == NULL ||
                strcmp(found->valuestring, cfg->command_token) != 0) {
                cJSON_Delete(root);
                reject(command, "invalid_token");
                return false;
            }
        }

        const cJSON *value = cJSON_GetObjectItem(root, "value");
        if (value != NULL) {
            char *dst = out->value;
            size_t cap = sizeof(out->value);
            if (cJSON_IsString(value)) {
                snprintf(dst, cap, "%s", value->valuestring != NULL
                                            ? value->valuestring
                                            : "");
            } else if (cJSON_IsNumber(value)) {
                snprintf(dst, cap, "%lld", (long long)value->valuedouble);
            } else if (cJSON_IsBool(value)) {
                snprintf(dst, cap, "%s", cJSON_IsTrue(value) ? "true" : "false");
            } else {
                cJSON_Delete(root);
                reject(command, "invalid_value_type");
                return false;
            }
            out->has_value = true;
        }
        cJSON_Delete(root);
    } else {
        // Plain form: only the token itself. No value can be transported.
        if (!plain_token_matches(cfg, payload, payload_len)) {
            reject(command, "invalid_token");
            return false;
        }
    }

    if (require_value && !out->has_value) {
        reject(command, "missing_value");
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Option-name mapping tables (stable wire format for HA selects)
// ---------------------------------------------------------------------------

static const char *const LOG_SEVERITY_NAMES[8] = {
    "EMERG", "ALERT", "CRIT", "ERR", "WARNING", "NOTICE", "INFO", "DEBUG"};

static const char *const SYSLOG_TRANSPORT_NAMES[3] = {"udp", "tcp", "tls"};

static const char *const WEEKDAY_NAMES[7] = {
    "sun", "mon", "tue", "wed", "thu", "fri", "sat"};

static const char *const LED_STATE_NAMES[11] = {
    "off", "on", "blink", "blink_inv", "blink_fast", "blink_slow",
    "blink_2x", "blink_3x", "breathing", "heartbeat", "strobe"};

static int name_index(const char *const *names, size_t count, const char *value)
{
    for (size_t i = 0; i < count; i++) {
        if (!strcmp(names[i], value)) {
            return (int)i;
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Settings-backed commands (Settings::save() persistence, same as WebUI)
// ---------------------------------------------------------------------------

// Generic persistence wrapper: apply the RAM mutation, save, roll the single
// field back when NVS rejects the write. Mirrors the settings POST handler
// without its full-snapshot machinery (single-field commands only).
template <typename T>
static bool persist_setting(Settings *s, const char *command,
                            T old_value, void (Settings::*setter)(T), T new_value)
{
    (s->*setter)(new_value);
    if (s->save() != ESP_OK) {
        (s->*setter)(old_value);
        (void)s->save();
        reject(command, "persist_failed");
        return false;
    }
    return true;
}

static bool cmd_led_brightness(const mqtt_config_t *cfg, const command_payload_t *p)
{
    long v;
    if (!str_to_long(p->value, &v) || !validateLEDBrightness((int)v)) {
        reject("led_brightness", "invalid_value");
        return true;
    }
    Settings *s = monitoring_get_settings();
    if (s == NULL) {
        return true;
    }
    const int old = s->getLEDBrightness();
    if (!persist_setting<int>(s, "led_brightness", old, &Settings::setLEDBrightness, (int)v)) {
        return true;
    }
    LED::setBrightness((int)v);
    mqtt_commands_publish_settings_states(cfg->topic_prefix);
    return true;
}

static const struct {
    const char *name;
    led_program_t program;
} LED_PROGRAM_COMMANDS[7] = {
    {"led_program_idle", LED_PROG_IDLE},
    {"led_program_ccu_disconnected", LED_PROG_CCU_DISCONNECTED},
    {"led_program_ccu_connected", LED_PROG_CCU_CONNECTED},
    {"led_program_update_available", LED_PROG_UPDATE_AVAILABLE},
    {"led_program_error", LED_PROG_ERROR},
    {"led_program_booting", LED_PROG_BOOTING},
    {"led_program_update_in_progress", LED_PROG_UPDATE_IN_PROGRESS},
};

static bool cmd_led_program(const mqtt_config_t *cfg, const char *command,
                            const command_payload_t *p, led_program_t program)
{
    const int state = name_index(LED_STATE_NAMES, 11, p->value);
    if (state < 0) {
        reject(command, "invalid_value");
        return true;
    }
    Settings *s = monitoring_get_settings();
    if (s == NULL) {
        return true;
    }
    const int old = s->getLedProgram((int)program);
    s->setLedProgram((int)program, state);
    if (s->save() != ESP_OK) {
        s->setLedProgram((int)program, old);
        (void)s->save();
        reject(command, "persist_failed");
        return true;
    }
    LED::setProgram(program, (led_state_t)state);
    mqtt_commands_publish_settings_states(cfg->topic_prefix);
    return true;
}

static bool cmd_system_log_enabled(const mqtt_config_t *cfg, const command_payload_t *p)
{
    bool v;
    if (!str_to_bool(p->value, &v)) {
        reject("system_log_enabled", "invalid_value");
        return true;
    }
    Settings *s = monitoring_get_settings();
    if (s == NULL) {
        return true;
    }
    const bool old = s->getSystemLogEnabled();
    s->setSystemLogEnabled(v);
    if (s->save() != ESP_OK) {
        s->setSystemLogEnabled(old);
        (void)s->save();
        reject("system_log_enabled", "persist_failed");
        return true;
    }
    // Same runtime transition as /api/log/enable and /api/log/disable.
    if (v) {
        LogManager::begin();
    } else {
        LogManager::stop();
    }
    mqtt_commands_publish_settings_states(cfg->topic_prefix);
    return true;
}

static bool cmd_restart_schedule_enabled(const mqtt_config_t *cfg, const command_payload_t *p)
{
    bool v;
    if (!str_to_bool(p->value, &v)) {
        reject("restart_schedule_enabled", "invalid_value");
        return true;
    }
    Settings *s = monitoring_get_settings();
    if (s == NULL) {
        return true;
    }
    if (!persist_setting<bool>(s, "restart_schedule_enabled",
                               s->getRestartScheduleEnabled(),
                               &Settings::setRestartScheduleEnabled, v)) {
        return true;
    }
    mqtt_commands_publish_settings_states(cfg->topic_prefix);
    return true;
}

static bool cmd_restart_schedule_day(const mqtt_config_t *cfg, const command_payload_t *p)
{
    const int day = name_index(WEEKDAY_NAMES, 7, p->value);
    if (day < 0) {
        reject("restart_schedule_day", "invalid_value");
        return true;
    }
    Settings *s = monitoring_get_settings();
    if (s == NULL) {
        return true;
    }
    const uint8_t old = s->getRestartScheduleDay();
    s->setRestartScheduleDay((uint8_t)day);
    if (s->save() != ESP_OK) {
        s->setRestartScheduleDay(old);
        (void)s->save();
        reject("restart_schedule_day", "persist_failed");
        return true;
    }
    mqtt_commands_publish_settings_states(cfg->topic_prefix);
    return true;
}

static bool cmd_restart_schedule_hour(const mqtt_config_t *cfg, const command_payload_t *p)
{
    long v;
    if (!str_to_long(p->value, &v) || v < 0 || v > 23) {
        reject("restart_schedule_hour", "invalid_value");
        return true;
    }
    Settings *s = monitoring_get_settings();
    if (s == NULL) {
        return true;
    }
    const uint8_t old = s->getRestartScheduleHour();
    s->setRestartScheduleHour((uint8_t)v);
    if (s->save() != ESP_OK) {
        s->setRestartScheduleHour(old);
        (void)s->save();
        reject("restart_schedule_hour", "persist_failed");
        return true;
    }
    mqtt_commands_publish_settings_states(cfg->topic_prefix);
    return true;
}

static bool cmd_restart_schedule_minute(const mqtt_config_t *cfg, const command_payload_t *p)
{
    long v;
    if (!str_to_long(p->value, &v) || v < 0 || v > 59) {
        reject("restart_schedule_minute", "invalid_value");
        return true;
    }
    Settings *s = monitoring_get_settings();
    if (s == NULL) {
        return true;
    }
    const uint8_t old = s->getRestartScheduleMinute();
    s->setRestartScheduleMinute((uint8_t)v);
    if (s->save() != ESP_OK) {
        s->setRestartScheduleMinute(old);
        (void)s->save();
        reject("restart_schedule_minute", "persist_failed");
        return true;
    }
    mqtt_commands_publish_settings_states(cfg->topic_prefix);
    return true;
}

// Note: hostname changes persist immediately but only take effect after a
// restart (Ethernet/mDNS pick the name up at boot), exactly like the WebUI.
static bool cmd_hostname(const mqtt_config_t *cfg, const command_payload_t *p)
{
    if (!validateHostname(p->value)) {
        reject("hostname", "invalid_value");
        return true;
    }
    Settings *s = monitoring_get_settings();
    if (s == NULL) {
        return true;
    }
    char old[64];
    snprintf(old, sizeof(old), "%s", s->getHostname());
    // Hostname is part of setNetworkSettings(); all other fields are passed
    // through unchanged so an MQTT hostname edit can never touch the IP
    // configuration (deliberately not MQTT-writable).
    s->setNetworkSettings(p->value, s->getUseDHCP(), s->getLocalIP(),
                          s->getNetmask(), s->getGateway(), s->getDns1(),
                          s->getDns2());
    if (s->save() != ESP_OK) {
        s->setNetworkSettings(old, s->getUseDHCP(), s->getLocalIP(),
                              s->getNetmask(), s->getGateway(), s->getDns1(),
                              s->getDns2());
        (void)s->save();
        reject("hostname", "persist_failed");
        return true;
    }
    mqtt_commands_publish_settings_states(cfg->topic_prefix);
    mqtt_handler_publish_event("event/command_note",
                               "hostname: restart required");
    return true;
}

static bool cmd_ccu_ip(const mqtt_config_t *cfg, const command_payload_t *p)
{
    if (p->value[0] != '\0' && !validateCcuAddress(p->value)) {
        reject("ccu_ip", "invalid_value");
        return true;
    }
    Settings *s = monitoring_get_settings();
    if (s == NULL) {
        return true;
    }
    char old[64];
    snprintf(old, sizeof(old), "%s", s->getCCUIP());
    s->setCCUIP(p->value);
    if (s->save() != ESP_OK) {
        s->setCCUIP(old);
        (void)s->save();
        reject("ccu_ip", "persist_failed");
        return true;
    }
    mqtt_commands_publish_settings_states(cfg->topic_prefix);
    return true;
}

static bool cmd_ntp_server(const mqtt_config_t *cfg, const command_payload_t *p)
{
    if (!validateNtpServer(p->value)) {
        reject("ntp_server", "invalid_value");
        return true;
    }
    Settings *s = monitoring_get_settings();
    if (s == NULL) {
        return true;
    }
    char old[65];
    snprintf(old, sizeof(old), "%s", s->getNtpServer());
    s->setNtpServer(p->value);
    if (s->save() != ESP_OK) {
        s->setNtpServer(old);
        (void)s->save();
        reject("ntp_server", "persist_failed");
        return true;
    }
    mqtt_commands_publish_settings_states(cfg->topic_prefix);
    return true;
}

// The timezone setter applies setenv("TZ") + tzset() itself, so local-time
// consumers (maintenance window) pick the change up without a restart.
static bool cmd_timezone(const mqtt_config_t *cfg, const command_payload_t *p)
{
    if (!validateTimezone(p->value)) {
        reject("timezone", "invalid_value");
        return true;
    }
    Settings *s = monitoring_get_settings();
    if (s == NULL) {
        return true;
    }
    char old[65];
    snprintf(old, sizeof(old), "%s", s->getTimezone());
    s->setTimezone(p->value);
    if (s->save() != ESP_OK) {
        s->setTimezone(old);
        (void)s->save();
        reject("timezone", "persist_failed");
        return true;
    }
    mqtt_commands_publish_settings_states(cfg->topic_prefix);
    return true;
}

static bool cmd_update_mode(const mqtt_config_t *cfg, const command_payload_t *p)
{
    const int mode = update_mode_from_name(p->value);
    // update_mode_from_name maps unknown input to OFF - reject unknown
    // names explicitly so a typo cannot silently disable the schedule.
    if (strcmp(p->value, "off") != 0 && strcmp(p->value, "notify") != 0 &&
        strcmp(p->value, "auto_webui") != 0 && strcmp(p->value, "auto_all") != 0) {
        reject("update_mode", "invalid_value");
        return true;
    }
    Settings *s = monitoring_get_settings();
    if (s == NULL) {
        return true;
    }
    const int old = s->getUpdMode();
    s->setUpdMode(mode);
    if (s->save() != ESP_OK) {
        s->setUpdMode(old);
        (void)s->save();
        reject("update_mode", "persist_failed");
        return true;
    }
    mqtt_commands_publish_settings_states(cfg->topic_prefix);
    return true;
}

static bool cmd_update_channel(const mqtt_config_t *cfg, const command_payload_t *p)
{
    long v;
    if (!str_to_long(p->value, &v)) {
        if (!update_channel_name_valid(p->value)) {
            reject("update_channel", "invalid_value");
            return true;
        }
        v = strcmp(p->value, "beta") == 0 ? 1 : 0;
    }
    if (v != 0 && v != 1) {
        reject("update_channel", "invalid_value");
        return true;
    }
    Settings *s = monitoring_get_settings();
    if (s == NULL) {
        return true;
    }
    const int old = s->getUpdChannel();
    s->setUpdChannel((int)v);
    if (s->save() != ESP_OK) {
        s->setUpdChannel(old);
        (void)s->save();
        reject("update_channel", "persist_failed");
        return true;
    }
    mqtt_commands_publish_settings_states(cfg->topic_prefix);
    return true;
}

static bool cmd_update_interval_hours(const mqtt_config_t *cfg, const command_payload_t *p)
{
    long v;
    if (!str_to_long(p->value, &v) || !update_interval_valid((int)v)) {
        reject("update_interval_hours", "invalid_value");
        return true;
    }
    Settings *s = monitoring_get_settings();
    if (s == NULL) {
        return true;
    }
    const int old = s->getUpdInterval();
    s->setUpdInterval((int)v);
    if (s->save() != ESP_OK) {
        s->setUpdInterval(old);
        (void)s->save();
        reject("update_interval_hours", "persist_failed");
        return true;
    }
    mqtt_commands_publish_settings_states(cfg->topic_prefix);
    return true;
}

static bool cmd_update_window_start(const mqtt_config_t *cfg, const command_payload_t *p)
{
    long v;
    if (!str_to_long(p->value, &v) || !update_window_start_valid((int)v)) {
        reject("update_window_start", "invalid_value");
        return true;
    }
    Settings *s = monitoring_get_settings();
    if (s == NULL) {
        return true;
    }
    const int old = s->getUpdWinStart();
    s->setUpdWinStart((int)v);
    if (s->save() != ESP_OK) {
        s->setUpdWinStart(old);
        (void)s->save();
        reject("update_window_start", "persist_failed");
        return true;
    }
    mqtt_commands_publish_settings_states(cfg->topic_prefix);
    return true;
}

static bool cmd_update_window_length(const mqtt_config_t *cfg, const command_payload_t *p)
{
    long v;
    if (!str_to_long(p->value, &v) || !update_window_len_valid((int)v)) {
        reject("update_window_length", "invalid_value");
        return true;
    }
    Settings *s = monitoring_get_settings();
    if (s == NULL) {
        return true;
    }
    const int old = s->getUpdWinLen();
    s->setUpdWinLen((int)v);
    if (s->save() != ESP_OK) {
        s->setUpdWinLen(old);
        (void)s->save();
        reject("update_window_length", "persist_failed");
        return true;
    }
    mqtt_commands_publish_settings_states(cfg->topic_prefix);
    return true;
}

static bool cmd_update_idle_only(const mqtt_config_t *cfg, const command_payload_t *p)
{
    bool v;
    if (!str_to_bool(p->value, &v)) {
        reject("update_idle_only", "invalid_value");
        return true;
    }
    Settings *s = monitoring_get_settings();
    if (s == NULL) {
        return true;
    }
    if (!persist_setting<bool>(s, "update_idle_only", s->getUpdIdleOnly(),
                               &Settings::setUpdIdleOnly, v)) {
        return true;
    }
    mqtt_commands_publish_settings_states(cfg->topic_prefix);
    return true;
}

// ---------------------------------------------------------------------------
// Monitoring-backed commands (async config transaction, same as REST POST)
// ---------------------------------------------------------------------------

// The async completion needs the topic prefix to republish state topics.
// A small heap context keeps it race-free without another mutex; freed by
// the completion itself.
typedef struct {
    char topic_prefix[65];
} monitoring_cmd_ctx_t;

static void monitoring_cmd_done(esp_err_t result, void *context)
{
    monitoring_cmd_ctx_t *ctx = (monitoring_cmd_ctx_t *)context;
    if (ctx == NULL) {
        return;
    }
    if (result == ESP_OK) {
        mqtt_commands_publish_monitoring_states(ctx->topic_prefix);
    } else {
        mqtt_handler_publish_event("event/command_rejected",
                                   "reason=monitoring_apply_failed");
    }
    free(ctx);
}

// One shared path: copy the live config, hand the field edit to the caller,
// then run the same bounded async transition the REST API uses.
static bool apply_monitoring_field(const mqtt_config_t *cfg, const char *command,
                                   bool (*edit)(monitoring_config_t *, const char *value),
                                   const command_payload_t *p)
{
    monitoring_config_t config;
    if (monitoring_get_config(&config) != ESP_OK) {
        reject(command, "config_unavailable");
        return true;
    }
    if (!edit(&config, p->value)) {
        reject(command, "invalid_value");
        return true;
    }
    monitoring_cmd_ctx_t *ctx =
        (monitoring_cmd_ctx_t *)calloc(1, sizeof(monitoring_cmd_ctx_t));
    if (ctx == NULL) {
        reject(command, "no_memory");
        return true;
    }
    snprintf(ctx->topic_prefix, sizeof(ctx->topic_prefix), "%s", cfg->topic_prefix);
    if (monitoring_schedule_update_config(&config, monitoring_cmd_done, ctx) != ESP_OK) {
        free(ctx);
        reject(command, "schedule_failed");
    }
    return true;
}

static bool edit_syslog_enabled(monitoring_config_t *c, const char *value)
{
    bool v;
    return str_to_bool(value, &v) ? (c->syslog.enabled = v, true) : false;
}

static bool edit_syslog_server(monitoring_config_t *c, const char *value)
{
    // Mirror the REST rule: a non-empty value (or an enabled service) must
    // be a valid address.
    if ((c->syslog.enabled || value[0] != '\0') &&
        !validateServerAddress(value, sizeof(c->syslog.server) - 1)) {
        return false;
    }
    snprintf(c->syslog.server, sizeof(c->syslog.server), "%s", value);
    return true;
}

static bool edit_syslog_port(monitoring_config_t *c, const char *value)
{
    long v;
    return str_to_long(value, &v) && validatePort((int)v)
               ? (c->syslog.port = (uint16_t)v, true)
               : false;
}

static bool edit_syslog_transport(monitoring_config_t *c, const char *value)
{
    const int t = name_index(SYSLOG_TRANSPORT_NAMES, 3, value);
    if (t < 0) {
        long v;
        if (!str_to_long(value, &v) || v < 0 || v > 2) {
            return false;
        }
        c->syslog.transport = (uint8_t)v;
        return true;
    }
    c->syslog.transport = (uint8_t)t;
    return true;
}

static bool edit_syslog_min_severity(monitoring_config_t *c, const char *value)
{
    const int sev = name_index(LOG_SEVERITY_NAMES, 8, value);
    if (sev >= 0) {
        c->syslog.min_severity = (uint8_t)sev;
        return true;
    }
    long v;
    return str_to_long(value, &v) && v >= 0 && v <= 7
               ? (c->syslog.min_severity = (uint8_t)v, true)
               : false;
}

static bool edit_checkmk_enabled(monitoring_config_t *c, const char *value)
{
    bool v;
    return str_to_bool(value, &v) ? (c->checkmk.enabled = v, true) : false;
}

static bool edit_checkmk_port(monitoring_config_t *c, const char *value)
{
    long v;
    return str_to_long(value, &v) && validatePort((int)v)
               ? (c->checkmk.port = (uint16_t)v, true)
               : false;
}

static bool edit_prometheus_enabled(monitoring_config_t *c, const char *value)
{
    bool v;
    return str_to_bool(value, &v) ? (c->prometheus.enabled = v, true) : false;
}

static bool edit_prometheus_port(monitoring_config_t *c, const char *value)
{
    long v;
    return str_to_long(value, &v) && validatePort((int)v)
               ? (c->prometheus.port = (uint16_t)v, true)
               : false;
}

static bool edit_notify_enabled(monitoring_config_t *c, const char *value)
{
    bool v;
    return str_to_bool(value, &v) ? (c->notify.enabled = v, true) : false;
}

static bool edit_notify_cooldown_seconds(monitoring_config_t *c, const char *value)
{
    long v;
    if (!str_to_long(value, &v) || v < 0 || v > 86400) {
        return false;
    }
    c->notify.cooldown_seconds = (uint16_t)v;
    return true;
}

// ---------------------------------------------------------------------------
// Action commands
// ---------------------------------------------------------------------------

static bool cmd_update_check(const mqtt_config_t *cfg, const command_payload_t *p)
{
    (void)cfg; // outcome is reported via event topics only
    Settings *s = monitoring_get_settings();
    char channel[8] = "stable";
    if (p->has_value && update_channel_name_valid(p->value)) {
        // update_channel_name_valid limits the input to "stable"/"beta";
        // the precision bound proves that to the compiler as well.
        snprintf(channel, sizeof(channel), "%.7s", p->value);
    } else if (s != NULL) {
        snprintf(channel, sizeof(channel), "%s",
                 s->getUpdChannel() == 1 ? "beta" : "stable");
    }

    switch (update_check_trigger(channel)) {
    case UPDATE_CHECK_TRIGGER_ACCEPTED:
        mqtt_handler_publish_event("event/update_check", channel);
        break;
    case UPDATE_CHECK_TRIGGER_BUSY:
        reject("update_check", "busy");
        break;
    case UPDATE_CHECK_TRIGGER_COOLDOWN:
        reject("update_check", "cooldown");
        break;
    default:
        reject("update_check", "no_worker");
        break;
    }
    return true;
}

static bool cmd_update_install(const mqtt_config_t *cfg, const char *command,
                               const char *target)
{
    switch (update_install_start(target)) {
    case UPDATE_INSTALL_ACCEPTED:
        mqtt_handler_publish_event("event/update_install", target);
        // Progress lands on <prefix>/status/update_<target> via the status
        // cycle; publish once immediately so HA reacts without the 60 s wait.
        mqtt_commands_publish_update_states(cfg);
        break;
    case UPDATE_INSTALL_BUSY:
        reject(command, "busy");
        break;
    case UPDATE_INSTALL_INVALID_TARGET:
        reject(command, "invalid_target");
        break;
    default:
        reject(command, "no_worker");
        break;
    }
    return true;
}

static bool cmd_reset_relay_stats(const mqtt_config_t *cfg)
{
    (void)cfg;
    // Same window reset as POST /api/system/relay-stats/reset: both the
    // latency high-water marks and the UART failure counters, so a fresh
    // observation window starts on both sides of the bridge.
    raw_uart_reset_latency_high_water();
    radio_uart_reset_window();
    ESP_LOGI(TAG, "CCU relay stats reset via MQTT command");
    mqtt_handler_publish_event("event/relay_stats_reset", "ok");
    return true;
}

// The connectivity self-test blocks for the duration of a network (possibly
// TLS) round trip - never run it on the ESP-MQTT event task. Same pattern
// as the restart command: short-lived task, bounded stack.
typedef struct {
    char target[12];
} diag_task_ctx_t;

static void diag_test_task(void *arg)
{
    diag_task_ctx_t *ctx = (diag_task_ctx_t *)arg;
    bool ok = false;
    char code[64] = {0};
    char message[96] = {0};
    esp_err_t result = monitoring_run_diagnostic(
        ctx->target, &ok, code, sizeof(code), message, sizeof(message),
        NULL, 0, NULL, NULL);
    char payload[192];
    if (result == ESP_OK) {
        snprintf(payload, sizeof(payload),
                 "target=%s ok=%s code=%.48s", ctx->target,
                 ok ? "true" : "false", code);
    } else {
        snprintf(payload, sizeof(payload), "target=%s error", ctx->target);
    }
    mqtt_handler_publish_event("event/monitoring_test", payload);
    free(ctx);
    vTaskDelete(NULL);
}

static bool spawn_diag_test(const char *target)
{
    diag_task_ctx_t *ctx = (diag_task_ctx_t *)calloc(1, sizeof(diag_task_ctx_t));
    if (ctx == NULL) {
        return false;
    }
    snprintf(ctx->target, sizeof(ctx->target), "%s", target);
    // TLS-based targets (notify via SMTP/webhook, syslog TLS) need the
    // mbedTLS handshake stack; 8 KiB matches the outbound fetch workers.
    if (xTaskCreate(diag_test_task, "mqtt_diag", 8192, ctx, 5, NULL) != pdPASS) {
        free(ctx);
        return false;
    }
    return true;
}

static bool cmd_test_notify(const mqtt_config_t *cfg)
{
    (void)cfg;
    if (!spawn_diag_test("notify")) {
        reject("test_notify", "no_worker");
    }
    return true;
}

static bool cmd_log_level(const mqtt_config_t *cfg, const command_payload_t *p)
{
    (void)cfg; // RAM-only override, nothing to republish
    // Format "TAG:LEVEL" - reuses the RAM-override machinery of
    // POST /api/loglevel so the WebUI shows the same overrides.
    char buf[MQTT_COMMAND_VALUE_MAX];
    snprintf(buf, sizeof(buf), "%s", p->value);
    char *colon = strchr(buf, ':');
    if (colon == NULL || colon == buf || colon[1] == '\0') {
        reject("log_level", "invalid_value");
        return true;
    }
    *colon = '\0';
    if (!webui_apply_log_level_override(buf, colon + 1)) {
        reject("log_level", "invalid_value");
        return true;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

bool mqtt_commands_try_handle(const char *command, const char *payload, int payload_len)
{
    if (command == NULL) {
        return false;
    }

    // --- Action commands (token-only payload or JSON) --------------------
    if (!strcmp(command, "update_check")) {
        const mqtt_config_t *cfg = mqtt_handler_current_config();
        command_payload_t p;
        if (!command_payload_decode(cfg, command, payload, payload_len, false, &p)) {
            return true;
        }
        return cmd_update_check(cfg, &p);
    }
    if (!strcmp(command, "update_install_firmware")) {
        const mqtt_config_t *cfg = mqtt_handler_current_config();
        command_payload_t p;
        if (!command_payload_decode(cfg, command, payload, payload_len, false, &p)) {
            return true;
        }
        return cmd_update_install(cfg, command, "firmware");
    }
    if (!strcmp(command, "update_install_webui")) {
        const mqtt_config_t *cfg = mqtt_handler_current_config();
        command_payload_t p;
        if (!command_payload_decode(cfg, command, payload, payload_len, false, &p)) {
            return true;
        }
        return cmd_update_install(cfg, command, "webui");
    }
    if (!strcmp(command, "reset_relay_stats")) {
        const mqtt_config_t *cfg = mqtt_handler_current_config();
        command_payload_t p;
        if (!command_payload_decode(cfg, command, payload, payload_len, false, &p)) {
            return true;
        }
        return cmd_reset_relay_stats(cfg);
    }
    if (!strcmp(command, "test_notify")) {
        const mqtt_config_t *cfg = mqtt_handler_current_config();
        command_payload_t p;
        if (!command_payload_decode(cfg, command, payload, payload_len, false, &p)) {
            return true;
        }
        return cmd_test_notify(cfg);
    }
    if (!strcmp(command, "log_level")) {
        const mqtt_config_t *cfg = mqtt_handler_current_config();
        command_payload_t p;
        if (!command_payload_decode(cfg, command, payload, payload_len, true, &p)) {
            return true;
        }
        return cmd_log_level(cfg, &p);
    }

    // --- Monitoring commands (JSON with value) ---------------------------
    static const struct {
        const char *name;
        bool (*edit)(monitoring_config_t *, const char *);
    } MONITORING_COMMANDS[] = {
        {"syslog_enabled", edit_syslog_enabled},
        {"syslog_server", edit_syslog_server},
        {"syslog_port", edit_syslog_port},
        {"syslog_transport", edit_syslog_transport},
        {"syslog_min_severity", edit_syslog_min_severity},
        {"checkmk_enabled", edit_checkmk_enabled},
        {"checkmk_port", edit_checkmk_port},
        {"prometheus_enabled", edit_prometheus_enabled},
        {"prometheus_port", edit_prometheus_port},
        {"notify_enabled", edit_notify_enabled},
        {"notify_cooldown_seconds", edit_notify_cooldown_seconds},
    };
    for (size_t i = 0; i < sizeof(MONITORING_COMMANDS) / sizeof(MONITORING_COMMANDS[0]); i++) {
        if (!strcmp(command, MONITORING_COMMANDS[i].name)) {
            const mqtt_config_t *cfg = mqtt_handler_current_config();
            command_payload_t p;
            if (!command_payload_decode(cfg, command, payload, payload_len, true, &p)) {
                return true;
            }
            return apply_monitoring_field(cfg, command, MONITORING_COMMANDS[i].edit, &p);
        }
    }

    // --- Settings commands (JSON with value) ------------------------------
    typedef bool (*settings_cmd_fn)(const mqtt_config_t *, const command_payload_t *);
    static const struct {
        const char *name;
        settings_cmd_fn fn;
    } SETTINGS_COMMANDS[] = {
        {"led_brightness", cmd_led_brightness},
        {"system_log_enabled", cmd_system_log_enabled},
        {"restart_schedule_enabled", cmd_restart_schedule_enabled},
        {"restart_schedule_day", cmd_restart_schedule_day},
        {"restart_schedule_hour", cmd_restart_schedule_hour},
        {"restart_schedule_minute", cmd_restart_schedule_minute},
        {"hostname", cmd_hostname},
        {"ccu_ip", cmd_ccu_ip},
        {"ntp_server", cmd_ntp_server},
        {"timezone", cmd_timezone},
        {"update_mode", cmd_update_mode},
        {"update_channel", cmd_update_channel},
        {"update_interval_hours", cmd_update_interval_hours},
        {"update_window_start", cmd_update_window_start},
        {"update_window_length", cmd_update_window_length},
        {"update_idle_only", cmd_update_idle_only},
    };
    for (size_t i = 0; i < sizeof(SETTINGS_COMMANDS) / sizeof(SETTINGS_COMMANDS[0]); i++) {
        if (!strcmp(command, SETTINGS_COMMANDS[i].name)) {
            const mqtt_config_t *cfg = mqtt_handler_current_config();
            command_payload_t p;
            if (!command_payload_decode(cfg, command, payload, payload_len, true, &p)) {
                return true;
            }
            return SETTINGS_COMMANDS[i].fn(cfg, &p);
        }
    }
    for (size_t i = 0; i < 7; i++) {
        if (!strcmp(command, LED_PROGRAM_COMMANDS[i].name)) {
            const mqtt_config_t *cfg = mqtt_handler_current_config();
            command_payload_t p;
            if (!command_payload_decode(cfg, command, payload, payload_len, true, &p)) {
                return true;
            }
            return cmd_led_program(cfg, command, &p, LED_PROGRAM_COMMANDS[i].program);
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// Retained state topics (<prefix>/status/settings/<key>)
// ---------------------------------------------------------------------------

static void publish_setting(const char *topic_prefix, const char *key, const char *value)
{
    char topic[192];
    snprintf(topic, sizeof(topic), "%s/status/settings/%s", topic_prefix, key);
    mqtt_handler_publish_retained(topic, value, 0);
}

void mqtt_commands_publish_settings_states(const char *topic_prefix)
{
    Settings *s = monitoring_get_settings();
    if (s == NULL) {
        return;
    }

    char buf[80];
    snprintf(buf, sizeof(buf), "%d", s->getLEDBrightness());
    publish_setting(topic_prefix, "led_brightness", buf);

    static const char *const KEYS[7] = {
        "led_program_idle", "led_program_ccu_disconnected",
        "led_program_ccu_connected", "led_program_update_available",
        "led_program_error", "led_program_booting",
        "led_program_update_in_progress"};
    for (int i = 0; i < 7; i++) {
        const int state = s->getLedProgram(i);
        snprintf(buf, sizeof(buf), "%s",
                 (state >= 0 && state <= 10) ? LED_STATE_NAMES[state] : "off");
        publish_setting(topic_prefix, KEYS[i], buf);
    }

    publish_setting(topic_prefix, "system_log_enabled",
                    s->getSystemLogEnabled() ? "true" : "false");
    publish_setting(topic_prefix, "restart_schedule_enabled",
                    s->getRestartScheduleEnabled() ? "true" : "false");
    const uint8_t day = s->getRestartScheduleDay();
    publish_setting(topic_prefix, "restart_schedule_day",
                    day <= 6 ? WEEKDAY_NAMES[day] : "sun");
    snprintf(buf, sizeof(buf), "%u", (unsigned)s->getRestartScheduleHour());
    publish_setting(topic_prefix, "restart_schedule_hour", buf);
    snprintf(buf, sizeof(buf), "%u", (unsigned)s->getRestartScheduleMinute());
    publish_setting(topic_prefix, "restart_schedule_minute", buf);

    publish_setting(topic_prefix, "hostname", s->getHostname());
    publish_setting(topic_prefix, "ccu_ip", s->getCCUIP());
    publish_setting(topic_prefix, "ntp_server", s->getNtpServer());
    publish_setting(topic_prefix, "timezone", s->getTimezone());

    publish_setting(topic_prefix, "update_mode", update_mode_name(s->getUpdMode()));
    publish_setting(topic_prefix, "update_channel",
                    s->getUpdChannel() == 1 ? "beta" : "stable");
    snprintf(buf, sizeof(buf), "%d", s->getUpdInterval());
    publish_setting(topic_prefix, "update_interval_hours", buf);
    snprintf(buf, sizeof(buf), "%d", s->getUpdWinStart());
    publish_setting(topic_prefix, "update_window_start", buf);
    snprintf(buf, sizeof(buf), "%d", s->getUpdWinLen());
    publish_setting(topic_prefix, "update_window_length", buf);
    publish_setting(topic_prefix, "update_idle_only",
                    s->getUpdIdleOnly() ? "true" : "false");
}

void mqtt_commands_publish_monitoring_states(const char *topic_prefix)
{
    monitoring_config_t config;
    if (monitoring_get_config(&config) != ESP_OK) {
        return;
    }

    char buf[80];
    publish_setting(topic_prefix, "syslog_enabled",
                    config.syslog.enabled ? "true" : "false");
    publish_setting(topic_prefix, "syslog_server", config.syslog.server);
    snprintf(buf, sizeof(buf), "%u", (unsigned)config.syslog.port);
    publish_setting(topic_prefix, "syslog_port", buf);
    publish_setting(topic_prefix, "syslog_transport",
                    config.syslog.transport <= 2
                        ? SYSLOG_TRANSPORT_NAMES[config.syslog.transport]
                        : "udp");
    publish_setting(topic_prefix, "syslog_min_severity",
                    config.syslog.min_severity <= 7
                        ? LOG_SEVERITY_NAMES[config.syslog.min_severity]
                        : "INFO");

    publish_setting(topic_prefix, "checkmk_enabled",
                    config.checkmk.enabled ? "true" : "false");
    snprintf(buf, sizeof(buf), "%u", (unsigned)config.checkmk.port);
    publish_setting(topic_prefix, "checkmk_port", buf);

    publish_setting(topic_prefix, "prometheus_enabled",
                    config.prometheus.enabled ? "true" : "false");
    snprintf(buf, sizeof(buf), "%u", (unsigned)config.prometheus.port);
    publish_setting(topic_prefix, "prometheus_port", buf);

    publish_setting(topic_prefix, "notify_enabled",
                    config.notify.enabled ? "true" : "false");
    snprintf(buf, sizeof(buf), "%u", (unsigned)config.notify.cooldown_seconds);
    publish_setting(topic_prefix, "notify_cooldown_seconds", buf);
}

void mqtt_commands_publish_states(const mqtt_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }
    mqtt_commands_publish_settings_states(cfg->topic_prefix);
    mqtt_commands_publish_monitoring_states(cfg->topic_prefix);
    mqtt_commands_publish_update_states(cfg);
}

// ---------------------------------------------------------------------------
// Update entity state topics (<prefix>/status/update_firmware|webui)
// ---------------------------------------------------------------------------

static void publish_update_state(const mqtt_config_t *cfg, bool firmware)
{
    SysInfo *sysInfo = monitoring_get_sysinfo();
    if (sysInfo == NULL) {
        return;
    }

    const UpdateCheckStatus chk = update_check_get_status();
    const UpdateInstallStatus inst = update_install_get_status();

    char installed[48];
    if (firmware) {
        snprintf(installed, sizeof(installed), "%s", sysInfo->getCurrentVersion());
    } else {
        webui_storage_get_effective_version(installed, sizeof(installed));
    }

    // Before the first manifest fetch nothing is known about a newer
    // release. Reporting latest = installed renders the entity as "up to
    // date" instead of unavailable; press "Check update" for a real answer.
    const char *latest = installed;
    if (chk.everChecked) {
        latest = firmware ? chk.latestFirmware : chk.latestWebui;
        if (latest[0] == '\0') {
            latest = installed;
        }
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "title", firmware ? "Firmware" : "WebUI");
    cJSON_AddStringToObject(root, "installed_version", installed);
    cJSON_AddStringToObject(root, "latest_version", latest);
    if (chk.notesUrl[0] != '\0') {
        cJSON_AddStringToObject(root, "release_url", chk.notesUrl);
    }

    const bool active = inst.state == UPDATE_INSTALL_PREPARING ||
                        inst.state == UPDATE_INSTALL_DOWNLOADING ||
                        inst.state == UPDATE_INSTALL_VERIFYING ||
                        inst.state == UPDATE_INSTALL_FINALIZING;
    const bool target_matches =
        active && ((firmware && inst.target == UPDATE_INSTALL_TARGET_FIRMWARE) ||
                   (!firmware && inst.target == UPDATE_INSTALL_TARGET_WEBUI));
    if (target_matches) {
        // HA update schema: "in_progress" is strictly boolean; the percent
        // value belongs in the separate "update_percentage" field.
        cJSON_AddBoolToObject(root, "in_progress", true);
        if (inst.state == UPDATE_INSTALL_DOWNLOADING) {
            cJSON_AddNumberToObject(root, "update_percentage", inst.progress);
        }
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        return;
    }
    char topic[192];
    snprintf(topic, sizeof(topic), "%s/status/update_%s", cfg->topic_prefix,
             firmware ? "firmware" : "webui");
    mqtt_handler_publish_retained(topic, json, 0);
    free(json);
}

void mqtt_commands_publish_update_states(const mqtt_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }
    publish_update_state(cfg, true);
    publish_update_state(cfg, false);
}

// ---------------------------------------------------------------------------
// HA discovery for the controllable entities
// ---------------------------------------------------------------------------

// Security note (extends the existing restart-button model): when a command
// token is configured it is embedded in the discovery configs (payload_press,
// payload_install, command_template). Anyone who can read the discovery tree
// on the broker can extract it - restrict <ha_prefix>/# with broker ACLs and
// use TLS/mTLS to the broker. Without a token the payloads carry plain
// values and only broker ACLs protect the command topics.
static void build_command_template(char *out, size_t size, const mqtt_config_t *cfg)
{
    if (cfg->command_token[0] != '\0') {
        // Token charset is validated to [A-Za-z0-9-_.] so no JSON escaping
        // of the embedded token is required.
        snprintf(out, size, "{\"token\":\"%s\",\"value\":\"{{ value }}\"}",
                 cfg->command_token);
    } else {
        snprintf(out, size, "{{ value }}");
    }
}

static void publish_entity(const mqtt_config_t *cfg, const char *serial,
                           const cJSON *device, const char *component,
                           const char *object_id, cJSON *root)
{
    char unique_id[128];
    snprintf(unique_id, sizeof(unique_id), "hb-rf-eth-%s_%s", serial, object_id);
    cJSON_AddStringToObject(root, "unique_id", unique_id);
    cJSON_AddItemToObject(root, "device", cJSON_Duplicate(device, 1));

    char *json = cJSON_PrintUnformatted(root);
    if (json != NULL) {
        char topic[256];
        snprintf(topic, sizeof(topic), "%s/%s/hb-rf-eth-%s/%s/config",
                 cfg->ha_discovery_prefix, component, serial, object_id);
        mqtt_handler_publish_retained(topic, json, 1);
        free(json);
    }
    cJSON_Delete(root);
}

static void publish_switch(const mqtt_config_t *cfg, const char *serial,
                           const cJSON *device, const char *key, const char *name,
                           const char *icon)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", name);

    char state_topic[192];
    snprintf(state_topic, sizeof(state_topic), "%s/status/settings/%s",
             cfg->topic_prefix, key);
    cJSON_AddStringToObject(root, "state_topic", state_topic);
    cJSON_AddStringToObject(root, "state_on", "true");
    cJSON_AddStringToObject(root, "state_off", "false");

    char command_topic[192];
    snprintf(command_topic, sizeof(command_topic), "%s/command/%s",
             cfg->topic_prefix, key);
    cJSON_AddStringToObject(root, "command_topic", command_topic);
    cJSON_AddStringToObject(root, "payload_on", "true");
    cJSON_AddStringToObject(root, "payload_off", "false");

    char tmpl[160];
    build_command_template(tmpl, sizeof(tmpl), cfg);
    cJSON_AddStringToObject(root, "command_template", tmpl);

    cJSON_AddStringToObject(root, "entity_category", "config");
    if (icon) {
        cJSON_AddStringToObject(root, "icon", icon);
    }
    publish_entity(cfg, serial, device, "switch", key, root);
}

static void publish_number(const mqtt_config_t *cfg, const char *serial,
                           const cJSON *device, const char *key, const char *name,
                           int min, int max, const char *unit, const char *icon)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", name);

    char state_topic[192];
    snprintf(state_topic, sizeof(state_topic), "%s/status/settings/%s",
             cfg->topic_prefix, key);
    cJSON_AddStringToObject(root, "state_topic", state_topic);

    char command_topic[192];
    snprintf(command_topic, sizeof(command_topic), "%s/command/%s",
             cfg->topic_prefix, key);
    cJSON_AddStringToObject(root, "command_topic", command_topic);

    char tmpl[160];
    build_command_template(tmpl, sizeof(tmpl), cfg);
    cJSON_AddStringToObject(root, "command_template", tmpl);

    cJSON_AddNumberToObject(root, "min", min);
    cJSON_AddNumberToObject(root, "max", max);
    cJSON_AddNumberToObject(root, "step", 1);
    if (unit) {
        cJSON_AddStringToObject(root, "unit_of_measurement", unit);
    }
    cJSON_AddStringToObject(root, "entity_category", "config");
    if (icon) {
        cJSON_AddStringToObject(root, "icon", icon);
    }
    publish_entity(cfg, serial, device, "number", key, root);
}

static void publish_select(const mqtt_config_t *cfg, const char *serial,
                           const cJSON *device, const char *key, const char *name,
                           const char *const *options, size_t option_count,
                           const char *icon)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", name);

    char state_topic[192];
    snprintf(state_topic, sizeof(state_topic), "%s/status/settings/%s",
             cfg->topic_prefix, key);
    cJSON_AddStringToObject(root, "state_topic", state_topic);

    char command_topic[192];
    snprintf(command_topic, sizeof(command_topic), "%s/command/%s",
             cfg->topic_prefix, key);
    cJSON_AddStringToObject(root, "command_topic", command_topic);

    char tmpl[160];
    build_command_template(tmpl, sizeof(tmpl), cfg);
    cJSON_AddStringToObject(root, "command_template", tmpl);

    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; i < option_count; i++) {
        cJSON_AddItemToArray(list, cJSON_CreateString(options[i]));
    }
    cJSON_AddItemToObject(root, "options", list);

    cJSON_AddStringToObject(root, "entity_category", "config");
    if (icon) {
        cJSON_AddStringToObject(root, "icon", icon);
    }
    publish_entity(cfg, serial, device, "select", key, root);
}

static void publish_text(const mqtt_config_t *cfg, const char *serial,
                         const cJSON *device, const char *key, const char *name,
                         int max_len, const char *icon)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", name);

    char state_topic[192];
    snprintf(state_topic, sizeof(state_topic), "%s/status/settings/%s",
             cfg->topic_prefix, key);
    cJSON_AddStringToObject(root, "state_topic", state_topic);

    char command_topic[192];
    snprintf(command_topic, sizeof(command_topic), "%s/command/%s",
             cfg->topic_prefix, key);
    cJSON_AddStringToObject(root, "command_topic", command_topic);

    char tmpl[160];
    build_command_template(tmpl, sizeof(tmpl), cfg);
    cJSON_AddStringToObject(root, "command_template", tmpl);

    cJSON_AddNumberToObject(root, "max", max_len);
    cJSON_AddNumberToObject(root, "min", 0);

    cJSON_AddStringToObject(root, "entity_category", "config");
    if (icon) {
        cJSON_AddStringToObject(root, "icon", icon);
    }
    publish_entity(cfg, serial, device, "text", key, root);
}

static void publish_action_button(const mqtt_config_t *cfg, const char *serial,
                                  const cJSON *device, const char *key,
                                  const char *name, const char *icon)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", name);

    char command_topic[192];
    snprintf(command_topic, sizeof(command_topic), "%s/command/%s",
             cfg->topic_prefix, key);
    cJSON_AddStringToObject(root, "command_topic", command_topic);

    // Fixed payload: the configured token (or the command name as an
    // inert fallback when no token is set), mirroring the restart button.
    const char *payload =
        cfg->command_token[0] != '\0' ? cfg->command_token : key;
    cJSON_AddStringToObject(root, "payload_press", payload);

    cJSON_AddStringToObject(root, "entity_category", "diagnostic");
    if (icon) {
        cJSON_AddStringToObject(root, "icon", icon);
    }
    publish_entity(cfg, serial, device, "button", key, root);
}

static void publish_update_entity(const mqtt_config_t *cfg, const char *serial,
                                  const cJSON *device, const char *key,
                                  const char *title, const char *command,
                                  const char *icon)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", title);
    cJSON_AddStringToObject(root, "title", title);

    char state_topic[192];
    snprintf(state_topic, sizeof(state_topic), "%s/status/update_%s",
             cfg->topic_prefix, key);
    cJSON_AddStringToObject(root, "state_topic", state_topic);

    char command_topic[192];
    snprintf(command_topic, sizeof(command_topic), "%s/command/%s",
             cfg->topic_prefix, command);
    cJSON_AddStringToObject(root, "command_topic", command_topic);

    // Fixed install payload (no command_template support on update
    // entities): the command token, or the legacy plain word.
    const char *payload =
        cfg->command_token[0] != '\0' ? cfg->command_token : "install";
    cJSON_AddStringToObject(root, "payload_install", payload);

    cJSON_AddStringToObject(root, "device_class", "firmware");
    cJSON_AddStringToObject(root, "entity_category", "config");
    if (icon) {
        cJSON_AddStringToObject(root, "icon", icon);
    }
    publish_entity(cfg, serial, device, "update", key, root);
}

void mqtt_commands_publish_discovery(const mqtt_config_t *cfg, const char *serial,
                                     const cJSON *device)
{
    if (cfg == NULL || serial == NULL || device == NULL) {
        return;
    }
    if (!cfg->ha_discovery_enabled) {
        return;
    }

    // With commands disabled the device ignores every payload, so no
    // clickable entities are announced (same rule as the restart button).
    if (!cfg->command_enabled) {
        return;
    }

    // --- Switches: monitoring services ---------------------------------
    publish_switch(cfg, serial, device, "syslog_enabled", "Syslog",
                   "mdi:file-document-outline");
    publish_switch(cfg, serial, device, "checkmk_enabled", "CheckMK Agent",
                   "mdi:server");
    publish_switch(cfg, serial, device, "prometheus_enabled", "Prometheus",
                   "mdi:chart-areaspline");
    publish_switch(cfg, serial, device, "notify_enabled", "Event Notifications",
                   "mdi:bell-outline");
    publish_switch(cfg, serial, device, "system_log_enabled", "System Log Capture",
                   "mdi:script-text-outline");
    publish_switch(cfg, serial, device, "restart_schedule_enabled",
                   "Maintenance Restart", "mdi:restart");
    publish_switch(cfg, serial, device, "update_idle_only",
                   "Update Only When CCU Idle", "mdi:sleep");

    // --- Numbers: ports, windows, brightness ----------------------------
    publish_number(cfg, serial, device, "led_brightness", "LED Brightness", 0, 100,
                   "%", "mdi:brightness-6");
    publish_number(cfg, serial, device, "syslog_port", "Syslog Port", 1, 65535,
                   NULL, "mdi:lan");
    publish_number(cfg, serial, device, "checkmk_port", "CheckMK Port", 1, 65535,
                   NULL, "mdi:lan");
    publish_number(cfg, serial, device, "prometheus_port", "Prometheus Port", 1,
                   65535, NULL, "mdi:lan");
    publish_number(cfg, serial, device, "notify_cooldown_seconds",
                   "Notification Cooldown", 0, 86400, "s", "mdi:timer-outline");
    publish_number(cfg, serial, device, "update_interval_hours", "Update Check Interval",
                   UPDATE_INTERVAL_MIN_HOURS, UPDATE_INTERVAL_MAX_HOURS, "h",
                   "mdi:update");
    publish_number(cfg, serial, device, "update_window_start", "Update Window Start",
                   0, 23, "h", "mdi:window-closed");
    publish_number(cfg, serial, device, "update_window_length", "Update Window Length",
                   1, 24, "h", "mdi:window-closed-open");
    publish_number(cfg, serial, device, "restart_schedule_hour",
                   "Maintenance Restart Hour", 0, 23, "h", "mdi:clock-outline");
    publish_number(cfg, serial, device, "restart_schedule_minute",
                   "Maintenance Restart Minute", 0, 59, "min", "mdi:clock-outline");

    // --- Selects ---------------------------------------------------------
    publish_select(cfg, serial, device, "syslog_transport", "Syslog Transport",
                   SYSLOG_TRANSPORT_NAMES, 3, "mdi:swap-horizontal");
    publish_select(cfg, serial, device, "syslog_min_severity", "Syslog Min Severity",
                   LOG_SEVERITY_NAMES, 8, "mdi:filter-variant");
    publish_select(cfg, serial, device, "restart_schedule_day",
                   "Maintenance Restart Day", WEEKDAY_NAMES, 7,
                   "mdi:calendar-outline");
    static const char *const UPDATE_MODES[4] = {"off", "notify", "auto_webui",
                                                "auto_all"};
    static const char *const UPDATE_CHANNELS[2] = {"stable", "beta"};
    publish_select(cfg, serial, device, "update_mode", "Update Mode", UPDATE_MODES,
                   4, "mdi:cog-outline");
    publish_select(cfg, serial, device, "update_channel", "Update Channel",
                   UPDATE_CHANNELS, 2, "mdi:source-branch");

    static const char *const KEYS[7] = {
        "led_program_idle", "led_program_ccu_disconnected",
        "led_program_ccu_connected", "led_program_update_available",
        "led_program_error", "led_program_booting",
        "led_program_update_in_progress"};
    static const char *const NAMES[7] = {
        "LED Idle", "LED CCU Disconnected", "LED CCU Connected",
        "LED Update Available", "LED Error", "LED Booting",
        "LED Update In Progress"};
    for (int i = 0; i < 7; i++) {
        publish_select(cfg, serial, device, KEYS[i], NAMES[i], LED_STATE_NAMES, 11,
                       "mdi:lightbulb-outline");
    }

    // --- Text ------------------------------------------------------------
    publish_text(cfg, serial, device, "syslog_server", "Syslog Server", 64,
                 "mdi:server");
    publish_text(cfg, serial, device, "hostname", "Hostname", 63,
                 "mdi:rename-box");
    publish_text(cfg, serial, device, "ccu_ip", "CCU Address", 63,
                 "mdi:ip-network");
    publish_text(cfg, serial, device, "ntp_server", "NTP Server", 64,
                 "mdi:clock-check-outline");
    publish_text(cfg, serial, device, "timezone", "Timezone", 64,
                 "mdi:earth-clock");

    // --- Buttons (diagnostic actions) -------------------------------------
    publish_action_button(cfg, serial, device, "update_check", "Check Update",
                          "mdi:magnify");
    publish_action_button(cfg, serial, device, "reset_relay_stats",
                          "Reset Relay Stats", "mdi:counter");
    publish_action_button(cfg, serial, device, "test_notify", "Test Notification",
                          "mdi:send-outline");

    // --- Update entities (native HA update card with Install) -------------
    publish_update_entity(cfg, serial, device, "firmware_update", "Firmware",
                          "update_install_firmware", "mdi:chip");
    publish_update_entity(cfg, serial, device, "webui_update", "WebUI",
                          "update_install_webui", "mdi:web");
}
