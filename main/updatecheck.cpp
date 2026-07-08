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
#include "reset_info.h"
#include "system_reset.h"
#include "semver.h"
#include "ota_config.h"
#include "monitoring.h"
#include "events.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include "supporter_crl.h"

static const char *TAG = "UpdateCheck";

// Static manifests keep the device away from the GitHub Releases API. The
// Releases API response grows with release notes and asset metadata and is also
// subject to unauthenticated API rate limits. These raw files are tiny and
// deterministic; GitHub Releases remain only the binary hosting target. Add a
// query value per request so GitHub's raw CDN cannot serve a stale manifest
// after the release workflow has just updated beta.json/latest.json.
static const char *UPDATE_MANIFEST_BASE =
    "https://raw.githubusercontent.com/Xerolux/HB-RF-ETH-ng/main";

// Cap for the heap buffer used to receive latest.json / beta.json. A normal
// manifest is below 1 KB; 4 KB leaves room for future fields without reviving
// the old large-response heap pressure.
static const size_t MANIFEST_RESPONSE_CAP = 4 * 1024;

// esp_https_ota in IDF 6.x no longer exposes ESP_ERR_HTTPS_OTA_INCOMPLETE; use
// a private application code to report a download that ended prematurely.
#define OTA_ERR_DOWNLOAD_INCOMPLETE 0x10001

void _update_check_task_func(void *parameter)
{
  ((UpdateCheck *)parameter)->_taskFunc();
}

// ---- Testable helpers ------------------------------------------------------

void buildUpdateManifestUrl(bool beta, char* out, size_t outLen)
{
    snprintf(out, outLen, "%s/%s.json?t=%llu", UPDATE_MANIFEST_BASE,
             beta ? "beta" : "latest",
             (unsigned long long)(esp_timer_get_time() / 1000000ULL));
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
    // snprintf always null-terminates; strncpy triggers -Wstringop-truncation
    // when the compiler deduces source and destination have the same length.
    snprintf(out, outLen, "%s", p);
}

// ---- HTTP response accumulator --------------------------------------------

struct ManifestResponse {
    char *buf;        // heap-allocated lazily (MANIFEST_RESPONSE_CAP + terminator)
    size_t len;
    size_t cap;
    int httpStatus;
    bool allocFailed; // true if the lazy buffer allocation could not be served
    bool truncated;   // true when the response exceeded cap
};

static esp_err_t _manifest_event_handler(esp_http_client_event_t *evt)
{
    ManifestResponse *r = (ManifestResponse *)evt->user_data;
    if (!r) return ESP_OK;

    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (r->httpStatus == 0) {
            r->httpStatus = esp_http_client_get_status_code(evt->client);
        }
        // Only buffer successful responses. Error bodies are small and not
        // worth parsing.
        if (r->httpStatus == 200) {
            // Allocate the response buffer lazily, only once body data is
            // actually arriving. By this point the TLS handshake is already
            // complete, so the manifest buffer never overlaps with the peak
            // handshake memory pressure.
            if (!r->buf && !r->allocFailed) {
                r->buf = (char *)malloc(r->cap + 1);
                if (!r->buf) {
                    r->allocFailed = true;
                    ESP_LOGE(TAG, "Failed to allocate %u bytes for update manifest",
                             (unsigned)r->cap);
                    // Abort the transfer immediately: there is no point
                    // downloading the rest of a response we cannot store.
                    // _doFetch reports the error via allocFailed.
                    return ESP_ERR_NO_MEM;
                }
            }
            if (r->buf) {
                size_t copy = evt->data_len;
                if (r->len + copy > r->cap) {
                    copy = (r->len < r->cap) ? (r->cap - r->len) : 0;
                    r->truncated = true;
                }
                if (copy > 0) {
                    memcpy(r->buf + r->len, evt->data, copy);
                    r->len += copy;
                }
                if (r->truncated) {
                    // Never accept a manifest parsed from an incomplete JSON
                    // document. Abort early and report a deterministic error.
                    return ESP_ERR_INVALID_SIZE;
                }
            }
        }
    }
    return ESP_OK;
}

static const char* json_string(cJSON *root, const char *name)
{
    cJSON *item = cJSON_GetObjectItem(root, name);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static bool json_bool(cJSON *root, const char *name, bool fallback)
{
    cJSON *item = cJSON_GetObjectItem(root, name);
    if (cJSON_IsBool(item)) return cJSON_IsTrue(item);
    return fallback;
}

static bool is_hex_sha256(const char *value)
{
    if (!value || strlen(value) != 64) return false;
    for (const char *p = value; *p; p++) {
        bool digit = (*p >= '0' && *p <= '9');
        bool lower = (*p >= 'a' && *p <= 'f');
        bool upper = (*p >= 'A' && *p <= 'F');
        if (!digit && !lower && !upper) return false;
    }
    return true;
}

static bool copy_string_field(char *dst, size_t dstLen, const char *src)
{
    if (!dst || dstLen == 0 || !src || !src[0]) return false;
    snprintf(dst, dstLen, "%s", src);
    return true;
}

static bool _parseUpdateManifest(const char *buf, ReleaseInfo *out)
{
    if (!buf || !out) return false;
    memset(out, 0, sizeof(*out));

    cJSON *root = cJSON_Parse(buf);
    if (!root) return false;

    const char *version = json_string(root, "version");
    const char *downloadUrl = json_string(root, "downloadUrl");
    if (!downloadUrl) downloadUrl = json_string(root, "url");
    const char *sha256 = json_string(root, "sha256");
    const char *releaseUrl = json_string(root, "releaseUrl");
    const char *publishedAt = json_string(root, "publishedAt");
    const char *notes = json_string(root, "releaseNotes");
    if (!notes) notes = json_string(root, "notes");
    const char *notesUrl = json_string(root, "notesUrl");

    bool ok = false;
    if (version && downloadUrl && is_hex_sha256(sha256)) {
        normalizeTag(version, out->version, sizeof(out->version));
        ok = out->version[0] &&
             copy_string_field(out->downloadUrl, sizeof(out->downloadUrl), downloadUrl) &&
             copy_string_field(out->sha256, sizeof(out->sha256), sha256);
        if (releaseUrl) copy_string_field(out->releaseUrl, sizeof(out->releaseUrl), releaseUrl);
        if (publishedAt) copy_string_field(out->publishedAt, sizeof(out->publishedAt), publishedAt);
        if (notes) copy_string_field(out->body, sizeof(out->body), notes);
        else if (notesUrl) snprintf(out->body, sizeof(out->body), "Release notes: %s", notesUrl);
        out->isPrerelease = json_bool(root, "isPrerelease", json_bool(root, "prerelease", false));
        out->valid = ok;
    }

    cJSON_Delete(root);
    return ok;
}

// ---- UpdateCheck -----------------------------------------------------------

UpdateCheck::UpdateCheck(Settings* settings, SysInfo* sysInfo, LED *statusLED)
    : _sysInfo(sysInfo), _statusLED(statusLED), _settings(settings)
{
    _stateMutex = xSemaphoreCreateMutex();
    _fetchLock = xSemaphoreCreateMutex();
    if (!_stateMutex || !_fetchLock) {
        ESP_LOGE(TAG, "Failed to allocate UpdateCheck synchronization objects");
    }
}

void UpdateCheck::start()
{
    if (_tHandle) return;
    // ReleaseInfo body is 4 KB – the struct exceeds 5 KB on the stack when
    // copied by getReleaseInfo() / refresh(). 12 KB gives the task comfortable
    // headroom and avoids stack-overflow → watchdog-reset loops.
    if (xTaskCreate(_update_check_task_func, "UpdateCheck", 12288,
                    this, 3, &_tHandle) != pdPASS) {
        _tHandle = NULL;
        ESP_LOGE(TAG, "Failed to create UpdateCheck task");
    }
}

void UpdateCheck::stop()
{
    if (_tHandle) {
        vTaskDelete(_tHandle);
        _tHandle = NULL;
    }
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
    ota_state_t prev = _otaState;
    _otaState = state;
    if (state == OTA_STATE_IDLE) {
        _otaProgress = -1;
        _otaErrorText[0] = '\0';
        _otaErrorCode = 0;
    }

    // Emit user-visible transitions. events_emit is non-blocking and
    // internally debounced, so calling it from this locked context is safe.
    // Only emit on actual transitions to avoid duplicate notifications.
    if (prev != state) {
        if (state == OTA_STATE_STARTING) {
            events_emit(EVENT_OTA_STARTED, nullptr);
        } else if (state == OTA_STATE_SUCCESS) {
            events_emit(EVENT_OTA_SUCCEEDED, nullptr);
        } else if (state == OTA_STATE_FAILED) {
            events_emit(EVENT_OTA_FAILED, _otaErrorText[0] ? _otaErrorText : nullptr);
        }
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

bool UpdateCheck::tryBeginOtaOperation()
{
    bool expected = false;
    return _otaInProgress.compare_exchange_strong(expected, true);
}

void UpdateCheck::finishOtaOperation()
{
    _otaInProgress.store(false);
}

bool UpdateCheck::_doFetch(ReleaseInfo *out)
{
    bool beta = _settings ? _settings->getBetaChannel() : false;
    char url[128];
    buildUpdateManifestUrl(beta, url, sizeof(url));

    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    ESP_LOGI(TAG, "Fetching %s update manifest (beta: %d, heap free: %u KB)",
             beta ? "beta" : "stable", beta ? 1 : 0,
             (unsigned)(free_heap / 1024));

    // Serialize with the changelog proxy — two TLS handshakes at once exhaust
    // the heap on the ESP32. 15 s timeout: GitHub should never take that long
    // for a single release.
    bool net_locked = false;
    if (g_net_fetch_mutex) {
        if (xSemaphoreTake(g_net_fetch_mutex, pdMS_TO_TICKS(15000)) != pdTRUE) {
            snprintf(out->error, sizeof(out->error), "network busy");
            ESP_LOGW(TAG, "Update manifest fetch skipped: another HTTPS operation is still active");
            return false;
        }
        net_locked = true;
    }

    // Declared here so goto cleanup never skips an initialisation (C++).
    esp_http_client_handle_t client = NULL;
    esp_err_t err = ESP_FAIL;
    int status = 0;
    size_t bodyLen = 0;
    bool parsedOk = false;

    // These are declared before the first goto so they're never skipped.
    ManifestResponse resp = {};
    esp_http_client_config_t config = {};

    // The response buffer is allocated lazily in the event handler once body
    // data starts arriving, so it is not held during the TLS handshake.
    resp.cap = MANIFEST_RESPONSE_CAP;

    configure_ota_http_client(config, url);
    config.timeout_ms = 10000;
    config.buffer_size = 2048;
    config.event_handler = _manifest_event_handler;
    config.user_data = &resp;

    client = esp_http_client_init(&config);
    if (!client) {
        snprintf(out->error, sizeof(out->error), "HTTP client init failed");
        ESP_LOGE(TAG, "Failed to init HTTP client");
        goto cleanup;
    }

    // Raw GitHub accepts normal JSON. Keep a User-Agent for proxy hygiene and
    // request identity encoding because ESP-IDF does not decompress gzip.
    esp_http_client_set_header(client, "User-Agent", "HB-RF-ETH-ng");
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Accept-Encoding", "identity");

    err = esp_http_client_perform(client);
    status = resp.httpStatus;
    bodyLen = resp.len;

    // Free the HTTP client (and its TLS context) BEFORE parsing the response.
    // The TLS session + http-client buffers hold several KB; releasing them
    // here roughly doubles the heap available to the parser and eliminates the
    // out-of-memory crash that aborted the update check on the WROOM-32 (no
    // PSRAM). resp.buf is a separate malloc and survives the cleanup.
    esp_http_client_cleanup(client);
    client = NULL;

    parsedOk = false;
    if (resp.allocFailed) {
        snprintf(out->error, sizeof(out->error), "out of memory");
        ESP_LOGE(TAG, "Update manifest buffer allocation failed");
    } else if (resp.truncated) {
        snprintf(out->error, sizeof(out->error), "update manifest too large");
        ESP_LOGE(TAG, "Update manifest exceeded %u-byte limit", (unsigned)resp.cap);
    } else if (err == ESP_OK && status == 200 && bodyLen > 0 && resp.buf) {
        // Null-terminate before parsing.
        resp.buf[bodyLen] = 0;

        // Diagnostic preview of the first bytes (handy for spotting gzip,
        // HTML captchas or rate-limit responses).
        {
            size_t dumpLen = bodyLen < 96 ? bodyLen : 96;
            char hex[192];
            size_t pos = 0;
            for (size_t i = 0; i < dumpLen && pos < sizeof(hex) - 4; i++) {
                unsigned char c = (unsigned char)resp.buf[i];
                if (c >= 0x20 && c <= 0x7E) {
                    hex[pos++] = (char)c;
                } else if (c == 0x0A) {
                    hex[pos++] = '\\'; hex[pos++] = 'n';
                } else if (c == 0x0D) {
                    /* skip CR */
                } else {
                    hex[pos++] = '.';
                }
            }
            hex[pos] = 0;
            ESP_LOGI(TAG, "Response preview (%u/%u bytes): [%s]",
                     (unsigned)dumpLen, (unsigned)bodyLen, hex);
        }

        size_t heapBeforeParse = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        ESP_LOGI(TAG, "Parsing update manifest (heap free %u KB)...",
                 (unsigned)(heapBeforeParse / 1024));

        parsedOk = _parseUpdateManifest(resp.buf, out);

        ESP_LOGI(TAG, "Parse %s (heap free %u KB)",
                 parsedOk ? "ok" : "failed",
                 (unsigned)(heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024));

        if (!parsedOk) {
            snprintf(out->error, sizeof(out->error), "could not parse update manifest");
            ESP_LOGE(TAG, "Update manifest parse failed");
        }
    } else if (err == ESP_OK && status == 404) {
        snprintf(out->error, sizeof(out->error),
                 "update manifest not found");
        ESP_LOGW(TAG, "Update manifest returned 404");
    } else if (err == ESP_OK && status == 403) {
        snprintf(out->error, sizeof(out->error),
                 "update manifest access denied");
        ESP_LOGW(TAG, "Update manifest access denied (HTTP 403)");
    } else {
        snprintf(out->error, sizeof(out->error),
                 "HTTP %d (%s)", status, esp_err_to_name(err));
        ESP_LOGE(TAG, "Update manifest fetch failed: HTTP %d (%s)", status, esp_err_to_name(err));
    }

cleanup:
    free(resp.buf);   // free(NULL) is safe when no body ever arrived
    if (net_locked) xSemaphoreGive(g_net_fetch_mutex);

    size_t heap_after = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    ESP_LOGI(TAG, "Fetch complete: heap free %u KB (delta %+d KB)",
             (unsigned)(heap_after / 1024),
             (int)((int)heap_after - (int)free_heap) / 1024);

    if (parsedOk) {
        ESP_LOGI(TAG, "Latest %s manifest: %s%s (asset: %s, sha256: %.12s...)",
                 beta ? "beta" : "stable",
                 out->version,
                 out->isPrerelease ? " [prerelease]" : "",
                 out->downloadUrl[0] ? out->downloadUrl : "(none)",
                 out->sha256);
    } else {
        ESP_LOGW(TAG, "Update manifest fetch failed: %s", out->error);
    }

    return parsedOk;
}

void UpdateCheck::_taskFunc()
{
  // some time for initial network connection
  vTaskDelay(pdMS_TO_TICKS(30000));

  for (;;)
  {
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    ESP_LOGI(TAG, "Stack high water: %u B, heap free: %u KB",
             (unsigned)uxTaskGetStackHighWaterMark(NULL),
             (unsigned)(free_heap / 1024));
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
    // delay down to ~500 s - hammering the update manifest every ~8 minutes
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
    if (!tryBeginOtaOperation()) {
        ESP_LOGW(TAG, "OTA update rejected: another OTA operation is active");
        return;
    }

    // Only the URL/version are needed here. Avoid copying the 4 KB release
    // notes field onto the relatively small OTA worker stack.
    char downloadUrl[sizeof(_release.downloadUrl)] = {0};
    char version[sizeof(_release.version)] = "n/a";
    bool releaseValid = false;
    if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
        releaseValid = _release.valid;
        snprintf(downloadUrl, sizeof(downloadUrl), "%s", _release.downloadUrl);
        snprintf(version, sizeof(version), "%s", _release.version);
        xSemaphoreGive(_stateMutex);
    }

    if (!releaseValid || downloadUrl[0] == 0) {
        ESP_LOGE(TAG, "No firmware asset URL available (latest: %s)",
                 releaseValid ? version : "n/a");
        if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
            _setOtaStateLocked(OTA_STATE_FAILED);
            _setOtaErrorLocked(ESP_ERR_NOT_FOUND, "no firmware asset URL");
            xSemaphoreGive(_stateMutex);
        }
        finishOtaOperation();
        return;
    }

    ESP_LOGI(TAG, "Starting OTA update from %s", downloadUrl);
    _statusLED->setState(LED_STATE_BLINK_FAST);

    // Mark "starting" so MQTT / WebUI can show the new state immediately.
    if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
        _setOtaStateLocked(OTA_STATE_STARTING);
        _setOtaProgressLocked(0);
        xSemaphoreGive(_stateMutex);
    }

    esp_http_client_config_t config = {};
    configure_ota_http_client(config, downloadUrl);
    config.timeout_ms = 60000;
    config.buffer_size = 4096;

    bool net_locked = false;
    if (g_net_fetch_mutex) {
        if (xSemaphoreTake(g_net_fetch_mutex, pdMS_TO_TICKS(30000)) != pdTRUE) {
            ESP_LOGE(TAG, "OTA update could not start: HTTPS subsystem busy");
            if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
                _setOtaStateLocked(OTA_STATE_FAILED);
                _setOtaErrorLocked(ESP_ERR_TIMEOUT, "HTTPS subsystem busy");
                xSemaphoreGive(_stateMutex);
            }
            finishOtaOperation();
            return;
        }
        net_locked = true;
    }

    uint32_t paused_monitoring = 0;
    auto releaseOperation = [&](bool resume_monitoring) {
        net_fetch_set_ota_active(false);
        if (net_locked) {
            xSemaphoreGive(g_net_fetch_mutex);
            net_locked = false;
        }
        if (resume_monitoring) {
            monitoring_resume_after_ota(paused_monitoring);
        }
        finishOtaOperation();
    };

    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &config;

    // Signal lower-priority TLS consumers (event notifications, syslog
    // forwarding) to stand down for the duration of the download so they
    // don't contend for g_net_fetch_mutex or the limited TLS heap.
    net_fetch_set_ota_active(true);

    // Free heap by stopping monitoring workers + CRL so the GitHub TLS
    // handshake + download has enough room. Without this, the WROOM-32
    // (no PSRAM) OOMs at ~60 KB free heap during the TLS download.
    ESP_LOGI(TAG, "Freeing heap for OTA (current: %u KB)...",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024));
    paused_monitoring = monitoring_pause_for_ota();
    supporter_crl_stop_refresh_task();
    // Stop our own background task (12 KB stack) — it only sleeps in a 24h
    // loop and would just waste heap during the download.
    stop();
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP_LOGI(TAG, "Heap after OTA prep: %u KB",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024));

    // Use the advanced esp_https_ota API so we can report real progress to
    // MQTT / WebUI. The simple esp_https_ota(&ota_config) call is a thin
    // wrapper around these steps but hides all intermediate state.
    //
    // Retry the begin a couple of times: the GitHub asset redirect forces a
    // second TLS handshake to objects.githubusercontent.com which can
    // transiently OOM on the WROOM-32. Mirrors the WebUI OTA path.
    esp_https_ota_handle_t ota_handle = NULL;
    esp_err_t ret = ESP_FAIL;
    for (int attempt = 1; attempt <= 3; ++attempt) {
        ret = esp_https_ota_begin(&ota_config, &ota_handle);
        if (ret == ESP_OK) {
            break;
        }
        ESP_LOGW(TAG, "esp_https_ota_begin attempt %d/3 failed: %s",
                 attempt, esp_err_to_name(ret));
        if (attempt < 3) {
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_begin failed after retries: %s", esp_err_to_name(ret));
        if (_stateMutex && xSemaphoreTake(_stateMutex, portMAX_DELAY) == pdTRUE) {
            _setOtaStateLocked(OTA_STATE_FAILED);
            _setOtaErrorLocked(ret, esp_err_to_name(ret));
            xSemaphoreGive(_stateMutex);
        }
        ResetInfo::storeResetReason(RESET_REASON_UPDATE_FAILED);
        _statusLED->setState(LED_STATE_ON);
        releaseOperation(true);
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
    int otaChunk = 0;
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
        // Yield periodically so the idle task (and thus the task watchdog) and
        // other FreeRTOS tasks (MQTT, httpd) get CPU time during the long
        // download. perform() blocks on socket reads, but a defensive yield
        // keeps the watchdog happy even on slow networks.
        if ((++otaChunk & 0x07) == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
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
        releaseOperation(true);
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
        releaseOperation(false);
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
        releaseOperation(true);
    }
}
