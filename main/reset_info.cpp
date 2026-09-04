/*
 *  reset_info.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "reset_info.h"
#include "nvs_flash.h"
#include "nvs_storage_lock.h"
#include "nvs.h"
#include "esp_system.h"
#include "esp_log.h"
#include "crash_blackbox.h"
#include <cstring>

static const char *TAG = "ResetInfo";
static const char *NVS_NAMESPACE = "reset_info";
static const char *NVS_KEY = "reason";
static const char *NVS_DIAG_KEY = "diag";

// Buffer for reset reason text
static char reset_text_buffer[256];
// Buffer for the diagnostic string of the last non-normal reset. Filled by
// getLastDiag() on first access and cleared from NVS so a normal reboot does
// not show stale data. Kept deliberately small (96 bytes) so that the
// combined "reason (esp_reason) - diag" string always fits into
// reset_text_buffer[256] without triggering -Wformat-truncation under the
// strict ESP-IDF 6.0.2 build flags.
static char last_diag_buffer[96];

static const char* get_reason_text(reset_reason_type_t reason) {
    switch (reason) {
        case RESET_REASON_NORMAL:
            return "Normaler Start";
        case RESET_REASON_USER_RESTART:
            return "Manueller Neustart";
        case RESET_REASON_FACTORY_RESET:
            return "Werkseinstellungen wiederhergestellt";
        case RESET_REASON_FIRMWARE_UPDATE:
            return "Firmware-Update erfolgreich";
        case RESET_REASON_UPDATE_FAILED:
            return "Firmware-Update fehlgeschlagen";
        case RESET_REASON_SYSTEM_ERROR:
            return "Systemfehler";
        case RESET_REASON_BROWNOUT:
            return "Spannungsabfall erkannt";
        case RESET_REASON_WATCHDOG:
            return "Watchdog Reset";
        case RESET_REASON_UNKNOWN:
        default:
            return "Unbekannter Grund";
    }
}

void ResetInfo::init() {
    NvsStorageLock storage_lock(portMAX_DELAY, "reset_info.nvs_init");
    if (!storage_lock) {
        ESP_LOGE(TAG, "Could not reserve NVS storage during initialization");
        return;
    }
    // Initialize NVS if not already done
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    // Ignore if already initialized
}

void ResetInfo::storeResetReason(reset_reason_type_t reason) {
    storeResetReason(reason, NULL);
}

void ResetInfo::storeResetReason(reset_reason_type_t reason, const char *diag) {
    NvsStorageLock storage_lock(portMAX_DELAY, "reset_info.store_reason");
    if (!storage_lock) {
        ESP_LOGE(TAG, "Could not reserve NVS storage for reset reason");
        return;
    }
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for storing reset reason: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_u8(nvs_handle, NVS_KEY, (uint8_t)reason);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store reset reason: %s", esp_err_to_name(err));
    } else {
        // Optional diagnostic string. Stored verbatim; callers are expected
        // to keep it short (the in-tree caller passes a fixed 200-byte stack
        // buffer with a snprintf of bounded numerics). Cleared whenever the
        // reason is cleared.
        if (diag && diag[0]) {
            nvs_set_str(nvs_handle, NVS_DIAG_KEY, diag);
        } else {
            nvs_erase_key(nvs_handle, NVS_DIAG_KEY);
        }
        err = nvs_commit(nvs_handle);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to commit reset reason: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Stored reset reason: %d%s%s", reason,
                     diag ? " (" : "", diag ? diag : "");
        }
    }
    nvs_close(nvs_handle);
}

const char *ResetInfo::getLastDiag() {
    // Lazily read + clear on first access so the value is shown once for the
    // post-reset WebUI render but does not linger forever.
    if (last_diag_buffer[0] == '\0') {
        nvs_handle_t nvs_handle;
        if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle) == ESP_OK) {
            size_t len = sizeof(last_diag_buffer);
            if (nvs_get_str(nvs_handle, NVS_DIAG_KEY, last_diag_buffer, &len) != ESP_OK) {
                last_diag_buffer[0] = '\0';
            }
            nvs_close(nvs_handle);
        }
    }
    return last_diag_buffer;
}

reset_reason_type_t ResetInfo::getResetReasonType() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return RESET_REASON_NORMAL;  // Default if nothing stored
    }

    uint8_t reason = RESET_REASON_NORMAL;
    err = nvs_get_u8(nvs_handle, NVS_KEY, &reason);
    nvs_close(nvs_handle);

    if (err != ESP_OK) {
        return RESET_REASON_NORMAL;
    }

    return (reset_reason_type_t)reason;
}

const char* ResetInfo::getResetReasonText() {
    reset_reason_type_t reason = getResetReasonType();
    return get_reason_text(reason);
}

const char* ResetInfo::getEspResetReason() {
    esp_reset_reason_t reason = esp_reset_reason();

    switch (reason) {
        case ESP_RST_POWERON:
            return "Einschalten";
        case ESP_RST_SW:
            return "Software";
        case ESP_RST_PANIC:
            return "Exception/Panic";
        case ESP_RST_INT_WDT:
            return "Interrupt Watchdog";
        case ESP_RST_TASK_WDT:
            return "Task Watchdog";
        case ESP_RST_WDT:
            return "Watchdog";
        case ESP_RST_DEEPSLEEP:
            return "Deep Sleep";
        case ESP_RST_BROWNOUT:
            return "Brownout";
        case ESP_RST_SDIO:
            return "SDIO";
        case ESP_RST_EXT:
            return "Externer Reset";
        default:
            return "Unbekannt";
    }
}

const char* ResetInfo::getResetDetails() {
    static bool initialized = false;

    if (!initialized) {
        initialized = true;
        reset_reason_type_t stored_reason = getResetReasonType();
        const char* esp_reason = getEspResetReason();
        esp_reset_reason_t hw = esp_reset_reason();

        // Auto-classify crashes the software did NOT tag itself. If the
        // hardware reports a panic or watchdog but our NVS reason is still
        // NORMAL (i.e. no subsystem called storeResetReason before the
        // crash), surface it as SYSTEM_ERROR / WATCHDOG so the user is not
        // told "Normaler Start" right after an unexplained reboot. This is
        // the path that catches task-watchdog timeouts and unhandled
        // exceptions — both of which look like "device crashed after some
        // time" with no prior log.
        if (stored_reason == RESET_REASON_NORMAL) {
            if (hw == ESP_RST_PANIC) {
                stored_reason = RESET_REASON_SYSTEM_ERROR;
            } else if (hw == ESP_RST_TASK_WDT || hw == ESP_RST_INT_WDT ||
                       hw == ESP_RST_WDT) {
                stored_reason = RESET_REASON_WATCHDOG;
            } else if (hw == ESP_RST_BROWNOUT) {
                stored_reason = RESET_REASON_BROWNOUT;
            }
        }

        if (stored_reason != RESET_REASON_NORMAL) {
            const char *diag = getLastDiag();
            // Copy diag into a local buffer whose size GCC can see, so the
            // combined "reason (esp_reason) - diag" snprintf below provably
            // fits reset_text_buffer[256] (96 + 32 + 32 + literal overhead).
            // Without this copy, -Wformat-truncation fires because getLastDiag
            // returns const char* with no length the compiler can reason about.
            char diag_bounded[96];
            diag_bounded[0] = '\0';
            if (diag && diag[0]) {
                strncpy(diag_bounded, diag, sizeof(diag_bounded) - 1);
                diag_bounded[sizeof(diag_bounded) - 1] = '\0';
            } else {
                // No subsystem left a stored diag (the heap_watchdog only
                // stores one when IT triggers a clean restart). For a sudden
                // watchdog/panic/brownout reset there is no prior hint, so
                // surface the RTC crash black box: the last heap sample taken
                // before the reset. This is the only way to tell whether heap
                // exhaustion preceded an Interrupt-Watchdog reboot (#362),
                // given that coredump is disabled and the RAM log is lost.
                if (hw == ESP_RST_INT_WDT || hw == ESP_RST_TASK_WDT ||
                    hw == ESP_RST_WDT || hw == ESP_RST_PANIC ||
                    hw == ESP_RST_BROWNOUT) {
                    // The op-tag flight recorder answers "what was the
                    // firmware doing", which is strictly more actionable than
                    // the heap snapshot below (that only rules heap exhaustion
                    // in or out). Prefer it whenever an operation was still
                    // in flight — pushed by NvsStorageLock/g_net_fetch_mutex
                    // but never popped — when the reset hit.
                    const char *stuck_op = crash_blackbox_describe_stuck_op();
                    const crash_blackbox_t *bb = crash_blackbox_read();
                    if (stuck_op) {
                        if (bb) {
                            snprintf(diag_bounded, sizeof(diag_bounded),
                                     "stuck op: %s (up=%us free=%u)",
                                     stuck_op, (unsigned)bb->uptime_s,
                                     (unsigned)bb->free_heap);
                        } else {
                            snprintf(diag_bounded, sizeof(diag_bounded),
                                     "stuck op: %s", stuck_op);
                        }
                        ESP_LOGI(TAG,
                                 "Crash black box: operation still in flight "
                                 "at reset time: %s", stuck_op);
                    } else if (bb) {
                        snprintf(diag_bounded, sizeof(diag_bounded),
                                 "pre-crash heap: free=%u larg=%u min=%u int=%u up=%us",
                                 (unsigned)bb->free_heap,
                                 (unsigned)bb->largest_block,
                                 (unsigned)bb->min_heap,
                                 (unsigned)bb->internal_free,
                                 (unsigned)bb->uptime_s);
                        ESP_LOGI(TAG,
                                 "Crash black box (last sample before reset): "
                                 "free=%u largest=%u min_ever=%u internal=%u "
                                 "uptime=%us low_streak=%u samples=%u",
                                 (unsigned)bb->free_heap,
                                 (unsigned)bb->largest_block,
                                 (unsigned)bb->min_heap,
                                 (unsigned)bb->internal_free,
                                 (unsigned)bb->uptime_s,
                                 (unsigned)bb->low_streak,
                                 (unsigned)bb->sample_count);
                    }
                    if (bb) {
                        // Tick sentinel readout — the discriminating fact for
                        // issue #362: the interrupt watchdog can only trip
                        // when a core stops ticking for >300 ms, so WHICH
                        // core went silent (and how it relates to the last
                        // heap sample) narrows the fault class decisively.
                        // Deltas are against the last heap sample (sampled
                        // every 60 s); both clocks are uptime-based ms, so
                        // expect a few ms of benign offset.
                        if (bb->tick_magic == CRASH_BLACKBOX_TICK_MAGIC) {
                            const int32_t sample_ms =
                                (int32_t)(bb->uptime_s * 1000u);
                            const int32_t d0 =
                                (int32_t)bb->last_tick_ms[0] - sample_ms;
                            const int32_t d1 =
                                (int32_t)bb->last_tick_ms[1] - sample_ms;
                            ESP_LOGI(TAG,
                                     "Tick sentinel: cpu0 last tick at %u ms "
                                     "(%+d ms vs sample), cpu1 at %u ms "
                                     "(%+d ms vs sample)",
                                     (unsigned)bb->last_tick_ms[0], (int)d0,
                                     (unsigned)bb->last_tick_ms[1], (int)d1);
                        }
                        // Pre-reset values (Beta.5+): these are the ones that
                        // matter. The line above shows the CURRENT session's
                        // ticks and is kept only for sanity checks.
                        if (bb->prev_tick_magic == CRASH_BLACKBOX_TICK_MAGIC) {
                            const int32_t sample_ms =
                                (int32_t)(bb->uptime_s * 1000u);
                            const int32_t d0 =
                                (int32_t)bb->prev_tick_ms[0] - sample_ms;
                            const int32_t d1 =
                                (int32_t)bb->prev_tick_ms[1] - sample_ms;
                            const int32_t core_gap =
                                (int32_t)bb->prev_tick_ms[0] -
                                (int32_t)bb->prev_tick_ms[1];
                            ESP_LOGI(TAG,
                                     "Tick sentinel (pre-reset): cpu0 last "
                                     "tick at %u ms (%+d ms vs sample), cpu1 "
                                     "at %u ms (%+d ms vs sample), core gap "
                                     "%+d ms",
                                     (unsigned)bb->prev_tick_ms[0], (int)d0,
                                     (unsigned)bb->prev_tick_ms[1], (int)d1,
                                     (int)core_gap);
                        }
                        crash_blackbox_clear();
                    }
                }
            }

            if (diag_bounded[0]) {
                snprintf(reset_text_buffer, sizeof(reset_text_buffer),
                         "%s (%s) - %s",
                         get_reason_text(stored_reason), esp_reason, diag_bounded);
            } else {
                snprintf(reset_text_buffer, sizeof(reset_text_buffer), "%s (%s)",
                         get_reason_text(stored_reason), esp_reason);
            }
            clearResetReason();
        } else {
            snprintf(reset_text_buffer, sizeof(reset_text_buffer), "%s", esp_reason);
        }
    }

    return reset_text_buffer;
}

void ResetInfo::clearResetReason() {
    NvsStorageLock storage_lock(portMAX_DELAY, "reset_info.clear_reason");
    if (!storage_lock) return;

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return;
    }

    err = nvs_erase_key(nvs_handle, NVS_KEY);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to erase reset reason: %s", esp_err_to_name(err));
    }
    // Best-effort erase of the diagnostic string; ignore not-found.
    nvs_erase_key(nvs_handle, NVS_DIAG_KEY);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
}
