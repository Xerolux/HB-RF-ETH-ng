/*
 *  log_stream.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "log_stream.h"
#include "log_manager.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <atomic>
#include <inttypes.h>
#include <string>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#if !defined(CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT) || !CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT || \
    !defined(CONFIG_HTTPD_WS_POST_HANDSHAKE_CB_SUPPORT) || !CONFIG_HTTPD_WS_POST_HANDSHAKE_CB_SUPPORT
#error "The log WebSocket requires ESP-IDF pre- and post-handshake callbacks"
#endif

extern esp_err_t validate_auth(httpd_req_t *req);
extern bool check_admin_token(const char *token);

static const char *TAG = "log_stream";

static constexpr int MAX_SUBSCRIBERS = 4;

struct Subscriber {
    int fd;                // socket fd, -1 = free slot
    bool delivery_enabled; // snapshot checkpoint established
    bool acknowledged;     // application-level ready frame sent
    uint64_t checkpoint;   // ignore queued frames already in the snapshot
};
static Subscriber s_subs[MAX_SUBSCRIBERS];
static SemaphoreHandle_t s_mutex = NULL;
static std::atomic<httpd_handle_t> s_server{NULL};
static std::atomic<bool> s_subscriber_registered{false};

// Forward declarations — defined below publish_worker.
static bool register_subscriber(int fd);
static std::string activate_subscriber_with_snapshot(int fd, uint64_t requested_offset,
                                                     uint64_t *checkpoint);
static void acknowledge_subscriber(int fd);
static void unregister_subscriber(int fd);
static void close_all_subscribers(httpd_handle_t server);

// Decouple publish (called from log_vprintf, any task) from the actual WS
// send (which is queued onto the HTTP server task and may need heap). The
// worker drains this queue and broadcasts.
struct StreamItem {
    char  payload[256];   // single log line (truncated if longer)
    size_t len;
    uint64_t end_offset;
};
static QueueHandle_t s_publish_q = NULL;
static TaskHandle_t   s_worker   = NULL;
static std::atomic<bool> s_worker_running{false};
static std::atomic<bool> s_publish_overflow{false};

static void publish_worker(void *)
{
    ESP_LOGI(TAG, "publish worker started");
    while (s_worker_running.load()) {
        StreamItem it;
        if (xQueueReceive(s_publish_q, &it, pdMS_TO_TICKS(500)) != pdTRUE) continue;
        if (it.len == 0) continue;

        httpd_handle_t srv = s_server.load();
        if (s_publish_overflow.exchange(false)) {
            // At least one absolute byte range was lost. Drop queued frames
            // and reconnect every client so its next snapshot fills the gap.
            StreamItem discarded;
            while (xQueueReceive(s_publish_q, &discarded, 0) == pdTRUE) {}
            if (srv) close_all_subscribers(srv);
            continue;
        }

        // Snapshot the ready subscriber fd list under the lock, send unlocked.
        int fds[MAX_SUBSCRIBERS];
        int n = 0;
        // A handshake briefly holds this lock while taking its ring snapshot.
        // Wait for that atomic checkpoint instead of dropping the item in the
        // tiny snapshot/activation window.
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
            if (s_subs[i].fd >= 0 && s_subs[i].delivery_enabled &&
                it.end_offset > s_subs[i].checkpoint) {
                fds[n++] = s_subs[i].fd;
            }
        }
        xSemaphoreGive(s_mutex);

        if (!srv || n == 0) continue;

        char wire[320];
        int header_len = snprintf(wire, sizeof(wire), "stream data %" PRIu64 "\n", it.end_offset);
        if (header_len <= 0 || (size_t)header_len + it.len > sizeof(wire)) continue;
        memcpy(wire + header_len, it.payload, it.len);

        httpd_ws_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        frame.type = HTTPD_WS_TYPE_TEXT;
        frame.payload = (uint8_t *)wire;
        frame.len = (size_t)header_len + it.len;
        frame.final = true;

        for (int i = 0; i < n; i++) {
            // A numeric fd can be reused after close. Verify that it still
            // belongs to an active WebSocket before queueing data for it.
            if (httpd_ws_get_fd_info(srv, fds[i]) != HTTPD_WS_CLIENT_WEBSOCKET) {
                unregister_subscriber(fds[i]);
                continue;
            }

            // Serialize the write through the httpd task. Direct calls to
            // httpd_ws_send_frame_async() from this worker can race with the
            // server's own control-frame writes.
            esp_err_t r = httpd_ws_send_data(srv, fds[i], &frame);
            if (r != ESP_OK) {
                unregister_subscriber(fds[i]);
                // Best-effort cleanup. The close callback is idempotent and
                // removes the fd before the network stack can reuse it.
                httpd_sess_trigger_close(srv, fds[i]);
            }
        }
    }
    ESP_LOGI(TAG, "publish worker stopped");
    vTaskDelete(NULL);
}

void log_stream_publish(const char *message, size_t len, uint64_t end_offset)
{
    if (!s_publish_q || !message || len == 0 || end_offset == 0) return;

    StreamItem it;
    size_t n = len;
    if (n > sizeof(it.payload)) n = sizeof(it.payload);
    memcpy(it.payload, message, n);
    it.len = n;
    it.end_offset = end_offset;

    // Non-blocking logging path. A full queue is recovered by forcing a
    // snapshot reconnect in the worker instead of hiding missing lines.
    if (xQueueSend(s_publish_q, &it, 0) != pdTRUE) {
        s_publish_overflow.store(true);
    }
}

// Subscriber hook for LogManager: forward the raw formatted line.
static void log_stream_subscriber(const char *line, size_t len, uint64_t end_offset)
{
    if (!line || len == 0) return;
    log_stream_publish(line, len, end_offset);
}

int log_stream_subscriber_count(void)
{
    int n = 0;
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
            if (s_subs[i].fd >= 0 && s_subs[i].acknowledged) n++;
        }
        xSemaphoreGive(s_mutex);
    }
    return n;
}

static bool register_subscriber(int fd)
{
    if (!s_mutex) return false;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int  free_slot = -1;
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (s_subs[i].fd == fd) {
            xSemaphoreGive(s_mutex);
            return false;
        }
        if (s_subs[i].fd < 0 && free_slot < 0) free_slot = i;
    }
    bool registered = free_slot >= 0;
    if (registered) {
        s_subs[free_slot].fd = fd;
        s_subs[free_slot].delivery_enabled = false;
        s_subs[free_slot].acknowledged = false;
        s_subs[free_slot].checkpoint = 0;
    }
    xSemaphoreGive(s_mutex);
    return registered;
}

static std::string activate_subscriber_with_snapshot(int fd, uint64_t requested_offset,
                                                     uint64_t *checkpoint)
{
    std::string snapshot;
    if (!s_mutex || !checkpoint) return snapshot;

    // Keep the worker out until both the ring snapshot and the subscriber's
    // checkpoint are established. Log callbacks only enqueue and never take
    // s_mutex, so this lock order cannot deadlock LogManager::write().
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    snapshot = LogManager::instance().getLogSnapshot(requested_offset, checkpoint);
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (s_subs[i].fd == fd) {
            s_subs[i].delivery_enabled = true;
            s_subs[i].checkpoint = *checkpoint;
            break;
        }
    }
    xSemaphoreGive(s_mutex);
    return snapshot;
}

static void acknowledge_subscriber(int fd)
{
    if (!s_mutex) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (s_subs[i].fd == fd) {
            s_subs[i].acknowledged = true;
            break;
        }
    }
    xSemaphoreGive(s_mutex);
}

static void unregister_subscriber(int fd)
{
    if (!s_mutex) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (s_subs[i].fd == fd) {
            s_subs[i].fd = -1;
            s_subs[i].delivery_enabled = false;
            s_subs[i].acknowledged = false;
            s_subs[i].checkpoint = 0;
        }
    }
    xSemaphoreGive(s_mutex);
}

static void close_all_subscribers(httpd_handle_t server)
{
    if (!s_mutex || !server) return;

    int fds[MAX_SUBSCRIBERS];
    int count = 0;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (s_subs[i].fd >= 0) {
            fds[count++] = s_subs[i].fd;
            s_subs[i].fd = -1;
            s_subs[i].delivery_enabled = false;
            s_subs[i].acknowledged = false;
            s_subs[i].checkpoint = 0;
        }
    }
    xSemaphoreGive(s_mutex);

    for (int i = 0; i < count; i++) {
        httpd_sess_trigger_close(server, fds[i]);
    }
}

void log_stream_init(void)
{
    static std::atomic<bool> s_table_initialised{false};
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
    }
    // Initialise the subscriber table exactly once. Calling log_stream_init
    // multiple times (e.g. from repeated handler invocations) must NOT wipe
    // active subscribers.
    if (!s_table_initialised.exchange(true)) {
        for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
            s_subs[i].fd = -1;
            s_subs[i].delivery_enabled = false;
            s_subs[i].acknowledged = false;
            s_subs[i].checkpoint = 0;
        }
    }
    if (s_publish_q == NULL) {
        // 8 slots × sizeof(StreamItem) (~272 B) = ~2.2 KB. The previous depth
        // of 32 reserved ~8.7 KB of heap just for a diagnostic-only stream
        // that only matters while a WebUI client has the log page open.
        // Overflow is already handled by s_publish_overflow (forces a client
        // resync), so a shallower queue costs no log lines in practice — it
        // only drops them earlier under sustained load, which is the safer
        // failure mode on a heap-starved WROOM-32.
        s_publish_q = xQueueCreate(8, sizeof(StreamItem));
    }
    // Register once with LogManager; stays registered for the whole boot.
    if (!s_subscriber_registered.exchange(true)) {
        LogManager::instance().addSubscriber(log_stream_subscriber);
    }
    if (!s_worker_running.exchange(true)) {
        // 3 KB stack: the worker does snprintf into a fixed 320-byte wire
        // buffer plus httpd_ws_send_data (no recursion, no cJSON, no TLS).
        // High-water mark on a chatty device stayed well under 1 KB.
        xTaskCreate(publish_worker, "log_stream", 3072, NULL, 4, &s_worker);
    }
}

static bool authenticate_websocket(httpd_req_t *req)
{
    char q[256];
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK) {
        char token[80] = {};
        if (httpd_query_key_value(q, "token", token, sizeof(token)) == ESP_OK && token[0]) {
            return check_admin_token(token);
        }
    }

    // Non-browser clients may authenticate with the normal HTTP header.
    return validate_auth(req) == ESP_OK;
}

static uint64_t websocket_requested_offset(httpd_req_t *req)
{
    char query[256];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) return 0;

    char value[24] = {};
    if (httpd_query_key_value(query, "offset", value, sizeof(value)) != ESP_OK) return 0;
    return strtoull(value, nullptr, 10);
}

static esp_err_t send_text_frame(httpd_req_t *req, const char *payload, size_t len)
{
    httpd_ws_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.type = HTTPD_WS_TYPE_TEXT;
    frame.payload = (uint8_t *)const_cast<char *>(payload);
    frame.len = len;
    frame.final = true;
    return httpd_ws_send_frame(req, &frame);
}

static esp_err_t log_stream_pre_handshake(httpd_req_t *req)
{
    if (authenticate_websocket(req)) return ESP_OK;

    httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "auth required");
    return ESP_FAIL;
}

static esp_err_t log_stream_post_handshake(httpd_req_t *req)
{
    log_stream_init();
    s_server.store(req->handle);

    int fd = httpd_req_to_sockfd(req);
    if (fd < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!register_subscriber(fd)) {
        ESP_LOGW(TAG, "subscriber limit reached");
        return ESP_ERR_NO_MEM;
    }

    // Take one authoritative ring snapshot for the offset supplied by the
    // browser. Newer queued items carry absolute offsets above checkpoint;
    // older queue entries are skipped for this subscriber.
    uint64_t checkpoint = 0;
    std::string snapshot = activate_subscriber_with_snapshot(
        fd, websocket_requested_offset(req), &checkpoint);

    // Enable worker delivery before sending the snapshot. Worker sends are
    // queued onto this httpd task and therefore cannot overtake the three
    // direct frames below while this post-handshake callback is running.
    char protocol[64];
    int protocol_len = snprintf(protocol, sizeof(protocol),
                                "stream snapshot %" PRIu64 "\n", checkpoint);
    esp_err_t r = protocol_len > 0
        ? send_text_frame(req, protocol, (size_t)protocol_len)
        : ESP_FAIL;
    if (r == ESP_OK) {
        protocol_len = snprintf(protocol, sizeof(protocol),
                                "stream backlog %zu\n", snapshot.size());
        r = protocol_len > 0
            ? send_text_frame(req, protocol, (size_t)protocol_len)
            : ESP_FAIL;
    }
    if (r == ESP_OK && !snapshot.empty()) {
        r = send_text_frame(req, snapshot.data(), snapshot.size());
    }
    if (r == ESP_OK) {
        protocol_len = snprintf(protocol, sizeof(protocol),
                                "stream connected %" PRIu64 "\n", checkpoint);
        r = protocol_len > 0
            ? send_text_frame(req, protocol, (size_t)protocol_len)
            : ESP_FAIL;
    }
    if (r != ESP_OK) {
        unregister_subscriber(fd);
        return r;
    }

    acknowledge_subscriber(fd);
    return ESP_OK;
}

// Called for frames received after the handshake. Authentication and
// registration happen in the pre/post callbacks above.
esp_err_t log_stream_handler(httpd_req_t *req)
{
    uint8_t buf[128];
    httpd_ws_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.payload = buf;

    // frame.len must be zero on the first call. ESP-IDF then parses the
    // second header byte, payload length and mask before reading the payload.
    esp_err_t r = httpd_ws_recv_frame(req, &frame, sizeof(buf));
    if (r != ESP_OK) return r;

    return ESP_OK;
}

void log_stream_close_socket(httpd_handle_t handle, int fd)
{
    (void)handle;
    // httpd invokes close_fn before close(). Removing the fd first prevents a
    // concurrent publisher from writing to a descriptor reused by lwIP.
    unregister_subscriber(fd);
    close(fd);
}

httpd_uri_t log_stream_ws_uri = {
    .uri       = "/api/log/stream",
    .method    = HTTP_GET,
    .handler   = log_stream_handler,
    .user_ctx  = NULL,
    .is_websocket = true,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL,
    .ws_pre_handshake_cb = log_stream_pre_handshake,
    .ws_post_handshake_cb = log_stream_post_handshake,
};
