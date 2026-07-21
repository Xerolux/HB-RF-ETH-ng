/*
 *  updatecheck.h is part of the HB-RF-ETH firmware v2.0
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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "sysinfo.h"
#include "led.h"
#include "settings.h"
#include <atomic>

struct WebUIReleaseInfo {
    bool valid = false;
    char version[32] = {0};
    char design[16] = {0};
    int apiVersion = 0;
    char minFirmwareVersion[32] = {0};
    char downloadUrl[256] = {0};
    char sha256[65] = {0};
    uint32_t size = 0;
    char partition[16] = {0};
    int format = 0;
    char releaseUrl[256] = {0};
    char publishedAt[32] = {0};
};

// Snapshot of the latest release known to the firmware.
// All fields are safe to copy by value.
struct ReleaseInfo {
    bool valid;                 // true once a successful fetch has populated the fields
    char version[32];           // normalized, e.g. "2.1.11" or "2.2.0-beta.1"
    char downloadUrl[256];      // firmware binary URL advertised by the manifest
    char sha256[65];            // expected SHA-256 of the firmware binary
    char releaseUrl[256];       // html_url of the release (view on GitHub)
    char publishedAt[32];       // ISO timestamp from GitHub
    bool isPrerelease;          // matches GitHub "prerelease" flag
    bool betaChannel;           // channel used to populate this cache
    WebUIReleaseInfo webui;     // optional WebUI block from the same manifest
    char body[1024];            // compact release-note excerpt or notes URL
    int64_t fetchedAtMs;        // epoch millis of the last successful fetch
    char error[128];            // human-readable last error (empty when valid)
};

// OTA state machine, published via MQTT status/ota_state. Thread-safe
// snapshot via getOtaState(). The MQTT / web layers read this without
// locking; updates go through the internal mutex.
typedef enum {
    OTA_STATE_IDLE = 0,         // No OTA activity
    OTA_STATE_CHECKING,         // refresh() in flight (querying GitHub)
    OTA_STATE_STARTING,         // esp_https_ota_begin about to be called
    OTA_STATE_DOWNLOADING,      // esp_https_ota_perform loop running
    OTA_STATE_FLASHING,         // Writing final chunks + switching partition
    OTA_STATE_SUCCESS,          // OTA finished, restart pending
    OTA_STATE_FAILED,           // OTA failed; check ota_error for details
} ota_state_t;

struct OtaSnapshot {
    ota_state_t state = OTA_STATE_IDLE;
    int progress_pct = 0;       // 0..100 (bytes downloaded / image size); -1 if unknown
    int error_code = 0;         // esp_err_t value when state == OTA_STATE_FAILED
    char error_text[64] = {0};  // human readable, e.g. "ESP_ERR_OTA_VALIDATE_FAILED"
};

// Lightweight release snapshot — omits URLs, WebUI metadata and the note
// excerpt so periodic callers do not copy the full cached manifest on stack.
// WebUI callers use
// the full ReleaseInfo via getReleaseInfo().
struct VersionSnapshot {
    bool valid = false;
    char version[32] = "n/a";
    bool isPrerelease = false;
    bool webuiValid = false;
    char webuiVersion[32] = {0};
    char error[128] = {0};
};

class UpdateCheck
{
private:
    SysInfo* _sysInfo;
    LED *_statusLED;
    Settings* _settings;
    // Persistent, staggered timer replacing the former always-sleeping task.
    // The short-lived worker is created only when heap checks pass and the
    // stored 24-hour window is due.
    // resident for the ~5 s of the actual check.
    esp_timer_handle_t _periodicTimer = NULL;
    esp_timer_handle_t _initialTimer = NULL;

    // One-shot timer armed by triggerManualFetch(). It defers the real fetch
    // off the httpd worker thread (a GitHub TLS handshake can take several
    // seconds) so POST /api/check_update can answer immediately while the
    // manifest fetch runs on its own short-lived task like the daily check.
    esp_timer_handle_t _manualFetchTimer = NULL;
    // Epoch millis of the last accepted manual trigger, guarded by _stateMutex.
    // Enforces a 60 s manual cooldown independently of the 24 h automatic
    // window so the WebUI "search now" button cannot hammer the manifest.
    int64_t _lastManualFetchMs = 0;

    // Serializes writes/reads of the cached _release snapshot.
    SemaphoreHandle_t _stateMutex = NULL;
    // Held while a network fetch is in progress; prevents concurrent fetches
    // from the background timer and from the OTA path.
    SemaphoreHandle_t _fetchLock = NULL;

    ReleaseInfo _release{};
    // Mirror of _release.version for the legacy const char* accessor.
    char _latestVersion[32] = "n/a";

    // OTA progress / state (guarded by _stateMutex). Read by MQTT publish task
    // and the HTTP layer; written by performOnlineUpdate() running on its own
    // task.
    ota_state_t _otaState = OTA_STATE_IDLE;
    int _otaProgress = -1;
    int _otaErrorCode = 0;
    char _otaErrorText[64] = {0};
    // Shared by WebUI upload/URL OTA and MQTT OTA. An atomic gate is used
    // because the WebUI request task reserves the operation and the worker
    // task releases it after completion.
    std::atomic<bool> _otaInProgress{false};

    bool _doFetch(ReleaseInfo* out);
    void _setOtaStateLocked(ota_state_t state);
    void _setOtaProgressLocked(int percent);
    void _setOtaErrorLocked(int code, const char* text);

public:
    UpdateCheck(Settings* settings, SysInfo* sysInfo, LED *statusLED);
    void start();
    void stop();

    // Perform a synchronous fetch from the static update manifest. Returns true
    // on success, false on network/parse failure or if another fetch is in
    // progress. Respects the configured beta channel setting.
    bool refresh();

    // Performs an online fetch only when the persistent 24 h window is due.
    // Reboots and page visits cannot bypass this limit.
    bool refreshIfDue();

    // Arm an immediate online fetch from a manual WebUI/MQTT request. Bypasses
    // the 24 h window (the user explicitly asked) but enforces a 60 s manual
    // cooldown and never stacks onto a fetch already in progress. The actual
    // GitHub request is deferred to a short-lived task so this never blocks the
    // httpd worker. Returns true when a new fetch was armed, false if one is
    // already running or the cooldown has not elapsed. The 24 h automatic
    // timer is not affected.
    bool triggerManualFetch();

    // Compares the cached release against the running version and drives the
    // status LED (update-available blink). Public because it is called by the
    // free-function _periodic_check_task trampoline in updatecheck.cpp after
    // refresh() completes.
    void _evaluateReleaseInfo();

    // True while a network fetch is in progress (started by the periodic
    // timer or by refresh()). Used by the HTTP layer to check fetch state.
    bool isFetchInProgress();

    // Returns a thread-safe snapshot of the currently cached release info.
    ReleaseInfo getReleaseInfo();

    // Lightweight alternative: omits the 4 KB body / URL fields. Use this
    // from periodic tasks (MQTT publish, background checker) to avoid ~5 KB
    // on the stack.
    VersionSnapshot getVersionSnapshot();

    // Returns a thread-safe snapshot of the OTA state machine. The MQTT layer
    // publishes this as status/ota_state + status/ota_progress so that
    // Home Assistant can show update progress and react on completion.
    OtaSnapshot getOtaState();

    // Serialize every writer of an OTA partition, including WebUI workers.
    // A successful reservation must be paired with finishOtaOperation().
    bool tryBeginOtaOperation();
    void finishOtaOperation();

    // Triggers OTA from the cached downloadUrl advertised by the manifest.
    // Updates the OTA state machine while running. Blocks until OTA either
    // succeeds (and restarts) or fails.
    void performOnlineUpdate();

    // Legacy accessor: returns a pointer to the cached version string
    // ("n/a" if never fetched). Valid until the next refresh().
    const char* getLatestVersion();
};

// ---- Testable helpers (no ESP-IDF dependencies) ---------------------------

// Build the static update manifest URL for the given channel.
//   beta=false -> raw.githubusercontent.com/.../latest.json
//   beta=true  -> raw.githubusercontent.com/.../beta.json
void buildUpdateManifestUrl(bool beta, char* out, size_t outLen);

// Normalize a Git tag (strip leading 'v'/'V', copy into out).
// "v2.1.11"        -> "2.1.11"
// "V2.2.0-beta.1"  -> "2.2.0-beta.1"
void normalizeTag(const char* tag, char* out, size_t outLen);
