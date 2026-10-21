/*
 *  update_scheduler.cpp is part of the HB-RF-ETH firmware v2.0
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
 */

#include "update_scheduler.h"

#include "settings.h"
#include "update_check.h"
#include "update_install.h"
#include "update_schedule_policy.h"
#include "monitoring.h"
#include "nvs_storage_lock.h"
#include "rawuartudplistener.h"

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "nvs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <atomic>
#include <ctime>
#include <string.h>

// Scheduled automatic updates, stages B2/C/D of docs/AUTO_UPDATE_PLAN.md.
//
// Shape (rules 5 and 6 of the plan: never a long delay, never work in a
// timer callback):
//
//   esp_timer (1 h periodic, + one-shot first check at 10 min + jitter)
//       └─ callback only spawns the transient tick task ("upd_sched",
//          3 KiB, priority 2) and returns
//            └─ tick: stage marker → pending → scheduled search →
//                     auto-install decision, then vTaskDelete(NULL)
//
// An idle device carries no task stack for any of this: only the two
// esp_timers (~200 bytes) and a small RAM state.
//
// Job order inside a tick is fixed and deliberate:
//   1. two-stage marker (completes an already-authorized firmware update
//      regardless of mode; never gated on window/idle — the WebUI sink does
//      not reboot the relay path),
//   2. pending one-shot install (operator-scheduled; window/time/idle gated,
//      but mode independent),
//   3. scheduled search (mode != off),
//   4. auto-install from cached search results (auto modes; window/time/idle
//      gated — evaluated every tick so an update found at 23:00 installs at
//      the 03:00 window without a fresh search).
//
// Every install goes through update_install_start(), which re-fetches the
// manifest and re-validates version, host, size and digest itself; nothing
// here can force bytes into flash that the manual path would reject.

static const char *TAG = "UpdateScheduler";

// The cache namespace is prepared in settings.cpp (factory reset erases it);
// keep the name in sync with UPD_CACHE_NVS_NAMESPACE there.
static const char *CACHE_NVS_NAMESPACE = "upd_cache";
static const char *CACHE_KEY_SEEN_FW   = "upd_seenfw";
static const char *CACHE_KEY_SEEN_UI   = "upd_seenui";
static const char *CACHE_KEY_CHECK_AT  = "upd_chkt";
static const char *CACHE_KEY_STAGE     = "upd_stage";
static const char *CACHE_KEY_STAGE_BOOT = "upd_stgboot";
static const char *CACHE_KEY_BOOTS     = "upd_boots";
static const char *CACHE_KEY_PENDING   = "upd_pend";

static constexpr uint32_t TICK_TASK_STACK_BYTES = 3072;
static constexpr UBaseType_t TICK_TASK_PRIORITY = 2;
static constexpr int64_t HOUR_US = 3600LL * 1000000LL;
// Slack over the 20 s fetch deadline of the check worker before the tick
// stops waiting for the result of a search it started.
static constexpr uint32_t RESULT_WAIT_MS = 40000;
static constexpr uint32_t RESULT_POLL_MS = 500;

typedef enum
{
    TICK_SOURCE_FIRST = 0, // first-check one-shot: always attempts a search
    TICK_SOURCE_HOURLY,    // hourly tick: counts one accumulated hour first
} tick_source_t;

struct scheduler_state {
    // Raw NVS-mirrored entries (empty string = absent).
    char pending[UPDATE_PENDING_MAX_BYTES];
    char stage[UPDATE_STAGE_MAX_BYTES];
    int32_t stage_boot;   // boot counter value when the stage was written
    int32_t boots;        // incremented once per update_scheduler_init()
    // Purely RAM scheduling counters.
    uint32_t hours_accumulated;
    uint32_t consecutive_failures;
    int64_t next_check_unix;
};

static Settings *s_settings = nullptr;
static SemaphoreHandle_t s_lock = nullptr;
static scheduler_state s_state = {};
static esp_timer_handle_t s_hourly_timer = nullptr;
static esp_timer_handle_t s_first_timer = nullptr;
static std::atomic<bool> s_tick_active{false};

// Mirrors of the persisted last-seen record (upd_seenfw/upd_seenui/upd_chkt),
// loaded once at init, refreshed after every successful search, and served by
// update_scheduler_get_status() so the WebUI can show the last result right
// after a reboot without triggering a fetch.
static char s_scheduler_seen_fw[UPDATE_CHECK_VERSION_LEN] = {0};
static char s_scheduler_seen_ui[UPDATE_CHECK_VERSION_LEN] = {0};
static int64_t s_scheduler_last_check = 0;

// --- tiny NVS cache helpers (best-effort; a failed write only degrades the
// next-boot display or makes an entry retry, it never bricks anything) ------

static bool cache_open(nvs_handle_t *handle)
{
    NvsStorageLock lock(pdMS_TO_TICKS(1000), "upd_cache");
    if (!lock) {
        ESP_LOGW(TAG, "Could not reserve NVS storage for update cache");
        return false;
    }
    const esp_err_t err = nvs_open(CACHE_NVS_NAMESPACE, NVS_READWRITE, handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not open update cache namespace: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

static bool cache_read_str(const char *key, char *out, size_t size)
{
    nvs_handle_t handle = 0;
    if (!cache_open(&handle)) return false;
    size_t length = size;
    const esp_err_t err = nvs_get_str(handle, key, out, &length);
    nvs_close(handle);
    return err == ESP_OK;
}

static bool cache_write_str(const char *key, const char *value)
{
    nvs_handle_t handle = 0;
    if (!cache_open(&handle)) return false;
    const esp_err_t err = nvs_set_str(handle, key, value);
    const esp_err_t commit = err == ESP_OK ? nvs_commit(handle) : err;
    nvs_close(handle);
    if (commit != ESP_OK) {
        ESP_LOGW(TAG, "Could not persist %s: %s", key, esp_err_to_name(commit));
    }
    return commit == ESP_OK;
}

static void cache_erase(const char *key)
{
    nvs_handle_t handle = 0;
    if (!cache_open(&handle)) return;
    const esp_err_t err = nvs_erase_key(handle, key);
    const esp_err_t commit = err == ESP_OK ? nvs_commit(handle) : err;
    nvs_close(handle);
    if (commit != ESP_OK && commit != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Could not erase %s: %s", key, esp_err_to_name(commit));
    }
}

static bool cache_read_i32(const char *key, int32_t *out)
{
    nvs_handle_t handle = 0;
    if (!cache_open(&handle)) return false;
    const esp_err_t err = nvs_get_i32(handle, key, out);
    nvs_close(handle);
    return err == ESP_OK;
}

static bool cache_write_i32(const char *key, int32_t value)
{
    nvs_handle_t handle = 0;
    if (!cache_open(&handle)) return false;
    const esp_err_t err = nvs_set_i32(handle, key, value);
    const esp_err_t commit = err == ESP_OK ? nvs_commit(handle) : err;
    nvs_close(handle);
    return commit == ESP_OK;
}

// --- gate inputs -------------------------------------------------------------

static int64_t now_unix()
{
    return (int64_t)time(NULL);
}

static bool clock_valid()
{
    return update_schedule_time_valid(now_unix());
}

static int local_hour_now()
{
    time_t wall = time(NULL);
    struct tm local = {};
    localtime_r(&wall, &local);
    return local.tm_hour;
}

static bool relay_idle()
{
    // Same uptime-ms idiom as the tcpip liveness watchdog in monitoring.cpp.
    const uint32_t last_rx_ms = raw_uart_last_tcpip_rx_ms();
    const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    const uint32_t rx_age_s = last_rx_ms != 0 ? (now_ms - last_rx_ms) / 1000 : UINT32_MAX;
    return update_schedule_idle_ok(raw_uart_session_active(), rx_age_s);
}

static bool inside_window_locked()
{
    return update_window_contains(local_hour_now(),
                                  s_settings->getUpdWinStart(),
                                  s_settings->getUpdWinLen());
}

static bool operation_busy()
{
    return ota_operation_active() || net_fetch_ota_active();
}

static const char *channel_name()
{
    return s_settings->getUpdChannel() == 1 ? "beta" : "stable";
}

// --- state mutations (s_lock held by caller) ----------------------------------

static void clear_stage_locked()
{
    s_state.stage[0] = '\0';
    s_state.stage_boot = 0;
    cache_erase(CACHE_KEY_STAGE);
    cache_erase(CACHE_KEY_STAGE_BOOT);
}

static void write_stage_locked(const char *fw_version, const char *ui_version)
{
    char encoded[UPDATE_STAGE_MAX_BYTES];
    if (!update_stage_format(encoded, sizeof(encoded), fw_version, ui_version)) {
        ESP_LOGW(TAG, "Could not encode stage marker for %s/%s", fw_version, ui_version);
        return;
    }
    if (!cache_write_str(CACHE_KEY_STAGE, encoded)) return;
    cache_write_i32(CACHE_KEY_STAGE_BOOT, s_state.boots);
    strlcpy(s_state.stage, encoded, sizeof(s_state.stage));
    s_state.stage_boot = s_state.boots;
    ESP_LOGI(TAG, "Two-stage update staged: firmware %s, WebUI %s follows after reboot",
             fw_version, ui_version);
}

static void clear_pending_locked()
{
    s_state.pending[0] = '\0';
    cache_erase(CACHE_KEY_PENDING);
}

// --- tick work ----------------------------------------------------------------

static void handle_stage_marker_locked()
{
    if (s_state.stage[0] == '\0') return;

    char stage_fw[UPDATE_CHECK_VERSION_LEN];
    char stage_ui[UPDATE_CHECK_VERSION_LEN];
    if (!update_stage_parse(s_state.stage, stage_fw, sizeof(stage_fw),
                            stage_ui, sizeof(stage_ui))) {
        ESP_LOGW(TAG, "Stored stage marker is malformed; discarding");
        clear_stage_locked();
        return;
    }

    const esp_app_desc_t *app = esp_app_get_description();
    const uint32_t boots_since = (uint32_t)(s_state.boots - s_state.stage_boot);
    if (!update_stage_applicable(boots_since, stage_fw, app ? app->version : "")) {
        // Expired (more than two boots) or rolled back: either way the staged
        // WebUI no longer belongs to the running firmware.
        ESP_LOGI(TAG, "Stage marker not applicable (%u boots since write, running %s); discarding",
                 (unsigned)boots_since, app ? app->version : "?");
        clear_stage_locked();
        return;
    }

    // Continuation of an already-authorized firmware update: no window and no
    // idle gate (the WebUI sink remounts without touching the relay path).
    // update_install_start() re-verifies the manifest itself.
    const update_install_result_t result = update_install_start("webui");
    if (result == UPDATE_INSTALL_ACCEPTED) {
        ESP_LOGI(TAG, "Completing two-stage update: installing WebUI %s on firmware %s",
                 stage_ui, stage_fw);
        clear_stage_locked();
    } else if (result != UPDATE_INSTALL_BUSY) {
        ESP_LOGE(TAG, "Could not start staged WebUI install (%d); discarding marker", (int)result);
        clear_stage_locked();
    }
    // BUSY: another operation holds the gate; the marker stays and the next
    // hourly tick retries while the two-boot window is open.
}

static void handle_pending_locked(bool time_valid, bool in_window, bool idle)
{
    if (s_state.pending[0] == '\0') return;

    bool firmware_target = false;
    char version[UPDATE_CHECK_VERSION_LEN];
    int64_t created = 0;
    if (!update_pending_parse(s_state.pending, &firmware_target, version,
                              sizeof(version), &created)) {
        ESP_LOGW(TAG, "Stored pending entry is malformed; discarding");
        clear_pending_locked();
        return;
    }
    if (!update_pending_valid(created, now_unix())) {
        ESP_LOGW(TAG, "Scheduled install for %s %s expired; discarding",
                 firmware_target ? "firmware" : "WebUI", version);
        clear_pending_locked();
        return;
    }

    if (!in_window || !time_valid || !idle || operation_busy()) {
        // installBlocked in get_status() explains the wait; nothing to do.
        return;
    }

    // A scheduled firmware install with a pending WebUI update becomes the
    // same two-stage transaction an automatic one is.
    if (firmware_target) {
        const UpdateCheckStatus check = update_check_get_status();
        if (check.webuiUpdateAvailable) {
            write_stage_locked(check.latestFirmware, check.latestWebui);
        }
    }

    const update_install_result_t result =
        update_install_start(firmware_target ? "firmware" : "webui");
    if (result == UPDATE_INSTALL_ACCEPTED) {
        ESP_LOGI(TAG, "Starting scheduled install of %s %s in the maintenance window",
                 firmware_target ? "firmware" : "WebUI", version);
        clear_pending_locked();
    } else if (result == UPDATE_INSTALL_BUSY) {
        // Retry on the next hourly tick; the seven-day expiry bounds this.
    } else {
        ESP_LOGE(TAG, "Scheduled install of %s could not start (%d); discarding",
                 firmware_target ? "firmware" : "WebUI", (int)result);
        clear_pending_locked();
    }
}

// Wait for a triggered search to finish and classify its outcome.
typedef enum
{
    SEARCH_SUCCEEDED = 0,
    SEARCH_SKIPPED,
    SEARCH_FAILED,
    SEARCH_TIMED_OUT,
} search_outcome_t;

static search_outcome_t wait_for_search_result()
{
    for (uint32_t waited = 0; waited < RESULT_WAIT_MS; waited += RESULT_POLL_MS) {
        vTaskDelay(pdMS_TO_TICKS(RESULT_POLL_MS));
        const UpdateCheckStatus status = update_check_get_status();
        if (status.state != UPDATE_CHECK_RUNNING) {
            if (status.lastError[0] != '\0') return SEARCH_FAILED;
            if (status.lastSkipReason[0] != '\0') return SEARCH_SKIPPED;
            return SEARCH_SUCCEEDED;
        }
    }
    return SEARCH_TIMED_OUT;
}

static void run_scheduled_search_locked()
{
    const uint32_t interval = (uint32_t)s_settings->getUpdInterval();
    const uint32_t due_hours =
        update_schedule_delay_after_failures(s_state.consecutive_failures, interval);
    const bool due = s_state.hours_accumulated >= due_hours;

    if (!due) return;

    const update_check_trigger_result_t trigger = update_check_trigger(channel_name());
    switch (trigger) {
        case UPDATE_CHECK_TRIGGER_ACCEPTED: {
            const search_outcome_t outcome = wait_for_search_result();
            s_state.hours_accumulated = 0;
            switch (outcome) {
                case SEARCH_SUCCEEDED:
                    s_state.consecutive_failures = 0;
                    s_state.next_check_unix = now_unix() + (int64_t)interval * 3600;
                    // Persist what was seen so the WebUI can show it right
                    // after the next reboot, before any fresh search.
                    {
                        const UpdateCheckStatus status = update_check_get_status();
                        strlcpy(s_scheduler_seen_fw, status.latestFirmware,
                                 sizeof(s_scheduler_seen_fw));
                        strlcpy(s_scheduler_seen_ui, status.latestWebui,
                                 sizeof(s_scheduler_seen_ui));
                        cache_write_str(CACHE_KEY_SEEN_FW, status.latestFirmware);
                        cache_write_str(CACHE_KEY_SEEN_UI, status.latestWebui);
                        if (status.lastCheckUnix > 0) {
                            s_scheduler_last_check = status.lastCheckUnix;
                            cache_write_i32(CACHE_KEY_CHECK_AT,
                                            (int32_t)status.lastCheckUnix);
                        }
                    }
                    break;
                case SEARCH_SKIPPED:
                    // A skip is not a failure: the gates refused, nothing was
                    // fetched. Retry at the full interval, no backoff ladder.
                    s_state.next_check_unix = now_unix() + (int64_t)interval * 3600;
                    break;
                case SEARCH_FAILED:
                    s_state.consecutive_failures++;
                    s_state.next_check_unix = now_unix() +
                        (int64_t)update_schedule_delay_after_failures(
                            s_state.consecutive_failures, interval) * 3600;
                    break;
                case SEARCH_TIMED_OUT:
                    // The worker is still running past all deadlines; treat
                    // as a failure for backoff purposes and let it wind down.
                    s_state.consecutive_failures++;
                    s_state.next_check_unix = now_unix() + (int64_t)interval * 3600;
                    break;
            }
            break;
        }
        case UPDATE_CHECK_TRIGGER_COOLDOWN:
            // A manual search ran less than 60 s ago; try again next hour.
            s_state.hours_accumulated = 0;
            s_state.next_check_unix = now_unix() + 3600;
            break;
        case UPDATE_CHECK_TRIGGER_BUSY:
        case UPDATE_CHECK_TRIGGER_NO_WORKER:
            // Leave the hour counter untouched so the next tick is due again.
            break;
    }
}

static void run_auto_install_locked(bool time_valid, bool in_window, bool idle)
{
    const int mode = s_settings->getUpdMode();
    if (mode != UPDATE_MODE_AUTO_WEBUI && mode != UPDATE_MODE_AUTO_ALL) return;
    if (operation_busy()) return;

    const UpdateCheckStatus check = update_check_get_status();
    const bool effective_idle = s_settings->getUpdIdleOnly() ? idle : true;

    const update_auto_action_t action = update_schedule_auto_action(
        mode, check.firmwareUpdateAvailable, check.webuiUpdateAvailable,
        in_window, time_valid, effective_idle);
    if (action == UPDATE_AUTO_ACTION_NONE) return;

    if (action == UPDATE_AUTO_ACTION_INSTALL_FIRMWARE) {
        write_stage_locked(check.latestFirmware, check.latestWebui);
        const update_install_result_t result = update_install_start("firmware");
        if (result != UPDATE_INSTALL_ACCEPTED && result != UPDATE_INSTALL_BUSY) {
            ESP_LOGE(TAG, "Automatic firmware install could not start (%d); clearing stage",
                     (int)result);
            clear_stage_locked();
        }
        // BUSY keeps the marker: an hourly retry continues while it applies,
        // and a reboot cleans it up via the applicability check.
        return;
    }

    const update_install_result_t result = update_install_start("webui");
    if (result != UPDATE_INSTALL_ACCEPTED && result != UPDATE_INSTALL_BUSY) {
        ESP_LOGE(TAG, "Automatic WebUI install could not start (%d)", (int)result);
    }
}

static void scheduler_tick(tick_source_t source)
{
    if (s_settings == nullptr) return;
    // Never two ticks at once (hourly + first-check overlap).
    bool expected = false;
    if (!s_tick_active.compare_exchange_strong(expected, true)) return;

    const bool time_valid = clock_valid();
    bool in_window = false;
    bool idle = false;

    if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY);

    // The install gates read the settings once per tick.
    in_window = inside_window_locked();
    idle = relay_idle();

    handle_stage_marker_locked();
    handle_pending_locked(time_valid, in_window, idle);

    const int mode = s_settings->getUpdMode();
    if (mode != UPDATE_MODE_OFF) {
        if (source == TICK_SOURCE_HOURLY) {
            s_state.hours_accumulated++;
        }
        run_scheduled_search_locked();
        run_auto_install_locked(time_valid, in_window, idle);
    }

    if (s_lock) xSemaphoreGive(s_lock);
    s_tick_active.store(false);
}

static void scheduler_tick_task(void *parameter)
{
    const tick_source_t source =
        (uintptr_t)parameter == (uintptr_t)TICK_SOURCE_HOURLY
            ? TICK_SOURCE_HOURLY
            : TICK_SOURCE_FIRST;
    scheduler_tick(source);
    vTaskDelete(NULL);
}

// Rule 6: a timer callback must never do the work itself; it only spawns the
// transient tick task (the same discipline as mqtt_retry_guard_timer).
static void spawn_tick_task(tick_source_t source)
{
    if (xTaskCreate(scheduler_tick_task, "upd_sched", TICK_TASK_STACK_BYTES,
                    (void *)(uintptr_t)source, TICK_TASK_PRIORITY, NULL) != pdPASS) {
        ESP_LOGW(TAG, "Could not spawn scheduler tick task; retrying next timer fire");
    }
}

static void hourly_timer_callback(void *parameter)
{
    (void)parameter;
    spawn_tick_task(TICK_SOURCE_HOURLY);
}

static void first_check_timer_callback(void *parameter)
{
    (void)parameter;
    spawn_tick_task(TICK_SOURCE_FIRST);
}

// --- public API ---------------------------------------------------------------

void update_scheduler_init(Settings *settings)
{
    if (s_lock == nullptr) {
        s_lock = xSemaphoreCreateMutex();
    }
    s_settings = settings;

    if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY);
    s_state = {};
    s_state.stage_boot = 0;

    // One boot further. The counter bounds the two-stage marker window
    // (§4.5) and is deliberately monotonic across the whole device life.
    int32_t stored_boots = 0;
    cache_read_i32(CACHE_KEY_BOOTS, &stored_boots);
    s_state.boots = stored_boots + 1;
    cache_write_i32(CACHE_KEY_BOOTS, s_state.boots);

    if (!cache_read_str(CACHE_KEY_STAGE, s_state.stage, sizeof(s_state.stage))) {
        s_state.stage[0] = '\0';
    }
    int32_t stage_boot = 0;
    if (cache_read_i32(CACHE_KEY_STAGE_BOOT, &stage_boot)) {
        s_state.stage_boot = stage_boot;
    }
    if (!cache_read_str(CACHE_KEY_PENDING, s_state.pending, sizeof(s_state.pending))) {
        s_state.pending[0] = '\0';
    }

    // Recover the persisted last-seen record for the status API; the very
    // first tick refreshes it.
    {
        char seen[UPDATE_CHECK_VERSION_LEN];
        if (cache_read_str(CACHE_KEY_SEEN_FW, seen, sizeof(seen))) {
            strlcpy(s_scheduler_seen_fw, seen, sizeof(s_scheduler_seen_fw));
        }
        if (cache_read_str(CACHE_KEY_SEEN_UI, seen, sizeof(seen))) {
            strlcpy(s_scheduler_seen_ui, seen, sizeof(s_scheduler_seen_ui));
        }
        int32_t check_at = 0;
        if (cache_read_i32(CACHE_KEY_CHECK_AT, &check_at) && check_at > 0) {
            s_scheduler_last_check = check_at;
        }
    }

    if (s_lock) xSemaphoreGive(s_lock);

    // First check: 10 min + MAC-derived jitter so a fleet does not hit Pages
    // in the same minute (rule 14).
    uint8_t base_mac[6] = {};
    esp_read_mac(base_mac, ESP_MAC_WIFI_STA);
    const uint32_t first_delay_min = update_schedule_first_delay_minutes(base_mac[5]);

    const esp_timer_create_args_t hourly_args = {
        .callback = &hourly_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "upd_sched_hour",
        .skip_unhandled_events = true,
    };
    if (esp_timer_create(&hourly_args, &s_hourly_timer) == ESP_OK) {
        esp_timer_start_periodic(s_hourly_timer, HOUR_US);
    } else {
        ESP_LOGE(TAG, "Could not create the hourly scheduler timer");
    }

    const esp_timer_create_args_t first_args = {
        .callback = &first_check_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "upd_sched_first",
        .skip_unhandled_events = true,
    };
    if (esp_timer_create(&first_args, &s_first_timer) == ESP_OK) {
        esp_timer_start_once(s_first_timer,
                             (int64_t)first_delay_min * 60LL * 1000000LL);
    } else {
        ESP_LOGE(TAG, "Could not create the first-check timer");
    }

    ESP_LOGI(TAG, "Scheduler ready: mode=%s channel=%s first check in %u min",
             update_mode_name(s_settings->getUpdMode()), channel_name(),
             (unsigned)first_delay_min);
}

UpdateSchedulerStatus update_scheduler_get_status()
{
    UpdateSchedulerStatus out = {};
    if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY);

    if (s_state.pending[0] != '\0') {
        bool firmware_target = false;
        char version[UPDATE_CHECK_VERSION_LEN];
        int64_t created = 0;
        if (update_pending_parse(s_state.pending, &firmware_target, version,
                                 sizeof(version), &created) &&
            update_pending_valid(created, now_unix())) {
            out.pendingValid = true;
            out.pendingFirmware = firmware_target;
            strlcpy(out.pendingVersion, version, sizeof(out.pendingVersion));
        }
    }

    if (s_state.stage[0] != '\0') {
        char stage_fw[UPDATE_CHECK_VERSION_LEN];
        char stage_ui[UPDATE_CHECK_VERSION_LEN];
        if (update_stage_parse(s_state.stage, stage_fw, sizeof(stage_fw),
                               stage_ui, sizeof(stage_ui))) {
            out.stageValid = true;
            strlcpy(out.stageFwVersion, stage_fw, sizeof(out.stageFwVersion));
            strlcpy(out.stageUiVersion, stage_ui, sizeof(out.stageUiVersion));
        }
    }

    out.nextCheckUnix = s_state.next_check_unix;
    out.lastCheckUnix = s_scheduler_last_check;
    strlcpy(out.seenFirmware, s_scheduler_seen_fw, sizeof(out.seenFirmware));
    strlcpy(out.seenWebui, s_scheduler_seen_ui, sizeof(out.seenWebui));

    if (s_lock) xSemaphoreGive(s_lock);
    return out;
}

const char *update_scheduler_install_blocked_reason()
{
    if (s_settings == nullptr) return NULL;

    const int mode = s_settings->getUpdMode();
    const UpdateCheckStatus check = update_check_get_status();
    const UpdateSchedulerStatus sched = update_scheduler_get_status();

    const bool auto_wants = (mode == UPDATE_MODE_AUTO_WEBUI || mode == UPDATE_MODE_AUTO_ALL) &&
                            (check.firmwareUpdateAvailable || check.webuiUpdateAvailable);
    const bool wanted = sched.pendingValid || auto_wants;
    if (!wanted) return NULL;

    const bool idle = s_settings->getUpdIdleOnly() ? relay_idle() : true;
    const update_install_blocked_t blocked = update_schedule_install_blocked(
        true, clock_valid(), inside_window_locked(), idle, operation_busy());
    return update_install_blocked_name(blocked);
}

bool update_scheduler_set_pending(bool firmware_target, const char *version)
{
    if (version == nullptr || version[0] == '\0' ||
        strlen(version) >= UPDATE_CHECK_VERSION_LEN) {
        return false;
    }
    if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY);
    char encoded[UPDATE_PENDING_MAX_BYTES];
    const bool ok = update_pending_format(encoded, sizeof(encoded), firmware_target,
                                          version, now_unix());
    if (ok && cache_write_str(CACHE_KEY_PENDING, encoded)) {
        strlcpy(s_state.pending, encoded, sizeof(s_state.pending));
    }
    if (s_lock) xSemaphoreGive(s_lock);
    return ok;
}

bool update_scheduler_clear_pending()
{
    if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY);
    const bool existed = s_state.pending[0] != '\0';
    if (existed) {
        clear_pending_locked();
    }
    if (s_lock) xSemaphoreGive(s_lock);
    return existed;
}
