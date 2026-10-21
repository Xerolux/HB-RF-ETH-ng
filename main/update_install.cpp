/*
 *  update_install.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "update_install.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "cJSON.h"
#include "crash_blackbox.h"
#include "esp_app_desc.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "led.h"
#include "mbedtls/md.h"
#include "monitoring.h"
#include "webui_internal.h"
#include "ota_config.h"
#include "reset_info.h"
#include "semver.h"
#include "system_reset.h"
#include "update_check.h"
#include "update_url_policy.h"
#include "webui_compatibility.h"
#include "webui_internal.h"
#include "webui_storage.h"

// Online installer (docs/AUTO_UPDATE_PLAN.md module 2, manual path).
//
// One downloader, two sinks — the code that writes flash already exists and is
// not duplicated here:
//
//     HTTPS stream --4 KiB--> esp_ota_write()               (firmware)
//                          \-> webui_storage_update_write()  (WebUI image)
//
// The rules this module implements are the plan's acceptance criteria, not
// suggestions: the heap-heavy monitoring workers are stopped BEFORE the TLS
// session is built (their released stacks pay for it), one outbound TLS
// session at a time through g_net_fetch_mutex, absolute deadlines with a
// stall detector, SHA-256 verified before anything is activated, and every
// abort leaves the device fully operational without a reboot.

static const char *TAG = "UpdateInstall";

static constexpr size_t DOWNLOAD_BUFFER_SIZE = 4096;

// Plan rule 12: a stalled peer must not wedge the device. Absolute caps
// instead of per-read hopes. Both values are milliseconds.
static constexpr int64_t DOWNLOAD_TOTAL_TIMEOUT_MS = 10LL * 60LL * 1000LL;
static constexpr int64_t DOWNLOAD_STALL_TIMEOUT_MS = 30LL * 1000LL;
static constexpr int CONNECT_STEP_RETRY_MS         = 10;
static constexpr int NET_MUTEX_WAIT_MS             = 5000;

// Plan rule 13: yield between flash writes so the UDP/UART relay chain keeps
// running during a download.
static constexpr uint32_t YIELD_EVERY_BYTES = 16 * 1024;

static SemaphoreHandle_t s_lock     = NULL;
static UpdateInstallStatus s_status = {};

// ---------------------------------------------------------------------------
// State helpers. Fields are written under s_lock so a status poll never sees
// a half-updated snapshot.
// ---------------------------------------------------------------------------

static void set_state(update_install_state_t state)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.state = state;
    xSemaphoreGive(s_lock);
}

static void set_progress(uint8_t progress)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.progress = progress;
    xSemaphoreGive(s_lock);
}

static void set_version(const char *version)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    snprintf(s_status.targetVersion, sizeof(s_status.targetVersion), "%s", version ? version : "");
    xSemaphoreGive(s_lock);
}

static void finish_with_error(const char *reason)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    snprintf(s_status.error, sizeof(s_status.error), "%s", reason ? reason : "install failed");
    s_status.state = UPDATE_INSTALL_ERROR;
    xSemaphoreGive(s_lock);
    ESP_LOGE(TAG, "Online install failed: %s", reason ? reason : "install failed");
}

const char *update_install_target_name(update_install_target_t target)
{
    return target == UPDATE_INSTALL_TARGET_FIRMWARE ? "firmware"
           : target == UPDATE_INSTALL_TARGET_WEBUI   ? "webui"
                                                     : NULL;
}

// ---------------------------------------------------------------------------
// Bounded asynchronous HTTP, modelled on the manifest fetch in update_check.cpp
// (which took the deadline pattern from events.cpp). One absolute deadline for
// the whole exchange plus an independent stall deadline for the body.
// ---------------------------------------------------------------------------

static int remaining_ms(int64_t deadline_ms)
{
    const int64_t left = deadline_ms - (esp_timer_get_time() / 1000);
    if (left <= 0) return 0;
    return left > INT32_MAX ? INT32_MAX : static_cast<int>(left);
}

static bool step_ready(esp_http_client_handle_t client, int64_t deadline_ms)
{
    const int left = remaining_ms(deadline_ms);
    return left > 0 && esp_http_client_set_timeout_ms(client, left) == ESP_OK;
}

static void retry_delay(int64_t deadline_ms)
{
    int delay = remaining_ms(deadline_ms);
    if (delay > CONNECT_STEP_RETRY_MS) delay = CONNECT_STEP_RETRY_MS;
    if (delay > 0) vTaskDelay(pdMS_TO_TICKS(delay));
}

// ---------------------------------------------------------------------------
// Manifest interpretation. The document is the same fixed-shape schema the
// update search reads; the installer additionally consumes the file entries.
// ---------------------------------------------------------------------------

struct install_manifest {
    char firmwareVersion[UPDATE_INSTALL_VERSION_LEN];
    char firmwareUrl[160];
    char firmwareSha[65];
    uint32_t firmwareSize;
    char firmwareMinFrom[UPDATE_INSTALL_VERSION_LEN];

    char webuiVersion[UPDATE_INSTALL_VERSION_LEN];
    char webuiUrl[160];
    char webuiSha[65];
    uint32_t webuiSize;
    int webuiApi;
    char webuiMinFw[UPDATE_INSTALL_VERSION_LEN];
};

static bool valid_sha256_hex(const char *value)
{
    if (!value || strlen(value) != 64) return false;
    for (size_t i = 0; i < 64; i++) {
        const char c = value[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

static void copy_field(char *dest, size_t size, const cJSON *root, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    snprintf(dest, size, "%s",
             (cJSON_IsString(item) && item->valuestring) ? item->valuestring : "");
}

static bool parse_install_manifest(const char *body, install_manifest *out, char *error,
                                   size_t error_size)
{
    memset(out, 0, sizeof(*out));
    out->webuiApi = -1;

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        snprintf(error, error_size, "Manifest is not valid JSON");
        return false;
    }

    bool ok = false;
    const cJSON *schema = cJSON_GetObjectItemCaseSensitive(root, "s");
    if (!cJSON_IsNumber(schema) || schema->valueint != 1) {
        snprintf(error, error_size, "Unsupported manifest schema");
        goto out;
    }

    copy_field(out->firmwareVersion, sizeof(out->firmwareVersion), root, "fw");
    copy_field(out->firmwareUrl, sizeof(out->firmwareUrl), root, "fwUrl");
    copy_field(out->firmwareSha, sizeof(out->firmwareSha), root, "fwSha");
    copy_field(out->firmwareMinFrom, sizeof(out->firmwareMinFrom), root, "fwMinFrom");
    copy_field(out->webuiVersion, sizeof(out->webuiVersion), root, "ui");
    copy_field(out->webuiUrl, sizeof(out->webuiUrl), root, "uiUrl");
    copy_field(out->webuiSha, sizeof(out->webuiSha), root, "uiSha");
    copy_field(out->webuiMinFw, sizeof(out->webuiMinFw), root, "uiMinFw");
    {
        const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "fwSize");
        if (cJSON_IsNumber(item)) out->firmwareSize = (uint32_t)item->valuedouble;
        item = cJSON_GetObjectItemCaseSensitive(root, "uiSize");
        if (cJSON_IsNumber(item)) out->webuiSize = (uint32_t)item->valuedouble;
        item = cJSON_GetObjectItemCaseSensitive(root, "uiApi");
        if (cJSON_IsNumber(item)) out->webuiApi = item->valueint;
    }

    if (!out->firmwareVersion[0] || !out->webuiVersion[0]) {
        snprintf(error, error_size, "Manifest is missing version fields");
        goto out;
    }
    if (!out->firmwareUrl[0] || !out->firmwareSha[0] || out->firmwareSize == 0 ||
        !out->webuiUrl[0] || !out->webuiSha[0] || out->webuiSize == 0 || out->webuiApi < 1) {
        snprintf(error, error_size, "Manifest has no complete install entries");
        goto out;
    }
    if (!valid_sha256_hex(out->firmwareSha) || !valid_sha256_hex(out->webuiSha)) {
        snprintf(error, error_size, "Manifest SHA-256 fields are malformed");
        goto out;
    }
    if (!update_url_policy_allows(out->firmwareUrl) ||
        !update_url_policy_allows(out->webuiUrl)) {
        // Never fetched, never retried elsewhere: a manifest pointing outside
        // the allowlist is rejected before the first byte moves.
        snprintf(error, error_size, "Manifest download URL is not allowed");
        goto out;
    }

    ok = true;

out:
    cJSON_Delete(root);
    return ok;
}

// ---------------------------------------------------------------------------
// SHA-256 helper over the streaming download, mirroring the proven context
// handling in webui_storage.cpp.
// ---------------------------------------------------------------------------

struct sha_context {
    mbedtls_md_context_t ctx;
    bool ready;
};

static bool sha_begin(sha_context *sha)
{
    sha->ready = false;
    mbedtls_md_init(&sha->ctx);
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!info || mbedtls_md_setup(&sha->ctx, info, 0) != 0 ||
        mbedtls_md_starts(&sha->ctx) != 0) {
        mbedtls_md_free(&sha->ctx);
        return false;
    }
    sha->ready = true;
    return true;
}

static void sha_update(sha_context *sha, const unsigned char *data, size_t len)
{
    mbedtls_md_update(&sha->ctx, data, len);
}

static void sha_finish_hex(sha_context *sha, char *hex, size_t hex_size)
{
    unsigned char digest[32];
    if (sha->ready && mbedtls_md_finish(&sha->ctx, digest) == 0) {
        size_t pos = 0;
        for (size_t i = 0; i < 32 && pos + 2 < hex_size; i++) {
            pos += (size_t)snprintf(hex + pos, hex_size - pos, "%02x", digest[i]);
        }
    } else {
        hex[0] = '\0';
    }
    mbedtls_md_free(&sha->ctx);
    sha->ready = false;
}

static void sha_abort(sha_context *sha)
{
    if (sha->ready) {
        mbedtls_md_free(&sha->ctx);
        sha->ready = false;
    }
}

// ---------------------------------------------------------------------------
// Installer task. Transient like the update-check worker: it exists for one
// installation and deletes itself, so an idle device carries no stack for it.
// ---------------------------------------------------------------------------

struct install_args {
    update_install_target_t target;
    char channel[8];
};

static void install_task(void *parameter)
{
    install_args *args = static_cast<install_args *>(parameter);
    char error[UPDATE_INSTALL_REASON_LEN] = {};

    // Everything acquired below is released in `cleanup` on every path. The
    // firmware success path ends in a restart and deliberately hands its
    // operation reservation to the reserved restart instead.
    bool reservation_held         = false;
    bool ota_flag_set             = false;
    bool net_gate_held            = false;
    bool monitoring_released_by_us = false;
    uint32_t monitoring_mask      = 0;

    char *body            = NULL;
    uint8_t *download_buf = NULL;
    sha_context sha       = {};
    bool sha_started      = false;

    bool fw_ota_begun            = false;
    esp_ota_handle_t fw_ota_handle = 0;
    const esp_partition_t *fw_partition = NULL;
    bool ui_update_begun         = false;

    install_manifest manifest;

    // --- Gates -----------------------------------------------------------
    // Rule 3/4: one operation at a time, and the monitoring workers are
    // stopped BEFORE the TLS session is built - their stacks and TLS state
    // pay for the session that downloads the image.
    if (net_fetch_ota_active()) {
        finish_with_error("An update installation is already running");
        goto cleanup;
    }
    if (ota_operation_try_begin()) {
        reservation_held = true;
    } else {
        finish_with_error("Another device operation is active");
        goto cleanup;
    }

    net_fetch_set_ota_active(true);
    ota_flag_set = true;

    {
        const esp_err_t pause_result = monitoring_pause_for_ota(&monitoring_mask);
        if (pause_result == ESP_OK) {
            monitoring_released_by_us = true;
        } else if (pause_result != ESP_ERR_INVALID_STATE) {
            finish_with_error("Monitoring workers could not be released");
            goto cleanup;
        }
        // ESP_ERR_INVALID_STATE: workers already stopped by another
        // transaction; the heap is as released as it gets and the resume is
        // not ours to perform.
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    if (g_net_fetch_mutex == NULL) {
        finish_with_error("Network subsystem not ready");
        goto cleanup;
    }
    if (xSemaphoreTake(g_net_fetch_mutex, pdMS_TO_TICKS(NET_MUTEX_WAIT_MS)) != pdTRUE) {
        finish_with_error("Network subsystem busy, please try again");
        goto cleanup;
    }
    net_gate_held = true;
    crash_blackbox_net_op_begin("update_install");

    // --- Manifest ---------------------------------------------------------
    // +1 terminator, +1 so an oversized body is detected, exactly like the
    // search path.
    body = static_cast<char *>(malloc(UPDATE_CHECK_MANIFEST_MAX_BYTES + 2));
    if (!body) {
        finish_with_error("Not enough memory for the manifest buffer");
        goto cleanup;
    }
    if (update_check_fetch_manifest(args->channel, body, UPDATE_CHECK_MANIFEST_MAX_BYTES, error,
                                    sizeof(error)) < 0) {
        finish_with_error(error[0] ? error : "Manifest could not be fetched");
        goto cleanup;
    }
    if (!parse_install_manifest(body, &manifest, error, sizeof(error))) {
        finish_with_error(error[0] ? error : "Manifest could not be read");
        goto cleanup;
    }
    free(body);
    body = NULL;

    // --- Target validation, all before any flash is touched ----------------
    if (args->target == UPDATE_INSTALL_TARGET_FIRMWARE) {
        const esp_app_desc_t *app = esp_app_get_description();
        const char *running = app ? app->version : "";

        if (!running[0] || compareVersions(manifest.firmwareVersion, running) <= 0) {
            snprintf(error, sizeof(error), "Firmware %s is not newer than the running version",
                     manifest.firmwareVersion);
            finish_with_error(error);
            goto cleanup;
        }
        if (manifest.firmwareMinFrom[0] && compareVersions(running, manifest.firmwareMinFrom) < 0) {
            // Migration gate (plan 11.4): this device must take the manual
            // path; an automated jump could break its layout.
            snprintf(error, sizeof(error), "Update from %s requires the manual upload path",
                     running);
            finish_with_error(error);
            goto cleanup;
        }
        fw_partition = esp_ota_get_next_update_partition(NULL);
        if (!fw_partition) {
            finish_with_error("No OTA partition available");
            goto cleanup;
        }
        if (manifest.firmwareSize < 0x10000 || manifest.firmwareSize > fw_partition->size) {
            finish_with_error("Manifest firmware size is not plausible");
            goto cleanup;
        }
        set_version(manifest.firmwareVersion);
    } else {
        char running_webui[UPDATE_INSTALL_VERSION_LEN] = {};
        webui_storage_get_effective_version(running_webui, sizeof(running_webui));
        const esp_app_desc_t *app = esp_app_get_description();
        const char *running_fw = app ? app->version : "";

        if (!running_webui[0] || compareVersions(manifest.webuiVersion, running_webui) <= 0) {
            snprintf(error, sizeof(error), "WebUI %s is not newer than the running version",
                     manifest.webuiVersion);
            finish_with_error(error);
            goto cleanup;
        }
        if (manifest.webuiApi != HB_SUPPORTED_WEBUI_API_VERSION) {
            finish_with_error("WebUI requires an unsupported firmware API version");
            goto cleanup;
        }
        if (manifest.webuiMinFw[0] &&
            (!running_fw[0] || compareVersions(running_fw, manifest.webuiMinFw) < 0)) {
            finish_with_error("WebUI requires a newer firmware (install firmware first)");
            goto cleanup;
        }
        set_version(manifest.webuiVersion);
    }

    // --- Download -----------------------------------------------------------
    set_state(UPDATE_INSTALL_DOWNLOADING);
    set_progress(0);
    webui_status_led()->setState(LED_STATE_BLINK_FAST);

    download_buf = static_cast<uint8_t *>(malloc(DOWNLOAD_BUFFER_SIZE));
    if (!download_buf) {
        finish_with_error("Not enough memory for the download buffer");
        goto cleanup;
    }
    if (!sha_begin(&sha)) {
        finish_with_error("Could not initialize SHA-256 verification");
        goto cleanup;
    }
    sha_started = true;

    {
        const char *url = args->target == UPDATE_INSTALL_TARGET_FIRMWARE
                              ? manifest.firmwareUrl
                              : manifest.webuiUrl;
        const uint32_t expected_size = args->target == UPDATE_INSTALL_TARGET_FIRMWARE
                                           ? manifest.firmwareSize
                                           : manifest.webuiSize;

        const int64_t deadline_ms = esp_timer_get_time() / 1000 + DOWNLOAD_TOTAL_TIMEOUT_MS;
        int64_t last_progress_ms  = esp_timer_get_time() / 1000;
        bool download_ok          = false;
        // Declared before the client so `goto download_done` below cannot
        // cross its initialization (C++ forbids that even for scalars).
        bool opened               = false;

        // Plan rule 7 + GitHub reality: release assets answer with a 302 to a
        // signed CDN URL (currently release-assets.githubusercontent.com,
        // carrying well over a kilobyte of Azure SAS parameters). The
        // streaming API (open/fetch_headers) does not follow redirects the
        // way esp_http_client_perform does, and the host allowlist has to be
        // re-checked on every hop anyway - so each redirect is taken
        // explicitly, with a fresh client (a new TLS session per host) and
        // the policy check in between.
        static constexpr size_t EFFECTIVE_URL_MAX_BYTES = 2048;
        static constexpr int MAX_REDIRECT_HOPS = 5;
        esp_http_client_handle_t client = NULL;
        int redirect_hops = 0;
        char *effective_url = static_cast<char *>(malloc(EFFECTIVE_URL_MAX_BYTES));
        if (!effective_url) {
            finish_with_error("Not enough memory for the redirect buffer");
            goto download_done;
        }
        strlcpy(effective_url, url, EFFECTIVE_URL_MAX_BYTES);

        for (;;) {
            opened = false;

            esp_http_client_config_t cfg = {};
            configure_ota_http_client(cfg, effective_url);
            cfg.method     = HTTP_METHOD_GET;
            cfg.timeout_ms = remaining_ms(deadline_ms);
            cfg.is_async   = true;

            client = esp_http_client_init(&cfg);
            if (!client) {
                finish_with_error("HTTP client could not be created");
                goto download_done;
            }

            for (;;) {
                if (!step_ready(client, deadline_ms)) {
                    finish_with_error("Timeout while connecting");
                    goto client_done;
                }
                const esp_err_t err = esp_http_client_open(client, 0);
                if (err == ESP_OK) {
                    opened = true;
                    break;
                }
                if (err != ESP_ERR_HTTP_EAGAIN) {
                    snprintf(error, sizeof(error), "Connection failed: %s", esp_err_to_name(err));
                    finish_with_error(error);
                    goto client_done;
                }
                retry_delay(deadline_ms);
            }
            for (;;) {
                if (!step_ready(client, deadline_ms)) {
                    finish_with_error("Timeout while reading response headers");
                    goto client_done;
                }
                const int64_t headers = esp_http_client_fetch_headers(client);
                if (headers >= 0) break;
                if (headers != -ESP_ERR_HTTP_EAGAIN) {
                    finish_with_error("Could not read response headers");
                    goto client_done;
                }
                retry_delay(deadline_ms);
            }

            const int status = esp_http_client_get_status_code(client);
            if (status == 200) break;

            if (status == 301 || status == 302 || status == 307 || status == 308) {
                if (++redirect_hops > MAX_REDIRECT_HOPS) {
                    finish_with_error("Too many redirects");
                    goto client_done;
                }
                // The pointer targets the client's header storage; copy
                // before the client is torn down.
                char *location = NULL;
                if (esp_http_client_get_header(client, "Location", &location) != ESP_OK ||
                    location == NULL) {
                    finish_with_error("Redirect without a Location header");
                    goto client_done;
                }
                if (strncmp(location, "https://", 8) != 0 ||
                    !update_url_policy_allows(location)) {
                    snprintf(error, sizeof(error), "Redirect target not allowed: %.48s", location);
                    finish_with_error(error);
                    goto client_done;
                }
                if (strlen(location) >= EFFECTIVE_URL_MAX_BYTES) {
                    finish_with_error("Redirect URL too long");
                    goto client_done;
                }
                strlcpy(effective_url, location, EFFECTIVE_URL_MAX_BYTES);
                if (opened) esp_http_client_close(client);
                esp_http_client_cleanup(client);
                client = NULL;
                continue;
            }

            snprintf(error, sizeof(error), "Server returned HTTP %d", status);
            finish_with_error(error);
            goto client_done;
        }

        if (esp_http_client_get_content_length(client) != (int64_t)expected_size) {
            finish_with_error("Download size does not match the manifest");
            goto client_done;
        }

        // The WebUI sink opens before the first byte: its transaction marker
        // must be committed before the partition erase starts (the same
        // ordering the browser upload uses). The firmware sink opens on the
        // first chunk, after the ESP32 magic byte was seen, so a wrong file
        // never erases the OTA partition.
        if (args->target == UPDATE_INSTALL_TARGET_WEBUI) {
            const esp_err_t begin_result = webui_storage_update_begin(expected_size,
                                                                      manifest.webuiSha);
            if (begin_result != ESP_OK) {
                snprintf(error, sizeof(error), "Could not start the WebUI update: %s",
                         esp_err_to_name(begin_result));
                finish_with_error(error);
                goto client_done;
            }
            ui_update_begun = true;
        }

        {
            uint32_t received          = 0;
            uint32_t bytes_since_yield = 0;
            bool first_chunk           = true;

            while (received < expected_size) {
                if (!step_ready(client, deadline_ms)) {
                    finish_with_error("Download timed out");
                    goto client_done;
                }
                if (esp_timer_get_time() / 1000 - last_progress_ms > DOWNLOAD_STALL_TIMEOUT_MS) {
                    finish_with_error("Download stalled");
                    goto client_done;
                }

                errno        = 0;
                const int got = esp_http_client_read(client,
                                                     reinterpret_cast<char *>(download_buf),
                                                     DOWNLOAD_BUFFER_SIZE);
                if (got <= 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR ||
                                 (got == 0 && errno == 0 &&
                                  !esp_http_client_is_complete_data_received(client)))) {
                    retry_delay(deadline_ms);
                    continue;
                }
                if (got == 0) {
                    // Server closed the stream early (or announced the end
                    // while we still expect bytes - both are truncations).
                    snprintf(error, sizeof(error), "Download incomplete (%u of %u bytes)",
                             (unsigned)received, (unsigned)expected_size);
                    finish_with_error(error);
                    goto client_done;
                }
                if (got < 0) {
                    finish_with_error("Connection lost during the download");
                    goto client_done;
                }

                last_progress_ms = esp_timer_get_time() / 1000;

                if (first_chunk) {
                    first_chunk = false;
                    if (args->target == UPDATE_INSTALL_TARGET_FIRMWARE) {
                        if (download_buf[0] != 0xE9) {
                            finish_with_error("Download is not an ESP32 firmware image");
                            goto client_done;
                        }
                        const esp_err_t begin_result =
                            esp_ota_begin(fw_partition, expected_size, &fw_ota_handle);
                        if (begin_result != ESP_OK) {
                            snprintf(error, sizeof(error), "Could not start the OTA write: %s",
                                     esp_err_to_name(begin_result));
                            finish_with_error(error);
                            goto client_done;
                        }
                        fw_ota_begun = true;
                    }
                }

                sha_update(&sha, download_buf, static_cast<size_t>(got));

                const esp_err_t write_result =
                    args->target == UPDATE_INSTALL_TARGET_FIRMWARE
                        ? esp_ota_write(fw_ota_handle, download_buf, static_cast<size_t>(got))
                        : webui_storage_update_write(download_buf, static_cast<size_t>(got));
                if (write_result != ESP_OK) {
                    snprintf(error, sizeof(error), "Flash write failed: %s",
                             esp_err_to_name(write_result));
                    finish_with_error(error);
                    goto client_done;
                }

                received += static_cast<uint32_t>(got);
                bytes_since_yield += static_cast<uint32_t>(got);
                if (bytes_since_yield >= YIELD_EVERY_BYTES) {
                    // Rule 13: keep the relay chain's CPU free.
                    bytes_since_yield = 0;
                    vTaskDelay(pdMS_TO_TICKS(2));
                }
                set_progress(static_cast<uint8_t>(received * 100 / expected_size));
            }

            download_ok = true;
        }

    client_done:
        if (client) {
            if (opened) esp_http_client_close(client);
            esp_http_client_cleanup(client);
        }

    download_done:
        free(effective_url);
        if (!download_ok) {
            if (sha_started) {
                sha_abort(&sha);
                sha_started = false;
            }
            goto cleanup;
        }
    }

    // --- Verify + activate ---------------------------------------------------
    set_state(UPDATE_INSTALL_VERIFYING);
    {
        char digest_hex[65];
        sha_finish_hex(&sha, digest_hex, sizeof(digest_hex));
        sha_started = false;
        const char *expected_sha = args->target == UPDATE_INSTALL_TARGET_FIRMWARE
                                       ? manifest.firmwareSha
                                       : manifest.webuiSha;
        if (digest_hex[0] == '\0' || strcasecmp(digest_hex, expected_sha) != 0) {
            finish_with_error("SHA-256 mismatch, image discarded");
            goto cleanup;
        }
    }

    if (args->target == UPDATE_INSTALL_TARGET_FIRMWARE) {
        // Activate only after the digest matched; the 15-second self-test
        // rollback window covers the boot that follows.
        const esp_err_t end_result = esp_ota_end(fw_ota_handle);
        fw_ota_begun = false; // after esp_ota_end the handle is gone either way
        if (end_result != ESP_OK) {
            snprintf(error, sizeof(error), "Downloaded image is not valid: %s",
                     esp_err_to_name(end_result));
            finish_with_error(error);
            goto cleanup;
        }
        const esp_err_t boot_result = esp_ota_set_boot_partition(fw_partition);
        if (boot_result != ESP_OK || esp_ota_get_boot_partition() != fw_partition) {
            // Nothing was activated; the running partition stays selected.
            snprintf(error, sizeof(error), "Could not select the downloaded image: %s",
                     esp_err_to_name(boot_result));
            finish_with_error(error);
            goto cleanup;
        }

        ESP_LOGI(TAG, "Online firmware install verified, restarting to activate %s",
                 manifest.firmwareVersion);

        set_state(UPDATE_INSTALL_FINALIZING);
        webui_status_led()->setState(LED_STATE_OFF);
        ResetInfo::storeResetReason(RESET_REASON_FIRMWARE_UPDATE);

        // Release the shared network state before the restart machinery takes
        // over; the operation reservation is deliberately kept because the
        // reserved restart consumes it.
        crash_blackbox_net_op_end();
        xSemaphoreGive(g_net_fetch_mutex);
        net_gate_held = false;
        net_fetch_set_ota_active(false);
        ota_flag_set = false;

        // Give the polling UI a moment to see "finalizing" before the link
        // goes down for the restart.
        vTaskDelay(pdMS_TO_TICKS(3000));
        refresh_restart_sync_from_settings();
        full_system_restart_with_reserved_operation();
        // Reached only if the reserved restart was rejected: fall through to
        // the normal cleanup so the device stays fully operational.
        finish_with_error("Restart after install was rejected");
    } else {
        const esp_err_t finish_result = webui_storage_update_finish();
        ui_update_begun = false; // finish() ends the transaction either way
        if (finish_result != ESP_OK) {
            snprintf(error, sizeof(error), "WebUI image was rejected: %s",
                     esp_err_to_name(finish_result));
            finish_with_error(error);
            goto cleanup;
        }
        ESP_LOGI(TAG, "Online WebUI install verified and mounted: %s", manifest.webuiVersion);
        webui_status_led()->setState(LED_STATE_OFF);
        set_state(UPDATE_INSTALL_SUCCESS);
        set_progress(100);
    }

cleanup:
    if (net_gate_held) {
        crash_blackbox_net_op_end();
        xSemaphoreGive(g_net_fetch_mutex);
        net_gate_held = false;
    }
    if (fw_ota_begun) {
        esp_ota_abort(fw_ota_handle);
    }
    if (ui_update_begun) {
        webui_storage_update_abort();
    }
    if (sha_started) {
        sha_abort(&sha);
    }
    free(download_buf);
    free(body);
    if (ota_flag_set) {
        net_fetch_set_ota_active(false);
    }
    if (monitoring_released_by_us) {
        monitoring_resume_after_ota(monitoring_mask);
    }
    if (reservation_held) {
        ota_operation_finish();
    }
    webui_status_led()->setState(LED_STATE_OFF);
    free(args);
    vTaskDelete(NULL);
}

// ---------------------------------------------------------------------------
// Public entry points.
// ---------------------------------------------------------------------------

void update_install_init()
{
    if (s_lock) return;
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        ESP_LOGE(TAG, "Could not create state mutex; online install disabled");
        return;
    }
    memset(&s_status, 0, sizeof(s_status));
    s_status.state  = UPDATE_INSTALL_IDLE;
    s_status.target = UPDATE_INSTALL_TARGET_FIRMWARE;
}

static bool install_state_busy(update_install_state_t state)
{
    return state == UPDATE_INSTALL_PREPARING || state == UPDATE_INSTALL_DOWNLOADING ||
           state == UPDATE_INSTALL_VERIFYING || state == UPDATE_INSTALL_FINALIZING;
}

update_install_result_t update_install_start(const char *target)
{
    if (!s_lock) return UPDATE_INSTALL_NO_WORKER;

    update_install_target_t parsed;
    if (target && strcmp(target, "firmware") == 0) {
        parsed = UPDATE_INSTALL_TARGET_FIRMWARE;
    } else if (target && strcmp(target, "webui") == 0) {
        parsed = UPDATE_INSTALL_TARGET_WEBUI;
    } else {
        return UPDATE_INSTALL_INVALID_TARGET;
    }

    // Which manifest the install reads: a search that ran THIS boot names
    // the channel the operator is looking at; otherwise (typical for a
    // scheduled install after a reboot - the search status is RAM-only) the
    // persisted channel setting decides. Falling back to a hard "stable"
    // instead silently installed from the wrong manifest - found live when a
    // beta-scheduled WebUI install rejected 1.0.7 as "not newer" because it
    // had read the stable manifest's 1.0.6.
    const UpdateCheckStatus check = update_check_get_status();
    char channel[8];
    if (check.everChecked && update_check_valid_channel(check.channel)) {
        snprintf(channel, sizeof(channel), "%s", check.channel);
    } else {
        snprintf(channel, sizeof(channel), "%s",
                 webui_settings()->getUpdChannel() == 1 ? "beta" : "stable");
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (install_state_busy(s_status.state)) {
        xSemaphoreGive(s_lock);
        return UPDATE_INSTALL_BUSY;
    }
    s_status.state             = UPDATE_INSTALL_PREPARING;
    s_status.target            = parsed;
    s_status.progress          = 0;
    s_status.targetVersion[0]  = '\0';
    s_status.error[0]          = '\0';
    xSemaphoreGive(s_lock);

    install_args *args = static_cast<install_args *>(calloc(1, sizeof(install_args)));
    if (args) {
        args->target = parsed;
        snprintf(args->channel, sizeof(args->channel), "%s", channel);
    }

    // 8 KiB stack: this task runs an mbedTLS handshake, cJSON parsing and the
    // OTA write machinery - everything the update-check worker (6 KiB) does
    // plus the flash sink. Priority 2 keeps it below the relay chain.
    if (!args || xTaskCreate(install_task, "upd_install", 8192, args, 2, NULL) != pdPASS) {
        free(args);
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_status.state = UPDATE_INSTALL_IDLE;
        xSemaphoreGive(s_lock);
        ESP_LOGE(TAG, "Could not start install worker");
        return UPDATE_INSTALL_NO_WORKER;
    }

    return UPDATE_INSTALL_ACCEPTED;
}

UpdateInstallStatus update_install_get_status()
{
    UpdateInstallStatus copy = {};
    if (!s_lock) return copy;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    copy = s_status;
    xSemaphoreGive(s_lock);
    return copy;
}
