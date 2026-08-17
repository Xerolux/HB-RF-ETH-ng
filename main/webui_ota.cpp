/*
 *  webui_ota.cpp is part of the HB-RF-ETH firmware v2.0
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <sys/param.h>
#include <atomic>
#include <memory>
#include <new>
#include "webui.h"
#include "webui_internal.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "esp_ota_ops.h"
#include "monitoring.h"
#include "monitoring_api.h"
#include "security_headers.h"
#include "secure_utils.h"
#include "log_manager.h"
#include "reset_info.h"
#include "nvs_storage_lock.h"
#include "system_reset.h"
#include "crash_blackbox.h"
#include "events.h"
#include "theme_api.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "validation.h"
#include "settings.h"
#include "led.h"

// Firmware upload and the system-level actions that share its restart
// machinery (restart, factory reset, upload status), extracted from webui.cpp.
//
// These belong together because they all take the exclusive device-operation
// reservation and all end in a controlled reboot; splitting them further would
// separate prepare_ota_heap() from its only callers.

static const char *TAG = "WebUI.ota";

// Forward declaration: prepare_ota_heap() is defined further down (after the
// upload handler) but is now also used on the success path of the upload
// handler so that heap/network-active subsystems are stopped before the
// restart. Without this the upload handler would not compile.
static esp_err_t prepare_ota_heap(uint32_t *paused_monitoring);
struct ota_finalize_ctx {
    const esp_partition_t *update_partition;
    const esp_partition_t *running;
};
static void ota_finalize_task(void *pv);

#define OTA_CHECK(a, str, ...)                                                                     \
    do {                                                                                           \
        if (!(a)) {                                                                                \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__);                  \
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, str);                        \
            goto err;                                                                              \
        }                                                                                          \
    } while (0)

#define OTA_BUFFER_SIZE 4096

// OTA status tracking for the remaining manual firmware upload endpoint
// (/ota_update). Automatic update checks and URL-based OTA were removed, but
// this gate still prevents two administrators from writing the update
// partition concurrently.
enum ota_status_t { OTA_IDLE = 0, OTA_DOWNLOADING, OTA_FINALIZING, OTA_SUCCESS, OTA_FAILED };

static std::atomic<ota_status_t> _ota_status{OTA_IDLE};
static std::atomic<int> _ota_progress{0}; // 0-100
static char _ota_error[128]        = {0};
static portMUX_TYPE _ota_error_mux = portMUX_INITIALIZER_UNLOCKED;

static void set_ota_error(const char *format, ...)
{
    char text[sizeof(_ota_error)];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    portENTER_CRITICAL(&_ota_error_mux);
    snprintf(_ota_error, sizeof(_ota_error), "%s", text);
    portEXIT_CRITICAL(&_ota_error_mux);
}

static void copy_ota_error(char *dest, size_t size)
{
    portENTER_CRITICAL(&_ota_error_mux);
    snprintf(dest, size, "%s", _ota_error);
    portEXIT_CRITICAL(&_ota_error_mux);
}

esp_err_t post_ota_update_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    bool net_gate_held = false;

    if (validate_auth(req) != ESP_OK) {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    if (!ota_operation_try_begin()) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "OTA update already in progress");
        return ESP_OK;
    }
    _ota_status   = OTA_DOWNLOADING;
    _ota_progress = 0;
    set_ota_error("");

    // The inbound upload itself is not TLS, but its 4 KiB buffer plus flash
    // erase/write pressure must not overlap an outbound TLS handshake on a
    // WROOM-32. Mark the operation before waiting so best-effort consumers
    // defer; hold the ownershipless gate only while bytes are streamed.
    net_fetch_set_ota_active(true);
    if (g_net_fetch_mutex != NULL) {
        if (xSemaphoreTake(g_net_fetch_mutex, pdMS_TO_TICKS(20000)) != pdTRUE) {
            ESP_LOGE(TAG, "Could not reserve network/TLS gate for OTA upload");
            _ota_status = OTA_FAILED;
            set_ota_error("Network/TLS subsystem busy");
            net_fetch_set_ota_active(false);
            ota_operation_finish();
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                       "Network/TLS subsystem busy");
        }
        net_gate_held = true;
        crash_blackbox_net_op_begin("webui_ota_upload");
    }

    esp_ota_handle_t ota_handle = 0;
    bool ota_begun              = false;

    char *ota_buff = (char *)malloc(OTA_BUFFER_SIZE);
    if (!ota_buff) {
        ESP_LOGE(TAG, "Failed to allocate OTA buffer");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        _ota_status = OTA_FAILED;
        if (net_gate_held) {
            crash_blackbox_net_op_end();
            xSemaphoreGive(g_net_fetch_mutex);
        }
        net_fetch_set_ota_active(false);
        ota_operation_finish();
        return ESP_FAIL;
    }

    const size_t content_length = req->content_len;
    if (content_length == 0x50000) {
        ESP_LOGW(TAG, "Rejected 320 KiB WebUI image on firmware endpoint");
        free(ota_buff);
        _ota_status = OTA_FAILED;
        if (net_gate_held) {
            crash_blackbox_net_op_end();
            xSemaphoreGive(g_net_fetch_mutex);
        }
        net_fetch_set_ota_active(false);
        ota_operation_finish();
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                   "Falsche Datei: Das 327680-Byte-WebUI-/WWW-Image muss unter "
                                   "System -> WebUI installiert werden.");
    }
    size_t content_received = 0;
    int recv_len;
    int timeout_retries                                        = 5;
    static constexpr int64_t OTA_UPLOAD_TOTAL_TIMEOUT_US       = 5LL * 60LL * 1000LL * 1000LL;
    static constexpr int64_t OTA_UPLOAD_NO_PROGRESS_TIMEOUT_US = 30LL * 1000LL * 1000LL;
    const int64_t upload_started_us                            = esp_timer_get_time();
    int64_t last_progress_us                                   = upload_started_us;
    bool is_req_body_started                                   = false;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    const esp_partition_t *running          = NULL;
    esp_err_t ota_end_result                = ESP_OK;

    // Validate update partition exists
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA update partition found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition available");
        goto err;
    }

    // HTTP Content-Length is size_t. Narrowing it to int before MIN() could
    // turn a value above INT_MAX negative and then back into a huge size_t in
    // httpd_req_recv(), overflowing the fixed 4 KiB buffer. Also reject images
    // which cannot fit the selected application partition before reading a
    // single byte from the socket.
    if (content_length == 0 || content_length > update_partition->size ||
        content_length > static_cast<size_t>(INT32_MAX)) {
        ESP_LOGW(TAG, "Rejected OTA content length: %u (partition %u)", (unsigned)content_length,
                 (unsigned)update_partition->size);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid firmware image size");
        goto err;
    }

    ESP_LOGI(TAG, "Starting OTA update, partition: %s, size: %u bytes", update_partition->label,
             (unsigned)content_length);

    do {
        const int64_t now_us = esp_timer_get_time();
        if (now_us - upload_started_us > OTA_UPLOAD_TOTAL_TIMEOUT_US ||
            now_us - last_progress_us > OTA_UPLOAD_NO_PROGRESS_TIMEOUT_US) {
            ESP_LOGE(TAG, "OTA upload deadline exceeded after %u of %u bytes",
                     (unsigned)content_received, (unsigned)content_length);
            set_ota_error("Firmware upload timed out");
            httpd_resp_set_status(req, "408 Request Timeout");
            httpd_resp_sendstr(req, "Firmware upload timed out");
            goto err;
        }

        const size_t receive_cap =
            MIN(content_length - content_received, static_cast<size_t>(OTA_BUFFER_SIZE));
        if ((recv_len = httpd_req_recv(req, ota_buff, receive_cap)) < 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT && timeout_retries-- > 0) {
                // Transient timeout - retry a bounded number of times. An
                // unbounded retry loop would wedge the single httpd task
                // forever if the client stalls mid-upload.
                continue;
            } else {
                ESP_LOGE(TAG, "OTA socket error %d, received %u of %u bytes", recv_len,
                         (unsigned)content_received, (unsigned)content_length);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                    "Network error during upload");
                goto err;
            }
        } else if (recv_len == 0) {
            // Connection closed by client
            ESP_LOGE(TAG, "OTA connection closed prematurely, received %u of %u bytes",
                     (unsigned)content_received, (unsigned)content_length);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Incomplete upload");
            goto err;
        }

        // A peer may trickle a few bytes before each socket timeout. Reset the
        // transient retry budget on real progress, while the independent
        // no-progress and total deadlines keep the single httpd task bounded.
        last_progress_us = esp_timer_get_time();
        timeout_retries  = 5;

        if (!is_req_body_started) {
            is_req_body_started = true;

            if (recv_len <= 0 || static_cast<unsigned char>(ota_buff[0]) != 0xE9) {
                ESP_LOGW(TAG, "Rejected non-ESP firmware image (magic 0x%02x)",
                         recv_len > 0 ? static_cast<unsigned char>(ota_buff[0]) : 0);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                    "Falsche Datei: kein gueltiges ESP32-Firmware-Abbild. WebUI "
                                    "unter System -> WebUI installieren.");
                goto err;
            }

            // Only raw binary uploads are supported (the WebUI posts the file
            // as the request body). The previous multipart/form-data path was
            // broken by design: it compared stripped body bytes against the
            // full content length (loop never terminated) and wrote the
            // trailing boundary into flash.
            char content_type[64] = {0};
            if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type,
                                            sizeof(content_type)) == ESP_OK &&
                strstr(content_type, "multipart/form-data") != NULL) {
                ESP_LOGE(TAG, "Multipart firmware uploads are not supported - send the raw binary "
                              "as request body");
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                    "Multipart uploads not supported, send raw binary body");
                goto err;
            }

            OTA_CHECK(esp_ota_begin(update_partition, content_length, &ota_handle) == ESP_OK,
                      "Could not start OTA");
            ota_begun = true;
            ESP_LOGW(TAG, "Begin OTA Update to partition %s, File Size: %u",
                     update_partition->label, (unsigned)content_length);
            webui_status_led()->setState(LED_STATE_BLINK_FAST);

            OTA_CHECK(esp_ota_write(ota_handle, ota_buff, recv_len) == ESP_OK, "Error writing OTA");
            content_received += static_cast<size_t>(recv_len);
            _ota_progress = static_cast<int>(content_received * 100 / content_length);
            ESP_LOGI(TAG, "OTA progress: %u / %u bytes (%d%%)", (unsigned)content_received,
                     (unsigned)content_length, _ota_progress.load());
        } else {
            OTA_CHECK(esp_ota_write(ota_handle, ota_buff, recv_len) == ESP_OK, "Error writing OTA");
            content_received += static_cast<size_t>(recv_len);
            _ota_progress = static_cast<int>(content_received * 100 / content_length);
            ESP_LOGI(TAG, "OTA progress: %u / %u bytes (%d%%)", (unsigned)content_received,
                     (unsigned)content_length, _ota_progress.load());
        }
    } while (content_received < content_length);

    // Verify complete firmware was received
    if (content_received != content_length) {
        ESP_LOGE(TAG, "Incomplete firmware: received %u of %u bytes", (unsigned)content_received,
                 (unsigned)content_length);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Incomplete firmware upload");
        goto err;
    }

    // Validate and finalize OTA
    // esp_ota_end() consumes/removes the OTA handle even when image
    // verification fails. Clear the ownership flag before testing its result
    // so the error path never calls esp_ota_abort() on a stale handle.
    ota_end_result = esp_ota_end(ota_handle);
    ota_begun      = false;
    OTA_CHECK(ota_end_result == ESP_OK, "Error finalizing OTA");

    // Verify the firmware image before setting boot partition
    ESP_LOGI(TAG, "Validating firmware image...");
    running = esp_ota_get_running_partition();

    if (running == NULL || update_partition == running) {
        ESP_LOGE(TAG, "Cannot update running partition!");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid OTA partition");
        goto err;
    }

    // The image is finalized and no longer needs the upload buffer. Release it
    // before stopping workers so a fragmented heap does not turn an otherwise
    // valid update into a cleanup timeout.
    free(ota_buff);
    ota_buff = NULL;

    // Release before stopping MQTT/other workers: an MQTT reconnect callback
    // may be waiting on this gate and its cooperative stop must be allowed to
    // finish. No more upload buffer or flash write is active beyond this point.
    if (net_gate_held) {
        crash_blackbox_net_op_end();
        xSemaphoreGive(g_net_fetch_mutex);
        net_gate_held = false;
    }
    net_fetch_set_ota_active(false);

    // Offload the post-upload sequence (worker stop → boot select → restart)
    // to a background task so the httpd thread stays free to serve status
    // polls during the potentially 90 s sequential worker-stop phase.
    _ota_status = OTA_FINALIZING;

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(
        req, "{\"success\":true,\"message\":\"Firmware received, finalizing and restarting...\"}");

    // Scope ctx so goto err (from the upload phase above) cannot jump over
    // its initialization — C++ forbids crossing a declaration with an
    // initializer even for pointer types.
    {
        ota_finalize_ctx *ctx = (ota_finalize_ctx *)calloc(1, sizeof(ota_finalize_ctx));
        if (!ctx) {
            ESP_LOGE(TAG, "Could not allocate OTA finalize context");
            _ota_status = OTA_FAILED;
            set_ota_error("Memory allocation failed during finalize");
            ota_operation_finish();
            return ESP_OK;
        }
        ctx->update_partition = update_partition;
        ctx->running          = running;

        if (xTaskCreate(ota_finalize_task, "ota_finalize", 8192, ctx, 2, NULL) != pdPASS) {
            ESP_LOGE(TAG, "Could not create OTA finalize task");
            _ota_status = OTA_FAILED;
            set_ota_error("Could not start finalize task");
            free(ctx);
            ota_operation_finish();
            return ESP_OK;
        }
    }

    return ESP_OK;

err:
    if (ota_buff) free(ota_buff);
    if (net_gate_held) {
        crash_blackbox_net_op_end();
        xSemaphoreGive(g_net_fetch_mutex);
    }
    net_fetch_set_ota_active(false);
    webui_status_led()->setState(LED_STATE_OFF);
    _ota_status = OTA_FAILED;

    // Abort OTA if it was started but not completed
    if (ota_begun) {
        ESP_LOGW(TAG, "Aborting OTA operation due to error");
        esp_ota_abort(ota_handle);
    }

    // Store reset reason for failed firmware update
    ResetInfo::storeResetReason(RESET_REASON_UPDATE_FAILED);
    ota_operation_finish();
    return ESP_FAIL;
}

httpd_uri_t post_ota_update_handler = {.uri      = "/ota_update",
                                       .method   = HTTP_POST,
                                       .handler  = post_ota_update_handler_func,
                                       .user_ctx = NULL};

esp_err_t post_restart_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }

    httpd_resp_set_type(req, "application/json");
    /* CORS header removed - same-origin requests only */
    httpd_resp_sendstr(req, "{\"success\":true}");

    // Store reset reason before restart
    ResetInfo::storeResetReason(RESET_REASON_USER_RESTART);

    // Queue the notification before the delay below so the events worker has
    // a chance to pick it up (queue poll: 500ms) and start delivery before
    // full_system_restart()'s monitoring_pause_for_ota() stops the worker —
    // that stop path waits up to 30s for an in-flight send to finish, but
    // only if the worker already dequeued the entry.
    events_emit(EVENT_RESTART, "manual restart via WebUI/API");

    // Restart after a short delay to allow response to be sent
    vTaskDelay(pdMS_TO_TICKS(1000));
    refresh_restart_sync_from_settings();
    full_system_restart();

    return ESP_OK;
}

httpd_uri_t post_restart_handler = {.uri      = "/api/restart",
                                    .method   = HTTP_POST,
                                    .handler  = post_restart_handler_func,
                                    .user_ctx = NULL};

esp_err_t post_factory_reset_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }

    ScopedOperationReservation operation;
    if (!operation) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, "Another configuration or restart operation is active");
    }

    // Queue the notification against the still-live (pre-wipe) notify config
    // before anything is erased. Same delivery-window reasoning as
    // post_restart_handler_func: the events worker gets the 1s pre-restart
    // delay plus up to 30s inside monitoring_pause_for_ota() to actually
    // send it, as long as it dequeues the entry before that stop begins.
    events_emit(EVENT_FACTORY_RESET, "factory reset via WebUI/API");

    // Erase every user-controlled namespace first. reset_info is deliberately
    // part of Settings::clear(), so store the one allowed post-reset metadata
    // value and report success only after the complete wipe succeeded.
    const esp_err_t clear_result = webui_settings()->clear();
    if (clear_result != ESP_OK) {
        ESP_LOGE(TAG, "Factory reset failed while clearing settings: %s",
                 esp_err_to_name(clear_result));
        return send_json_error(req, "500 Internal Server Error", "factory_reset_failed",
                               "settings");
    }
    ResetInfo::storeResetReason(RESET_REASON_FACTORY_RESET);

    httpd_resp_set_type(req, "application/json");
    /* CORS header removed - same-origin requests only */
    httpd_resp_sendstr(req, "{\"success\":true}");

    // Restart after a short delay to allow response to be sent
    vTaskDelay(pdMS_TO_TICKS(1000));
    refresh_restart_sync_from_settings();
    full_system_restart_with_reserved_operation();

    return ESP_OK;
}

httpd_uri_t post_factory_reset_handler = {.uri      = "/api/factory-reset",
                                          .method   = HTTP_POST,
                                          .handler  = post_factory_reset_handler_func,
                                          .user_ctx = NULL};

static esp_err_t get_ota_status_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }

    const char *status_str;
    const ota_status_t status = _ota_status.load();
    switch (status) {
        case OTA_DOWNLOADING:
            status_str = "downloading";
            break;
        case OTA_FINALIZING:
            status_str = "finalizing";
            break;
        case OTA_SUCCESS:
            status_str = "success";
            break;
        case OTA_FAILED:
            status_str = "failed";
            break;
        default:
            status_str = "idle";
            break;
    }

    const char *flashPause =
        (webui_settings() && webui_settings()->getFlashPause()) ? "true" : "false";
    // The escaped OTA error can use up to twice the 128-byte source buffer.
    // Leave enough room for the surrounding JSON without truncating it.
    char buf[384];

    if (status == OTA_FAILED) {
        char error[sizeof(_ota_error)];
        copy_ota_error(error, sizeof(error));
        if (error[0] != '\0') {
            // Escape any quotes in the error string just in case
            char esc_error[sizeof(_ota_error) * 2] = {0};
            int j                                  = 0;
            for (int i = 0; error[i] && j < sizeof(esc_error) - 2; i++) {
                if (error[i] == '"' || error[i] == '\\') {
                    esc_error[j++] = '\\';
                }
                esc_error[j++] = error[i];
            }
            snprintf(buf, sizeof(buf),
                     "{\"status\":\"%s\",\"progress\":%d,\"flashPause\":%s,\"error\":\"%s\"}",
                     status_str, _ota_progress.load(), flashPause, esc_error);
        } else {
            snprintf(buf, sizeof(buf), "{\"status\":\"%s\",\"progress\":%d,\"flashPause\":%s}",
                     status_str, _ota_progress.load(), flashPause);
        }
    } else {
        snprintf(buf, sizeof(buf), "{\"status\":\"%s\",\"progress\":%d,\"flashPause\":%s}",
                 status_str, _ota_progress.load(), flashPause);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, buf);

    return ESP_OK;
}

httpd_uri_t get_ota_status_handler = {.uri      = "/api/ota_status",
                                      .method   = HTTP_GET,
                                      .handler  = get_ota_status_handler_func,
                                      .user_ctx = NULL};

// Free heap for the manual firmware upload restart by shutting down heap-heavy
// subsystems.
// The ESP32-WROOM-32 has no PSRAM; with MQTT/monitoring running, only
// ~60 KB heap can remain. Keeping those workers alive while finalizing the
// uploaded image and taking Ethernet down also creates avoidable contention.
// On OTA success the device restarts and
// everything comes back; on failure the returned mask is used to resume the
// paused monitoring workers without requiring a manual restart.
static esp_err_t prepare_ota_heap(uint32_t *paused_monitoring)
{
    if (!paused_monitoring) return ESP_ERR_INVALID_ARG;
    *paused_monitoring = 0;
    ESP_LOGI(TAG, "Preparing heap for manual OTA restart (current free: %u KB)",
             (unsigned)(esp_get_free_heap_size() / 1024));

    // Stop MQTT, CheckMK, Prometheus, Syslog and notification workers. Besides
    // TLS state, this can free several task stacks (6-8 KB each) before OTA.
    esp_err_t monitoring_pause_result = monitoring_pause_for_ota(paused_monitoring);
    if (monitoring_pause_result != ESP_OK) {
        ESP_LOGE(TAG, "Cannot continue OTA/restart while monitoring is stopping: %s",
                 esp_err_to_name(monitoring_pause_result));
        return monitoring_pause_result;
    }

    // Note: the automatic update-check feature (former UpdateCheck esp_timer)
    // was removed, so there is no background fetch task left to stop here.

    // Brief settle for heap de-fragmentation
    vTaskDelay(pdMS_TO_TICKS(200));
    return ESP_OK;
}

// Background finalize task: stop workers, select boot partition, restart.
// Runs on its own task so the httpd thread is free to serve status polls
// during the (potentially 90 s) sequential worker-stop phase.
static void ota_finalize_task(void *pv)
{
    ota_finalize_ctx *ctx      = static_cast<ota_finalize_ctx *>(pv);
    uint32_t paused_monitoring = 0;

    esp_err_t prepare_result = prepare_ota_heap(&paused_monitoring);
    if (prepare_result != ESP_OK) {
        webui_status_led()->setState(LED_STATE_OFF);
        _ota_status = OTA_FAILED;
        set_ota_error("Restart deferred: background network cleanup failed");
        ResetInfo::storeResetReason(RESET_REASON_UPDATE_FAILED);
        monitoring_resume_after_ota(paused_monitoring);
        ota_operation_finish();
        free(ctx);
        vTaskDelete(NULL);
        return;
    }

    esp_err_t boot_result                = esp_ota_set_boot_partition(ctx->update_partition);
    const esp_partition_t *selected_boot = esp_ota_get_boot_partition();
    if (selected_boot != ctx->update_partition) {
        ESP_LOGE(TAG, "Could not select uploaded OTA partition: set=%s selected=%s",
                 esp_err_to_name(boot_result), selected_boot ? selected_boot->label : "unknown");

        bool running_restored = (selected_boot == ctx->running);
        for (int attempt = 0; !running_restored && attempt < 3; ++attempt) {
            esp_err_t restore_result = esp_ota_set_boot_partition(ctx->running);
            selected_boot            = esp_ota_get_boot_partition();
            running_restored         = (restore_result == ESP_OK && selected_boot == ctx->running);
            if (!running_restored) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }

        webui_status_led()->setState(LED_STATE_OFF);
        _ota_status = OTA_FAILED;
        set_ota_error(running_restored ? "Could not activate uploaded firmware"
                                       : "Boot selection uncertain; restarting safely");
        ResetInfo::storeResetReason(RESET_REASON_UPDATE_FAILED);

        if (running_restored) {
            monitoring_resume_after_ota(paused_monitoring);
            ota_operation_finish();
            free(ctx);
            vTaskDelete(NULL);
            return;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
        refresh_restart_sync_from_settings();
        full_system_restart_with_reserved_operation();
        free(ctx);
        return;
    }

    ESP_LOGI(TAG, "OTA finished successfully, restarting to activate new firmware.");
    webui_status_led()->setState(LED_STATE_OFF);
    _ota_progress = 100;
    _ota_status   = OTA_SUCCESS;
    ResetInfo::storeResetReason(RESET_REASON_FIRMWARE_UPDATE);

    vTaskDelay(pdMS_TO_TICKS(3000));
    refresh_restart_sync_from_settings();
    full_system_restart_with_reserved_operation();
    free(ctx);
}
