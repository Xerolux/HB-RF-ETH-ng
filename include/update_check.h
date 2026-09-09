/*
 *  update_check.h is part of the HB-RF-ETH firmware v2.0
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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Manual update search.
//
// The device fetches one small, fixed-shape manifest published alongside the
// releases and compares its versions against the running firmware and WebUI.
// It never downloads or installs anything - this stage only reports.
//
// Deliberately manual: there is no timer and no background activity. A fetch
// happens only when an administrator asks for one, so an unattended device
// never opens an outbound connection on its own. The automatic schedule is a
// separate, later step (docs/AUTO_UPDATE_PLAN.md, stage B2).

#define UPDATE_CHECK_VERSION_LEN 32
#define UPDATE_CHECK_URL_LEN 128
#define UPDATE_CHECK_REASON_LEN 96

enum update_check_state_t {
    UPDATE_CHECK_IDLE = 0,
    UPDATE_CHECK_RUNNING,
};

struct UpdateCheckStatus {
    update_check_state_t state;
    bool everChecked;
    // Wall-clock seconds of the last completed fetch, 0 when never or when the
    // clock was not yet synchronised.
    int64_t lastCheckUnix;
    char channel[8]; // "stable" or "beta"

    char latestFirmware[UPDATE_CHECK_VERSION_LEN];
    char latestWebui[UPDATE_CHECK_VERSION_LEN];
    char notesUrl[UPDATE_CHECK_URL_LEN];
    bool firmwareUpdateAvailable;
    bool webuiUpdateAvailable;

    // Exactly one of these may be set after a completed attempt. They are kept
    // apart on purpose: a skip is not a failure, and neither may ever be
    // rendered as "up to date" - that conflation was a real defect once.
    char lastError[UPDATE_CHECK_REASON_LEN];
    char lastSkipReason[UPDATE_CHECK_REASON_LEN];
};

enum update_check_trigger_result_t {
    UPDATE_CHECK_TRIGGER_ACCEPTED = 0,
    UPDATE_CHECK_TRIGGER_BUSY,      // a fetch is already running
    UPDATE_CHECK_TRIGGER_COOLDOWN,  // asked again too soon
    UPDATE_CHECK_TRIGGER_NO_WORKER, // the worker task could not be created
};

/** Prepare internal state. Starts nothing and touches no network. */
void update_check_init();

/**
 * @brief Ask for one manifest fetch.
 *
 * Returns immediately; the work runs on a short-lived worker task so an HTTP
 * server thread is never held for the duration of a TLS exchange. Poll
 * update_check_get_status() for the outcome.
 */
update_check_trigger_result_t update_check_trigger(const char *channel);

/** Snapshot of the current state. Safe to call from any task. */
UpdateCheckStatus update_check_get_status();

/** True when `channel` is one this firmware knows how to fetch. */
bool update_check_valid_channel(const char *channel);
