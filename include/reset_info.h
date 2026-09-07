/*
 *  reset_info.h is part of the HB-RF-ETH firmware v2.0
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

#pragma once

#include <cstdint>

// Reset reason types stored in NVS
typedef enum {
    RESET_REASON_NORMAL = 0,        // Normal power-on
    RESET_REASON_USER_RESTART = 1,  // User clicked "Restart"
    RESET_REASON_FACTORY_RESET = 2, // User clicked "Factory Reset"
    RESET_REASON_FIRMWARE_UPDATE = 3, // After firmware update
    RESET_REASON_UPDATE_FAILED = 4,  // Firmware update failed
    RESET_REASON_SYSTEM_ERROR = 5,   // System error/panic
    RESET_REASON_BROWNOUT = 6,       // Power brownout
    RESET_REASON_WATCHDOG = 7,       // Watchdog reset
    RESET_REASON_UNKNOWN = 255
} reset_reason_type_t;

class ResetInfo {
public:
    // Initialize the reset info system
    static void init();

    // Store reset reason before restart
    static void storeResetReason(reset_reason_type_t reason);

    // Store reset reason plus a short diagnostic string (free heap, largest
    // block, uptime, ...) so that after the reboot the user can see WHY the
    // device restarted. The diagnostic survives the reboot in NVS and is
    // surfaced through the WebUI "last reset details" field. Max ~240 chars
    // — longer strings are truncated.
    static void storeResetReason(reset_reason_type_t reason, const char *diag);

    // Get detailed reset reason description
    static const char* getResetReasonText();

    // Get the raw reset reason type
    static reset_reason_type_t getResetReasonType();

    // Get the ESP reset reason (hardware level)
    static const char* getEspResetReason();

    // Raw RTC reset cause of CPU0 (esp_rom_get_reset_reason), as a short
    // name. For interrupt-watchdog resets this distinguishes the two paths
    // that esp_reset_reason() folds into ESP_RST_INT_WDT: SW_CPU (the stage-0
    // watchdog interrupt was serviced and the panic handler rebooted the
    // chip - a transcript exists) versus TG1WDT_SYS (stage-1 hard reset: the
    // stalled core never took the level-4 interrupt). Issue #362.
    static const char* getRtcResetCause();

    // Get combined detailed message
    static const char* getResetDetails();

    // Returns the stored diagnostic string for the last non-normal reset, or
    // an empty string if none was stored. The buffer is internal and valid
    // until the next call.
    static const char* getLastDiag();

    // Clear stored reset reason (call after reading)
    static void clearResetReason();
};
