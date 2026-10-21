/*
 *  update_scheduler.h is part of the HB-RF-ETH firmware v2.0
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
#include <stdint.h>

class Settings;

// Scheduled automatic updates (AUTO_UPDATE_PLAN.md stages B2/C/D) plus the
// operator-scheduled one-shot install ("Im Wartungsfenster installieren").
//
// The scheduler owns exactly three jobs:
//  1. fire update checks on the interval/jitter/backoff plan (mode != off),
//  2. install what a check found once the maintenance window/time/idle gates
//     open — either continuously in an auto mode, or once for a scheduled
//     pending entry,
//  3. complete a two-stage firmware->WebUI update after the reboot.
//
// All rules live in the host-tested update_schedule_policy.h; this module
// only owns counters and I/O. It creates no permanent task: an hourly
// esp_timer (plus one first-check timer) spawns a transient tick task that
// deletes itself, exactly like the check/install workers.

typedef struct
{
    // Operator-scheduled one-shot install (NVS-backed, survives reboots).
    bool pendingValid;
    bool pendingFirmware;     // false = WebUI target
    char pendingVersion[32];  // version at schedule time; the installer
                              // re-reads the manifest and stays authoritative.
    // Two-stage marker: a firmware install was started and the WebUI still
    // has to follow on a later boot of that firmware.
    bool stageValid;
    char stageFwVersion[32];
    char stageUiVersion[32];
    // Estimated next scheduled check (unix seconds, TZ-independent); 0 when
    // nothing is scheduled yet (mode off before the first tick, or no clock).
    int64_t nextCheckUnix;
    // Persisted record of the last successful check (survives reboots so the
    // WebUI can show something before the first fresh search).
    int64_t lastCheckUnix;
    char seenFirmware[32];
    char seenWebui[32];
} UpdateSchedulerStatus;

// Called once from app_main after update_check_init()/update_install_init().
// Creates the timers, increments the boot counter and loads the NVS-backed
// pending/stage state. Safe to call before the network is up: nothing here
// talks to the network, and the first check is >= 10 minutes away.
void update_scheduler_init(Settings *settings);

// Snapshot for /api/update/status. Cheap; takes no locks beyond the module
// mutex and performs no I/O.
UpdateSchedulerStatus update_scheduler_get_status();

// Schedule the one-shot install ("target" resolved from the last search
// result). Returns false when the version string is missing/oversized.
bool update_scheduler_set_pending(bool firmware_target, const char *version);

// Cancel the scheduled one-shot install. Returns true when an entry existed.
bool update_scheduler_clear_pending();

// Why a wanted install cannot run yet; NULL when nothing is blocking.
// Computed live from the clock, window, CCU session and operation gates.
const char *update_scheduler_install_blocked_reason();
