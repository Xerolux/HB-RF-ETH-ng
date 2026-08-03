/*
 *  events.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "events.h"
#include "monitoring.h"
#include "settings.h"
#include "metrics.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "mbedtls/ssl.h"
#include "mbedtls/base64.h"
#include "mbedtls/net_sockets.h"
#include <atomic>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <errno.h>

extern Settings *monitoring_get_settings(void);
extern SemaphoreHandle_t g_net_fetch_mutex;

static const char *TAG = "events";

// ---------------------------------------------------------------------------
// Config snapshot (copied on events_start).
// ---------------------------------------------------------------------------
static notify_config_t s_cfg = {};

static std::atomic<bool>         s_running{false};
static std::atomic<bool>         s_restart_requested{false};
static std::atomic<TaskHandle_t> s_task{NULL};
static std::atomic<int>          s_active_socket{-1};
static QueueHandle_t             s_queue = NULL;
static StaticSemaphore_t         s_lifecycle_mutex_buffer;

static SemaphoreHandle_t events_lifecycle_mutex()
{
    // Lifecycle coordination must remain available under heap pressure; a
    // dynamically allocated mutex that fails once in a function-local static
    // would otherwise disable start/stop until the next reboot.
    static SemaphoreHandle_t mutex =
        xSemaphoreCreateMutexStatic(&s_lifecycle_mutex_buffer);
    return mutex;
}

struct EventEntry {
    Event   id;
    int64_t timestamp;
    char    detail[128];
};
static constexpr int QUEUE_DEPTH = 16;

// Per-event-type cooldown tracker (last emission time, microseconds).
// Indexed by Event enum value.
static constexpr int MAX_EVENT_ID = 32;
static int64_t s_last_sent[MAX_EVENT_ID] = {};

// Metric counters
static MetricsCounter g_sent_total("hbrfeth_notify_sent_total",
                                   "Notifications successfully delivered");
static MetricsCounter g_failed_total("hbrfeth_notify_failed_total",
                                     "Notification delivery attempts that failed");
static MetricsCounter g_suppressed_total("hbrfeth_notify_suppressed_total",
                                         "Events suppressed by cooldown window");

static constexpr int EVENT_HTTP_TOTAL_TIMEOUT_MS = 12000;
static constexpr int SMTP_TOTAL_TIMEOUT_MS = 12000;
static constexpr int EVENT_HTTP_ASYNC_RETRY_MS = 10;

static int remaining_deadline_ms(int64_t deadline_us)
{
    const int64_t remaining_us = deadline_us - esp_timer_get_time();
    if (remaining_us <= 0) return 0;
    const int64_t rounded_ms = (remaining_us + 999) / 1000;
    return rounded_ms > INT32_MAX ? INT32_MAX : static_cast<int>(rounded_ms);
}

static bool is_https_url(const char *url)
{
    return url && strncasecmp(url, "https://", 8) == 0;
}

static bool prepare_event_http_step(esp_http_client_handle_t client,
                                    int64_t deadline_us)
{
    if (!s_running.load(std::memory_order_acquire)) return false;
    const int remaining_ms = remaining_deadline_ms(deadline_us);
    return remaining_ms > 0 &&
           esp_http_client_set_timeout_ms(client, remaining_ms) == ESP_OK;
}

static void event_http_retry_delay(int64_t deadline_us)
{
    int delay_ms = remaining_deadline_ms(deadline_us);
    if (delay_ms > EVENT_HTTP_ASYNC_RETRY_MS) {
        delay_ms = EVENT_HTTP_ASYNC_RETRY_MS;
    }
    if (delay_ms > 0) vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

// Send only the request and parse the response headers. Notification endpoints
// do not return data we consume, so esp_http_client_perform() would needlessly
// read an arbitrarily large/slow response body and could keep the worker alive
// beyond events_stop()'s lifecycle bound. HTTPS uses IDF's asynchronous
// transport; every retry is checked against one absolute request deadline.
static esp_err_t post_event_http(esp_http_client_handle_t client,
                                 const char *body, size_t body_len,
                                 int64_t deadline_us)
{
    if (!client || !body || body_len > static_cast<size_t>(INT32_MAX)) {
        return ESP_ERR_INVALID_ARG;
    }

    for (;;) {
        if (!prepare_event_http_step(client, deadline_us)) {
            return s_running.load(std::memory_order_acquire)
                       ? ESP_ERR_TIMEOUT
                       : ESP_ERR_INVALID_STATE;
        }
        const esp_err_t result =
            esp_http_client_open(client, static_cast<int>(body_len));
        if (result == ESP_OK) break;
        if (result != ESP_ERR_HTTP_EAGAIN) return result;
        event_http_retry_delay(deadline_us);
    }

    size_t written = 0;
    while (written < body_len) {
        if (!prepare_event_http_step(client, deadline_us)) {
            return s_running.load(std::memory_order_acquire)
                       ? ESP_ERR_TIMEOUT
                       : ESP_ERR_INVALID_STATE;
        }
        errno = 0;
        const int result = esp_http_client_write(
            client, body + written,
            static_cast<int>(body_len - written));
        if (result > 0) {
            written += static_cast<size_t>(result);
            continue;
        }
        if ((result == 0 && (errno == 0 || errno == EAGAIN ||
                            errno == EWOULDBLOCK)) ||
            (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK ||
                            errno == EINTR))) {
            event_http_retry_delay(deadline_us);
            continue;
        }
        return ESP_FAIL;
    }

    for (;;) {
        if (!prepare_event_http_step(client, deadline_us)) {
            return s_running.load(std::memory_order_acquire)
                       ? ESP_ERR_TIMEOUT
                       : ESP_ERR_INVALID_STATE;
        }
        const int64_t result = esp_http_client_fetch_headers(client);
        if (result >= 0) return ESP_OK;
        if (result != -ESP_ERR_HTTP_EAGAIN) return ESP_FAIL;
        event_http_retry_delay(deadline_us);
    }
}

static bool apply_socket_deadline(int sock, int64_t deadline_us)
{
    const int remaining_ms = remaining_deadline_ms(deadline_us);
    if (remaining_ms <= 0) return false;
    struct timeval timeout = {
        .tv_sec = remaining_ms / 1000,
        .tv_usec = (remaining_ms % 1000) * 1000,
    };
    return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                      &timeout, sizeof(timeout)) == 0 &&
           setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
                      &timeout, sizeof(timeout)) == 0;
}

struct SmtpTlsIoContext {
    mbedtls_net_context net;
    int64_t deadline_us;
};

static int smtp_tls_send(void *ctx, const unsigned char *buf, size_t len)
{
    SmtpTlsIoContext *io = static_cast<SmtpTlsIoContext *>(ctx);
    if (!s_running.load(std::memory_order_acquire) ||
        !apply_socket_deadline(io->net.fd, io->deadline_us)) {
        return MBEDTLS_ERR_SSL_TIMEOUT;
    }
    return mbedtls_net_send(&io->net, buf, len);
}

static int smtp_tls_recv_timeout(void *ctx, unsigned char *buf, size_t len,
                                 uint32_t)
{
    SmtpTlsIoContext *io = static_cast<SmtpTlsIoContext *>(ctx);
    if (!s_running.load(std::memory_order_acquire) ||
        !apply_socket_deadline(io->net.fd, io->deadline_us)) {
        return MBEDTLS_ERR_SSL_TIMEOUT;
    }
    return mbedtls_net_recv(&io->net, buf, len);
}

static bool publish_active_socket(int sock)
{
    SemaphoreHandle_t lifecycle = events_lifecycle_mutex();
    if (!lifecycle) return false;
    xSemaphoreTake(lifecycle, portMAX_DELAY);
    const bool running = s_running.load(std::memory_order_acquire);
    if (running) s_active_socket.store(sock, std::memory_order_release);
    xSemaphoreGive(lifecycle);
    return running;
}

static void close_plain_socket(int sock)
{
    if (sock < 0) return;
    SemaphoreHandle_t lifecycle = events_lifecycle_mutex();
    if (lifecycle) xSemaphoreTake(lifecycle, portMAX_DELAY);
    if (s_active_socket.load(std::memory_order_acquire) == sock) {
        s_active_socket.store(-1, std::memory_order_release);
    }
    close(sock);
    if (lifecycle) xSemaphoreGive(lifecycle);
}

// ---------------------------------------------------------------------------
// Event metadata.
// ---------------------------------------------------------------------------
struct EventMeta {
    const char *key;     // stable wire key (JSON, email subject)
    const char *default_msg;
};

static const EventMeta &meta_for(Event e)
{
    // String-keyed designated initializers are not valid C++; use a switch.
    // The pointers are static so returning by reference is safe.
    static const EventMeta m_eth_down     = { "eth_link_down",      "Ethernet link went down" };
    static const EventMeta m_eth_up       = { "eth_link_up",        "Ethernet link came up" };
    static const EventMeta m_rf_lost      = { "rf_module_lost",     "Radio module no longer responds" };
    static const EventMeta m_rf_detected  = { "rf_module_detected", "Radio module detected" };
    static const EventMeta m_mqtt_disc    = { "mqtt_disconnected",  "MQTT broker connection lost" };
    static const EventMeta m_mqtt_recon   = { "mqtt_reconnected",   "MQTT broker connection re-established" };
    static const EventMeta m_factory      = { "factory_reset",      "Factory reset initiated" };
    static const EventMeta m_restart      = { "restart",            "Device restart initiated" };
    static const EventMeta m_test         = { "test",               "Test notification from HB-RF-ETH-ng" };
    static const EventMeta m_unknown      = { "unknown",            "Unknown event" };

    switch (e) {
        case EVENT_ETH_LINK_DOWN:      return m_eth_down;
        case EVENT_ETH_LINK_UP:        return m_eth_up;
        case EVENT_RF_MODULE_LOST:     return m_rf_lost;
        case EVENT_RF_MODULE_DETECTED: return m_rf_detected;
        case EVENT_MQTT_DISCONNECTED:  return m_mqtt_disc;
        case EVENT_MQTT_RECONNECTED:   return m_mqtt_recon;
        case EVENT_FACTORY_RESET:      return m_factory;
        case EVENT_RESTART:            return m_restart;
        case EVENT_TEST:               return m_test;
        default:                       return m_unknown;
    }
}

// ---------------------------------------------------------------------------
// Channel senders.
// ---------------------------------------------------------------------------

// --- Webhook ---
static bool send_webhook(const EventEntry &e, const EventMeta &m,
                         const notify_config_t &config)
{
    if (config.webhook_url[0] == '\0') {
        return false;
    }
    if (!is_https_url(config.webhook_url)) {
        ESP_LOGE(TAG, "Rejected non-HTTPS webhook URL");
        return false;
    }

    // Heap-allocate body so we can host it through the http client lifecycle.
    char *body = (char *)malloc(512);
    if (!body) return false;

    const char *host = "hb-rf-eth-ng";
    Settings *s = monitoring_get_settings();
    if (s && s->getHostname() && s->getHostname()[0]) host = s->getHostname();

    int n = snprintf(body, 512,
        "{\"event\":\"%s\",\"device\":\"%s\",\"detail\":\"%s\",\"ts\":%lld}",
        m.key, host,
        e.detail[0] ? e.detail : "",
        (long long)(e.timestamp / 1000000LL));
    if (n <= 0 || n >= 512) { free(body); return false; }

    const int64_t deadline_us = esp_timer_get_time() +
        static_cast<int64_t>(EVENT_HTTP_TOTAL_TIMEOUT_MS) * 1000;
    esp_http_client_config_t cfg = {};
    cfg.url = config.webhook_url;
    cfg.method = HTTP_METHOD_POST;
    cfg.timeout_ms = remaining_deadline_ms(deadline_us);
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    // Manual open/write/fetch does not implement redirects. Rejecting them
    // keeps credentials and the absolute deadline scoped to one endpoint.
    cfg.disable_auto_redirect = true;
    // ESP-IDF supports non-blocking esp_http_client only over HTTPS. Requiring
    // HTTPS above keeps every operation under the absolute deadline.
    cfg.is_async = true;

    bool ok = false;
    // Outbound HTTPS must serialise on g_net_fetch_mutex.
    const int mutex_wait_ms = remaining_deadline_ms(deadline_us);
    if (g_net_fetch_mutex && mutex_wait_ms > 0 &&
        xSemaphoreTake(g_net_fetch_mutex,
                       pdMS_TO_TICKS(mutex_wait_ms)) == pdTRUE) {
        if (!s_running.load(std::memory_order_acquire)) {
            xSemaphoreGive(g_net_fetch_mutex);
            free(body);
            return false;
        }
        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (client) {
            esp_http_client_set_header(client, "Content-Type", "application/json");
            if (config.webhook_secret[0]) {
                esp_http_client_set_header(client, "X-HB-RF-ETH-Secret", config.webhook_secret);
            }
            esp_err_t err = post_event_http(
                client, body, static_cast<size_t>(n), deadline_us);
            if (err == ESP_OK) {
                int code = esp_http_client_get_status_code(client);
                ok = (code >= 200 && code < 300);
            }
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
        }
        xSemaphoreGive(g_net_fetch_mutex);
    }
    free(body);
    return ok;
}

// --- Telegram ---
static bool send_telegram(const EventEntry &e, const EventMeta &m,
                          const notify_config_t &config)
{
    if (config.telegram_token[0] == '\0' || config.telegram_chatid[0] == '\0') {
        return false;
    }

    const char *host = "hb-rf-eth-ng";
    Settings *s = monitoring_get_settings();
    if (s && s->getHostname() && s->getHostname()[0]) host = s->getHostname();

    char url[256];
    int url_len = snprintf(url, sizeof(url),
                           "https://api.telegram.org/bot%s/sendMessage",
                           config.telegram_token);
    if (url_len <= 0 || url_len >= static_cast<int>(sizeof(url))) return false;

    char *body = (char *)malloc(640);
    if (!body) return false;
    int n = snprintf(body, 640,
        "{\"chat_id\":\"%s\",\"text\":\"[%s] %s: %s\",\"disable_web_page_preview\":true}",
        config.telegram_chatid, host, m.default_msg,
        e.detail[0] ? e.detail : "");
    if (n <= 0 || n >= 640) { free(body); return false; }

    const int64_t deadline_us = esp_timer_get_time() +
        static_cast<int64_t>(EVENT_HTTP_TOTAL_TIMEOUT_MS) * 1000;
    esp_http_client_config_t cfg = {};
    cfg.url = url;
    cfg.method = HTTP_METHOD_POST;
    cfg.timeout_ms = remaining_deadline_ms(deadline_us);
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.disable_auto_redirect = true;
    cfg.is_async = true;

    bool ok = false;
    const int mutex_wait_ms = remaining_deadline_ms(deadline_us);
    if (g_net_fetch_mutex && mutex_wait_ms > 0 &&
        xSemaphoreTake(g_net_fetch_mutex,
                       pdMS_TO_TICKS(mutex_wait_ms)) == pdTRUE) {
        if (!s_running.load(std::memory_order_acquire)) {
            xSemaphoreGive(g_net_fetch_mutex);
            free(body);
            return false;
        }
        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (client) {
            esp_http_client_set_header(client, "Content-Type", "application/json");
            esp_err_t err = post_event_http(
                client, body, static_cast<size_t>(n), deadline_us);
            if (err == ESP_OK) {
                int code = esp_http_client_get_status_code(client);
                ok = (code >= 200 && code < 300);
            }
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
        }
        xSemaphoreGive(g_net_fetch_mutex);
    }
    free(body);
    return ok;
}

// --- Email (SMTP) ---
// Minimal SMTP client supporting plaintext / STARTTLS / implicit TLS.
// Every connect/handshake/read/write shares one absolute deadline. Socket
// timeouts are refreshed from the remaining budget before every operation, so
// a talkative or stalled peer cannot reset the timeout indefinitely.
static int smtp_read_byte(int sock, mbedtls_ssl_context *ssl,
                          unsigned char *byte, int64_t deadline_us)
{
    while (remaining_deadline_ms(deadline_us) > 0 &&
           s_running.load(std::memory_order_acquire)) {
        if (!apply_socket_deadline(sock, deadline_us)) return -1;
        if (ssl) {
            int r = mbedtls_ssl_read(ssl, byte, 1);
            if (r == MBEDTLS_ERR_SSL_WANT_READ ||
                r == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
            return r == 1 ? 1 : -1;
        }
        ssize_t r = recv(sock, byte, 1, 0);
        if (r == 1) return 1;
        if (r < 0 && errno == EINTR) continue;
        return -1;
    }
    return -1;
}

static int smtp_read_reply(int sock, mbedtls_ssl_context *ssl,
                           char *buf, size_t cap, int64_t deadline_us)
{
    size_t total = 0;
    int first_code = -1;
    bool multiline = false;
    for (;;) {
        const size_t line_start = total;
        while (total + 1 < cap) {
            unsigned char byte = 0;
            if (smtp_read_byte(sock, ssl, &byte, deadline_us) != 1) return -1;
            buf[total++] = static_cast<char>(byte);
            if (buf[total - 1] == '\n') break;
        }
        buf[total] = '\0';
        if (total - line_start < 4) return -1;
        if (first_code < 0) {
            first_code = (buf[0] >= '0' && buf[0] <= '9') ? atoi(buf) : -1;
            multiline = buf[3] == '-';
            if (!multiline) return first_code;
        } else if (buf[line_start + 3] == ' ') {
            return first_code;
        }
        if (total + 1 >= cap) return -1;
    }
}

static bool smtp_write_all(int sock, mbedtls_ssl_context *ssl,
                           const unsigned char *data, size_t len,
                           int64_t deadline_us)
{
    while (len > 0) {
        if (!s_running.load(std::memory_order_acquire) ||
            !apply_socket_deadline(sock, deadline_us)) return false;
        int written;
        if (ssl) {
            written = mbedtls_ssl_write(ssl, data, len);
            if (written == MBEDTLS_ERR_SSL_WANT_READ ||
                written == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        } else {
            ssize_t result = send(sock, data, len, 0);
            if (result < 0 && errno == EINTR) continue;
            written = static_cast<int>(result);
        }
        if (written <= 0) return false;
        data += written;
        len -= static_cast<size_t>(written);
    }
    return true;
}

static bool smtp_send_line(int sock, mbedtls_ssl_context *ssl,
                           const char *line, int64_t deadline_us)
{
    return smtp_write_all(sock, ssl,
                          reinterpret_cast<const unsigned char *>(line),
                          strlen(line), deadline_us) &&
           smtp_write_all(sock, ssl,
                          reinterpret_cast<const unsigned char *>("\r\n"),
                          2, deadline_us);
}

static bool smtp_setup_tls(mbedtls_ssl_context *ssl,
                           mbedtls_ssl_config *conf,
                           SmtpTlsIoContext *io,
                           int sock, const char *host,
                           int64_t deadline_us, bool *setup_complete)
{
    io->net.fd = sock;
    io->deadline_us = deadline_us;
    if (mbedtls_ssl_config_defaults(conf, MBEDTLS_SSL_IS_CLIENT,
                                    MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
        return false;
    }
    mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    esp_crt_bundle_attach(conf);
    const int remaining_ms = remaining_deadline_ms(deadline_us);
    if (remaining_ms <= 0) return false;
    mbedtls_ssl_conf_read_timeout(conf, static_cast<uint32_t>(remaining_ms));
    if (mbedtls_ssl_setup(ssl, conf) != 0) return false;
    *setup_complete = true;
    mbedtls_ssl_set_bio(ssl, io, smtp_tls_send, NULL,
                        smtp_tls_recv_timeout);
    if (mbedtls_ssl_set_hostname(ssl, host) != 0) return false;

    while (remaining_deadline_ms(deadline_us) > 0 &&
           s_running.load(std::memory_order_acquire)) {
        if (!apply_socket_deadline(sock, deadline_us)) return false;
        int r = mbedtls_ssl_handshake(ssl);
        if (r == 0) return true;
        if (r != MBEDTLS_ERR_SSL_WANT_READ &&
            r != MBEDTLS_ERR_SSL_WANT_WRITE) return false;
    }
    return false;
}

static bool send_email(const EventEntry &e, const EventMeta &m,
                       const notify_config_t &config)
{
    if (config.smtp_server[0] == '\0' || config.smtp_from[0] == '\0' ||
        config.smtp_to[0] == '\0' || !g_net_fetch_mutex) return false;

    const int64_t deadline_us = esp_timer_get_time() +
        static_cast<int64_t>(SMTP_TOTAL_TIMEOUT_MS) * 1000;
    int mutex_wait_ms = remaining_deadline_ms(deadline_us);
    if (mutex_wait_ms <= 0 ||
        xSemaphoreTake(g_net_fetch_mutex,
                       pdMS_TO_TICKS(mutex_wait_ms)) != pdTRUE) return false;

    // Implicit TLS: full TLS from the start. STARTTLS: plaintext then upgrade.
    const bool use_tls = config.smtp_tls == 2;
    const bool use_starttls = config.smtp_tls == 1;

    bool ok = false;
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res = NULL;
    char port_str[8];
    int sock = -1;
    int flags = -1;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    SmtpTlsIoContext tls_io;
    bool tls_setup = false;
    bool tls_active = false;
    char line[256];
    unsigned char obuf[128];
    size_t olen = 0;

    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_net_init(&tls_io.net);
    tls_io.deadline_us = deadline_us;

    do {
        snprintf(port_str, sizeof(port_str), "%u",
                 config.smtp_port ? config.smtp_port : 587);
        if (getaddrinfo(config.smtp_server, port_str, &hints, &res) != ESP_OK ||
            !res || remaining_deadline_ms(deadline_us) <= 0 ||
            !s_running.load(std::memory_order_acquire)) break;

        sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock < 0) break;
        if (!publish_active_socket(sock)) break;

        flags = fcntl(sock, F_GETFL, 0);
        if (flags >= 0) fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        int connect_result = connect(sock, res->ai_addr, res->ai_addrlen);
        if (connect_result != 0 && errno != EINPROGRESS &&
            errno != EWOULDBLOCK) break;

        if (connect_result != 0) {
            const int connect_ms = remaining_deadline_ms(deadline_us);
            if (connect_ms <= 0) break;
            fd_set wset;
            FD_ZERO(&wset);
            FD_SET(sock, &wset);
            struct timeval timeout = {
                .tv_sec = connect_ms / 1000,
                .tv_usec = (connect_ms % 1000) * 1000,
            };
            if (select(sock + 1, NULL, &wset, NULL, &timeout) <= 0) break;
            int soerr = 0;
            socklen_t sl = sizeof(soerr);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &soerr, &sl) != 0 ||
                soerr != 0) break;
        }
        if (flags >= 0) fcntl(sock, F_SETFL, flags);
        if (!apply_socket_deadline(sock, deadline_us)) break;

        if (use_tls) {
            if (!smtp_setup_tls(&ssl, &conf, &tls_io, sock,
                                config.smtp_server, deadline_us,
                                &tls_setup)) break;
            tls_active = true;
        }
        mbedtls_ssl_context *active_ssl = tls_active ? &ssl : NULL;

        if (smtp_read_reply(sock, active_ssl, line, sizeof(line), deadline_us) / 100 != 2 ||
            !smtp_send_line(sock, active_ssl, "EHLO hb-rf-eth-ng", deadline_us) ||
            smtp_read_reply(sock, active_ssl, line, sizeof(line), deadline_us) / 100 != 2) break;

        if (use_starttls) {
            if (!smtp_send_line(sock, NULL, "STARTTLS", deadline_us) ||
                smtp_read_reply(sock, NULL, line, sizeof(line), deadline_us) / 100 != 2 ||
                !smtp_setup_tls(&ssl, &conf, &tls_io, sock,
                                config.smtp_server, deadline_us,
                                &tls_setup)) break;
            tls_active = true;
            active_ssl = &ssl;
            if (!smtp_send_line(sock, active_ssl, "EHLO hb-rf-eth-ng", deadline_us) ||
                smtp_read_reply(sock, active_ssl, line, sizeof(line), deadline_us) / 100 != 2) break;
        }

        if (config.smtp_user[0]) {
            if (!smtp_send_line(sock, active_ssl, "AUTH LOGIN", deadline_us) ||
                smtp_read_reply(sock, active_ssl, line, sizeof(line), deadline_us) / 100 != 3) break;
            if (mbedtls_base64_encode(obuf, sizeof(obuf) - 1, &olen,
                    reinterpret_cast<const unsigned char *>(config.smtp_user),
                    strlen(config.smtp_user)) != 0 || olen >= sizeof(obuf)) break;
            obuf[olen] = '\0';
            if (!smtp_send_line(sock, active_ssl,
                                reinterpret_cast<const char *>(obuf), deadline_us) ||
                smtp_read_reply(sock, active_ssl, line, sizeof(line), deadline_us) / 100 != 3) break;
            if (mbedtls_base64_encode(obuf, sizeof(obuf) - 1, &olen,
                    reinterpret_cast<const unsigned char *>(config.smtp_password),
                    strlen(config.smtp_password)) != 0 || olen >= sizeof(obuf)) break;
            obuf[olen] = '\0';
            if (!smtp_send_line(sock, active_ssl,
                                reinterpret_cast<const char *>(obuf), deadline_us) ||
                smtp_read_reply(sock, active_ssl, line, sizeof(line), deadline_us) / 100 != 2) break;
        }

        snprintf(line, sizeof(line), "MAIL FROM:<%s>", config.smtp_from);
        if (!smtp_send_line(sock, active_ssl, line, deadline_us) ||
            smtp_read_reply(sock, active_ssl, line, sizeof(line), deadline_us) / 100 != 2) break;
        snprintf(line, sizeof(line), "RCPT TO:<%s>", config.smtp_to);
        if (!smtp_send_line(sock, active_ssl, line, deadline_us) ||
            smtp_read_reply(sock, active_ssl, line, sizeof(line), deadline_us) / 100 != 2 ||
            !smtp_send_line(sock, active_ssl, "DATA", deadline_us) ||
            smtp_read_reply(sock, active_ssl, line, sizeof(line), deadline_us) / 100 != 3) break;

        snprintf(line, sizeof(line), "From: %s", config.smtp_from);
        if (!smtp_send_line(sock, active_ssl, line, deadline_us)) break;
        snprintf(line, sizeof(line), "To: %s", config.smtp_to);
        if (!smtp_send_line(sock, active_ssl, line, deadline_us)) break;
        snprintf(line, sizeof(line), "Subject: [%s] %s",
                 config.smtp_from, m.default_msg);
        if (!smtp_send_line(sock, active_ssl, line, deadline_us) ||
            !smtp_send_line(sock, active_ssl,
                            "Content-Type: text/plain; charset=utf-8", deadline_us) ||
            !smtp_send_line(sock, active_ssl, "", deadline_us)) break;
        snprintf(line, sizeof(line), "%s: %s", m.default_msg,
                 e.detail[0] ? e.detail : "");
        if (!smtp_send_line(sock, active_ssl, line, deadline_us) ||
            !smtp_send_line(sock, active_ssl, ".", deadline_us) ||
            smtp_read_reply(sock, active_ssl, line, sizeof(line), deadline_us) / 100 != 2) break;

        (void)smtp_send_line(sock, active_ssl, "QUIT", deadline_us);
        ok = true;
    } while (false);

    if (res) freeaddrinfo(res);
    if (tls_active && remaining_deadline_ms(deadline_us) > 0) {
        (void)apply_socket_deadline(sock, deadline_us);
        (void)mbedtls_ssl_close_notify(&ssl);
    }
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    if (tls_setup) {
        SemaphoreHandle_t lifecycle = events_lifecycle_mutex();
        if (lifecycle) xSemaphoreTake(lifecycle, portMAX_DELAY);
        if (s_active_socket.load(std::memory_order_acquire) == sock) {
            s_active_socket.store(-1, std::memory_order_release);
        }
        mbedtls_net_free(&tls_io.net);
        if (lifecycle) xSemaphoreGive(lifecycle);
    } else if (sock >= 0) {
        close_plain_socket(sock);
    }
    xSemaphoreGive(g_net_fetch_mutex);
    return ok;
}

// ---------------------------------------------------------------------------
// Worker task.
// ---------------------------------------------------------------------------
static void events_task(void *)
{
    for (;;) {
        ESP_LOGI(TAG, "event worker started");
        while (s_running.load(std::memory_order_acquire)) {
            EventEntry e;
            if (xQueueReceive(s_queue, &e, pdMS_TO_TICKS(500)) != pdTRUE) continue;
            if (!s_running.load(std::memory_order_acquire)) break;

            // Defer notification delivery while a manual firmware upload is in
            // progress. The upload reserves heap and flash bandwidth; starting
            // webhook/Telegram/email TLS work at the same time could starve it.
            // We poll up to ~3 minutes; if the upload still isn't done, the event is
            // dropped (acceptable for non-critical notifications, and the cooldown
            // logic will suppress duplicates once delivery resumes).
            if (net_fetch_ota_active()) {
                int waited = 0;
                while (s_running.load(std::memory_order_acquire) &&
                       net_fetch_ota_active() && waited < 180000) {
                    vTaskDelay(pdMS_TO_TICKS(500));
                    waited += 500;
                }
                if (!s_running.load(std::memory_order_acquire)) break;
                if (net_fetch_ota_active()) {
                    ESP_LOGW(TAG, "dropping event %d: firmware upload still active after wait",
                             (int)e.id);
                    continue;
                }
            }

            notify_config_t config;
            SemaphoreHandle_t lifecycle = events_lifecycle_mutex();
            xSemaphoreTake(lifecycle, portMAX_DELAY);
            memcpy(&config, &s_cfg, sizeof(config));
            xSemaphoreGive(lifecycle);

            if ((int)e.id < MAX_EVENT_ID) {
                int64_t now = esp_timer_get_time();
                int64_t cooldown_us = (int64_t)config.cooldown_seconds * 1000000LL;
                if (cooldown_us > 0 && (now - s_last_sent[e.id]) < cooldown_us) {
                    g_suppressed_total.inc();
                    continue;
                }
                s_last_sent[e.id] = now;
            }

            const EventMeta &m = meta_for(e.id);

            // Snapshot the channels bitmask
            uint8_t channels = config.channels;
            bool any_ok = false;

            if (channels & NOTIFY_CHANNEL_WEBHOOK) {
                if (send_webhook(e, m, config)) any_ok = true;
            }
            if (s_running.load(std::memory_order_acquire) &&
                (channels & NOTIFY_CHANNEL_TELEGRAM)) {
                if (send_telegram(e, m, config)) any_ok = true;
            }
            if (s_running.load(std::memory_order_acquire) &&
                (channels & NOTIFY_CHANNEL_EMAIL)) {
                if (send_email(e, m, config)) any_ok = true;
            }

            if (any_ok) g_sent_total.inc();
            else        g_failed_total.inc();
        }
        ESP_LOGI(TAG, "event worker stopped");
        SemaphoreHandle_t lifecycle = events_lifecycle_mutex();
        xSemaphoreTake(lifecycle, portMAX_DELAY);
        if (s_restart_requested.exchange(false, std::memory_order_acq_rel)) {
            // A stop timed out and its caller requested restoration while this
            // worker was still unwinding TLS/socket state. Reuse the existing
            // task/stack after cleanup instead of racing a second task creation.
            xQueueReset(s_queue);
            s_active_socket.store(-1, std::memory_order_release);
            s_running.store(true, std::memory_order_release);
            xSemaphoreGive(lifecycle);
            ESP_LOGI(TAG, "event worker restart completed after deferred stop");
            continue;
        }
        s_running.store(false, std::memory_order_release);
        s_task.store(NULL, std::memory_order_release);
        xSemaphoreGive(lifecycle);
        break;
    }
    vTaskDelete(NULL);
}

// ---------------------------------------------------------------------------
// Public API.
// ---------------------------------------------------------------------------
void events_init(void)
{
    SemaphoreHandle_t lifecycle = events_lifecycle_mutex();
    if (!lifecycle) return;
    xSemaphoreTake(lifecycle, portMAX_DELAY);
    if (!s_queue) {
        s_queue = xQueueCreate(QUEUE_DEPTH, sizeof(EventEntry));
    }
    xSemaphoreGive(lifecycle);
}

esp_err_t events_start(const notify_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;
    if (!config->enabled) {
        return events_stop();
    }
    events_init();

    SemaphoreHandle_t lifecycle = events_lifecycle_mutex();
    if (!lifecycle || !s_queue) return ESP_ERR_NO_MEM;
    xSemaphoreTake(lifecycle, portMAX_DELAY);

    TaskHandle_t existing = s_task.load(std::memory_order_acquire);
    if (existing != NULL) {
        if (!s_running.load(std::memory_order_acquire)) {
            memcpy(&s_cfg, config, sizeof(s_cfg));
            s_restart_requested.store(true, std::memory_order_release);
            xSemaphoreGive(lifecycle);
            ESP_LOGW(TAG, "event worker restart queued until cleanup completes");
            return ESP_OK;
        }
        // Refresh the immutable-per-delivery snapshot used by the worker.
        memcpy(&s_cfg, config, sizeof(s_cfg));
        xSemaphoreGive(lifecycle);
        return ESP_OK;
    }

    memcpy(&s_cfg, config, sizeof(s_cfg));
    xQueueReset(s_queue);
    s_active_socket.store(-1, std::memory_order_release);
    s_restart_requested.store(false, std::memory_order_release);
    s_running.store(true, std::memory_order_release);

    TaskHandle_t h = NULL;
    // 8 KB stack — SMTP path uses mbedtls + base64 helpers.
    if (xTaskCreate(events_task, "events", 8192, NULL, 3, &h) != pdPASS) {
        ESP_LOGE(TAG, "task create failed");
        s_running.store(false, std::memory_order_release);
        xSemaphoreGive(lifecycle);
        return ESP_FAIL;
    }
    s_task.store(h, std::memory_order_release);
    xSemaphoreGive(lifecycle);
    return ESP_OK;
}

esp_err_t events_stop(void)
{
    SemaphoreHandle_t lifecycle = events_lifecycle_mutex();
    if (!lifecycle) return ESP_ERR_NO_MEM;
    xSemaphoreTake(lifecycle, portMAX_DELAY);
    TaskHandle_t task = s_task.load(std::memory_order_acquire);
    // An explicit stop always supersedes a restart queued by rollback.
    s_restart_requested.store(false, std::memory_order_release);
    if (!task) {
        s_running.store(false, std::memory_order_release);
        xSemaphoreGive(lifecycle);
        return ESP_OK;
    }

    s_running.store(false, std::memory_order_release);
    // Wake the worker if it's blocked on the queue.
    EventEntry dummy = {};
    if (s_queue) xQueueSend(s_queue, &dummy, 0);

    // Do NOT tear down the active socket from here. The worker owns the
    // mbedTLS context; calling shutdown() while a TLS handshake or data
    // exchange is in progress can leave mbedTLS in an inconsistent state
    // and corrupt the heap on the ESP32. SMTP deadlines (≤ 12 s) guarantee
    // the worker will unblock naturally well within our 30 s wait loop.
    xSemaphoreGive(lifecycle);

    for (int i = 0; i < 300 &&
         s_task.load(std::memory_order_acquire) != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (s_task.load(std::memory_order_acquire) != NULL) {
        ESP_LOGW(TAG, "event worker still stopping after timeout");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

bool events_is_running(void) { return s_running.load(); }

void events_emit(Event event, const char *detail)
{
    if (!s_queue) return;
    if (!s_running.load()) return;

    EventEntry e;
    e.id = event;
    e.timestamp = esp_timer_get_time();
    if (detail) {
        strncpy(e.detail, detail, sizeof(e.detail) - 1);
        e.detail[sizeof(e.detail) - 1] = '\0';
    } else {
        e.detail[0] = '\0';
    }
    // Non-blocking enqueue; if full, the event is dropped. The cooldown
    // logic will prevent identical replays from spamming once the queue
    // drains.
    xQueueSend(s_queue, &e, 0);
}

void events_emit_test(void)
{
    events_emit(EVENT_TEST, "manual test from monitoring diagnostic");
}
