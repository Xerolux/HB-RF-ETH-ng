/*
 *  updatecheck.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "updatecheck.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "esp_timer.h"
#include "string.h"
#include "cJSON.h"
#include "reset_info.h"
#include "system_reset.h"
#include "semver.h"
#include "ota_config.h"
#include "monitoring.h"

static const char *TAG = "UpdateCheck";

// GitHub repository for HB-RF-ETH-ng. Single source of truth for version,
// release notes and firmware binary.
static const char *GITHUB_REPO = "Xerolux/HB-RF-ETH-ng";

// Cap for the heap buffer used to receive the GitHub releases JSON.
// Per_page=3 returns up to 3 releases; each release body is the full
// release-notes markdown, which can run several KB on its own, so 32 KB
// was getting truncated mid-JSON on releases with a long changelog body
// (cJSON_Parse then fails on the cut-off response). 48 KB gives enough
// headroom for 3 releases with long bodies; the buffer is heap-allocated
// only for the duration of the mutex-serialized fetch, so it never
// overlaps with another TLS handshake's memory use.
static const size_t GH_RESPONSE_CAP = 48 * 1024;

// esp_https_ota in IDF 6.x no longer exposes ESP_ERR_HTTPS_OTA_INCOMPLETE; use
// a private application code to report a download that ended prematurely.
#define OTA_ERR_DOWNLOAD_INCOMPLETE 0x10001

void _update_check_task_func(void *parameter)
{
  ((UpdateCheck *)parameter)->_taskFunc();
}

// ---- Testable helpers ------------------------------------------------------

void buildReleasesApiUrl(bool beta, char* out, size_t outLen)
{
    // "/releases/latest" excludes prereleases; "/releases" lists every
    // non-draft release including prereleases, newest first.
    if (beta) {
        // per_page=3: GitHub's /releases endpoint does not always return
        // releases in strict chronological order (GitHub API quirk). By
        // fetching 3 releases we can iterate and pick the highest version
        // via semver comparison even when the API order is unreliable.
        snprintf(out, outLen,
                 "https://api.github.com/repos/%s/releases?per_page=3",
                 GITHUB_REPO);
    } else {
        snprintf(out, outLen,
                 "https://api.github.com/repos/%s/releases/latest",
                 GITHUB_REPO);
    }
}

void normalizeTag(const char* tag, char* out, size_t outLen)
{
    if (outLen == 0) return;
    out[0] = 0;
    if (!tag) return;

    const char* p = tag;
    // Strip a single leading v/V (GitHub tags are typically "v2.1.11").
    if ((p[0] == 'v' || p[0] == 'V') &&
        p[1] >= '0' && p[1] <= '9') {
        p++;
    }
    strncpy(out, p, outLen - 1);
    out[outLen - 1] = 0;
}

// ---- HTTP response accumulator --------------------------------------------

struct GhResponse {
    char *buf;        // heap-allocated lazily (up to GH_RESPONSE_CAP bytes)
    size_t len;
    size_t cap;
    int httpStatus;
    bool allocFailed; // true if the lazy buffer allocation could not be served
};

static esp_err_t _gh_event_handler(esp_http_client_event_t *evt)
{
    GhResponse *r = (GhResponse *)evt->user_data;
    if (!r) return ESP_OK;

    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (r->httpStatus == 0) {
            r->httpStatus = esp_http_client_get_status_code(evt->client);
        }
        // Only buffer successful responses. A 403 (rate limit) or 404 (no
        // releases yet) body is small and not worth parsing.
        if (r->httpStatus == 200) {
            // Allocate the (large) response buffer lazily, only once body data
            // is actually arriving. By this point the TLS handshake is already
            // complete, so we never hold ~48 KB of heap across the handshake -
            // that starved the PSA-crypto signature verification on the
            // WROOM-32 (no PSRAM) and aborted the handshake with
            // PSA_ERROR_INSUFFICIENT_MEMORY (-0x008D / -141).
            if (!r->buf && !r->allocFailed) {
                r->buf = (char *)malloc(r->cap);
                if (!r->buf) {
                    r->allocFailed = true;
                    ESP_LOGE(TAG, "Failed to allocate %u bytes for GitHub response",
                             (unsigned)r->cap);
                }
            }
            if (r->buf) {
                size_t copy = evt->data_len;
                if (r->len + copy > r->cap) {
                    copy = (r->len < r->cap) ? (r->cap - r->len) : 0;
                }
                if (copy > 0) {
                    memcpy(r->buf + r->len, evt->data, copy);
                    r->len += copy;
                }
            }
        }
    }
    return ESP_OK;
}

// Parse a single GitHub release JSON object into out. Returns true on success.
static bool _parseReleaseObject(const cJSON *obj, ReleaseInfo *out)
{
    if (!obj) return false;

    const char *tag = cJSON_GetStringValue(cJSON_GetObjectItem(obj, "tag_name"));
    if (!tag) return false;
    normalizeTag(tag, out->version, sizeof(out->version));

    const char *html = cJSON_GetStringValue(cJSON_GetObjectItem(obj, "html_url"));
    if (html) {
        strncpy(out->releaseUrl, html, sizeof(out->releaseUrl) - 1);
    }

    const char *published = cJSON_GetStringValue(cJSON_GetObjectItem(obj, "published_at"));
    if (published) {
        strncpy(out->publishedAt, published, sizeof(out->publishedAt) - 1);
    }

    cJSON *preItem = cJSON_GetObjectItem(obj, "prerelease");
    out->isPrerelease = preItem ? cJSON_IsTrue(preItem) : false;

    const char *body = cJSON_GetStringValue(cJSON_GetObjectItem(obj, "body"));
    if (body) {
        // Truncate body to last (sizeof-1) bytes so the WebUI still has a
        // useful preview; the full CHANGELOG is fetched separately.
        size_t n = strlen(body);
        size_t cap = sizeof(out->body) - 1;
        if (n > cap) {
            // Keep the most recent entries (end of the markdown body).
            memcpy(out->body, body + (n - cap), cap);
            out->body[cap] = 0;
        } else {
            strncpy(out->body, body, cap);
            out->body[cap] = 0;
        }
    }

    // Find the firmware binary asset. The release workflow uploads
    // "firmware_<version>.bin"; other assets (bootloader, partitions,
    // SHA256SUMS) are skipped.
    const cJSON *assets = cJSON_GetObjectItem(obj, "assets");
    bool foundAsset = false;
    if (cJSON_IsArray(assets)) {
        cJSON *asset;
        cJSON_ArrayForEach(asset, assets) {
            const char *name = cJSON_GetStringValue(cJSON_GetObjectItem(asset, "name"));
            if (!name) continue;
            if (strncmp(name, "firmware_", 9) != 0) continue;
            size_t nl = strlen(name);
            if (nl < 4 || strcmp(name + nl - 4, ".bin") != 0) continue;

            const char *url = cJSON_GetStringValue(cJSON_GetObjectItem(asset, "browser_download_url"));
            if (url) {
                strncpy(out->downloadUrl, url, sizeof(out->downloadUrl) - 1);
                out->downloadUrl[sizeof(out->downloadUrl) - 1] = 0;
                foundAsset = true;
                break;
            }
        }
    }

    if (!foundAsset) {
        ESP_LOGW(TAG, "Release %s has no matching firmware_*.bin asset", out->version);
        // Don't fail outright - the version is still informative, but OTA
        // will not be possible until the asset is uploaded.
        out->downloadUrl[0] = 0;
    }

    out->valid = true;
    out->error[0] = 0;
    return true;
}

// ---- UpdateCheck -----------------------------------------------------------

UpdateCheck::UpdateCheck(Settings* settings, SysInfo* sysInfo, LED *statusLED)
    : _sysInfo(sysInfo), _statusLED(statusLED), _settings(settings)
{
    _stateMutex = xSemaphoreCreateMutex();
    _fetchLock = xSemaphoreCreateMutex();
}

void UpdateCheck::start()
{
    // ReleaseInfo body is 4 KB – the struct exceeds 5 KB on the stack when
    // copied by getReleaseInfo() / refresh(). 12 KB gives the task comfortable
    // headroom and avoids stack-overflow → watchdog-reset loops.
    xTaskCreate(_update_check_task_func, "UpdateCheck", 12288, this, 3, &_tHandle);
}

void UpdateCheck::stop()
{
    vTaskDelete(_tHandle);
}

const char *UpdateCheck::getLatestVersion()
{
    return _latestVersion;
}

ReleaseInfo UpdateCheck::getReleaseInfo()
{
    ReleaseInfo snap;
    if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
        snap = _release;
        xSemaphoreGive(_stateMutex);
    } else {
        memset(&snap, 0, sizeof(snap));
        strncpy(snap.version, "n/a", sizeof(snap.version) - 1);
    }
    return snap;
}

VersionSnapshot UpdateCheck::getVersionSnapshot()
{
    VersionSnapshot s;
    if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
        s.valid = _release.valid;
        // snprintf (not strncpy) to avoid -Wstringop-truncation when source
        // and destination have the same size.
        snprintf(s.version, sizeof(s.version), "%s", _release.version);
        s.isPrerelease = _release.isPrerelease;
        snprintf(s.error, sizeof(s.error), "%s", _release.error);
        xSemaphoreGive(_stateMutex);
    }
    return s;
}

bool UpdateCheck::refresh()
{
    // Refuse to stack fetches - a single GitHub request can take several
    // seconds (DNS + TLS handshake + download) and the background timer
    // and a manual "Check now" can otherwise race.
    if (!_fetchLock || xSemaphoreTake(_fetchLock, 0) != pdTRUE) {
        ESP_LOGD(TAG, "refresh: another fetch is already in progress");
        return false;
    }

    // Reflect "checking" state for MQTT / WebUI. Only flip the state if no
    // OTA is currently running so we never clobber OTA_DOWNLOADING etc.
    if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
        if (_otaState == OTA_STATE_IDLE) {
            _setOtaStateLocked(OTA_STATE_CHECKING);
        }
        xSemaphoreGive(_stateMutex);
    }

    ReleaseInfo fresh = {};
    bool ok = _doFetch(&fresh);
    int64_t now = esp_timer_get_time() / 1000;

    // Publish atomically. On failure we keep the last-known-good snapshot
    // (version, URLs, body) so the WebUI continues to show useful data,
    // and just surface the error + retry timestamp.
    if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
        if (ok) {
            fresh.fetchedAtMs = now;
            _release = fresh;
            strncpy(_latestVersion, fresh.version, sizeof(_latestVersion) - 1);
            _latestVersion[sizeof(_latestVersion) - 1] = 0;
        } else {
            _release.fetchedAtMs = now;
            if (fresh.error[0]) {
                strncpy(_release.error, fresh.error, sizeof(_release.error) - 1);
                _release.error[sizeof(_release.error) - 1] = 0;
            }
            // If we've never had a valid snapshot, keep the "n/a" default
            // so legacy callers (sysinfo, MQTT) report the same.
            if (!_release.valid) {
                strncpy(_latestVersion, "n/a", sizeof(_latestVersion) - 1);
                _latestVersion[sizeof(_latestVersion) - 1] = 0;
            }
        }
        // Clear CHECKING state unless an OTA ran in parallel (very unlikely
        // but defensive). Reset back to IDLE so MQTT can show "idle".
        if (_otaState == OTA_STATE_CHECKING) {
            _setOtaStateLocked(OTA_STATE_IDLE);
        }
        xSemaphoreGive(_stateMutex);
    }

    xSemaphoreGive(_fetchLock);
    return ok;
}

bool UpdateCheck::isFetchInProgress()
{
    if (!_fetchLock) return false;
    // xSemaphoreGetMutexHolder returns the owning task handle or NULL. We
    // don't care who owns it - any non-NULL value means "busy".
    return xSemaphoreGetMutexHolder(_fetchLock) != NULL;
}

void UpdateCheck::_setOtaStateLocked(ota_state_t state)
{
    _otaState = state;
    if (state == OTA_STATE_IDLE) {
        _otaProgress = -1;
        _otaErrorText[0] = '\0';
        _otaErrorCode = 0;
    }
}

void UpdateCheck::_setOtaProgressLocked(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    _otaProgress = percent;
}

void UpdateCheck::_setOtaErrorLocked(int code, const char* text)
{
    _otaErrorCode = code;
    if (text) {
        strncpy(_otaErrorText, text, sizeof(_otaErrorText) - 1);
        _otaErrorText[sizeof(_otaErrorText) - 1] = '\0';
    } else {
        _otaErrorText[0] = '\0';
    }
}

OtaSnapshot UpdateCheck::getOtaState()
{
    OtaSnapshot snap;
    if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
        snap.state = _otaState;
        snap.progress_pct = _otaProgress;
        snap.error_code = _otaErrorCode;
        // snprintf (not strncpy) to avoid -Wstringop-truncation: both buffers
        // are 64 bytes and _otaErrorText is always null-terminated.
        snprintf(snap.error_text, sizeof(snap.error_text), "%s", _otaErrorText);
        xSemaphoreGive(_stateMutex);
    } else {
        snap.state = OTA_STATE_IDLE;
        snap.progress_pct = -1;
    }
    return snap;
}

bool UpdateCheck::_doFetch(ReleaseInfo *out)
{
    bool beta = _settings ? _settings->getBetaChannel() : false;
    char url[128];
    buildReleasesApiUrl(beta, url, sizeof(url));

    ESP_LOGI(TAG, "Fetching %s release info from GitHub (beta channel: %d)",
             beta ? "latest (incl. pre-release)" : "stable", beta ? 1 : 0);

    // Serialize with the changelog proxy — two TLS handshakes at once exhaust
    // the heap on the ESP32. 15 s timeout: GitHub should never take that long
    // for a single release.
    bool net_locked = false;
    if (g_net_fetch_mutex &&
        xSemaphoreTake(g_net_fetch_mutex, pdMS_TO_TICKS(15000)) == pdTRUE) {
        net_locked = true;
    }

    // Declared here so goto cleanup never skips an initialisation (C++).
    esp_http_client_handle_t client = NULL;
    esp_err_t err = ESP_FAIL;
    int status = 0;
    size_t bodyLen = 0;
    bool parsedOk = false;

    // These are declared before the first goto so they're never skipped.
    GhResponse resp = {};
    esp_http_client_config_t config = {};

    // The response buffer is allocated lazily in _gh_event_handler once body
    // data starts arriving, so its ~48 KB is not held during the TLS handshake.
    resp.cap = GH_RESPONSE_CAP;

    configure_ota_http_client(config, url);
    config.timeout_ms = 10000;
    config.buffer_size = 4096;
    config.event_handler = _gh_event_handler;
    config.user_data = &resp;

    client = esp_http_client_init(&config);
    if (!client) {
        snprintf(out->error, sizeof(out->error), "HTTP client init failed");
        ESP_LOGE(TAG, "Failed to init HTTP client");
        goto cleanup;
    }

    // GitHub requires a User-Agent and accepts a vendor media type for JSON.
    esp_http_client_set_header(client, "User-Agent", "HB-RF-ETH-ng");
    esp_http_client_set_header(client, "Accept", "application/vnd.github+json");

    err = esp_http_client_perform(client);
    status = resp.httpStatus;
    bodyLen = resp.len;

    parsedOk = false;
    if (resp.allocFailed) {
        snprintf(out->error, sizeof(out->error), "out of memory");
        ESP_LOGE(TAG, "GitHub response buffer allocation failed");
    } else if (err == ESP_OK && status == 200 && bodyLen > 0) {
        // Null-terminate before parsing.
        resp.buf[bodyLen < resp.cap ? bodyLen : resp.cap - 1] = 0;

        cJSON *root = cJSON_Parse(resp.buf);
        if (root) {
            if (cJSON_IsArray(root)) {
                int count = cJSON_GetArraySize(root);
                // Iterate all releases and pick the one with the highest
                // version.  The API may not return them in chronological
                // order (GitHub quirk), so the first item is not guaranteed
                // to be the newest.
                ReleaseInfo best = {};
                for (int i = 0; i < count; i++) {
                    cJSON *item = cJSON_GetArrayItem(root, i);
                    cJSON *draft = cJSON_GetObjectItem(item, "draft");
                    if (draft && cJSON_IsTrue(draft)) continue;
                    ReleaseInfo cur = {};
                    if (_parseReleaseObject(item, &cur)) {
                        if (!best.valid || compareVersions(cur.version, best.version) > 0) {
                            best = cur;
                        }
                    }
                }
                if (best.valid) {
                    *out = best;
                    parsedOk = true;
                } else {
                    snprintf(out->error, sizeof(out->error),
                             "no usable release in list");
                }
            } else {
                parsedOk = _parseReleaseObject(root, out);
                if (!parsedOk) {
                    snprintf(out->error, sizeof(out->error),
                             "release JSON missing tag_name");
                }
            }
            cJSON_Delete(root);
        } else {
            snprintf(out->error, sizeof(out->error), "JSON parse failed");
            ESP_LOGE(TAG, "cJSON_Parse failed for GitHub response");
        }
    } else if (err == ESP_OK && status == 404) {
        snprintf(out->error, sizeof(out->error),
                 "no stable release available yet");
        ESP_LOGW(TAG, "GitHub returned 404 - no stable release published");
    } else if (err == ESP_OK && status == 403) {
        snprintf(out->error, sizeof(out->error),
                 "GitHub API rate limit exceeded");
        ESP_LOGW(TAG, "GitHub API rate limited (HTTP 403)");
    } else {
        snprintf(out->error, sizeof(out->error),
                 "HTTP %d (%s)", status, esp_err_to_name(err));
        ESP_LOGE(TAG, "GitHub fetch failed: HTTP %d (%s)", status, esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);

cleanup:
    free(resp.buf);   // free(NULL) is safe when no body ever arrived
    if (net_locked) xSemaphoreGive(g_net_fetch_mutex);

    if (parsedOk) {
        ESP_LOGI(TAG, "Latest %s release: %s%s (asset: %s)",
                 beta ? "any" : "stable",
                 out->version,
                 out->isPrerelease ? " [prerelease]" : "",
                 out->downloadUrl[0] ? out->downloadUrl : "(none)");
    } else {
        ESP_LOGW(TAG, "GitHub release fetch failed: %s", out->error);
    }

    return parsedOk;
}

void UpdateCheck::_taskFunc()
{
  // some time for initial network connection
  vTaskDelay(pdMS_TO_TICKS(30000));

  for (;;)
  {
    ESP_LOGI(TAG, "Stack high water mark: %u bytes free",
             (unsigned)uxTaskGetStackHighWaterMark(NULL));
    ESP_LOGI(TAG, "Checking for firmware updates...");
    ESP_LOGI(TAG, "Current version: %s", _sysInfo->getCurrentVersion());

    // refresh() is non-blocking: it returns false when a fetch is already in
    // progress (coalesced) - that is NOT an error. Use info.valid as the
    // authoritative signal so we don't log a bogus error during the boot race.
    refresh();
    VersionSnapshot info = getVersionSnapshot();

    if (info.valid)
    {
      ESP_LOGI(TAG, "Latest available version: %s", info.version);

      if (compareVersions(_sysInfo->getCurrentVersion(), info.version) < 0)
      {
        ESP_LOGW(TAG, "An updated firmware with version %s is available!", info.version);
        _statusLED->setState(LED::getProgram(LED_PROG_UPDATE_AVAILABLE));
      }
      else if (compareVersions(_sysInfo->getCurrentVersion(), info.version) > 0)
      {
        ESP_LOGI(TAG, "Running version (%s) is newer than available version (%s)",
                 _sysInfo->getCurrentVersion(), info.version);
      }
      else
      {
        ESP_LOGI(TAG, "Firmware is up to date (version %s)", info.version);
      }
    }
    else if (info.error[0])
    {
      ESP_LOGE(TAG, "Failed to determine latest version: %s", info.error);
    }
    else
    {
      // Cache empty and no error yet - a fetch is in progress or has not run.
      ESP_LOGI(TAG, "Release info not available yet (fetch in progress).");
    }

    // 24h, split into 1h chunks: pdMS_TO_TICKS((TickType_t)ms * configTICK_RATE_HZ)
    // overflows 32-bit TickType_t arithmetic for a 24h millisecond value
    // (86,400,000 ms * 100 Hz > UINT32_MAX), which silently wrapped this
    // delay down to ~500 s - hammering the GitHub API every ~8 minutes
    // instead of once a day. 1h chunks stay well within range.
    for (int hour = 0; hour < 24; hour++)
    {
      vTaskDelay(pdMS_TO_TICKS(60 * 60000));
    }
  }

  vTaskDelete(NULL);
}

void UpdateCheck::performOnlineUpdate()
{
    ReleaseInfo info = getReleaseInfo();
    if (!info.valid || info.downloadUrl[0] == 0) {
        ESP_LOGE(TAG, "No firmware asset URL available (latest: %s)",
                 info.valid ? info.version : "n/a");
        if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
            _setOtaStateLocked(OTA_STATE_FAILED);
            _setOtaErrorLocked(ESP_ERR_NOT_FOUND, "no firmware asset URL");
            xSemaphoreGive(_stateMutex);
        }
        return;
    }

    ESP_LOGI(TAG, "Starting OTA update from %s", info.downloadUrl);
    _statusLED->setState(LED_STATE_BLINK_FAST);

    // Mark "starting" so MQTT / WebUI can show the new state immediately.
    if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
        _setOtaStateLocked(OTA_STATE_STARTING);
        _setOtaProgressLocked(0);
        xSemaphoreGive(_stateMutex);
    }

    esp_http_client_config_t config = {};
    configure_ota_http_client(config, info.downloadUrl);
    config.timeout_ms = 60000;
    config.buffer_size = 4096;

    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &config;

    // Use the advanced esp_https_ota API so we can report real progress to
    // MQTT / WebUI. The simple esp_https_ota(&ota_config) call is a thin
    // wrapper around these steps but hides all intermediate state.
    esp_https_ota_handle_t ota_handle = NULL;
    esp_err_t ret = esp_https_ota_begin(&ota_config, &ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_begin failed: %s", esp_err_to_name(ret));
        if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
            _setOtaStateLocked(OTA_STATE_FAILED);
            _setOtaErrorLocked(ret, esp_err_to_name(ret));
            xSemaphoreGive(_stateMutex);
        }
        ResetInfo::storeResetReason(RESET_REASON_UPDATE_FAILED);
        _statusLED->setState(LED_STATE_ON);
        return;
    }

    int image_size = esp_https_ota_get_image_size(ota_handle);
    ESP_LOGI(TAG, "OTA image size: %d bytes", image_size);

    if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
        _setOtaStateLocked(OTA_STATE_DOWNLOADING);
        xSemaphoreGive(_stateMutex);
    }

    // Loop until the full image has been streamed into the OTA partition.
    // perform() pulls one buffer chunk at a time and writes it to flash.
    while (true) {
        ret = esp_https_ota_perform(ota_handle);
        if (ret != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        if (image_size > 0) {
            int read = esp_https_ota_get_image_len_read(ota_handle);
            int pct = (int)((uint64_t)read * 100 / (uint64_t)image_size);
            if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
                _setOtaProgressLocked(pct);
                xSemaphoreGive(_stateMutex);
            }
        }
    }

    bool complete = esp_https_ota_is_complete_data_received(ota_handle);
    if (ret != ESP_OK || !complete) {
        ESP_LOGE(TAG, "OTA perform failed: ret=%s complete=%d",
                 esp_err_to_name(ret), complete ? 1 : 0);
        const char* err_text = (ret == ESP_OK) ? "download incomplete" : esp_err_to_name(ret);
        int err_code = (ret == ESP_OK) ? OTA_ERR_DOWNLOAD_INCOMPLETE : ret;
        if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
            _setOtaStateLocked(OTA_STATE_FAILED);
            _setOtaErrorLocked(err_code, err_text);
            xSemaphoreGive(_stateMutex);
        }
        esp_https_ota_abort(ota_handle);
        ResetInfo::storeResetReason(RESET_REASON_UPDATE_FAILED);
        _statusLED->setState(LED_STATE_ON);
        return;
    }

    // All bytes streamed - now validate and switch the boot partition.
    if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
        _setOtaStateLocked(OTA_STATE_FLASHING);
        _setOtaProgressLocked(100);
        xSemaphoreGive(_stateMutex);
    }

    ret = esp_https_ota_finish(ota_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA Update successful, restarting...");
        if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
            _setOtaStateLocked(OTA_STATE_SUCCESS);
            xSemaphoreGive(_stateMutex);
        }
        ResetInfo::storeResetReason(RESET_REASON_FIRMWARE_UPDATE);
        full_system_restart();
    } else {
        ESP_LOGE(TAG, "esp_https_ota_finish failed: %s", esp_err_to_name(ret));
        if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
            _setOtaStateLocked(OTA_STATE_FAILED);
            _setOtaErrorLocked(ret, esp_err_to_name(ret));
            xSemaphoreGive(_stateMutex);
        }
        ResetInfo::storeResetReason(RESET_REASON_UPDATE_FAILED);
        _statusLED->setState(LED_STATE_ON);
    }
}
