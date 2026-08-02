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
#include <sys/socket.h>
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
    uint32_t generation;   // protects against numeric fd reuse
};
static Subscriber s_subs[MAX_SUBSCRIBERS] = {
    {-1, false, false, 0, 0},
    {-1, false, false, 0, 0},
    {-1, false, false, 0, 0},
    {-1, false, false, 0, 0},
};
static StaticSemaphore_t s_stream_mutex_buffer;

static SemaphoreHandle_t stream_mutex()
{
    // Function-local static initialisation is thread-safe. Unlike a lazy raw
    // global assignment this remains safe if connect and close callbacks race
    // during HTTP server startup.
    static SemaphoreHandle_t mutex =
        xSemaphoreCreateMutexStatic(&s_stream_mutex_buffer);
    return mutex;
}

static std::atomic<httpd_handle_t> s_server{NULL};
static std::atomic<bool> s_pipeline_active{false};
static bool s_subscriber_registered = false; // guarded by stream_mutex()
static uint32_t s_next_generation = 1;       // guarded by stream_mutex()

// Forward declarations — defined below publish_worker.
static bool register_subscriber(int fd, uint64_t requested_offset,
                                std::string *snapshot,
                                uint64_t *checkpoint);
static void log_stream_subscriber(const char *line, size_t len,
                                  uint64_t end_offset);
static void acknowledge_subscriber(int fd);
static bool unregister_subscriber(int fd, uint32_t generation = 0);
static esp_err_t queue_close_all_subscribers(httpd_handle_t server);

// Decouple publish (called from log_vprintf, any task) from the actual WS
// send, which is queued onto the HTTP server task using fixed storage. The
// worker drains this queue and broadcasts.
struct StreamItem {
    char  payload[256];   // single log line (truncated if longer)
    size_t len;
    uint64_t end_offset;
};
static QueueHandle_t s_publish_q = NULL;
static TaskHandle_t   s_worker   = NULL;
static std::atomic<bool> s_publish_overflow{false};

static constexpr size_t STREAM_WIRE_SIZE = 320;

// Ownership transfers to the HTTP server task after httpd_queue_work()
// succeeds. Keeping the frame payload in the work item avoids passing the
// publish worker's stack storage across the asynchronous hand-off.
struct StreamSendWork {
    std::atomic<bool> in_use{false};
    httpd_handle_t server;
    int fd;
    uint32_t generation;
    size_t len;
    uint8_t payload[STREAM_WIRE_SIZE];
};

// Two complete four-client fan-outs can be pending at once without touching
// the heap. If the HTTP server falls further behind, the existing overflow
// path reconnects clients and repairs the gap from the LogManager snapshot.
// A fixed pool is deliberately preferable to malloc/free per log line: the
// latter fragments the small WROOM-32 heap during long live-log sessions.
static constexpr int STREAM_SEND_WORK_SLOTS = MAX_SUBSCRIBERS * 2;
static StreamSendWork s_send_work_pool[STREAM_SEND_WORK_SLOTS];

struct CloseTarget {
    int fd;
    uint32_t generation;
};

// Dedicated recovery storage remains available even when every send slot is
// occupied. Only one overflow recovery needs to be pending: it snapshots all
// subscribers which have observed the same stream gap.
struct CloseSubscribersWork {
    std::atomic<bool> in_use{false};
    httpd_handle_t server;
    int count;
    CloseTarget targets[MAX_SUBSCRIBERS];
};
static CloseSubscribersWork s_close_subscribers_work;

static StreamSendWork *acquire_send_work()
{
    for (int i = 0; i < STREAM_SEND_WORK_SLOTS; i++) {
        bool expected = false;
        if (s_send_work_pool[i].in_use.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel,
                std::memory_order_relaxed)) {
            return &s_send_work_pool[i];
        }
    }
    return NULL;
}

static void release_send_work(StreamSendWork *work)
{
    if (work) work->in_use.store(false, std::memory_order_release);
}

static int active_subscriber_count_locked()
{
    int count = 0;
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (s_subs[i].fd >= 0) count++;
    }
    return count;
}

static void clear_subscriber_locked(int index)
{
    s_subs[index].fd = -1;
    s_subs[index].delivery_enabled = false;
    s_subs[index].acknowledged = false;
    s_subs[index].checkpoint = 0;
    s_subs[index].generation = 0;
}

static void deactivate_pipeline_if_empty_locked()
{
    if (active_subscriber_count_locked() != 0) return;

    // Close the gate before unregistering. A callback that LogManager already
    // snapshotted may still run afterwards; the queue deliberately remains
    // allocated for the rest of the boot, so even a callback that observed
    // the old gate value cannot use freed storage.
    s_pipeline_active.store(false, std::memory_order_release);
    if (s_subscriber_registered) {
        LogManager::instance().removeSubscriber(log_stream_subscriber);
        s_subscriber_registered = false;
    }
    s_publish_overflow.store(false, std::memory_order_release);
}

static void send_in_httpd_context(void *arg)
{
    StreamSendWork *work = static_cast<StreamSendWork *>(arg);
    if (!work) return;

    // The HTTP task serialises this check with session close/accept. Numeric
    // fd reuse therefore cannot occur between validating the session and the
    // direct frame write below. The generation additionally rejects work
    // queued for an earlier WebSocket that used the same descriptor.
    bool still_current = false;
    SemaphoreHandle_t mutex = stream_mutex();
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
            if (s_subs[i].fd == work->fd &&
                s_subs[i].generation == work->generation &&
                s_subs[i].delivery_enabled) {
                still_current = true;
                break;
            }
        }
        xSemaphoreGive(mutex);
    }

    esp_err_t result = ESP_ERR_INVALID_STATE;
    if (still_current &&
        httpd_ws_get_fd_info(work->server, work->fd) ==
            HTTPD_WS_CLIENT_WEBSOCKET) {
        httpd_ws_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        frame.type = HTTPD_WS_TYPE_TEXT;
        frame.payload = work->payload;
        frame.len = work->len;
        frame.final = true;

        // This is the ESP-IDF low-level send intended for use from an
        // httpd_queue_work() callback. It completes before payload is freed.
        result = httpd_ws_send_frame_async(work->server, work->fd, &frame);
    }

    if (still_current && result != ESP_OK) {
        bool removed = unregister_subscriber(work->fd, work->generation);
        // This callback already runs in the HTTP server task. Do not call
        // IDF's deferred session-close helper queues another work item containing
        // a raw session pointer, which can outlive this generation and close a
        // newly accepted client after fd/session-slot reuse. shutdown() wakes
        // the server loop immediately; its normal close path owns close(fd).
        if (removed) shutdown(work->fd, SHUT_RDWR);
    }

    release_send_work(work);
}

static esp_err_t queue_stream_send(httpd_handle_t server, int fd,
                                   uint32_t generation,
                                   const char *payload, size_t len)
{
    if (!server || !payload || len == 0 || len > STREAM_WIRE_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    StreamSendWork *work = acquire_send_work();
    if (!work) return ESP_ERR_NO_MEM;

    work->server = server;
    work->fd = fd;
    work->generation = generation;
    work->len = len;
    memcpy(work->payload, payload, len);

    esp_err_t result = httpd_queue_work(server, send_in_httpd_context, work);
    if (result != ESP_OK) release_send_work(work);
    return result;
}

static void publish_worker(void *)
{
    ESP_LOGI(TAG, "publish worker started");
    for (;;) {
        SemaphoreHandle_t mutex = stream_mutex();
        QueueHandle_t queue = NULL;
        xSemaphoreTake(mutex, portMAX_DELAY);
        if (active_subscriber_count_locked() == 0) {
            // Publish the stopped state while holding the lifecycle mutex. A
            // concurrent first client will either keep this worker alive or
            // observe NULL and create exactly one replacement.
            s_worker = NULL;
            xSemaphoreGive(mutex);
            break;
        }
        queue = s_publish_q;
        xSemaphoreGive(mutex);

        httpd_handle_t srv = s_server.load(std::memory_order_acquire);
        if (s_publish_overflow.exchange(false, std::memory_order_acq_rel)) {
            // At least one absolute byte range was lost. Reconnect the clients
            // which observed it so their next snapshots fill the gap.
            if (queue_close_all_subscribers(srv) != ESP_OK) {
                // A previously queued recovery owns the single close slot, or
                // the HTTP control queue is temporarily unavailable. Retry
                // from the worker; never close a numeric fd from this task.
                s_publish_overflow.store(true, std::memory_order_release);
                // The HTTP control queue may remain full for a short burst.
                // Bound retries so recovery cannot turn into a busy loop.
                vTaskDelay(pdMS_TO_TICKS(20));
            }
            continue;
        }

        // Overflow recovery is checked before this bounded receive. In
        // particular, a failed hand-off of the final queued log item therefore
        // gets retried even when no producer ever submits another line.
        StreamItem it;
        if (!queue || xQueueReceive(queue, &it, pdMS_TO_TICKS(250)) != pdTRUE) continue;
        if (it.len == 0) continue;

        // Snapshot the ready subscriber fd list under the lock, send unlocked.
        struct Target {
            int fd;
            uint32_t generation;
        } targets[MAX_SUBSCRIBERS];
        int n = 0;
        // A handshake briefly holds this lock while taking its ring snapshot.
        // Wait for that atomic checkpoint instead of dropping the item in the
        // tiny snapshot/activation window.
        xSemaphoreTake(mutex, portMAX_DELAY);
        for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
            if (s_subs[i].fd >= 0 && s_subs[i].delivery_enabled &&
                it.end_offset > s_subs[i].checkpoint) {
                targets[n++] = {s_subs[i].fd, s_subs[i].generation};
            }
        }
        xSemaphoreGive(mutex);

        if (!srv || n == 0) continue;

        char wire[STREAM_WIRE_SIZE];
        int header_len = snprintf(wire, sizeof(wire), "stream data %" PRIu64 "\n", it.end_offset);
        if (header_len <= 0 || (size_t)header_len + it.len > sizeof(wire)) continue;
        memcpy(wire + header_len, it.payload, it.len);

        for (int i = 0; i < n; i++) {
            esp_err_t r = queue_stream_send(
                srv, targets[i].fd, targets[i].generation, wire,
                (size_t)header_len + it.len);
            if (r != ESP_OK) {
                // A failed hand-off loses an absolute byte range. Reconnect on
                // the worker's next pass so the ring snapshot repairs it.
                s_publish_overflow.store(true, std::memory_order_release);
                break;
            }
        }
    }
    ESP_LOGI(TAG, "publish worker stopped");
    vTaskDelete(NULL);
}

void log_stream_publish(const char *message, size_t len, uint64_t end_offset)
{
    if (!message || len == 0 || end_offset == 0 ||
        !s_pipeline_active.load(std::memory_order_acquire)) return;

    // Published by the release-store to s_pipeline_active and never deleted
    // after its first allocation. This makes delayed callbacks captured by
    // LogManager before removeSubscriber() safe without a blocking hot path.
    QueueHandle_t queue = s_publish_q;
    if (!queue) return;

    StreamItem it;
    size_t n = len;
    if (n > sizeof(it.payload)) n = sizeof(it.payload);
    memcpy(it.payload, message, n);
    it.len = n;
    it.end_offset = end_offset;

    // Non-blocking logging path. A full queue is recovered by forcing a
    // snapshot reconnect in the worker instead of hiding missing lines.
    if (xQueueSend(queue, &it, 0) != pdTRUE) {
        s_publish_overflow.store(true, std::memory_order_release);
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
    SemaphoreHandle_t mutex = stream_mutex();
    if (mutex && xSemaphoreTake(mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
            if (s_subs[i].fd >= 0 && s_subs[i].acknowledged) n++;
        }
        xSemaphoreGive(mutex);
    }
    return n;
}

static bool register_subscriber(int fd, uint64_t requested_offset,
                                std::string *snapshot,
                                uint64_t *checkpoint)
{
    SemaphoreHandle_t mutex = stream_mutex();
    if (!mutex || !snapshot || !checkpoint) return false;

    xSemaphoreTake(mutex, portMAX_DELAY);
    int  free_slot = -1;
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (s_subs[i].fd == fd) {
            xSemaphoreGive(mutex);
            return false;
        }
        if (s_subs[i].fd < 0 && free_slot < 0) free_slot = i;
    }
    if (free_slot < 0) {
        xSemaphoreGive(mutex);
        return false;
    }

    const bool first = active_subscriber_count_locked() == 0;
    s_subs[free_slot].fd = fd;
    s_subs[free_slot].delivery_enabled = false;
    s_subs[free_slot].acknowledged = false;
    s_subs[free_slot].checkpoint = 0;
    s_subs[free_slot].generation = s_next_generation++;
    if (s_next_generation == 0) s_next_generation = 1;

    if (first) {
        // Allocate diagnostics resources lazily. The queue is intentionally
        // long-lived after this point so an already-snapshotted LogManager
        // callback can never enqueue into freed memory.
        if (!s_publish_q) {
            s_publish_q = xQueueCreate(8, sizeof(StreamItem));
        }
        if (!s_publish_q) {
            clear_subscriber_locked(free_slot);
            xSemaphoreGive(mutex);
            return false;
        }

        if (!s_worker) {
            xQueueReset(s_publish_q);
            s_publish_overflow.store(false, std::memory_order_release);
            // 3 KB stack: fixed wire buffer plus queue and subscriber state.
            if (xTaskCreate(publish_worker, "log_stream", 3072, NULL, 4,
                            &s_worker) != pdPASS) {
                s_worker = NULL;
                clear_subscriber_locked(free_slot);
                xSemaphoreGive(mutex);
                return false;
            }
        }

        s_pipeline_active.store(true, std::memory_order_release);
        if (!s_subscriber_registered) {
            LogManager::instance().addSubscriber(log_stream_subscriber);
            s_subscriber_registered = true;
        }
    }

    // Establish the snapshot checkpoint before releasing the lifecycle lock.
    // This keeps the worker from consuming the registration-window items. The
    // LogManager callback itself never takes this mutex, so S -> LogManager's
    // mutex cannot deadlock a writer callback.
    *snapshot = LogManager::instance().getLogSnapshot(requested_offset,
                                                       checkpoint);
    s_subs[free_slot].delivery_enabled = true;
    s_subs[free_slot].checkpoint = *checkpoint;

    xSemaphoreGive(mutex);
    return true;
}

static void acknowledge_subscriber(int fd)
{
    SemaphoreHandle_t mutex = stream_mutex();
    if (!mutex) return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (s_subs[i].fd == fd) {
            s_subs[i].acknowledged = true;
            break;
        }
    }
    xSemaphoreGive(mutex);
}

static bool unregister_subscriber(int fd, uint32_t generation)
{
    SemaphoreHandle_t mutex = stream_mutex();
    if (!mutex) return false;

    bool removed = false;
    xSemaphoreTake(mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (s_subs[i].fd == fd &&
            (generation == 0 || s_subs[i].generation == generation)) {
            clear_subscriber_locked(i);
            removed = true;
            break;
        }
    }
    if (removed) deactivate_pipeline_if_empty_locked();
    xSemaphoreGive(mutex);
    return removed;
}

static void close_subscribers_in_httpd_context(void *arg)
{
    CloseSubscribersWork *work = static_cast<CloseSubscribersWork *>(arg);
    if (!work) return;

    for (int i = 0; i < work->count; i++) {
        const CloseTarget target = work->targets[i];
        // unregister_subscriber() performs the generation comparison while
        // this callback serialises session close/accept in the HTTP task. If
        // the descriptor has already been reused, the new generation remains
        // registered and its session is left untouched.
        if (unregister_subscriber(target.fd, target.generation)) {
            // Generation was validated in the HTTP task. shutdown() cannot be
            // deferred past fd/session reuse; the server's close_fn performs
            // the eventual close(fd).
            shutdown(target.fd, SHUT_RDWR);
        }
    }

    work->in_use.store(false, std::memory_order_release);
}

static esp_err_t queue_close_all_subscribers(httpd_handle_t server)
{
    if (!server) return ESP_ERR_INVALID_ARG;

    bool expected = false;
    if (!s_close_subscribers_work.in_use.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel,
            std::memory_order_relaxed)) {
        return ESP_ERR_INVALID_STATE;
    }

    s_close_subscribers_work.server = server;
    s_close_subscribers_work.count = 0;

    SemaphoreHandle_t mutex = stream_mutex();
    if (!mutex) {
        s_close_subscribers_work.in_use.store(false,
                                              std::memory_order_release);
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreTake(mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (s_subs[i].fd >= 0) {
            int index = s_close_subscribers_work.count++;
            s_close_subscribers_work.targets[index] = {
                s_subs[i].fd, s_subs[i].generation
            };
        }
    }
    xSemaphoreGive(mutex);

    if (s_close_subscribers_work.count == 0) {
        s_close_subscribers_work.in_use.store(false,
                                              std::memory_order_release);
        return ESP_OK;
    }

    esp_err_t result = httpd_queue_work(
        server, close_subscribers_in_httpd_context,
        &s_close_subscribers_work);
    if (result != ESP_OK) {
        s_close_subscribers_work.in_use.store(false,
                                              std::memory_order_release);
    }
    return result;
}

void log_stream_init(void)
{
    // Only initialise the lifecycle mutex here. Queue allocation, subscriber
    // registration and worker creation happen when the first WS client has
    // completed its handshake.
    (void)stream_mutex();
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
    s_server.store(req->handle, std::memory_order_release);

    int fd = httpd_req_to_sockfd(req);
    if (fd < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint64_t checkpoint = 0;
    std::string snapshot;
    if (!register_subscriber(fd, websocket_requested_offset(req),
                             &snapshot, &checkpoint)) {
        ESP_LOGW(TAG, "subscriber limit reached");
        return ESP_ERR_NO_MEM;
    }

    // Take one authoritative ring snapshot for the offset supplied by the
    // browser. Newer queued items carry absolute offsets above checkpoint;
    // older queue entries are skipped for this subscriber.
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
