/*
 *  reset_info.cpp is part of the HB-RF-ETH firmware v2.1
 *
 *  Modified work Copyright 2025 Xerolux
 *  Modernized fork - Updated to ESP-IDF 5.1 and modern toolchains
 *
 *  The HB-RF-ETH firmware is licensed under a
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 *
 *  You should have received a copy of the license along with this
 *  work.  If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
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

// Buffer for reset reason text
static char reset_text_buffer[256];

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
        nvs_commit(nvs_handle);
        ESP_LOGI(TAG, "Stored reset reason: %d", reason);
    }
    nvs_close(nvs_handle);
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

        if (stored_reason != RESET_REASON_NORMAL) {
            snprintf(reset_text_buffer, sizeof(reset_text_buffer), "%s (%s)",
                     get_reason_text(stored_reason), esp_reason);
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

    nvs_erase_key(nvs_handle, NVS_KEY);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
}
