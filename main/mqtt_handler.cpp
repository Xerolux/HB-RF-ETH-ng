/*
 *  mqtt_handler.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "mqtt_handler.h"
#include "monitoring.h"
#include "sysinfo.h"
#include "webui_storage.h"
#include "reset_info.h"
#include "crash_blackbox.h"
#include "system_reset.h"
#include "nvs_storage_lock.h"
#include "events.h"
#include "rawuartudplistener.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_crt_bundle.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "cJSON.h"
#include "lwip/ip4_addr.h"
#include "ethernet.h"
#include "radiomoduledetector.h"
#include "systemclock.h"

#include <string.h>
#include <stdlib.h>
#include <atomic>

static const char *TAG = "MQTT";

static esp_mqtt_client_handle_t client = NULL;
static std::atomic<bool> mqtt_running{false};
// Tracks the broker login state. Set in MQTT_EVENT_CONNECTED and cleared in
// MQTT_EVENT_DISCONNECTED / stop(). Read by prometheus.cpp and events.cpp.
static std::atomic<bool> mqtt_connected{false};
static std::atomic<TaskHandle_t> mqtt_publish_task_handle{NULL};
static std::atomic<TaskHandle_t> mqtt_cleanup_task_handle{NULL};
static std::atomic<bool> mqtt_publish_request{false};
static std::atomic<bool> mqtt_desired_running{false};
static std::atomic<bool> mqtt_tls_gate_held{false};
static std::atomic<uint32_t> mqtt_active_publishers{0};
static std::atomic<bool> mqtt_component_stop_in_progress{false};
static std::atomic<uint32_t> mqtt_component_stop_deadline_ticks{0};
static TimerHandle_t mqtt_stop_watchdog_timer = NULL;
static StaticTimer_t mqtt_stop_watchdog_timer_buffer;
static mqtt_config_t current_mqtt_config;
static mqtt_config_t *mqtt_pending_restart_config = NULL;
static char mqtt_lwt_topic[160];
static std::atomic<bool> mqtt_restart_command_pending{false};

// ESP-MQTT waits for half the configured reconnect interval while it is in
// MQTT_STATE_WAIT_RECONNECT. esp_mqtt_client_stop() does not wake that wait, so
// the public cleanup deadline must include those 15 seconds plus transport,
// publisher-retirement and scheduler margin.
static constexpr int MQTT_RECONNECT_TIMEOUT_MS = 30000;
static constexpr int MQTT_COMPONENT_STOP_WATCHDOG_MS =
    MQTT_RECONNECT_TIMEOUT_MS / 2 + 15000;
static constexpr int MQTT_STOP_WAIT_TIMEOUT_MS =
    MQTT_COMPONENT_STOP_WATCHDOG_MS + 5000;
static constexpr int MQTT_RESTART_RETRY_DELAY_MS = 10000;
static constexpr int MQTT_PUBLISH_DRAIN_TIMEOUT_MS = 5000;
static constexpr int MQTT_TLS_GATE_WAIT_SLICE_MS = 1000;
static constexpr int MQTT_TLS_GATE_MAX_WAIT_MS = 10 * 60 * 1000;

static_assert(std::atomic<uint32_t>::is_always_lock_free,
              "MQTT publisher lifetime guard must be native 32-bit");

// Protects an entire logical publish operation, including every read from
// current_mqtt_config and all payload/topic preparation. Cleanup closes
// mqtt_running before waiting for this lease count to reach zero, so a queued
// restart cannot overwrite the non-atomic configuration halfway through a
// status/discovery batch. mqtt_publish_connected() keeps its narrower nested
// lease as a defence for any future direct call site.
class MqttPublishOperation {
public:
    MqttPublishOperation()
    {
        mqtt_active_publishers.fetch_add(1, std::memory_order_seq_cst);
        admitted_ = mqtt_running.load(std::memory_order_seq_cst) &&
                    mqtt_connected.load(std::memory_order_acquire);
    }

    ~MqttPublishOperation()
    {
        mqtt_active_publishers.fetch_sub(1, std::memory_order_seq_cst);
    }

    MqttPublishOperation(const MqttPublishOperation &) = delete;
    MqttPublishOperation &operator=(const MqttPublishOperation &) = delete;

    explicit operator bool() const { return admitted_; }

private:
    bool admitted_ = false;
};

// "Running" only means the ESP-MQTT client and publisher task exist. The
// broker may still be connecting or reconnecting. Submitting QoS 0 packets in
// that state makes ESP-MQTT discard every packet and emit one warning per
// status topic. Keep the broker-login check in one place so no publish path
// can accidentally use the weaker lifecycle state.
static bool mqtt_can_publish()
{
    return mqtt_running.load(std::memory_order_acquire) &&
           mqtt_connected.load(std::memory_order_acquire);
}

static int mqtt_publish_connected(const char *topic, const char *data,
                                  int len, int qos, int retain)
{
    if (topic == NULL || data == NULL) {
        return -1;
    }

    // Close admission before client destruction and count every caller which
    // may cross into ESP-MQTT. Sequential consistency pairs with cleanup's
    // mqtt_running=false store: either cleanup observes this reader, or this
    // reader observes the closed lifecycle and never dereferences client.
    mqtt_active_publishers.fetch_add(1, std::memory_order_seq_cst);
    int result = -1;
    if (mqtt_running.load(std::memory_order_seq_cst) &&
        mqtt_connected.load(std::memory_order_acquire)) {
        esp_mqtt_client_handle_t publish_client = client;
        if (publish_client != NULL) {
            result = esp_mqtt_client_publish(
                publish_client, topic, data, len, qos, retain);
        }
    }
    mqtt_active_publishers.fetch_sub(1, std::memory_order_seq_cst);
    return result;
}

// Serializes mqtt_handler_start / mqtt_handler_stop so configuration updates
// cannot race with the publish task or a concurrent (re)start.
static SemaphoreHandle_t mqtt_lifecycle_mutex = NULL;
static StaticSemaphore_t mqtt_lifecycle_mutex_buffer;

// Latch set by mqtt_handler_trigger_status_publish() so the periodic task
// emits an immediate cycle out-of-band after an explicit status change.

// Forward declarations
extern SysInfo* monitoring_get_sysinfo(void);
extern Ethernet* monitoring_get_ethernet(void);
extern RadioModuleDetector* monitoring_get_radiomodule(void);
extern SystemClock* monitoring_get_systemclock(void);

void mqtt_handler_publish_ha_discovery(void);
static void publish_legacy_topic_cleanup(void);

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

// Validate the command payload against the optional shared-secret token.
//
// Semantics:
//   * If command_token is empty -> always accept (broker ACL must protect).
//   * Otherwise the payload (trimmed) must equal the configured token.
//
// HA integration: when a token is set, the HA discovery config publishes the
// token as payload_press, so the restart button "just works" in HA.
// This means the HA discovery topic contains the token in clear-text - lock
// down broker ACLs so only the device may publish to <ha_prefix>/#.
static bool command_token_ok(const char *payload, int payload_len)
{
    if (current_mqtt_config.command_token[0] == '\0') {
        return true;
    }
    if (payload == NULL || payload_len <= 0) {
        return false;
    }
    // Compare with length limit
    size_t expected = strlen(current_mqtt_config.command_token);
    if ((size_t)payload_len != expected) {
        return false;
    }
    return strncmp(payload, current_mqtt_config.command_token, expected) == 0;
}

static void mqtt_restart_command_task(void *)
{
    vTaskDelay(pdMS_TO_TICKS(300));
    // This task is intentionally separate from the ESP-MQTT event task. The
    // common restart path stops MQTT cooperatively and therefore must not be
    // invoked by the very task esp_mqtt_client_stop() waits to terminate.
    full_system_restart_with_reserved_operation();
    mqtt_restart_command_pending.store(false, std::memory_order_release);
    vTaskDelete(NULL);
}

static void handle_mqtt_command(const char* command, const char* payload, int payload_len)
{
    ESP_LOGI(TAG, "Received MQTT command: %s (payload %d bytes)", command, payload_len);

    if (!command_token_ok(payload, payload_len)) {
        ESP_LOGW(TAG, "Command %s rejected: missing/invalid token", command);
        char msg[96];
        snprintf(msg, sizeof(msg), "rejected cmd=%.48s reason=invalid_token", command);
        mqtt_handler_publish_event("event/command_rejected", msg);
        return;
    }

    if (strcmp(command, "restart") == 0) {
        // Reserve the same operation gate used by the manual upload handler.
        // This prevents an MQTT command from resetting the chip while the
        // httpd task owns an active esp_ota_handle/flash write.
        if (!ota_operation_try_begin()) {
            ESP_LOGW(TAG, "Restart rejected while firmware upload/config update is active");
            mqtt_handler_publish_event("event/command_rejected",
                                       "reason=operation_busy");
            return;
        }
        ESP_LOGI(TAG, "Restart command received via MQTT");
        ResetInfo::storeResetReason(RESET_REASON_USER_RESTART);
        mqtt_handler_publish_event("event/restart", "requested");
        bool expected = false;
        if (mqtt_restart_command_pending.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel)) {
            if (xTaskCreate(mqtt_restart_command_task, "mqtt_restart", 4096,
                            NULL, 5, NULL) != pdPASS) {
                mqtt_restart_command_pending.store(false,
                                                   std::memory_order_release);
                ESP_LOGE(TAG, "Could not create safe MQTT restart task");
                mqtt_handler_publish_event("event/restart", "task_create_failed");
                ota_operation_finish();
            }
        } else {
            ota_operation_finish();
        }
    } else {
        ESP_LOGW(TAG, "Unknown MQTT command: %s", command);
        mqtt_handler_publish_event("event/command_rejected", "reason=unknown_command");
    }
}

// g_net_fetch_mutex is deliberately an ownershipless binary semaphore. The
// MQTT task takes it for each TLS connection/reconnection, while a cleanup
// task may release it if ESP-MQTT exits without dispatching a terminal event.
static void mqtt_take_tls_gate_if_needed()
{
    if (!current_mqtt_config.tls_enable || g_net_fetch_mutex == NULL ||
        mqtt_tls_gate_held.load(std::memory_order_acquire)) {
        return;
    }
    // Never continue a TLS handshake without the serialization gate.  A
    // manual firmware upload can legitimately own it for longer than one
    // network timeout while receiving and flashing the image.  Blocking this
    // MQTT library task is scheduler-friendly; entering "degraded" here would
    // overlap the two largest heap consumers and recreate the WROOM-32 crash
    // condition. Bounded chunks retain diagnostics and prevent a lost
    // semaphore from leaving MQTT offline forever. Do not return merely
    // because stop was requested: ESP-MQTT proceeds directly into
    // esp_transport_connect() after this callback, which would otherwise run
    // a TLS handshake without the serialization gate while cleanup waits for
    // the component API lock.
    uint32_t waited_ms = 0;
    while (xSemaphoreTake(g_net_fetch_mutex,
                          pdMS_TO_TICKS(MQTT_TLS_GATE_WAIT_SLICE_MS)) != pdTRUE) {
        waited_ms += MQTT_TLS_GATE_WAIT_SLICE_MS;
        if (waited_ms == MQTT_TLS_GATE_WAIT_SLICE_MS ||
            waited_ms % 30000 == 0) {
            ESP_LOGW(TAG, "%s; MQTT TLS connection remains deferred",
                     net_fetch_ota_active()
                         ? "Firmware upload owns TLS gate"
                         : "TLS serialization gate is busy");
        }
        if (waited_ms >= MQTT_TLS_GATE_MAX_WAIT_MS) {
            ESP_LOGE(TAG, "MQTT TLS gate wedged for %u ms; rebooting safely",
                     (unsigned)waited_ms);
            ResetInfo::storeResetReason(RESET_REASON_SYSTEM_ERROR,
                                        "MQTT TLS serialization gate wedged");
            emergency_network_stop_before_reboot();
            esp_restart();
            return;
        }
    }
    mqtt_tls_gate_held.store(true, std::memory_order_release);
    crash_blackbox_net_op_begin("mqtt_tls");
}

static void mqtt_release_tls_gate_if_held()
{
    if (mqtt_tls_gate_held.exchange(false, std::memory_order_acq_rel) &&
        g_net_fetch_mutex != NULL) {
        crash_blackbox_net_op_end();
        xSemaphoreGive(g_net_fetch_mutex);
    }
}

static void mqtt_stop_watchdog_callback(TimerHandle_t)
{
    if (!mqtt_component_stop_in_progress.load(std::memory_order_acquire)) {
        return;
    }

    const uint32_t now = static_cast<uint32_t>(xTaskGetTickCount());
    const uint32_t deadline = mqtt_component_stop_deadline_ticks.load(
        std::memory_order_acquire);
    // Signed subtraction is wrap-safe for deadlines less than half the
    // 32-bit tick range away (this deadline is only 30 seconds).
    if (static_cast<int32_t>(now - deadline) < 0) {
        return;
    }

    mqtt_connected.store(false, std::memory_order_release);
    mqtt_running.store(false, std::memory_order_seq_cst);
    // Keep the TLS gate closed through reset diagnostics. Releasing it here
    // could start another large TLS allocation while the wedged MQTT task is
    // still inside its handshake.
    ESP_LOGE(TAG, "MQTT component cleanup wedged for %d ms; rebooting safely",
             MQTT_COMPONENT_STOP_WATCHDOG_MS);
    ResetInfo::storeResetReason(RESET_REASON_SYSTEM_ERROR,
                                "MQTT component stop watchdog");
    emergency_network_stop_before_reboot();
    esp_restart();
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_BEFORE_CONNECT:
        mqtt_take_tls_gate_if_needed();
        break;
    case MQTT_EVENT_CONNECTED:
        mqtt_release_tls_gate_if_held();
        if (!mqtt_running.load(std::memory_order_acquire)) {
            ESP_LOGD(TAG, "Ignoring MQTT_EVENT_CONNECTED during cleanup");
            mqtt_connected.store(false, std::memory_order_release);
            break;
        }
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        mqtt_connected.store(true, std::memory_order_release);
        // Close the event-vs-stop interleaving where cleanup clears the flag
        // between the running check above and this store.
        if (!mqtt_running.load(std::memory_order_acquire)) {
            mqtt_connected.store(false, std::memory_order_release);
            break;
        }
        // Notify subscribers. Suppressed by the cooldown window if this is a
        // rapid reconnect flap.
        events_emit(EVENT_MQTT_RECONNECTED, nullptr);
        // Birth message: announce we are online (retained so subscribers see
        // it immediately, even before the next status cycle). The matching
        // LWT below flips this to "offline" if the connection drops
        // unexpectedly (power loss, network outage, crash).
        if (current_mqtt_config.command_enabled || current_mqtt_config.ha_discovery_enabled) {
            // Subscribe to command topic whenever commands OR HA discovery
            // are enabled. Previously this was gated on ha_discovery only,
            // which blocked plain-MQTT users from triggering restart.
            char command_topic[128];
            snprintf(command_topic, sizeof(command_topic), "%s/command/#", current_mqtt_config.topic_prefix);
            esp_mqtt_client_subscribe(event->client, command_topic, 1);
            ESP_LOGI(TAG, "Subscribed to command topic: %s", command_topic);
        }
        // Clear legacy retained status/* topics the firmware no longer
        // publishes to. Must happen BEFORE the first publish_status so
        // subscribers don't briefly see a stale -127 temperature again
        // after we've just announced we're online.
        publish_legacy_topic_cleanup();
        // Publish initial status (includes online marker)
        mqtt_handler_publish_status();
        // Publish HA discovery config if enabled
        if (current_mqtt_config.ha_discovery_enabled) {
            mqtt_handler_publish_ha_discovery();
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        mqtt_release_tls_gate_if_held();
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        mqtt_connected.store(false);
        events_emit(EVENT_MQTT_DISCONNECTED, nullptr);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        if (current_mqtt_config.command_enabled) {
            // Note: event->topic is NOT null-terminated per ESP-IDF MQTT API.
            char command_topic_prefix[128];
            snprintf(command_topic_prefix, sizeof(command_topic_prefix), "%s/command/", current_mqtt_config.topic_prefix);
            size_t prefix_len = strlen(command_topic_prefix);

            if ((size_t)event->topic_len > prefix_len &&
                strncmp(event->topic, command_topic_prefix, prefix_len) == 0) {
                char command[64];
                size_t cmd_len = (size_t)event->topic_len - prefix_len;
                if (cmd_len >= sizeof(command)) cmd_len = sizeof(command) - 1;
                memcpy(command, event->topic + prefix_len, cmd_len);
                command[cmd_len] = '\0';
                handle_mqtt_command(command, event->data, event->data_len);
            }
        }
        break;
    case MQTT_EVENT_ERROR:
        mqtt_release_tls_gate_if_held();
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
    int publish_cycle = 0;

    while (mqtt_running.load()) {
        // esp_mqtt_client_start() returns before the asynchronous broker login
        // completes. During startup or reconnect, wait without formatting or
        // submitting a complete status batch. MQTT_EVENT_CONNECTED publishes
        // the initial retained status and HA discovery immediately.
        if (!mqtt_can_publish()) {
            (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(250));
            continue;
        }

        if (publish_cycle == 0) {
            ESP_LOGI(TAG, "mqtt_publish stack high water mark: %u bytes free",
                     (unsigned)uxTaskGetStackHighWaterMark(NULL));
        }
        publish_cycle++;
        mqtt_handler_publish_status();

        // Task-stack diagnostics move slowly; publishing every cycle
        // wastes broker storage for no insight. With the 60 s status cadence
        // below, every-6th-cycle ≈ 6 min — plenty to spot slow leaks or
        // post-update drift when the user files a bug.
        if (publish_cycle % 6 == 0) {
            mqtt_handler_publish_task_stacks();
        }

        // 60 s base cadence (was 10 s): each cycle publishes ~30 retained
        // status topics, so at 10 s that was ~11 000 esp_mqtt_client_publish
        // calls per hour — each doing small internal mallocs that slowly
        // fragment the WROOM-32 heap over hours/days, the prime suspect for
        // the long-uptime Interrupt-Watchdog crashes (issue #362). Raising the
        // cadence to 60 s cuts that churn to ~1 800/h (−84 %) without losing
        // useful monitoring resolution — the values barely change within a
        // minute. The 12-step subdivision keeps trigger_publish response at
        // ~5 s so explicitly requested status changes still publish promptly.
        for (int i = 0; i < 12 && mqtt_running.load(); i++) {
            if (mqtt_publish_request.exchange(false)) {
                break;  // run a fresh publish cycle immediately
            }
            (void)ulTaskNotifyTake(pdTRUE,
                                   pdMS_TO_TICKS(60000 / 12));
        }
    }
    // Coordinate handle retirement with every notifier. Without the mutex a
    // caller could load the last handle exactly while this task self-deletes.
    if (mqtt_lifecycle_mutex != NULL &&
        xSemaphoreTake(mqtt_lifecycle_mutex, portMAX_DELAY) == pdTRUE) {
        mqtt_publish_task_handle.store(NULL, std::memory_order_release);
        xSemaphoreGive(mqtt_lifecycle_mutex);
    } else {
        mqtt_publish_task_handle.store(NULL, std::memory_order_release);
    }
    vTaskDelete(NULL);
}

void mqtt_handler_trigger_status_publish(void)
{
    if (mqtt_lifecycle_mutex == NULL ||
        xSemaphoreTake(mqtt_lifecycle_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        // The request flag is level-triggered; the next periodic cycle will
        // still consume it even if lifecycle reconfiguration owns the lock.
        mqtt_publish_request.store(true, std::memory_order_release);
        return;
    }
    if (mqtt_running.load(std::memory_order_acquire)) {
        mqtt_publish_request.store(true, std::memory_order_release);
        TaskHandle_t task =
            mqtt_publish_task_handle.load(std::memory_order_acquire);
        if (task) xTaskNotifyGive(task);
    }
    xSemaphoreGive(mqtt_lifecycle_mutex);
}

void mqtt_handler_publish_event(const char *subtopic, const char *payload)
{
    if (!subtopic || !payload) {
        return;
    }
    MqttPublishOperation operation;
    if (!operation) {
        return;
    }
    char topic[160];
    snprintf(topic, sizeof(topic), "%s/%s", current_mqtt_config.topic_prefix, subtopic);
    // Non-retained, QoS 0: events are transient by definition.
    mqtt_publish_connected(topic, payload, 0, 0, 0);
}

// Publish the FreeRTOS task stack high-water marks as a single retained string.
// Called infrequently (every 60 s) because the values move slowly and the
// payload can be several hundred bytes — publishing it on the 5 s status cycle
// would waste bandwidth and MQTT broker storage for no diagnostic gain.
void mqtt_handler_publish_task_stacks(void)
{
    MqttPublishOperation operation;
    if (!operation) {
        return;
    }
    SysInfo *sysInfo = monitoring_get_sysinfo();
    if (!sysInfo) return;

    std::string stacks = sysInfo->getTaskStackInfo();
    if (stacks.empty()) return;

    char topic[160];
    snprintf(topic, sizeof(topic), "%s/status/task_stacks",
             current_mqtt_config.topic_prefix);
    mqtt_publish_connected(topic, stacks.c_str(), 0, 0, 1);
}

// Publish one value under <prefix>/<subtopic> with retain=1, QoS=0.
// Defined as a macro so the compiler can inline the snprintf chains.
#define PUBLISH_STR(subtopic, value) \
    do { \
        snprintf(topic, sizeof(topic), "%s/%s", current_mqtt_config.topic_prefix, subtopic); \
        mqtt_publish_connected(topic, value, 0, 0, 1); \
    } while (0)

#define PUBLISH_INT(subtopic, value) \
    do { \
        snprintf(payload, sizeof(payload), "%d", (int)(value)); \
        PUBLISH_STR(subtopic, payload); \
    } while (0)

#define PUBLISH_UINT64(subtopic, value) \
    do { \
        snprintf(payload, sizeof(payload), "%llu", (unsigned long long)(value)); \
        PUBLISH_STR(subtopic, payload); \
    } while (0)

#define PUBLISH_DOUBLE(subtopic, value, prec) \
    do { \
        snprintf(payload, sizeof(payload), "%.*f", prec, value); \
        PUBLISH_STR(subtopic, payload); \
    } while (0)

void mqtt_handler_publish_status(void)
{
    MqttPublishOperation operation;
    if (!operation) {
        return;
    }

    SysInfo* sysInfo = monitoring_get_sysinfo();
    Ethernet* eth = monitoring_get_ethernet();
    RadioModuleDetector* radio = monitoring_get_radiomodule();
    SystemClock* clk = monitoring_get_systemclock();

    if (sysInfo == NULL) {
        ESP_LOGW(TAG, "SysInfo not available");
        return;
    }

    char topic[160];
    char payload[96];

    // Birth/online marker. The matching LWT (set in mqtt_handler_start)
    // overwrites this with "offline" if the connection drops uncleanly.
    PUBLISH_STR("status/online", "online");

    // ---- Identity ---------------------------------------------------------
    PUBLISH_STR("status/serial", sysInfo->getSerialNumber());
    PUBLISH_STR("status/firmware_version", sysInfo->getCurrentVersion());
    char webuiVersion[32] = {};
    webui_storage_get_effective_version(webuiVersion, sizeof(webuiVersion));
    PUBLISH_STR("status/webui_version", webuiVersion);
    PUBLISH_STR("status/board_revision", sysInfo->getBoardRevisionString().c_str());

    // ---- System metrics ---------------------------------------------------
    PUBLISH_DOUBLE("status/cpu_usage", sysInfo->getCpuUsage(), 1);
    PUBLISH_DOUBLE("status/memory_usage", sysInfo->getMemoryUsage(), 1);
    PUBLISH_UINT64("status/uptime", sysInfo->getUptimeSeconds());

    // Uptime formatted
    {
        uint64_t uptime_s = sysInfo->getUptimeSeconds();
        uint32_t days  = (uint32_t)(uptime_s / 86400); uptime_s %= 86400;
        uint32_t hours = (uint32_t)(uptime_s / 3600);  uptime_s %= 3600;
        uint32_t mins  = (uint32_t)(uptime_s / 60);
        snprintf(payload, sizeof(payload), "%lu d, %lu h, %lu m",
                 (unsigned long)days, (unsigned long)hours, (unsigned long)mins);
        PUBLISH_STR("status/uptime_text", payload);
    }

    // Heap details - useful for memory leak monitoring in HA graphs.
    {
        multi_heap_info_t info;
        heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);
        PUBLISH_UINT64("status/free_heap", info.total_free_bytes);
        PUBLISH_UINT64("status/min_free_heap", esp_get_minimum_free_heap_size());
    }

    // ---- CCU relay latency -------------------------------------------------
    // The numbers users actually need when switching commands arrive late
    // (issues #411 / #362): whether a datagram sat in our receive queue, and
    // how often. Published in milliseconds because a Home Assistant graph in
    // microseconds is unreadable, and because sub-millisecond precision does
    // not matter when the reported symptom is measured in tens of seconds.
    {
        raw_uart_latency_t latency = {};
        raw_uart_get_latency(&latency);
        PUBLISH_UINT64("status/ccu_queue_wait_max_ms", latency.queue_wait_max_us / 1000);
        PUBLISH_UINT64("status/ccu_queue_depth_max", latency.queue_depth_max);
        PUBLISH_UINT64("status/ccu_delayed_frames",
                       latency.wait_over_10ms + latency.wait_over_100ms + latency.wait_over_1s);
        PUBLISH_UINT64("status/ccu_dropped_frames", latency.drops);
    }

    // Reset reason (combines app-level stored reason + ESP hardware reason).
    if (sysInfo->getResetReason()) {
        PUBLISH_STR("status/last_reset_reason", sysInfo->getResetReason());
    }

    // ---- Ethernet ---------------------------------------------------------
    if (eth) {
        PUBLISH_STR("status/eth_connected", eth->isConnected() ? "true" : "false");
        PUBLISH_INT("status/eth_link_speed", eth->getLinkSpeedMbps());
        if (eth->getDuplexMode()) {
            PUBLISH_STR("status/eth_duplex", eth->getDuplexMode());
        }
        ip4_addr_t ip, nm, gw, dns1, dns2;
        eth->getNetworkSettings(&ip, &nm, &gw, &dns1, &dns2);
        snprintf(payload, sizeof(payload), IPSTR, IP2STR(&ip));
        PUBLISH_STR("status/ip_address", payload);
        snprintf(payload, sizeof(payload), IPSTR, IP2STR(&nm));
        PUBLISH_STR("status/netmask", payload);
        snprintf(payload, sizeof(payload), IPSTR, IP2STR(&gw));
        PUBLISH_STR("status/gateway", payload);
        snprintf(payload, sizeof(payload), IPSTR, IP2STR(&dns1));
        PUBLISH_STR("status/dns1", payload);
        snprintf(payload, sizeof(payload), IPSTR, IP2STR(&dns2));
        PUBLISH_STR("status/dns2", payload);

        // IPv6 addresses (comma-separated; empty string when none assigned)
        char ipv6_addrs[4][48];
        int ipv6_count = eth->getIPv6AddressStrings(ipv6_addrs, 4);
        if (ipv6_count > 0) {
            char ipv6_buf[200];
            size_t off = 0;
            for (int i = 0; i < ipv6_count; i++) {
                if (i > 0 && off < sizeof(ipv6_buf) - 1) {
                    ipv6_buf[off++] = ',';
                }
                off += snprintf(ipv6_buf + off, sizeof(ipv6_buf) - off,
                                "%s", ipv6_addrs[i]);
            }
            PUBLISH_STR("status/ipv6_addresses", ipv6_buf);
        } else {
            PUBLISH_STR("status/ipv6_addresses", "");
        }
    }

    // ---- Radio module -----------------------------------------------------
    if (radio) {
        const char* type = "none";
        switch (radio->getRadioModuleType()) {
            case RADIO_MODULE_HM_MOD_RPI_PCB: type = "HM-MOD-RPI-PCB"; break;
            case RADIO_MODULE_RPI_RF_MOD:     type = "RPI-RF-MOD";     break;
            case RADIO_MODULE_HMIP_RFUSB:     type = "HmIP-RFUSB";     break;
            case RADIO_MODULE_NONE:           type = "none";           break;
            default:                          type = "unknown";        break;
        }
        PUBLISH_STR("status/radio_module_type", type);
        if (radio->getSerial()) {
            PUBLISH_STR("status/radio_module_serial", radio->getSerial());
        }
        const uint8_t* fw = radio->getFirmwareVersion();
        if (fw && (fw[0] || fw[1] || fw[2])) {
            snprintf(payload, sizeof(payload), "%d.%d.%d", fw[0], fw[1], fw[2]);
            PUBLISH_STR("status/radio_module_firmware", payload);
        }
    }

    // ---- System clock / NTP sync state -----------------------------------
    if (clk) {
        struct timeval sync = clk->getLastSyncTime();
        bool synced = (sync.tv_sec > 0);
        PUBLISH_STR("status/ntp_synced", synced ? "true" : "false");
        if (synced) {
            PUBLISH_UINT64("status/last_ntp_sync", (unsigned long long)sync.tv_sec);
        } else {
            PUBLISH_STR("status/last_ntp_sync", "0");
        }
    }
}

#undef PUBLISH_STR
#undef PUBLISH_INT
#undef PUBLISH_UINT64
#undef PUBLISH_DOUBLE

// One-shot broker cleanup of status/* topics the firmware no longer
// publishes to but used to emit with retain=1 in earlier versions
// (commit b13b484, "fix: stabilize OTA and clean firmware diagnostics").
//
// The ESP32 classic has no internal temperature sensor, so the legacy
// SysInfo::getTemperature() always returned -127 (the standard
// "no sensor" sentinel) and that value got retained on the broker.
// After the upgrade the firmware simply stops publishing to the topic,
// leaving stale retained payloads that subscribers like ioBroker keep
// displaying forever — see issue #396.
//
// We also clear supply_voltage (same removal commit) and the short-named
// version / latest_version topics that were renamed to firmware_version /
// latest_firmware_version. Each is an empty retained publish that the
// broker treats as "delete this retained value", mirroring the existing
// remove_config() pattern for HA discovery topics. Idempotent and cheap,
// so running it on every MQTT_EVENT_CONNECTED is safer than tracking
// per-boot state across reconnects.
static void publish_legacy_topic_cleanup(void)
{
    MqttPublishOperation operation;
    if (!operation) return;

    // Versioned one-shot migration gate (issue #404). The empty retained
    // payloads this function emits are the MQTT-standard way to DELETE a
    // retained value, but ioBroker's MQTT adapter is non-conformant: it
    // keeps/creates a datapoint with a null value instead of removing the
    // topic. For users who never had these legacy topics, running the cleanup
    // on every connect therefore creates the phantom "(null)" data points
    // reported in #404.
    //
    // A version counter (rather than a single boolean) is used so that adding
    // newly-retired topics — e.g. the update-check/OTA topics removed together
    // with the automatic update-check feature — re-runs the cleanup exactly
    // once on devices that already performed an older run. Bump CLEANUP_VERSION
    // whenever legacy_subtopics grows. This uses its own namespace so a normal
    // Settings generation rewrite cannot discard the completed migration.
    static const char *const CLEANUP_NS = "mqtt_cleanup";
    static const char *const CLEANUP_KEY = "mqttLgcyVer"; // 11 chars (NVS limit 15)
    static const uint8_t CLEANUP_VERSION = 2;
    bool needs_cleanup = false;
    {
        NvsStorageLock storage_lock(portMAX_DELAY, "mqtt.legacy_marker_read");
        if (!storage_lock) return;
        nvs_handle_t h;
        esp_err_t marker_result = nvs_open(CLEANUP_NS, NVS_READWRITE, &h);
        if (marker_result != ESP_OK) {
            ESP_LOGW(TAG, "Could not read legacy-topic cleanup marker: %s",
                     esp_err_to_name(marker_result));
            return;
        }
        uint8_t stored = 0;
        marker_result = nvs_get_u8(h, CLEANUP_KEY, &stored);
        nvs_close(h);
        if (marker_result == ESP_OK) {
            needs_cleanup = stored < CLEANUP_VERSION;
        } else if (marker_result == ESP_ERR_NVS_NOT_FOUND) {
            // The old HB-RF-ETH marker is intentionally ignored. One cleanup
            // on upgrade is safe and establishes the dedicated marker.
            needs_cleanup = true;
        } else {
            ESP_LOGW(TAG, "Invalid legacy-topic cleanup marker: %s",
                     esp_err_to_name(marker_result));
            return;
        }
    }
    if (!needs_cleanup) {
        return;
    }

    static const char *const legacy_subtopics[] = {
        "status/temperature",
        "status/supply_voltage",
        "status/version",
        "status/latest_version",
        // Topics retired together with the automatic update-check feature.
        "status/update_available",
        "status/latest_firmware_version",
        "status/latest_webui_version",
        "status/firmware_update_available",
        "status/webui_update_available",
        "status/ota_state",
        "status/ota_progress",
        "status/ota_error",
    };

    char topic[160];
    bool cleanup_succeeded = true;
    for (size_t i = 0; i < sizeof(legacy_subtopics) / sizeof(legacy_subtopics[0]); i++) {
        snprintf(topic, sizeof(topic), "%s/%s",
                 current_mqtt_config.topic_prefix, legacy_subtopics[i]);
        // qos=0, retain=1: an empty retained payload is the MQTT-standard
        // way to delete a retained value from the broker.
        if (mqtt_publish_connected(topic, "", 0, 0, 1) < 0) {
            cleanup_succeeded = false;
        }
    }
    if (!cleanup_succeeded) {
        ESP_LOGW(TAG,
                 "Legacy retained-topic cleanup was incomplete; marker not advanced");
        return;
    }

    // Mark this cleanup version complete so it never runs again until the
    // version is bumped again.
    {
        NvsStorageLock storage_lock(portMAX_DELAY, "mqtt.legacy_marker_write");
        if (!storage_lock) {
            ESP_LOGW(TAG, "Could not persist legacy-topic cleanup marker");
            return;
        }
        nvs_handle_t h;
        esp_err_t marker_result = nvs_open(CLEANUP_NS, NVS_READWRITE, &h);
        if (marker_result == ESP_OK) {
            marker_result = nvs_set_u8(h, CLEANUP_KEY, CLEANUP_VERSION);
            if (marker_result == ESP_OK) marker_result = nvs_commit(h);
            nvs_close(h);
        }
        if (marker_result == ESP_OK) {
            ESP_LOGI(TAG, "Legacy retained-topic cleanup performed (v%u); will not repeat",
                     (unsigned)CLEANUP_VERSION);
        } else {
            ESP_LOGW(TAG, "Could not persist legacy-topic cleanup marker: %s",
                     esp_err_to_name(marker_result));
        }
    }
}

void mqtt_handler_publish_ha_discovery(void)
{
    MqttPublishOperation operation;
    if (!operation || !current_mqtt_config.ha_discovery_enabled) {
        return;
    }

    SysInfo* sysInfo = monitoring_get_sysinfo();
    if (sysInfo == NULL) {
        return;
    }

    ESP_LOGI(TAG, "Publishing Home Assistant discovery configs");

    // The token callers must send as payload_press so the HA restart button
    // works even when a command_token is configured. Empty token -> plain
    // "restart" (legacy behaviour).
    const char* restart_payload  = current_mqtt_config.command_token[0] ? current_mqtt_config.command_token : "restart";
    // Device Info — use the configurable hostname as the HA device name so
    // multiple HB-RF-ETH boards can be told apart in the UI. Fall back to a
    // generic label if the hostname is unavailable.
    Ethernet* eth = monitoring_get_ethernet();
    const char* hostname = eth ? eth->getHostname() : "";
    if (!hostname || !hostname[0]) {
        hostname = "HB-RF-ETH";
    }

    cJSON *device = cJSON_CreateObject();
    char identifiers[64];
    snprintf(identifiers, sizeof(identifiers), "hb-rf-eth-%s", sysInfo->getSerialNumber());
    cJSON_AddStringToObject(device, "identifiers", identifiers);
    cJSON_AddStringToObject(device, "name", hostname);
    cJSON_AddStringToObject(device, "model", "HB-RF-ETH-ng");
    cJSON_AddStringToObject(device, "manufacturer", "Xerolux");
    cJSON_AddStringToObject(device, "sw_version", sysInfo->getCurrentVersion());
    cJSON_AddStringToObject(device, "hw_version", sysInfo->getBoardRevisionString().c_str());

    // Helper: publish a sensor / binary_sensor / button / update config.
    auto publish_config = [&](const char* component, const char* object_id, const char* name,
                              const char* device_class, const char* state_class,
                              const char* unit_of_measurement, const char* value_template,
                              const char* entity_category = NULL, const char* icon = NULL,
                              const char* payload_on = NULL, const char* payload_off = NULL) {

        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "name", name);

        char unique_id[128];
        snprintf(unique_id, sizeof(unique_id), "%s_%s", identifiers, object_id);
        cJSON_AddStringToObject(root, "unique_id", unique_id);

        char state_topic[160];
        snprintf(state_topic, sizeof(state_topic), "%s/status/%s", current_mqtt_config.topic_prefix, object_id);
        cJSON_AddStringToObject(root, "state_topic", state_topic);

        if (device_class) cJSON_AddStringToObject(root, "device_class", device_class);
        if (state_class) cJSON_AddStringToObject(root, "state_class", state_class);
        if (unit_of_measurement) cJSON_AddStringToObject(root, "unit_of_measurement", unit_of_measurement);
        if (value_template) cJSON_AddStringToObject(root, "value_template", value_template);
        if (payload_on) cJSON_AddStringToObject(root, "payload_on", payload_on);
        if (payload_off) cJSON_AddStringToObject(root, "payload_off", payload_off);
        if (entity_category) cJSON_AddStringToObject(root, "entity_category", entity_category);
        if (icon) cJSON_AddStringToObject(root, "icon", icon);

        cJSON_AddItemToObject(root, "device", cJSON_Duplicate(device, 1));

        char *json_str = cJSON_PrintUnformatted(root);
        char topic[256];
        snprintf(topic, sizeof(topic), "%s/%s/hb-rf-eth-%s/%s/config",
                 current_mqtt_config.ha_discovery_prefix, component, sysInfo->getSerialNumber(), object_id);
        mqtt_publish_connected(topic, json_str, 0, 1, 1);
        free(json_str);
        cJSON_Delete(root);
    };

    // Remove retained discovery entries created by older firmware versions.
    // These sensors were never backed by hardware on HB-RF-ETH boards.
    auto remove_config = [&](const char* component, const char* object_id) {
        char topic[256];
        snprintf(topic, sizeof(topic), "%s/%s/hb-rf-eth-%s/%s/config",
                 current_mqtt_config.ha_discovery_prefix, component,
                 sysInfo->getSerialNumber(), object_id);
        mqtt_publish_connected(topic, "", 0, 1, 1);
    };

    // ---- Sensors: system metrics ----------------------------------------
    publish_config("sensor", "cpu_usage", "CPU Usage", NULL, "measurement", "%", NULL, "diagnostic", "mdi:cpu-64-bit");
    publish_config("sensor", "memory_usage", "Memory Usage", NULL, "measurement", "%", NULL, "diagnostic", "mdi:memory");
    publish_config("sensor", "free_heap", "Free Heap", "data_size", "measurement", "B", NULL, "diagnostic", "mdi:memory");
    remove_config("sensor", "supply_voltage");
    remove_config("sensor", "temperature");
    publish_config("sensor", "uptime", "Uptime", "duration", "total_increasing", "s", NULL, "diagnostic", "mdi:clock-outline");
    // CCU relay latency — diagnostic entities for the delayed-switching reports.
    publish_config("sensor", "ccu_queue_wait_max_ms", "CCU Queue Wait (max)", "duration",
                   "measurement", "ms", NULL, "diagnostic", "mdi:timer-alert-outline");
    publish_config("sensor", "ccu_queue_depth_max", "CCU Queue Depth (max)", NULL, "measurement",
                   NULL, NULL, "diagnostic", "mdi:tray-full");
    publish_config("sensor", "ccu_delayed_frames", "CCU Delayed Frames", NULL, "total_increasing",
                   NULL, NULL, "diagnostic", "mdi:timer-sand");
    publish_config("sensor", "ccu_dropped_frames", "CCU Dropped Frames", NULL, "total_increasing",
                   NULL, NULL, "diagnostic", "mdi:package-variant-remove");
    publish_config("sensor", "uptime_text", "Uptime (Text)", NULL, NULL, NULL, NULL, "diagnostic", "mdi:clock-outline");
    // Remove the legacy short-named version sensors (renamed to
    // firmware_version / webui_version below). Empty retained discovery
    // payload removes the already-announced entities from HA.
    remove_config("sensor", "version");
    remove_config("sensor", "latest_version");
    publish_config("sensor", "firmware_version", "Firmware Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:package-variant");
    publish_config("sensor", "webui_version", "WebUI Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:web");
    // The automatic update-check feature was removed; drop the update-status
    // entities older firmware may still have retained in HA / ioBroker so
    // they don't linger as stale "(null)" / false sensors.
    remove_config("sensor", "latest_firmware_version");
    remove_config("sensor", "latest_webui_version");
    remove_config("binary_sensor", "firmware_update_available");
    remove_config("binary_sensor", "webui_update_available");
    remove_config("binary_sensor", "update_available");
    publish_config("sensor", "board_revision", "Board Revision", NULL, NULL, NULL, NULL, "diagnostic", "mdi:expansion-card");

    // ---- Sensors: network -----------------------------------------------
    // Binary sensors use payload_on/off instead of value_json because the
    // state topics publish plain strings ("online"/"offline", "true"/"false").
    // This makes the entities decode correctly in Home Assistant.
    publish_config("binary_sensor", "online", "Online", "connectivity", NULL, NULL,
                   NULL, "diagnostic", "mdi:lan-connect", "online", "offline");
    publish_config("binary_sensor", "eth_connected", "Ethernet Link", "connectivity", NULL, NULL,
                   NULL, "diagnostic", "mdi:ethernet", "true", "false");
    publish_config("sensor", "eth_link_speed", "Ethernet Speed", "data_rate", "measurement", "Mbit/s", NULL, "diagnostic", "mdi:speedometer");
    publish_config("sensor", "ip_address", "IP Address", NULL, NULL, NULL, NULL, "diagnostic", "mdi:ip");
    publish_config("sensor", "netmask", "Subnet Mask", NULL, NULL, NULL, NULL, "diagnostic", "mdi:ip-network");
    publish_config("sensor", "gateway", "Gateway", NULL, NULL, NULL, NULL, "diagnostic", "mdi:router-network");
    publish_config("sensor", "dns1", "Primary DNS", NULL, NULL, NULL, NULL, "diagnostic", "mdi:dns");
    publish_config("sensor", "dns2", "Secondary DNS", NULL, NULL, NULL, NULL, "diagnostic", "mdi:dns");
    publish_config("sensor", "ipv6_addresses", "IPv6 Addresses", NULL, NULL, NULL, NULL, "diagnostic", "mdi:ip-outline");

    // ---- Sensors: radio module ------------------------------------------
    publish_config("sensor", "radio_module_type", "Radio Module", NULL, NULL, NULL, NULL, "diagnostic", "mdi:radio-tower");
    publish_config("sensor", "radio_module_serial", "Radio Serial", NULL, NULL, NULL, NULL, "diagnostic", "mdi:barcode");
    publish_config("sensor", "radio_module_firmware", "Radio Firmware", NULL, NULL, NULL, NULL, "diagnostic", "mdi:chip");

    // ---- Sensors: time / NTP --------------------------------------------
    publish_config("binary_sensor", "ntp_synced", "NTP Synced", NULL, NULL, NULL,
                   NULL, "diagnostic", "mdi:clock-check", "true", "false");

    // ---- Retired OTA / update-check topics ------------------------------
    // Automatic update-check was removed. Delete any retained discovery
    // entries and (via publish_legacy_topic_cleanup) the retained status
    // values so HA / ioBroker don't keep stale sensors with null/false.
    remove_config("sensor", "ota_progress");
    remove_config("sensor", "ota_state");
    remove_config("sensor", "ota_error");

    // ---- Buttons ---------------------------------------------------------
    auto publish_button = [&](const char* object_id, const char* name,
                              const char* command, const char* payload_str,
                              const char* device_class = "restart",
                              const char* icon = nullptr) {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "name", name);
        char unique_id[128];
        snprintf(unique_id, sizeof(unique_id), "%s_%s", identifiers, object_id);
        cJSON_AddStringToObject(root, "unique_id", unique_id);

        char command_topic[160];
        snprintf(command_topic, sizeof(command_topic), "%s/command/%s",
                 current_mqtt_config.topic_prefix, command);
        cJSON_AddStringToObject(root, "command_topic", command_topic);
        cJSON_AddStringToObject(root, "payload_press", payload_str);

        cJSON_AddStringToObject(root, "entity_category", "config");
        if (device_class) cJSON_AddStringToObject(root, "device_class", device_class);
        if (icon) cJSON_AddStringToObject(root, "icon", icon);

        cJSON_AddItemToObject(root, "device", cJSON_Duplicate(device, 1));

        char *json_str = cJSON_PrintUnformatted(root);
        char topic[256];
        snprintf(topic, sizeof(topic), "%s/button/hb-rf-eth-%s/%s/config",
                 current_mqtt_config.ha_discovery_prefix, sysInfo->getSerialNumber(), object_id);
        mqtt_publish_connected(topic, json_str, 0, 1, 1);
        free(json_str);
        cJSON_Delete(root);
    };

    // When commands are disabled, the device ignores every payload - so we
    // must NOT publish buttons that look clickable. Hide them by skipping.
    if (current_mqtt_config.command_enabled) {
        publish_button("restart", "Restart", "restart", restart_payload, "restart", "mdi:restart");
    }

    // Remove destructive/retired entities retained by older firmware.
    remove_config("button", "factory_reset");
    remove_config("button", "check_update");
    remove_config("update", "firmware_update");

    cJSON_Delete(device);
}

esp_err_t mqtt_handler_init(void)
{
    if (mqtt_lifecycle_mutex == NULL) {
        mqtt_lifecycle_mutex =
            xSemaphoreCreateMutexStatic(&mqtt_lifecycle_mutex_buffer);
    }
    if (mqtt_lifecycle_mutex == NULL) {
        ESP_LOGE(TAG, "MQTT static lifecycle primitives unavailable");
        return ESP_ERR_NO_MEM;
    }

    if (mqtt_stop_watchdog_timer == NULL) {
        mqtt_stop_watchdog_timer = xTimerCreateStatic(
            "mqtt_stop_guard", pdMS_TO_TICKS(1000), pdTRUE, NULL,
            mqtt_stop_watchdog_callback, &mqtt_stop_watchdog_timer_buffer);
    }
    if (mqtt_stop_watchdog_timer == NULL ||
        (xTimerIsTimerActive(mqtt_stop_watchdog_timer) == pdFALSE &&
         xTimerStart(mqtt_stop_watchdog_timer,
                     pdMS_TO_TICKS(1000)) != pdPASS)) {
        ESP_LOGE(TAG, "MQTT static stop watchdog unavailable");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t mqtt_store_pending_restart_locked(
    const mqtt_config_t *config)
{
    mqtt_config_t *copy =
        static_cast<mqtt_config_t *>(malloc(sizeof(mqtt_config_t)));
    if (copy == NULL) {
        ESP_LOGE(TAG, "Could not queue MQTT restart configuration");
        return ESP_ERR_NO_MEM;
    }
    memcpy(copy, config, sizeof(*copy));
    free(mqtt_pending_restart_config);
    mqtt_pending_restart_config = copy;
    mqtt_desired_running.store(true, std::memory_order_release);
    return ESP_OK;
}

static void mqtt_cleanup_task(void *parameter);

// Begin teardown without ever invoking esp_mqtt_client_stop() in the calling
// task. ESP-MQTT 1.0.0 waits for STOPPED_BIT with portMAX_DELAY internally;
// isolating that wait keeps WebUI/config/firmware-upload tasks schedulable and bounded.
// Must be called with mqtt_lifecycle_mutex held.
static esp_err_t mqtt_begin_cleanup_locked()
{
    if (mqtt_cleanup_task_handle.load(std::memory_order_acquire) != NULL) {
        return ESP_OK;
    }
    if (client == NULL) {
        mqtt_running.store(false, std::memory_order_seq_cst);
        mqtt_connected.store(false, std::memory_order_release);
        return ESP_OK;
    }

    TaskHandle_t cleanup = NULL;
    if (xTaskCreate(mqtt_cleanup_task, "mqtt_cleanup", 3072,
                    client, 3, &cleanup) != pdPASS) {
        ESP_LOGE(TAG, "Could not create bounded MQTT cleanup worker");
        return ESP_ERR_NO_MEM;
    }

    mqtt_cleanup_task_handle.store(cleanup, std::memory_order_release);
    mqtt_running.store(false, std::memory_order_seq_cst);
    mqtt_connected.store(false, std::memory_order_release);
    TaskHandle_t publisher =
        mqtt_publish_task_handle.load(std::memory_order_acquire);
    if (publisher != NULL) {
        xTaskNotifyGive(publisher);
    }
    // The worker was deliberately created before lifecycle state changed, so
    // allocation failure leaves the live client untouched and recoverable.
    xTaskNotifyGive(cleanup);
    return ESP_OK;
}

// Starts a completely new client. The lifecycle mutex is held by the caller,
// client is NULL and no publisher from a previous generation exists.
static esp_err_t mqtt_start_locked(const mqtt_config_t *config)
{
    if (!config->enabled) {
        mqtt_desired_running.store(false, std::memory_order_release);
        return ESP_OK;
    }

    if (client != NULL ||
        mqtt_publish_task_handle.load(std::memory_order_acquire) != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (strlen(config->server) == 0) {
        ESP_LOGE(TAG, "MQTT Server address is empty");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting MQTT client connecting to %s:%d", config->server, config->port);

    memcpy(&current_mqtt_config, config, sizeof(mqtt_config_t));

    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = NULL;
    mqtt_cfg.broker.address.hostname = current_mqtt_config.server;
    mqtt_cfg.broker.address.port = current_mqtt_config.port;
    mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;

    if (current_mqtt_config.tls_enable) {
        mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_SSL;

        if (current_mqtt_config.tls_skip_verify) {
            mqtt_cfg.broker.verification.skip_cert_common_name_check = true;
        } else {
            if (strlen(current_mqtt_config.tls_ca_certs) > 0) {
                mqtt_cfg.broker.verification.certificate = current_mqtt_config.tls_ca_certs;
            } else {
                mqtt_cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
            }
        }

        if (strlen(current_mqtt_config.tls_certfile) > 0 && strlen(current_mqtt_config.tls_keyfile) > 0) {
            mqtt_cfg.credentials.authentication.certificate = current_mqtt_config.tls_certfile;
            mqtt_cfg.credentials.authentication.key         = current_mqtt_config.tls_keyfile;
        }
    }

    mqtt_cfg.network.timeout_ms = 2000;
    mqtt_cfg.network.reconnect_timeout_ms = MQTT_RECONNECT_TIMEOUT_MS;

    if (strlen(current_mqtt_config.user) > 0) {
        mqtt_cfg.credentials.username = current_mqtt_config.user;
    }
    if (strlen(current_mqtt_config.password) > 0) {
        mqtt_cfg.credentials.authentication.password = current_mqtt_config.password;
    }

    // Keep the LWT topic alive for the complete client lifetime. Pointing the
    // config at a block-local array relied on undocumented eager copying.
    snprintf(mqtt_lwt_topic, sizeof(mqtt_lwt_topic), "%s/status/online",
             current_mqtt_config.topic_prefix);
    mqtt_cfg.session.last_will.topic = mqtt_lwt_topic;
    mqtt_cfg.session.last_will.msg = "offline";
    mqtt_cfg.session.last_will.msg_len = 7;
    mqtt_cfg.session.last_will.qos = 1;
    mqtt_cfg.session.last_will.retain = 1;

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqtt_event_handler, NULL);

    // Publish guards must already see the correct lifecycle state if a very
    // fast broker dispatches MQTT_EVENT_CONNECTED before start() returns.
    mqtt_tls_gate_held.store(false, std::memory_order_release);
    mqtt_connected.store(false, std::memory_order_release);
    mqtt_running.store(true, std::memory_order_release);
    mqtt_desired_running.store(true, std::memory_order_release);
    esp_err_t err = esp_mqtt_client_start(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        mqtt_running.store(false, std::memory_order_release);
        mqtt_desired_running.store(false, std::memory_order_release);
        esp_mqtt_client_destroy(client);
        client = NULL;
        return err;
    }
    // The publish task formats several status payloads via snprintf into a
    // 96-byte stack buffer and calls esp_mqtt_client_publish — TLS handshakes
    // run in the esp-mqtt client task, not here. The observed high-water mark
    // (e.g. "mqtt_publish stack high water mark: 7536 bytes free") confirms
    // only ~650 B of the previous 8 KB were ever used, so 5 KB leaves a
    // >4 KB safety margin while returning ~3 KB to the WROOM-32 heap.
    TaskHandle_t pub_handle = NULL;
    if (xTaskCreate(mqtt_publish_task, "mqtt_publish", 5120,
                    NULL, 4, &pub_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create MQTT publish task");
        // Do not call esp_mqtt_client_stop() here: it contains an unbounded
        // wait. A dedicated cleanup worker owns that library call.
        mqtt_desired_running.store(false, std::memory_order_release);
        esp_err_t cleanup_result = mqtt_begin_cleanup_locked();
        if (cleanup_result != ESP_OK) {
            // Allocation failure leaves the client running (without the
            // optional periodic publisher) rather than destroying live state.
            mqtt_desired_running.store(true, std::memory_order_release);
        }
        return ESP_ERR_NO_MEM;
    }
    mqtt_publish_task_handle.store(pub_handle, std::memory_order_release);
    return ESP_OK;
}

static void mqtt_cleanup_task(void *parameter)
{
    esp_mqtt_client_handle_t target =
        static_cast<esp_mqtt_client_handle_t>(parameter);
    const TaskHandle_t self = xTaskGetCurrentTaskHandle();
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    for (;;) {
        if (target != NULL) {
            // ESP-MQTT waits without an internal deadline here. No application
            // mutex is held, and the static watchdog below resets the device
            // if the component does not acknowledge stop after the normal
            // reconnect/transport window.
            const uint32_t stop_deadline =
                static_cast<uint32_t>(xTaskGetTickCount()) +
                static_cast<uint32_t>(
                    pdMS_TO_TICKS(MQTT_COMPONENT_STOP_WATCHDOG_MS));
            mqtt_component_stop_deadline_ticks.store(
                stop_deadline, std::memory_order_release);
            mqtt_component_stop_in_progress.store(true,
                                                  std::memory_order_release);
            esp_err_t stop_result = esp_mqtt_client_stop(target);
            mqtt_connected.store(false, std::memory_order_release);
            if (stop_result != ESP_OK) {
                // `run == false` is ambiguous in ESP-MQTT 1.0.0: the task may
                // not have entered yet, or may still be tearing down. Never
                // destroy that state speculatively.
                ESP_LOGE(TAG, "MQTT component could not acknowledge stop (%s); rebooting safely",
                         esp_err_to_name(stop_result));
                ResetInfo::storeResetReason(RESET_REASON_SYSTEM_ERROR,
                                            "MQTT stop acknowledgement failed");
                vTaskDelay(pdMS_TO_TICKS(100));
                esp_restart();
                vTaskDelete(NULL);
                return;
            }

            // The periodic publisher owns its retirement, while event/syslog
            // callers may also be inside mqtt_publish_connected(). Both must
            // drain before destroying the shared client. New callers observe
            // the sequentially-consistent mqtt_running=false admission close
            // and leave without reading client.
            const int drain_iterations =
                (MQTT_PUBLISH_DRAIN_TIMEOUT_MS + 19) / 20;
            for (int i = 0; i < drain_iterations; ++i) {
                if (mqtt_publish_task_handle.load(
                        std::memory_order_acquire) == NULL &&
                    mqtt_active_publishers.load(
                        std::memory_order_seq_cst) == 0) {
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(20));
            }
            if (mqtt_publish_task_handle.load(std::memory_order_acquire) !=
                    NULL ||
                mqtt_active_publishers.load(std::memory_order_seq_cst) != 0) {
                ESP_LOGE(TAG,
                         "MQTT publishers did not drain within %d ms; rebooting safely",
                         MQTT_PUBLISH_DRAIN_TIMEOUT_MS);
                ResetInfo::storeResetReason(
                    RESET_REASON_SYSTEM_ERROR,
                    "MQTT publisher lifetime guard timeout");
                vTaskDelay(pdMS_TO_TICKS(100));
                esp_restart();
                vTaskDelete(NULL);
                return;
            }
            const esp_err_t destroy_result =
                esp_mqtt_client_destroy(target);
            if (destroy_result != ESP_OK) {
                ESP_LOGE(TAG, "MQTT client destroy failed (%s); rebooting safely",
                         esp_err_to_name(destroy_result));
                ResetInfo::storeResetReason(RESET_REASON_SYSTEM_ERROR,
                                            "MQTT client destroy failed");
                vTaskDelay(pdMS_TO_TICKS(100));
                esp_restart();
                vTaskDelete(NULL);
                return;
            }
            mqtt_component_stop_in_progress.store(false,
                                                   std::memory_order_release);
            mqtt_release_tls_gate_if_held();
        }

        // A cleanup worker is the durable owner of a queued restart. If the
        // restart fails before a new client exists, this same already-created
        // task retries without needing another heap allocation. If startup
        // created a client but could not create its publisher/cleanup task, we
        // adopt and tear down that client before trying again.
        bool retry_without_client = false;
        xSemaphoreTake(mqtt_lifecycle_mutex, portMAX_DELAY);
        if (target != NULL && client == target) client = NULL;
        target = NULL;

        if (mqtt_desired_running.load(std::memory_order_acquire) &&
            mqtt_pending_restart_config != NULL &&
            !monitoring_ota_pause_active()) {
            mqtt_config_t *candidate = mqtt_pending_restart_config;

            // Temporarily relinquish cleanup ownership so mqtt_start_locked()
            // may create a cleanup worker if publisher creation fails.
            mqtt_cleanup_task_handle.store(NULL, std::memory_order_release);
            esp_err_t restart_result = mqtt_start_locked(candidate);
            if (restart_result == ESP_OK) {
                mqtt_pending_restart_config = NULL;
                free(candidate);
            } else {
                // mqtt_start_locked() may clear this flag on a partial startup
                // failure. The queued candidate remains authoritative until a
                // later attempt actually succeeds or stop() cancels it.
                mqtt_desired_running.store(true, std::memory_order_release);
                ESP_LOGW(TAG, "Deferred MQTT restart failed; retrying: %s",
                         esp_err_to_name(restart_result));

                if (mqtt_cleanup_task_handle.load(
                        std::memory_order_acquire) == NULL) {
                    if (client != NULL) {
                        target = client;
                        mqtt_cleanup_task_handle.store(
                            self, std::memory_order_release);
                        mqtt_running.store(false,
                                           std::memory_order_seq_cst);
                        mqtt_connected.store(false,
                                             std::memory_order_release);
                        TaskHandle_t publisher = mqtt_publish_task_handle.load(
                            std::memory_order_acquire);
                        if (publisher != NULL) xTaskNotifyGive(publisher);
                    } else {
                        mqtt_cleanup_task_handle.store(
                            self, std::memory_order_release);
                        retry_without_client = true;
                    }
                }
                // Otherwise mqtt_start_locked() successfully installed a new
                // cleanup owner. It will retry this still-pending candidate
                // after retiring the partial generation.
            }
        } else {
            free(mqtt_pending_restart_config);
            mqtt_pending_restart_config = NULL;
            mqtt_desired_running.store(false, std::memory_order_release);
            mqtt_cleanup_task_handle.store(NULL, std::memory_order_release);
        }
        xSemaphoreGive(mqtt_lifecycle_mutex);

        if (target != NULL) {
            continue;
        }
        if (retry_without_client) {
            // start()/stop() notify this handle, so a new candidate or explicit
            // cancellation wakes the retry immediately instead of waiting the
            // complete backoff.
            (void)ulTaskNotifyTake(
                pdTRUE, pdMS_TO_TICKS(MQTT_RESTART_RETRY_DELAY_MS));
            continue;
        }
        vTaskDelete(NULL);
        return;
    }
}

esp_err_t mqtt_handler_start(const mqtt_config_t *config)
{
    if (config == NULL) return ESP_ERR_INVALID_ARG;
    if (mqtt_lifecycle_mutex == NULL) {
        ESP_LOGE(TAG, "MQTT lifecycle mutex not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(mqtt_lifecycle_mutex,
                       pdMS_TO_TICKS(15000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (monitoring_ota_pause_active()) {
        xSemaphoreGive(mqtt_lifecycle_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    if (!config->enabled) {
        mqtt_desired_running.store(false, std::memory_order_release);
        free(mqtt_pending_restart_config);
        mqtt_pending_restart_config = NULL;
        xSemaphoreGive(mqtt_lifecycle_mutex);
        return ESP_OK;
    }

    if (mqtt_running.load(std::memory_order_acquire)) {
        mqtt_desired_running.store(true, std::memory_order_release);
        xSemaphoreGive(mqtt_lifecycle_mutex);
        return ESP_OK;
    }

    if (mqtt_cleanup_task_handle.load(std::memory_order_acquire) != NULL ||
        client != NULL) {
        esp_err_t queued = mqtt_store_pending_restart_locked(config);
        TaskHandle_t cleanup =
            mqtt_cleanup_task_handle.load(std::memory_order_acquire);
        if (queued == ESP_OK && cleanup != NULL) {
            xTaskNotifyGive(cleanup);
        }
        xSemaphoreGive(mqtt_lifecycle_mutex);
        if (queued == ESP_OK) {
            ESP_LOGW(TAG, "MQTT restart queued until cleanup completes");
        }
        return queued;
    }

    mqtt_desired_running.store(true, std::memory_order_release);
    esp_err_t result = mqtt_start_locked(config);

    xSemaphoreGive(mqtt_lifecycle_mutex);
    return result;
}

esp_err_t mqtt_handler_stop(void)
{
    if (mqtt_lifecycle_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(mqtt_lifecycle_mutex,
                       pdMS_TO_TICKS(15000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    mqtt_desired_running.store(false, std::memory_order_release);
    free(mqtt_pending_restart_config);
    mqtt_pending_restart_config = NULL;

    if (mqtt_cleanup_task_handle.load(std::memory_order_acquire) == NULL &&
        client != NULL) {
        if (mqtt_running.load(std::memory_order_acquire)) {
            ESP_LOGI(TAG, "Stopping MQTT client");
        }
        esp_err_t begin_result = mqtt_begin_cleanup_locked();
        if (begin_result != ESP_OK) {
            xSemaphoreGive(mqtt_lifecycle_mutex);
            return begin_result;
        }
    } else if (client == NULL) {
        mqtt_running.store(false, std::memory_order_release);
        mqtt_connected.store(false, std::memory_order_release);
        TaskHandle_t publisher =
            mqtt_publish_task_handle.load(std::memory_order_acquire);
        if (publisher != NULL) xTaskNotifyGive(publisher);
    }
    TaskHandle_t cleanup =
        mqtt_cleanup_task_handle.load(std::memory_order_acquire);
    if (cleanup != NULL) xTaskNotifyGive(cleanup);
    xSemaphoreGive(mqtt_lifecycle_mutex);

    // WAIT_RECONNECT sleeps for reconnect_timeout/2 and stop() does not wake
    // it. Include that documented component delay plus cleanup margin. A
    // static watchdog reboots before this deadline if the third-party stop
    // wait ever wedges permanently, so later starts cannot become false-OK
    // queued requests behind an immortal cleanup owner.
    const int wait_iterations = (MQTT_STOP_WAIT_TIMEOUT_MS + 99) / 100;
    for (int i = 0; i < wait_iterations; ++i) {
        if (mqtt_cleanup_task_handle.load(std::memory_order_acquire) == NULL &&
            mqtt_publish_task_handle.load(std::memory_order_acquire) == NULL) {
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGW(TAG, "MQTT cleanup still running after %d ms",
             MQTT_STOP_WAIT_TIMEOUT_MS);
    return ESP_ERR_TIMEOUT;
}

bool mqtt_handler_is_connected(void)
{
    return mqtt_connected.load();
}
