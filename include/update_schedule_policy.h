/*
 *  update_schedule_policy.h is part of the HB-RF-ETH firmware v2.0
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

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Scheduling policy for the automatic update path (AUTO_UPDATE_PLAN.md §3.1,
// §4.3, §4.5, §6.1 — stages B2/C/D).
//
// Header-only and free of ESP-IDF dependencies so the exact boundaries are
// unit-tested on the host in CI (test/host/test_update_schedule_policy.cpp),
// following the established pattern of updatecheck_heap_policy.h. The module
// deliberately contains no counters or state of its own: the scheduler owns
// the mutable state (hour counter, failure streak, boot count) and asks these
// pure functions what to do, which keeps every rule reviewable in isolation.
//
// Rules mirrored from the plan (binding, not advisory):
//  - No long single delay: the firmware ticks hourly and compares hours
//    against hours (rule 5); nothing here ever converts days to ticks.
//  - First check 10 min + (MAC[5] % 60) after boot, deterministic per device
//    so Pages sees no synchronized fleet spike (rule 14).
//  - Failed checks back off 1h -> 2h -> 4h (three early retries), then the
//    full configured interval; skips (busy gates) never count as failures.
//  - A skipped cycle stays silent; an install that WANTS to run but may not
//    names its blocker (§4.3) so the WebUI can explain the wait.

// --- Modes and their bounds (§6.1) -----------------------------------------

typedef enum
{
    UPDATE_MODE_OFF = 0,       // No scheduled activity whatsoever (default on
                               // existing devices; a firmware update must never
                               // switch on uninvited outbound connections).
    UPDATE_MODE_NOTIFY = 1,    // Scheduled search only; nothing is ever flashed.
    UPDATE_MODE_AUTO_WEBUI = 2,// Search + automatic WebUI install in the window.
    UPDATE_MODE_AUTO_ALL = 3   // Search + automatic firmware (then WebUI via the
                               // two-stage marker) install in the window.
} update_mode_t;

static inline bool update_mode_valid(int mode)
{
    return mode == UPDATE_MODE_OFF || mode == UPDATE_MODE_NOTIFY ||
           mode == UPDATE_MODE_AUTO_WEBUI || mode == UPDATE_MODE_AUTO_ALL;
}

// The mode is persisted in Settings; these strings are the wire format of
// GET/POST /api/update/config.
static inline const char *update_mode_name(int mode)
{
    switch (mode) {
        case UPDATE_MODE_NOTIFY: return "notify";
        case UPDATE_MODE_AUTO_WEBUI: return "auto_webui";
        case UPDATE_MODE_AUTO_ALL: return "auto_all";
        default: return "off";
    }
}

static inline int update_mode_from_name(const char *name)
{
    if (name == NULL) return UPDATE_MODE_OFF;
    if (strcmp(name, "notify") == 0) return UPDATE_MODE_NOTIFY;
    if (strcmp(name, "auto_webui") == 0) return UPDATE_MODE_AUTO_WEBUI;
    if (strcmp(name, "auto_all") == 0) return UPDATE_MODE_AUTO_ALL;
    return UPDATE_MODE_OFF;
}

// --- Interval and window bounds (§6.1) --------------------------------------

#define UPDATE_INTERVAL_MIN_HOURS 6
#define UPDATE_INTERVAL_MAX_HOURS 168

static inline bool update_interval_valid(int hours)
{
    return hours >= UPDATE_INTERVAL_MIN_HOURS && hours <= UPDATE_INTERVAL_MAX_HOURS;
}

static inline bool update_window_start_valid(int hour)
{
    return hour >= 0 && hour <= 23;
}

static inline bool update_window_len_valid(int hours)
{
    return hours >= 1 && hours <= 24;
}

// A channel is the manifest file name; anything else must be rejected before
// it can reach a URL format string.
static inline bool update_channel_name_valid(const char *channel)
{
    return channel != NULL &&
           (strcmp(channel, "stable") == 0 || strcmp(channel, "beta") == 0);
}

// --- First check and backoff ladder (§3.1) ----------------------------------

/**
 * @brief Minutes after boot before the first scheduled check.
 *
 * 10 min minimum (link-up, NTP, monitoring start) plus a deterministic
 * per-device jitter of 0..59 min derived from the last MAC octet.
 */
static inline uint32_t update_schedule_first_delay_minutes(uint8_t mac_last_octet)
{
    return 10u + ((uint32_t)mac_last_octet % 60u);
}

/**
 * @brief Hours to wait before the next scheduled check after failures.
 *
 * @param consecutive_failures Failed checks since the last success. Skipped
 *                             cycles (busy gates, off-mode) do not count.
 * @param interval_hours       Configured interval; assumed already validated.
 *
 * Three accelerated retries (1 h, 2 h, 4 h), then the scheduler stops
 * retrying early and waits the full interval. The plan's 8-h rung is
 * unreachable by design: the minimum configurable interval (6 h) is above
 * the retry ladder, so the cap keeps intervals authoritative.
 */
static inline uint32_t update_schedule_delay_after_failures(uint32_t consecutive_failures,
                                                            uint32_t interval_hours)
{
    if (interval_hours < UPDATE_INTERVAL_MIN_HOURS) {
        interval_hours = UPDATE_INTERVAL_MIN_HOURS;
    }
    if (consecutive_failures == 0 || consecutive_failures > 3) {
        return interval_hours;
    }
    const uint32_t backoff = 1u << (consecutive_failures - 1); // 1, 2, 4
    return backoff < interval_hours ? backoff : interval_hours;
}

// --- Maintenance window and gates (§4.3, §6.1) ------------------------------

/**
 * @brief Whether the local wall-clock hour falls into the window.
 *
 * The window wraps midnight: start=23, len=4 covers 23,0,1,2. len=24 covers
 * the whole day. An out-of-range hour argument is never "inside".
 */
static inline bool update_window_contains(int local_hour, int start_hour, int length_hours)
{
    if (local_hour < 0 || local_hour > 23) return false;
    if (!update_window_start_valid(start_hour)) return false;
    if (!update_window_len_valid(length_hours)) return false;
    const int offset = (local_hour - start_hour + 24) % 24;
    return offset < length_hours;
}

// Same sanity floor as update_check.cpp: a wall clock before this epoch is
// "never synced", not "a long time ago".
#define UPDATE_SCHEDULE_MIN_VALID_UNIX 1600000000LL

static inline bool update_schedule_time_valid(int64_t unix_seconds)
{
    return unix_seconds > UPDATE_SCHEDULE_MIN_VALID_UNIX;
}

/**
 * @brief Whether the CCU relay session has been quiet long enough for an
 *        unattended install (§6.1 upd_idle_only: >= 5 min).
 *
 * @param session_active  raw_uart_session_active(): a connected CCU blocks.
 * @param rx_age_seconds  Seconds since the last raw-UART datagram; a session
 *                        that just dropped must not count as idle yet.
 */
static inline bool update_schedule_idle_ok(bool session_active, uint32_t rx_age_seconds)
{
    if (session_active) return false;
    return rx_age_seconds >= (5u * 60u);
}

// --- Install decision (§7 stages C/D) ---------------------------------------

typedef enum
{
    UPDATE_AUTO_ACTION_NONE = 0,
    UPDATE_AUTO_ACTION_INSTALL_WEBUI,
    UPDATE_AUTO_ACTION_INSTALL_FIRMWARE
} update_auto_action_t;

/**
 * @brief What the scheduler may install unattended after a successful check.
 *
 * Firmware precedes WebUI (§4.5): AUTO_ALL installs the firmware first and
 * the WebUI follows after the reboot via the two-stage marker. AUTO_WEBUI
 * never touches the firmware. Outside the window, without a valid local
 * time (§4.3) or without an idle relay session, nothing is installed — the
 * check itself keeps running and the operator can always install manually.
 */
static inline update_auto_action_t update_schedule_auto_action(int mode,
                                                               bool firmware_available,
                                                               bool webui_available,
                                                               bool in_window,
                                                               bool time_valid,
                                                               bool idle)
{
    if (mode != UPDATE_MODE_AUTO_WEBUI && mode != UPDATE_MODE_AUTO_ALL) {
        return UPDATE_AUTO_ACTION_NONE;
    }
    if (!in_window || !time_valid || !idle) {
        return UPDATE_AUTO_ACTION_NONE;
    }
    if (mode == UPDATE_MODE_AUTO_ALL && firmware_available) {
        return UPDATE_AUTO_ACTION_INSTALL_FIRMWARE;
    }
    if (webui_available) {
        return UPDATE_AUTO_ACTION_INSTALL_WEBUI;
    }
    return UPDATE_AUTO_ACTION_NONE;
}

// --- Why an install is waiting (§4.3) ---------------------------------------

typedef enum
{
    UPDATE_INSTALL_BLOCKED_NONE = 0,
    UPDATE_INSTALL_BLOCKED_NO_VALID_TIME, // §4.3: postponed, not failed
    UPDATE_INSTALL_BLOCKED_OUTSIDE_WINDOW,
    UPDATE_INSTALL_BLOCKED_SESSION_ACTIVE,
    UPDATE_INSTALL_BLOCKED_BUSY
} update_install_blocked_t;

static inline const char *update_install_blocked_name(update_install_blocked_t blocked)
{
    switch (blocked) {
        case UPDATE_INSTALL_BLOCKED_NO_VALID_TIME: return "no_valid_time";
        case UPDATE_INSTALL_BLOCKED_OUTSIDE_WINDOW: return "outside_window";
        case UPDATE_INSTALL_BLOCKED_SESSION_ACTIVE: return "session_active";
        case UPDATE_INSTALL_BLOCKED_BUSY: return "busy";
        default: return NULL;
    }
}

/**
 * @brief Classify why a WANTED install (auto mode or a scheduled pending
 *        install inside the window) cannot run yet.
 *
 * Time validity outranks the window: without a trustworthy clock the window
 * is meaningless (§4.3), so the reason names the clock first.
 */
static inline update_install_blocked_t update_schedule_install_blocked(bool wanted,
                                                                      bool time_valid,
                                                                      bool in_window,
                                                                      bool idle,
                                                                      bool busy)
{
    if (!wanted) return UPDATE_INSTALL_BLOCKED_NONE;
    if (busy) return UPDATE_INSTALL_BLOCKED_BUSY;
    if (!time_valid) return UPDATE_INSTALL_BLOCKED_NO_VALID_TIME;
    if (!in_window) return UPDATE_INSTALL_BLOCKED_OUTSIDE_WINDOW;
    if (!idle) return UPDATE_INSTALL_BLOCKED_SESSION_ACTIVE;
    return UPDATE_INSTALL_BLOCKED_NONE;
}

// --- Two-stage marker "firmware now, WebUI after reboot" (§4.5) -------------

#define UPDATE_STAGE_PREFIX "waf|" // webui_after_fw
#define UPDATE_STAGE_MAX_BYTES 96

/**
 * @brief Serialize the marker as "waf|<fwVersion>|<uiVersion>".
 * @return true when the result fits into size bytes including the NUL.
 */
static inline bool update_stage_format(char *out, size_t size,
                                       const char *fw_version, const char *ui_version)
{
    if (out == NULL || size == 0) return false;
    const int written = snprintf(out, size, "%s%s|%s", UPDATE_STAGE_PREFIX,
                                 fw_version != NULL ? fw_version : "",
                                 ui_version != NULL ? ui_version : "");
    return written > 0 && (size_t)written < size;
}

/**
 * @brief Parse a stored marker back into its version fields.
 * @return false for wrong prefix, missing fields or truncation.
 */
static inline bool update_stage_parse(const char *value, char *fw_version, size_t fw_size,
                                      char *ui_version, size_t ui_size)
{
    if (value == NULL || fw_version == NULL || ui_version == NULL) return false;
    const size_t prefix_len = strlen(UPDATE_STAGE_PREFIX);
    if (strncmp(value, UPDATE_STAGE_PREFIX, prefix_len) != 0) return false;

    const char *fw = value + prefix_len;
    const char *sep = strchr(fw, '|');
    if (sep == NULL) return false;

    const size_t fw_len = (size_t)(sep - fw);
    const char *ui = sep + 1;
    const size_t ui_len = strlen(ui);
    if (fw_len == 0 || ui_len == 0) return false;
    if (fw_len >= fw_size || ui_len >= ui_size) return false;

    memcpy(fw_version, fw, fw_len);
    fw_version[fw_len] = '\0';
    memcpy(ui_version, ui, ui_len);
    ui_version[ui_len] = '\0';
    return true;
}

/**
 * @brief Whether the marker obliges this boot to install the WebUI.
 *
 * boots_since_write is 0 on the boot the marker was written (the install
 * reboot is still ahead), 1 on the first boot of the new firmware, 2 on the
 * next. The plan's expiry is two boots; a rollback invalidates the marker
 * because the running firmware no longer matches the one the operator's
 * WebUI version was staged for — the marker is then stale and must be
 * cleaned up instead of acted upon.
 */
static inline bool update_stage_applicable(uint32_t boots_since_write,
                                           const char *stage_fw_version,
                                           const char *running_fw_version)
{
    if (boots_since_write < 1 || boots_since_write > 2) return false;
    if (stage_fw_version == NULL || running_fw_version == NULL) return false;
    return strcmp(stage_fw_version, running_fw_version) == 0;
}

// --- Scheduled (pending) one-shot install -----------------------------------

#define UPDATE_PENDING_MAX_BYTES 80
#define UPDATE_PENDING_EXPIRY_SECONDS (7LL * 24LL * 3600LL)

/**
 * @brief Serialize a pending install as "fw|<version>|<unix>" or "ui|<version>|<unix>".
 */
static inline bool update_pending_format(char *out, size_t size, bool firmware_target,
                                         const char *version, int64_t created_unix)
{
    if (out == NULL || size == 0 || version == NULL) return false;
    const int written = snprintf(out, size, "%s|%s|%lld",
                                 firmware_target ? "fw" : "ui", version,
                                 (long long)created_unix);
    return written > 0 && (size_t)written < size;
}

/**
 * @brief Parse a stored pending entry.
 */
static inline bool update_pending_parse(const char *value, bool *firmware_target,
                                        char *version, size_t version_size,
                                        int64_t *created_unix)
{
    if (value == NULL || firmware_target == NULL || version == NULL ||
        created_unix == NULL) {
        return false;
    }
    const char *sep1 = strchr(value, '|');
    if (sep1 == NULL || sep1 == value) return false;
    const size_t target_len = (size_t)(sep1 - value);
    if (target_len != 2) return false;
    if (value[0] == 'f' && value[1] == 'w') {
        *firmware_target = true;
    } else if (value[0] == 'u' && value[1] == 'i') {
        *firmware_target = false;
    } else {
        return false;
    }

    const char *ver = sep1 + 1;
    const char *sep2 = strchr(ver, '|');
    if (sep2 == NULL) return false;
    const size_t ver_len = (size_t)(sep2 - ver);
    if (ver_len == 0 || ver_len >= version_size) return false;

    const long long created = strtoll(sep2 + 1, NULL, 10);
    if (created <= 0) return false;

    memcpy(version, ver, ver_len);
    version[ver_len] = '\0';
    *created_unix = (int64_t)created;
    return true;
}

/**
 * @brief Whether a pending entry is still actionable. Entries older than
 *        seven days expire so a forgotten schedule cannot flash a weeks-old
 *        image unannounced; the manifest check inside the installer remains
 *        authoritative either way (it re-verifies version, size and digest).
 */
static inline bool update_pending_valid(int64_t created_unix, int64_t now_unix)
{
    if (!update_schedule_time_valid(created_unix) ||
        !update_schedule_time_valid(now_unix)) {
        return false;
    }
    return (now_unix - created_unix) <= UPDATE_PENDING_EXPIRY_SECONDS;
}
