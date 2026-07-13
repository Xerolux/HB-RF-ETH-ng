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
#include "nvs.h"
#include "esp_system.h"
#include "esp_log.h"
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
            if (diag && diag[0]) {
                strncpy(diag_bounded, diag, sizeof(diag_bounded) - 1);
                diag_bounded[sizeof(diag_bounded) - 1] = '\0';
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
