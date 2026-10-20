/*
 *  update_install.h is part of the HB-RF-ETH firmware v2.0
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

// Online installation (docs/AUTO_UPDATE_PLAN.md, module 2 — manual path).
//
// Downloads the release artifact named by the device manifest over HTTPS and
// streams it into the existing flash sinks (esp_ota_* for firmware,
// webui_storage_update_* for the WebUI image), verifying SHA-256 against the
// manifest before anything is activated. Deliberately manual: this module
// runs only when an administrator asked for it — the automatic schedule is a
// separate, later stage of the plan.
//
// The caller never supplies a URL. The download URL comes exclusively from
// the manifest and is re-checked against the host allowlist
// (update_url_policy.h) before the first byte is fetched.

#define UPDATE_INSTALL_VERSION_LEN 32
#define UPDATE_INSTALL_REASON_LEN  96

enum update_install_state_t {
    UPDATE_INSTALL_IDLE = 0,
    UPDATE_INSTALL_PREPARING,   // gates, manifest fetch, validation
    UPDATE_INSTALL_DOWNLOADING, // streaming into the sink
    UPDATE_INSTALL_VERIFYING,   // SHA-256 comparison, image activation
    UPDATE_INSTALL_FINALIZING,  // firmware: restart imminent; webui: remount
    UPDATE_INSTALL_SUCCESS,
    UPDATE_INSTALL_ERROR,
};

enum update_install_target_t {
    UPDATE_INSTALL_TARGET_FIRMWARE = 0,
    UPDATE_INSTALL_TARGET_WEBUI,
};

struct UpdateInstallStatus {
    update_install_state_t state;
    update_install_target_t target;
    uint8_t progress; // 0-100 while DOWNLOADING
    char targetVersion[UPDATE_INSTALL_VERSION_LEN];
    char error[UPDATE_INSTALL_REASON_LEN];
};

enum update_install_result_t {
    UPDATE_INSTALL_ACCEPTED = 0,
    UPDATE_INSTALL_BUSY,           // an installation is already running
    UPDATE_INSTALL_INVALID_TARGET, // target was neither "firmware" nor "webui"
    UPDATE_INSTALL_NO_WORKER,      // the worker task could not be created
};

/** Prepare internal state. Starts nothing and touches no network. */
void update_install_init();

/**
 * @brief Ask for one installation run.
 *
 * Returns immediately; the work runs on a short-lived worker task so no HTTP
 * server thread is held for the duration of the download. Poll
 * update_install_get_status() for progress and outcome.
 */
update_install_result_t update_install_start(const char *target);

/** Snapshot of the current state. Safe to call from any task. */
UpdateInstallStatus update_install_get_status();

/** Wire name of a target ("firmware"/"webui"), or NULL when unknown. */
const char *update_install_target_name(update_install_target_t target);
