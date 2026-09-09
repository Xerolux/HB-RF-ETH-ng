/*
 *  update_check.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "update_check.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "crash_blackbox.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "monitoring.h"
#include "ota_config.h"
#include "semver.h"
#include "updatecheck_heap_policy.h"
#include "webui_storage.h"

static const char *TAG = "UpdateCheck";

// The manifest the device consumes is deliberately tiny and fixed in shape.
// Anything beyond this is refused before it is parsed - the ESP32 has no room
// to be generous with an untrusted document. Kept in step with
// scripts/device_manifest.py (MAX_BYTES).
static constexpr size_t MANIFEST_MAX_BYTES = 1024;

static constexpr int FETCH_TOTAL_TIMEOUT_MS = 20000;
static constexpr int FETCH_ASYNC_RETRY_MS   = 10;
static constexpr int NET_MUTEX_WAIT_MS      = 5000;

// Repeated presses must not turn into repeated TLS handshakes.
static constexpr int64_t COOLDOWN_US = 60 * 1000000LL;

static constexpr const char *MANIFEST_URL_FORMAT =
    "https://xerolux.github.io/HB-RF-ETH-ng/updates/v1/%s.json";

static SemaphoreHandle_t s_lock   = NULL;
static UpdateCheckStatus s_status = {};
static int64_t s_last_attempt_us  = 0;

// ---------------------------------------------------------------------------
// State helpers. Every field is written under s_lock so a WebUI poll can never
// observe a half-updated snapshot.
// ---------------------------------------------------------------------------

static void copy_into(char *dest, size_t size, const char *value)
{
    snprintf(dest, size, "%s", value ? value : "");
}

// A skip and a failure are recorded separately, and both clear each other:
// carrying a stale reason forward is how "skipped" once came to be displayed
// as a successful "no update found".
static void finish_with_skip(const char *reason)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    copy_into(s_status.lastSkipReason, sizeof(s_status.lastSkipReason), reason);
    s_status.lastError[0] = '\0';
    s_status.state        = UPDATE_CHECK_IDLE;
    xSemaphoreGive(s_lock);
    ESP_LOGW(TAG, "Update check skipped: %s", reason);
}

static void finish_with_error(const char *reason)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    copy_into(s_status.lastError, sizeof(s_status.lastError), reason);
    s_status.lastSkipReason[0] = '\0';
    s_status.state             = UPDATE_CHECK_IDLE;
    xSemaphoreGive(s_lock);
    ESP_LOGE(TAG, "Update check failed: %s", reason);
}

bool update_check_valid_channel(const char *channel)
{
    return channel && (strcmp(channel, "stable") == 0 || strcmp(channel, "beta") == 0);
}

// ---------------------------------------------------------------------------
// Bounded asynchronous GET.
//
// Modelled on the notification sender in events.cpp: IDF's asynchronous HTTPS
// transport driven against one absolute deadline, so no single stalled read can
// keep the worker alive indefinitely. Unlike that sender this one consumes a
// response body, which is why the read loop is bounded explicitly rather than
// trusting Content-Length.
// ---------------------------------------------------------------------------

static int remaining_ms(int64_t deadline_us)
{
    const int64_t left = deadline_us - esp_timer_get_time();
    if (left <= 0) return 0;
    const int64_t ms = (left + 999) / 1000;
    return ms > INT32_MAX ? INT32_MAX : static_cast<int>(ms);
}

static bool step_ready(esp_http_client_handle_t client, int64_t deadline_us)
{
    const int left = remaining_ms(deadline_us);
    return left > 0 && esp_http_client_set_timeout_ms(client, left) == ESP_OK;
}

static void retry_delay(int64_t deadline_us)
{
    int delay = remaining_ms(deadline_us);
    if (delay > FETCH_ASYNC_RETRY_MS) delay = FETCH_ASYNC_RETRY_MS;
    if (delay > 0) vTaskDelay(pdMS_TO_TICKS(delay));
}

// Reads at most MANIFEST_MAX_BYTES into `buffer`, then one extra byte to detect
// an oversized document. Returns the length, or -1 on failure.
static int fetch_manifest(const char *url, char *buffer, size_t buffer_size, char *error,
                          size_t error_size)
{
    const int64_t deadline_us =
        esp_timer_get_time() + static_cast<int64_t>(FETCH_TOTAL_TIMEOUT_MS) * 1000;

    esp_http_client_config_t cfg = {};
    configure_ota_http_client(cfg, url);
    cfg.method     = HTTP_METHOD_GET;
    cfg.timeout_ms = remaining_ms(deadline_us);
    cfg.is_async   = true;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        snprintf(error, error_size, "HTTP client could not be created");
        return -1;
    }

    int result  = -1;
    bool opened = false;

    for (;;) {
        if (!step_ready(client, deadline_us)) {
            snprintf(error, error_size, "Timeout while connecting");
            goto done;
        }
        const esp_err_t err = esp_http_client_open(client, 0);
        if (err == ESP_OK) {
            opened = true;
            break;
        }
        if (err != ESP_ERR_HTTP_EAGAIN) {
            snprintf(error, error_size, "Connection failed: %s", esp_err_to_name(err));
            goto done;
        }
        retry_delay(deadline_us);
    }

    for (;;) {
        if (!step_ready(client, deadline_us)) {
            snprintf(error, error_size, "Timeout while reading response headers");
            goto done;
        }
        const int64_t headers = esp_http_client_fetch_headers(client);
        if (headers >= 0) break;
        if (headers != -ESP_ERR_HTTP_EAGAIN) {
            snprintf(error, error_size, "Could not read response headers");
            goto done;
        }
        retry_delay(deadline_us);
    }

    {
        const int status = esp_http_client_get_status_code(client);
        if (status != 200) {
            snprintf(error, error_size, "Server returned HTTP %d", status);
            goto done;
        }

        // One byte of headroom so an oversized body is detected rather than
        // silently truncated into something that might still parse.
        size_t total = 0;
        while (total <= buffer_size) {
            if (!step_ready(client, deadline_us)) {
                snprintf(error, error_size, "Timeout while reading the manifest");
                goto done;
            }
            errno          = 0;
            const int read = esp_http_client_read(client, buffer + total,
                                                  static_cast<int>(buffer_size + 1 - total));
            if (read > 0) {
                total += static_cast<size_t>(read);
                if (total > buffer_size) {
                    snprintf(error, error_size, "Manifest larger than %u bytes",
                             static_cast<unsigned>(buffer_size));
                    goto done;
                }
                continue;
            }
            if (read == 0) {
                if (esp_http_client_is_complete_data_received(client)) break;
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == 0) {
                    retry_delay(deadline_us);
                    continue;
                }
                break;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                retry_delay(deadline_us);
                continue;
            }
            snprintf(error, error_size, "Connection lost while reading");
            goto done;
        }

        if (total == 0) {
            snprintf(error, error_size, "Server returned an empty manifest");
            goto done;
        }
        buffer[total] = '\0';
        result        = static_cast<int>(total);
    }

done:
    if (opened) esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return result;
}

// ---------------------------------------------------------------------------
// Manifest interpretation.
// ---------------------------------------------------------------------------

static const char *json_string(const cJSON *root, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    return (cJSON_IsString(item) && item->valuestring && item->valuestring[0]) ? item->valuestring
                                                                               : NULL;
}

static bool apply_manifest(const char *body, char *error, size_t error_size)
{
    cJSON *root = cJSON_Parse(body);
    if (!root) {
        snprintf(error, error_size, "Manifest is not valid JSON");
        return false;
    }

    bool ok              = false;
    const cJSON *schema  = cJSON_GetObjectItemCaseSensitive(root, "s");
    const char *firmware = json_string(root, "fw");
    const char *webui    = json_string(root, "ui");
    const char *notes    = json_string(root, "notes");

    if (!cJSON_IsNumber(schema) || schema->valueint != 1) {
        snprintf(error, error_size, "Unsupported manifest schema");
        goto out;
    }
    if (!firmware || !webui) {
        snprintf(error, error_size, "Manifest is missing version fields");
        goto out;
    }

    {
        const esp_app_desc_t *app    = esp_app_get_description();
        const char *running_firmware = app ? app->version : "";

        char running_webui[UPDATE_CHECK_VERSION_LEN] = {};
        webui_storage_get_effective_version(running_webui, sizeof(running_webui));

        // A published version that is older than or equal to what is installed
        // is not an update. Comparing rather than testing inequality is what
        // keeps a device on a beta from being offered the older stable build.
        const bool firmware_newer =
            running_firmware[0] && compareVersions(firmware, running_firmware) > 0;
        const bool webui_newer = running_webui[0] && compareVersions(webui, running_webui) > 0;

        int64_t now = 0;
        time_t wall = time(NULL);
        // Before the first time sync the clock is meaningless; report 0 rather
        // than a 1970 timestamp the UI would render as a real check time.
        if (wall > 1600000000) now = static_cast<int64_t>(wall);

        xSemaphoreTake(s_lock, portMAX_DELAY);
        copy_into(s_status.latestFirmware, sizeof(s_status.latestFirmware), firmware);
        copy_into(s_status.latestWebui, sizeof(s_status.latestWebui), webui);
        copy_into(s_status.notesUrl, sizeof(s_status.notesUrl), notes ? notes : "");
        s_status.firmwareUpdateAvailable = firmware_newer;
        s_status.webuiUpdateAvailable    = webui_newer;
        s_status.everChecked             = true;
        s_status.lastCheckUnix           = now;
        s_status.lastError[0]            = '\0';
        s_status.lastSkipReason[0]       = '\0';
        s_status.state                   = UPDATE_CHECK_IDLE;
        xSemaphoreGive(s_lock);

        ESP_LOGI(TAG,
                 "Update check done: firmware %s (running %s, newer=%d), "
                 "WebUI %s (running %s, newer=%d)",
                 firmware, running_firmware, firmware_newer ? 1 : 0, webui, running_webui,
                 webui_newer ? 1 : 0);
        ok = true;
    }

out:
    cJSON_Delete(root);
    return ok;
}

// ---------------------------------------------------------------------------
// Worker. Short-lived by design: it exists only for the duration of one fetch
// and then deletes itself, so an idle device carries no task stack for a
// feature it is not using.
// ---------------------------------------------------------------------------

struct worker_args {
    char channel[8];
};

static void update_check_task(void *parameter)
{
    worker_args *args                   = static_cast<worker_args *>(parameter);
    char error[UPDATE_CHECK_REASON_LEN] = {};

    // Total free memory bounds whether a TLS session fits; the largest block
    // decides whether it can be allocated at all. Both are needed - see
    // include/updatecheck_heap_policy.h.
    const size_t free_bytes = esp_get_free_heap_size();
    const size_t largest    = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    if (!update_check_heap_allows(free_bytes, largest)) {
        char reason[UPDATE_CHECK_REASON_LEN];
        snprintf(reason, sizeof(reason), "Too little free memory (free=%u KB, largest block=%u KB)",
                 static_cast<unsigned>(free_bytes / 1024), static_cast<unsigned>(largest / 1024));
        finish_with_skip(reason);
        goto cleanup;
    }

    // A firmware or WebUI image is being written: that operation owns the
    // flash and the network budget, and a manifest is never worth disturbing it.
    if (net_fetch_ota_active()) {
        finish_with_skip("An update installation is currently running");
        goto cleanup;
    }

    if (g_net_fetch_mutex == NULL) {
        finish_with_error("Network subsystem not ready");
        goto cleanup;
    }

    // One outbound TLS session at a time, device-wide. Waiting rather than
    // queueing: if syslog or MQTT is mid-handshake, this check can simply be
    // asked for again.
    if (xSemaphoreTake(g_net_fetch_mutex, pdMS_TO_TICKS(NET_MUTEX_WAIT_MS)) != pdTRUE) {
        finish_with_skip("Network subsystem busy, please try again");
        goto cleanup;
    }

    {
        crash_blackbox_net_op_begin("update_check");

        char url[160];
        snprintf(url, sizeof(url), MANIFEST_URL_FORMAT, args->channel);

        // +1 for the terminator, +1 so an oversized body is detectable.
        char *body = static_cast<char *>(malloc(MANIFEST_MAX_BYTES + 2));
        if (!body) {
            crash_blackbox_net_op_end();
            xSemaphoreGive(g_net_fetch_mutex);
            finish_with_skip("Not enough memory for the manifest buffer");
            goto cleanup;
        }

        const int length = fetch_manifest(url, body, MANIFEST_MAX_BYTES, error, sizeof(error));

        crash_blackbox_net_op_end();
        xSemaphoreGive(g_net_fetch_mutex);

        if (length < 0) {
            finish_with_error(error[0] ? error : "Manifest could not be fetched");
        } else if (!apply_manifest(body, error, sizeof(error))) {
            finish_with_error(error[0] ? error : "Manifest could not be read");
        }
        free(body);
    }

cleanup:
    free(args);
    vTaskDelete(NULL);
}

// ---------------------------------------------------------------------------
// Public entry points.
// ---------------------------------------------------------------------------

void update_check_init()
{
    if (s_lock) return;
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        ESP_LOGE(TAG, "Could not create state mutex; update check disabled");
        return;
    }
    memset(&s_status, 0, sizeof(s_status));
    s_status.state = UPDATE_CHECK_IDLE;
    copy_into(s_status.channel, sizeof(s_status.channel), "stable");
}

update_check_trigger_result_t update_check_trigger(const char *channel)
{
    if (!s_lock) return UPDATE_CHECK_TRIGGER_NO_WORKER;
    if (!update_check_valid_channel(channel)) channel = "stable";

    const int64_t now = esp_timer_get_time();

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_status.state == UPDATE_CHECK_RUNNING) {
        xSemaphoreGive(s_lock);
        return UPDATE_CHECK_TRIGGER_BUSY;
    }
    if (s_last_attempt_us != 0 && now - s_last_attempt_us < COOLDOWN_US) {
        xSemaphoreGive(s_lock);
        return UPDATE_CHECK_TRIGGER_COOLDOWN;
    }
    s_status.state = UPDATE_CHECK_RUNNING;
    copy_into(s_status.channel, sizeof(s_status.channel), channel);
    s_last_attempt_us = now;
    xSemaphoreGive(s_lock);

    worker_args *args = static_cast<worker_args *>(calloc(1, sizeof(worker_args)));
    if (args) snprintf(args->channel, sizeof(args->channel), "%s", channel);

    // The fetch never runs on an HTTP server thread: a TLS exchange would hold
    // a server worker for seconds and starve the rest of the WebUI.
    if (!args || xTaskCreate(update_check_task, "upd_check", 5120, args, 3, NULL) != pdPASS) {
        free(args);
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_status.state = UPDATE_CHECK_IDLE;
        // Allow an immediate retry: nothing reached the network.
        s_last_attempt_us = 0;
        xSemaphoreGive(s_lock);
        ESP_LOGE(TAG, "Could not start update check worker");
        return UPDATE_CHECK_TRIGGER_NO_WORKER;
    }

    return UPDATE_CHECK_TRIGGER_ACCEPTED;
}

UpdateCheckStatus update_check_get_status()
{
    UpdateCheckStatus copy = {};
    if (!s_lock) return copy;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    copy = s_status;
    xSemaphoreGive(s_lock);
    return copy;
}
