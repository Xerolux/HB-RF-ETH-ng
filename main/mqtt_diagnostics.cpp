/*
 *  mqtt_diagnostics.cpp is part of the HB-RF-ETH firmware v2.0
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

// Stage-2 "Diagnose ohne WebUI" MQTT surface. See mqtt_diagnostics.h.
//
// Design notes:
//  * Everything published here is bounded: the journal snapshot carries at
//    most MQTT_DIAG_JOURNAL_ENTRIES * (~150 B), log and crash tails are
//    capped at 1 KiB each - the same budget the WebUI crash tail uses.
//  * The event-entity payload rides the existing non-retained event topic
//    path (mqtt_handler_publish_event) because HA discards replayed
//    retained messages on event entities anyway.

#include "mqtt_diagnostics.h"

#include <string.h>
#include <stdio.h>
#include <atomic>

#include "cJSON.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "mqtt_handler.h"
#include "log_manager.h"
#include "nvs_storage_lock.h"

static const char *TAG = "MQTTDIAG";

// Bounded payloads: the journal history carries at most this many entries,
// the log/crash tails at most this many bytes (LogManager's crash-tail
// budget; anything larger would double the retained footprint of the
// diagnostics tree for no diagnostic gain).
#define MQTT_DIAG_JOURNAL_ENTRIES 10
#define MQTT_DIAG_TAIL_MAX       1024

// Every event id the journal can record, as HA event-entity types. Kept in
// sync with event_id_name() in event_journal.cpp - the names are the stable
// wire format HA automations trigger on.
static const uint8_t DIAG_EVENT_IDS[] = {
    0, 1, 2, 3, 7, 8, 9, 10, 11, 12, 13, 14, 254,
    EVENT_JOURNAL_WITNESS_DEFERRED, EVENT_JOURNAL_SCHEDULED_RESTART};

// ---------------------------------------------------------------------------
// Shared entity publisher (same layout as the mqtt_commands helpers)
// ---------------------------------------------------------------------------

static void publish_entity(const mqtt_config_t *cfg, const char *serial,
                           const cJSON *device, const char *component,
                           const char *object_id, cJSON *root)
{
    char unique_id[144];
    snprintf(unique_id, sizeof(unique_id), "hb-rf-eth-%s_%s", serial, object_id);
    cJSON_AddStringToObject(root, "unique_id", unique_id);
    cJSON_AddItemToObject(root, "device", cJSON_Duplicate(device, 1));

    char *json = cJSON_PrintUnformatted(root);
    if (json != NULL) {
        char topic[272];
        snprintf(topic, sizeof(topic), "%s/%s/hb-rf-eth-%s/%s/config",
                 cfg->ha_discovery_prefix, component, serial, object_id);
        mqtt_handler_publish_retained(topic, json, 1);
        free(json);
    }
    cJSON_Delete(root);
}

static void publish_plain_sensor(const mqtt_config_t *cfg, const char *serial,
                                 const cJSON *device, const char *object_id,
                                 const char *name, const char *state_class,
                                 const char *unit, const char *icon)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", name);

    char state_topic[192];
    snprintf(state_topic, sizeof(state_topic), "%s/status/%s",
             cfg->topic_prefix, object_id);
    cJSON_AddStringToObject(root, "state_topic", state_topic);
    if (state_class) {
        cJSON_AddStringToObject(root, "state_class", state_class);
    }
    if (unit) {
        cJSON_AddStringToObject(root, "unit_of_measurement", unit);
    }
    cJSON_AddStringToObject(root, "entity_category", "diagnostic");
    if (icon) {
        cJSON_AddStringToObject(root, "icon", icon);
    }
    publish_entity(cfg, serial, device, "sensor", object_id, root);
}

// ---------------------------------------------------------------------------
// State topics
// ---------------------------------------------------------------------------

static void publish_journal_snapshot(const mqtt_config_t *cfg)
{
    cJSON *root = cJSON_CreateObject();
    const size_t count = event_journal_count();
    cJSON_AddNumberToObject(root, "count", (double)count);

    cJSON *events = cJSON_AddArrayToObject(root, "events");
    const size_t limit = count < MQTT_DIAG_JOURNAL_ENTRIES
                             ? count
                             : (size_t)MQTT_DIAG_JOURNAL_ENTRIES;
    for (size_t i = 0; i < limit; i++) {
        event_journal_entry_t entry;
        if (!event_journal_copy(i, &entry)) {
            break;
        }
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "time", (double)entry.epoch);
        cJSON_AddStringToObject(item, "event", event_journal_name(entry.event_id));
        cJSON_AddStringToObject(item, "detail", entry.detail);
        cJSON_AddItemToArray(events, item);
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        return;
    }
    char topic[192];
    snprintf(topic, sizeof(topic), "%s/status/event_journal", cfg->topic_prefix);
    mqtt_handler_publish_retained(topic, json, 0);
    free(json);
}

static void publish_log_tail(const mqtt_config_t *cfg)
{
    // 1 KiB tail buffer: heap, never task stack - this publisher may run on
    // the 2.3 KiB ESP event-loop task via the journal listener chain.
    char *tail = (char *)calloc(1, MQTT_DIAG_TAIL_MAX + 1);
    if (tail == NULL) {
        return;
    }
    size_t bytes = 0;
    const bool enabled = LogManager::instance().isEnabled();
    if (enabled) {
        bytes = LogManager::instance().readTail(tail, MQTT_DIAG_TAIL_MAX);
        tail[bytes < MQTT_DIAG_TAIL_MAX ? bytes : MQTT_DIAG_TAIL_MAX] = '\0';
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "enabled", enabled);
    cJSON_AddNumberToObject(root, "bytes",
                            (double)LogManager::instance().getBufferedBytes());
    cJSON_AddStringToObject(root, "tail", tail);
    free(tail);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        return;
    }
    char topic[192];
    snprintf(topic, sizeof(topic), "%s/status/log_tail", cfg->topic_prefix);
    mqtt_handler_publish_retained(topic, json, 0);
    free(json);
}

static void publish_crash_state(const mqtt_config_t *cfg)
{
    // Non-destructive read: unlike the WebUI endpoint, publishing the crash
    // tail does not consume it. Clearing is the explicit clear_crash_log
    // command, so the tail stays visible in HA until an operator dismisses
    // it.
    const std::string tail = LogManager::loadCrashTailNvs();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "available", !tail.empty());
    cJSON_AddStringToObject(root, "tail",
                            tail.size() > MQTT_DIAG_TAIL_MAX
                                ? tail.substr(0, MQTT_DIAG_TAIL_MAX).c_str()
                                : tail.c_str());

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        return;
    }
    char topic[192];
    snprintf(topic, sizeof(topic), "%s/status/crash_log", cfg->topic_prefix);
    mqtt_handler_publish_retained(topic, json, 0);
    free(json);
}

// Journal snapshot refresh request from the listener. The listener may run
// on any emitting task (often the 2.3 KiB ESP event-loop task), so it only
// publishes the small event payload itself and defers the larger snapshot
// to the mqtt_publish task.
static std::atomic<bool> s_journal_dirty{false};

void mqtt_diagnostics_note_journal_change(void)
{
    s_journal_dirty.store(true, std::memory_order_release);
}

bool mqtt_diagnostics_consume_journal_change(void)
{
    return s_journal_dirty.exchange(false, std::memory_order_acq_rel);
}

void mqtt_diagnostics_publish_journal_snapshot(const mqtt_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }
    publish_journal_snapshot(cfg);
}

void mqtt_diagnostics_publish_states(const mqtt_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }
    publish_journal_snapshot(cfg);
    publish_log_tail(cfg);
    publish_crash_state(cfg);
}

// ---------------------------------------------------------------------------
// Event entity (journal listener)
// ---------------------------------------------------------------------------

void mqtt_diagnostics_on_journal_event(const event_journal_entry_t *entry)
{
    if (entry == NULL) {
        return;
    }
    const mqtt_config_t *cfg = mqtt_handler_current_config();

    cJSON *root = cJSON_CreateObject();
    // HA event entities require the "event_type" key and discard replayed
    // retained payloads - this topic is published non-retained exactly like
    // the other event/* topics. Extra keys (time, detail) surface as event
    // attributes.
    cJSON_AddStringToObject(root, "event_type", event_journal_name(entry->event_id));
    cJSON_AddNumberToObject(root, "time", (double)entry->epoch);
    cJSON_AddStringToObject(root, "detail", entry->detail);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        return;
    }
    mqtt_handler_publish_event("event/device_events", json);
    free(json);

    // Defer the history refresh to the publish task (stack discipline); it
    // lands within the ~5 s status subdivision.
    mqtt_diagnostics_note_journal_change();
    (void)cfg;
}

bool mqtt_diagnostics_clear_crash_log(void)
{
    NvsStorageLock storage_lock(portMAX_DELAY, "mqttdiag.crash_clear");
    if (!storage_lock) {
        ESP_LOGW(TAG, "Could not acquire storage lock for crash-tail erase");
        return false;
    }
    nvs_handle_t handle;
    if (nvs_open("reset_info", NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGW(TAG, "reset_info namespace unavailable for crash-tail erase");
        return false;
    }
    const esp_err_t result = nvs_erase_key(handle, "clog");
    const esp_err_t commit = nvs_commit(handle);
    nvs_close(handle);
    if (result != ESP_OK && result != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Crash-tail erase failed: %s", esp_err_to_name(result));
        return false;
    }
    if (commit != ESP_OK) {
        ESP_LOGW(TAG, "Crash-tail erase commit failed: %s", esp_err_to_name(commit));
        return false;
    }
    ESP_LOGI(TAG, "Crash tail cleared via MQTT command");
    return true;
}

// ---------------------------------------------------------------------------
// Discovery
// ---------------------------------------------------------------------------

void mqtt_diagnostics_publish_discovery(const mqtt_config_t *cfg, const char *serial,
                                        const cJSON *device)
{
    if (cfg == NULL || serial == NULL || device == NULL) {
        return;
    }
    if (!cfg->ha_discovery_enabled) {
        return;
    }

    // --- Plain diagnostic sensors for already-published status topics ----
    // These topics ride the 60 s status cycle; they only lacked discovery.
    publish_plain_sensor(cfg, serial, device, "min_free_heap", "Minimum Free Heap",
                         "measurement", "B", "mdi:memory");
    publish_plain_sensor(cfg, serial, device, "radio_uart_overflows",
                         "Radio UART Overflows", "total_increasing", NULL,
                         "mdi:serial-port");
    publish_plain_sensor(cfg, serial, device, "radio_uart_lost_bytes",
                         "Radio UART Lost Bytes", "total_increasing", "B",
                         "mdi:serial-port");
    publish_plain_sensor(cfg, serial, device, "radio_uart_line_errors",
                         "Radio UART Line Errors", "total_increasing", NULL,
                         "mdi:serial-port");
    publish_plain_sensor(cfg, serial, device, "radio_uart_reset_line_events",
                         "Radio UART Reset Line Events", "total_increasing", NULL,
                         "mdi:restart");
    publish_plain_sensor(cfg, serial, device, "radio_uart_tx_errors",
                         "Radio UART TX Errors", "total_increasing", NULL,
                         "mdi:upload");
    publish_plain_sensor(cfg, serial, device, "radio_uart_rx_backlog_max",
                         "Radio UART RX Backlog (max)", "measurement", NULL,
                         "mdi:download");

    // --- Event entity: live device events as HA triggers ------------------
    cJSON *event = cJSON_CreateObject();
    cJSON_AddStringToObject(event, "name", "Device Events");
    char state_topic[192];
    snprintf(state_topic, sizeof(state_topic), "%s/event/device_events",
             cfg->topic_prefix);
    cJSON_AddStringToObject(event, "state_topic", state_topic);
    cJSON *types = cJSON_AddArrayToObject(event, "event_types");
    for (size_t i = 0; i < sizeof(DIAG_EVENT_IDS) / sizeof(DIAG_EVENT_IDS[0]); i++) {
        cJSON_AddItemToArray(types,
                             cJSON_CreateString(event_journal_name(DIAG_EVENT_IDS[i])));
    }
    cJSON_AddStringToObject(event, "entity_category", "diagnostic");
    cJSON_AddStringToObject(event, "icon", "mdi:bell-ring-outline");
    publish_entity(cfg, serial, device, "event", "device_events", event);

    // --- Journal history sensor (attributes carry the newest entries) -----
    cJSON *journal = cJSON_CreateObject();
    cJSON_AddStringToObject(journal, "name", "Event Journal");
    snprintf(state_topic, sizeof(state_topic), "%s/status/event_journal",
             cfg->topic_prefix);
    cJSON_AddStringToObject(journal, "state_topic", state_topic);
    cJSON_AddStringToObject(journal, "value_template", "{{ value_json.count }}");
    cJSON_AddStringToObject(journal, "json_attributes_topic", state_topic);
    cJSON_AddStringToObject(journal, "entity_category", "diagnostic");
    cJSON_AddStringToObject(journal, "icon", "mdi:history");
    publish_entity(cfg, serial, device, "sensor", "event_journal", journal);

    // --- System log tail sensor --------------------------------------------
    cJSON *log = cJSON_CreateObject();
    cJSON_AddStringToObject(log, "name", "System Log Tail");
    snprintf(state_topic, sizeof(state_topic), "%s/status/log_tail",
             cfg->topic_prefix);
    cJSON_AddStringToObject(log, "state_topic", state_topic);
    cJSON_AddStringToObject(log, "value_template",
                            "{{ value_json.bytes if value_json.enabled else -1 }}");
    cJSON_AddStringToObject(log, "json_attributes_topic", state_topic);
    cJSON_AddStringToObject(log, "entity_category", "diagnostic");
    cJSON_AddStringToObject(log, "icon", "mdi:text-long");
    publish_entity(cfg, serial, device, "sensor", "log_tail", log);

    // --- Crash tail sensor + availability binary sensor --------------------
    cJSON *crash = cJSON_CreateObject();
    cJSON_AddStringToObject(crash, "name", "Crash Tail");
    snprintf(state_topic, sizeof(state_topic), "%s/status/crash_log",
             cfg->topic_prefix);
    cJSON_AddStringToObject(crash, "state_topic", state_topic);
    cJSON_AddStringToObject(crash, "value_template",
                            "{{ 'available' if value_json.available else 'none' }}");
    cJSON_AddStringToObject(crash, "json_attributes_topic", state_topic);
    cJSON_AddStringToObject(crash, "entity_category", "diagnostic");
    cJSON_AddStringToObject(crash, "icon", "mdi:bug");
    publish_entity(cfg, serial, device, "sensor", "crash_log", crash);

    cJSON *crash_avail = cJSON_CreateObject();
    cJSON_AddStringToObject(crash_avail, "name", "Crash Tail Available");
    cJSON_AddStringToObject(crash_avail, "state_topic", state_topic);
    cJSON_AddStringToObject(crash_avail, "value_template",
                            "{{ 'true' if value_json.available else 'false' }}");
    cJSON_AddStringToObject(crash_avail, "payload_on", "true");
    cJSON_AddStringToObject(crash_avail, "payload_off", "false");
    cJSON_AddStringToObject(crash_avail, "entity_category", "diagnostic");
    cJSON_AddStringToObject(crash_avail, "icon", "mdi:bug-check");
    publish_entity(cfg, serial, device, "binary_sensor", "crash_log_available",
                   crash_avail);

    // --- Clear-crash-log button (control surface; commands only) -----------
    if (cfg->command_enabled) {
        cJSON *clear = cJSON_CreateObject();
        cJSON_AddStringToObject(clear, "name", "Clear Crash Tail");
        char command_topic[192];
        snprintf(command_topic, sizeof(command_topic), "%s/command/clear_crash_log",
                 cfg->topic_prefix);
        cJSON_AddStringToObject(clear, "command_topic", command_topic);
        const char *payload =
            cfg->command_token[0] != '\0' ? cfg->command_token : "clear_crash_log";
        cJSON_AddStringToObject(clear, "payload_press", payload);
        cJSON_AddStringToObject(clear, "entity_category", "diagnostic");
        cJSON_AddStringToObject(clear, "icon", "mdi:notification-clear-all");
        publish_entity(cfg, serial, device, "button", "clear_crash_log", clear);
    }
}
