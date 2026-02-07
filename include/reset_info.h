/*
 *  reset_info.h is part of the HB-RF-ETH firmware v2.1
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

    // Get detailed reset reason description
    static const char* getResetReasonText();

    // Get the raw reset reason type
    static reset_reason_type_t getResetReasonType();

    // Get the ESP reset reason (hardware level)
    static const char* getEspResetReason();

    // Get combined detailed message
    static const char* getResetDetails();

    // Clear stored reset reason (call after reading)
    static void clearResetReason();
};
