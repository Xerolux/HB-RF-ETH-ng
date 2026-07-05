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
#include <string.h>
#include <stdio.h>

extern esp_err_t validate_auth(httpd_req_t *req);
extern bool check_admin_token(const char *token);

static const char *TAG = "log_stream";

static constexpr int MAX_SUBSCRIBERS = 4;

struct Subscriber {
    int fd;                // socket fd, -1 = free slot
    bool wants_hello;      // send a greeting on first publish
};
static Subscriber s_subs[MAX_SUBSCRIBERS];
static SemaphoreHandle_t s_mutex = NULL;
static std::atomic<httpd_handle_t> s_server{NULL};
static std::atomic<bool> s_subscriber_registered{false};

// Forward declarations — defined below publish_worker.
static void register_subscriber(int fd);
static void unregister_subscriber(int fd);

// Decouple publish (called from log_vprintf, any task) from the actual WS
// send (which must use httpd_ws_send_frame_async and may need heap). The
// worker drains this queue and broadcasts.
struct StreamItem {
    char  payload[256];   // single log line (truncated if longer)
    size_t len;
};
static QueueHandle_t s_publish_q = NULL;
static TaskHandle_t   s_worker   = NULL;
static std::atomic<bool> s_worker_running{false};

static void publish_worker(void *)
{
    ESP_LOGI(TAG, "publish worker started");
    while (s_worker_running.load()) {
        StreamItem it;
        if (xQueueReceive(s_publish_q, &it, pdMS_TO_TICKS(500)) != pdTRUE) continue;
        if (it.len == 0) continue;

        // Snapshot the subscriber fd list under the lock, send unlocked.
        int fds[MAX_SUBSCRIBERS];
        int n = 0;
        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
                if (s_subs[i].fd >= 0) fds[n++] = s_subs[i].fd;
            }
            xSemaphoreGive(s_mutex);
        }

        httpd_handle_t srv = s_server.load();
        if (!srv || n == 0) continue;

        httpd_ws_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        frame.type = HTTPD_WS_TYPE_TEXT;
        frame.payload = (uint8_t *)it.payload;
        frame.len = it.len;
        frame.final = true;

        for (int i = 0; i < n; i++) {
            // Async send is non-blocking under normal conditions. On failure
            // (e.g. socket closed) we leave the fd in the table — the
            // handler will observe the close frame on its next invocation
            // and unregister it.
            esp_err_t r = httpd_ws_send_frame_async(srv, fds[i], &frame);
            if (r == ESP_ERR_NOT_FOUND || r == ESP_ERR_INVALID_ARG) {
                // Stale fd — drop it.
                unregister_subscriber(fds[i]);
            }
        }
    }
    ESP_LOGI(TAG, "publish worker stopped");
    vTaskDelete(NULL);
}

void log_stream_publish(int level, const char *tag, const char *message)
{
    (void)level; (void)tag;
    if (!s_publish_q) return;
    if (!message) return;

    StreamItem it;
    size_t n = strlen(message);
    if (n > sizeof(it.payload) - 2) n = sizeof(it.payload) - 2;  // room for "\n\0"
    memcpy(it.payload, message, n);
    it.payload[n] = '\n';
    it.payload[n + 1] = '\0';
    it.len = n + 1;

    // Non-blocking; drop if the worker can't keep up.
    xQueueSend(s_publish_q, &it, 0);
}

// Subscriber hook for LogManager: forward the raw formatted line.
static void log_stream_subscriber(const char *line, size_t len)
{
    if (!line || len == 0) return;
    log_stream_publish(0, nullptr, line);
}

int log_stream_subscriber_count(void)
{
    int n = 0;
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        for (int i = 0; i < MAX_SUBSCRIBERS; i++) if (s_subs[i].fd >= 0) n++;
        xSemaphoreGive(s_mutex);
    }
    return n;
}

static void register_subscriber(int fd)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool exists = false;
    int  free_slot = -1;
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (s_subs[i].fd == fd) { exists = true; break; }
        if (s_subs[i].fd < 0 && free_slot < 0) free_slot = i;
    }
    if (!exists && free_slot >= 0) {
        s_subs[free_slot].fd = fd;
        s_subs[free_slot].wants_hello = true;
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
            s_subs[i].wants_hello = false;
        }
    }
    xSemaphoreGive(s_mutex);
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
        for (int i = 0; i < MAX_SUBSCRIBERS; i++) s_subs[i].fd = -1;
    }
    if (s_publish_q == NULL) {
        s_publish_q = xQueueCreate(32, sizeof(StreamItem));
    }
    // Register once with LogManager; stays registered for the whole boot.
    if (!s_subscriber_registered.exchange(true)) {
        LogManager::instance().addSubscriber(log_stream_subscriber);
    }
    if (!s_worker_running.exchange(true)) {
        xTaskCreate(publish_worker, "log_stream", 4096, NULL, 4, &s_worker);
    }
}

// WebSocket handler. Called by httpd for the upgrade request AND for every
// subsequent incoming frame (ping/pong/close/text). We use it to:
//   - authenticate the upgrade (token via ?token=...)
//   - register the fd on first call
//   - unregister the fd on CLOSE / error
esp_err_t log_stream_handler(httpd_req_t *req)
{
    s_server.store(req->handle);
    log_stream_init();

    int fd = -1;
    if (httpd_ws_get_fd(req, &fd) != ESP_OK || fd < 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no fd");
        return ESP_OK;
    }

    // Authenticate only on the initial GET (no incoming frame yet). On a
    // frame-bearing call the connection is already authenticated; reading
    // the query string again is harmless but unnecessary.
    char q[256];
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK) {
        char token[80] = {};
        if (httpd_query_key_value(q, "token", token, sizeof(token)) == ESP_OK && token[0]) {
            if (!check_admin_token(token)) {
                httpd_resp_set_status(req, "401 Unauthorized");
                httpd_resp_sendstr(req, "auth required");
                return ESP_OK;
            }
        } else {
            // No token in query: rely on validate_auth header check.
            if (validate_auth(req) != ESP_OK) {
                httpd_resp_set_status(req, "401 Unauthorized");
                httpd_resp_sendstr(req, "auth required");
                return ESP_OK;
            }
        }
    } else if (validate_auth(req) != ESP_OK) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_sendstr(req, "auth required");
        return ESP_OK;
    }

    // Read the incoming frame (if any). On the upgrade request, this is a
    // no-op handshake; on subsequent calls it returns the client frame.
    uint8_t buf[128];
    httpd_ws_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.payload = buf;
    frame.len = sizeof(buf);
    esp_err_t r = httpd_ws_recv_frame(req, &frame);
    if (r != ESP_OK) {
        unregister_subscriber(fd);
        return ESP_OK;
    }
    if (frame.type == HTTPD_WS_TYPE_CLOSE) {
        unregister_subscriber(fd);
        return ESP_OK;
    }

    // Register the fd (idempotent).
    register_subscriber(fd);

    // If this was the first frame, send a greeting so the client sees
    // immediate feedback that the stream is live.
    bool say_hello = false;
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
            if (s_subs[i].fd == fd && s_subs[i].wants_hello) {
                s_subs[i].wants_hello = false;
                say_hello = true;
            }
        }
        xSemaphoreGive(s_mutex);
    }
    if (say_hello) {
        const char *hi = "stream connected\n";
        httpd_ws_frame_t hf;
        memset(&hf, 0, sizeof(hf));
        hf.type = HTTPD_WS_TYPE_TEXT;
        hf.payload = (uint8_t *)const_cast<char *>(hi);
        hf.len = strlen(hi);
        hf.final = true;
        httpd_ws_send_frame(req, &hf);
    }

    return ESP_OK;
}

httpd_uri_t log_stream_ws_uri = {
    .uri       = "/api/log/stream",
    .method    = HTTP_GET,
    .handler   = log_stream_handler,
    .user_ctx  = NULL,
    .is_websocket = true,
};
