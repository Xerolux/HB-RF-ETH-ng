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
#include "mbedtls/ssl.h"
#include "mbedtls/base64.h"
#include "mbedtls/net_sockets.h"
#include <atomic>
#include <string.h>
#include <stdio.h>
#include <errno.h>

extern Settings *monitoring_get_settings(void);
extern SemaphoreHandle_t g_net_fetch_mutex;

static const char *TAG = "events";

// ---------------------------------------------------------------------------
// Config snapshot (copied on events_start).
// ---------------------------------------------------------------------------
static notify_config_t s_cfg = {};
static SemaphoreHandle_t s_cfg_mutex = NULL;

static std::atomic<bool>         s_running{false};
static std::atomic<TaskHandle_t> s_task{NULL};
static QueueHandle_t             s_queue = NULL;

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
    static const EventMeta m_ota_start    = { "ota_started",        "Firmware update started" };
    static const EventMeta m_ota_ok       = { "ota_succeeded",      "Firmware update succeeded" };
    static const EventMeta m_ota_fail     = { "ota_failed",         "Firmware update failed" };
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
        case EVENT_OTA_STARTED:        return m_ota_start;
        case EVENT_OTA_SUCCEEDED:      return m_ota_ok;
        case EVENT_OTA_FAILED:         return m_ota_fail;
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
static bool send_webhook(const EventEntry &e, const EventMeta &m)
{
    if (s_cfg.webhook_url[0] == '\0') {
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
    if (n <= 0) { free(body); return false; }

    esp_http_client_config_t cfg = {};
    cfg.url = s_cfg.webhook_url;
    cfg.method = HTTP_METHOD_POST;
    cfg.timeout_ms = 8000;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.disable_auto_redirect = false;

    bool ok = false;
    // Outbound HTTPS must serialise on g_net_fetch_mutex.
    if (g_net_fetch_mutex && xSemaphoreTake(g_net_fetch_mutex, pdMS_TO_TICKS(15000)) == pdTRUE) {
        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (client) {
            esp_http_client_set_header(client, "Content-Type", "application/json");
            if (s_cfg.webhook_secret[0]) {
                esp_http_client_set_header(client, "X-HB-RF-ETH-Secret", s_cfg.webhook_secret);
            }
            esp_http_client_set_post_field(client, body, n);

            esp_err_t err = esp_http_client_perform(client);
            if (err == ESP_OK) {
                int code = esp_http_client_get_status_code(client);
                ok = (code >= 200 && code < 300);
            }
            esp_http_client_cleanup(client);
        }
        xSemaphoreGive(g_net_fetch_mutex);
    }
    free(body);
    return ok;
}

// --- Telegram ---
static bool send_telegram(const EventEntry &e, const EventMeta &m)
{
    if (s_cfg.telegram_token[0] == '\0' || s_cfg.telegram_chatid[0] == '\0') {
        return false;
    }

    const char *host = "hb-rf-eth-ng";
    Settings *s = monitoring_get_settings();
    if (s && s->getHostname() && s->getHostname()[0]) host = s->getHostname();

    char url[256];
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendMessage",
             s_cfg.telegram_token);

    char *body = (char *)malloc(640);
    if (!body) return false;
    int n = snprintf(body, 640,
        "{\"chat_id\":\"%s\",\"text\":\"[%s] %s: %s\",\"disable_web_page_preview\":true}",
        s_cfg.telegram_chatid, host, m.default_msg,
        e.detail[0] ? e.detail : "");
    if (n <= 0) { free(body); return false; }

    esp_http_client_config_t cfg = {};
    cfg.url = url;
    cfg.method = HTTP_METHOD_POST;
    cfg.timeout_ms = 8000;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;

    bool ok = false;
    if (g_net_fetch_mutex && xSemaphoreTake(g_net_fetch_mutex, pdMS_TO_TICKS(15000)) == pdTRUE) {
        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (client) {
            esp_http_client_set_header(client, "Content-Type", "application/json");
            esp_http_client_set_post_field(client, body, n);
            esp_err_t err = esp_http_client_perform(client);
            if (err == ESP_OK) {
                int code = esp_http_client_get_status_code(client);
                ok = (code >= 200 && code < 300);
            }
            esp_http_client_cleanup(client);
        }
        xSemaphoreGive(g_net_fetch_mutex);
    }
    free(body);
    return ok;
}

// --- Email (SMTP) ---
// Minimal SMTP client supporting plaintext / STARTTLS / implicit TLS.
// Reads/writes one line at a time; expects numeric reply codes.
static int smtp_read_reply(int sock, char *buf, size_t cap)
{
    size_t total = 0;
    while (total + 1 < cap) {
        ssize_t r = recv(sock, (uint8_t *)(buf + total), 1, 0);
        if (r <= 0) return -1;
        total += (size_t)r;
        if (buf[total - 1] == '\n') break;
    }
    buf[total] = '\0';
    // A multi-line reply uses "NNN-" for non-final lines and "NNN " for the
    // last. We loop until we see "NNN " or hit cap.
    while (total >= 4 && buf[3] == '-') {
        size_t line_start = total;
        while (total + 1 < cap) {
            ssize_t r = recv(sock, (uint8_t *)(buf + total), 1, 0);
            if (r <= 0) return -1;
            total += (size_t)r;
            if (buf[total - 1] == '\n') break;
        }
        buf[total] = '\0';
        if (total - line_start >= 4 && buf[line_start + 3] == ' ') break;
        if (total + 1 >= cap) break;
    }
    // Return the numeric code of the first line.
    return (buf[0] >= '0' && buf[0] <= '9') ? atoi(buf) : -1;
}

static int smtp_send_line(int sock, const char *line)
{
    size_t len = strlen(line);
    const char *p = line;
    while (len > 0) {
        ssize_t w = send(sock, (const uint8_t *)p, len, 0);
        if (w <= 0) return -1;
        p += w; len -= (size_t)w;
    }
    // Send CRLF.
    ssize_t w = send(sock, (const uint8_t *)"\r\n", 2, 0);
    return (w == 2) ? 0 : -1;
}

static bool send_email(const EventEntry &e, const EventMeta &m)
{
    if (s_cfg.smtp_server[0] == '\0' || s_cfg.smtp_from[0] == '\0' || s_cfg.smtp_to[0] == '\0') {
        return false;
    }

    // Implicit TLS: full TLS from the start. STARTTLS: plaintext then upgrade.
    bool use_tls = (s_cfg.smtp_tls == 2);
    bool use_starttls = (s_cfg.smtp_tls == 1);

    bool ok = false;
    if (g_net_fetch_mutex && xSemaphoreTake(g_net_fetch_mutex, pdMS_TO_TICKS(15000)) == pdTRUE) {

        struct addrinfo hints = {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo *res = NULL;
        char port_str[8];
        snprintf(port_str, sizeof(port_str), "%u", s_cfg.smtp_port ? s_cfg.smtp_port : 587);
        if (getaddrinfo(s_cfg.smtp_server, port_str, &hints, &res) != ESP_OK || !res) {
            goto release_mutex;
        }

        int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock < 0) { freeaddrinfo(res); goto release_mutex; }

        // 8s connect timeout.
        int flags = fcntl(sock, F_GETFL, 0);
        if (flags >= 0) fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        if (connect(sock, res->ai_addr, res->ai_addrlen) != 0
            && errno != EINPROGRESS && errno != EWOULDBLOCK) {
            close(sock); freeaddrinfo(res); goto release_mutex;
        }
        freeaddrinfo(res);

        fd_set wset; FD_ZERO(&wset); FD_SET(sock, &wset);
        struct timeval tv = { .tv_sec = 8, .tv_usec = 0 };
        if (select(sock + 1, NULL, &wset, NULL, &tv) <= 0) {
            close(sock); goto release_mutex;
        }
        int soerr = 0; socklen_t sl = sizeof(soerr);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &soerr, &sl) != 0 || soerr != 0) {
            close(sock); goto release_mutex;
        }
        if (flags >= 0) fcntl(sock, F_SETFL, flags);  // restore blocking

        // For implicit TLS we wrap the socket in mbedtls before SMTP talk.
        mbedtls_ssl_context ssl;
        mbedtls_ssl_config  conf;
        mbedtls_net_context net_fd;
        bool tls_active = false;
        if (use_tls) {
            mbedtls_ssl_init(&ssl);
            mbedtls_ssl_config_init(&conf);
            mbedtls_net_init(&net_fd);
            net_fd.fd = sock;
            if (mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
                                            MBEDTLS_SSL_TRANSPORT_STREAM,
                                            MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
                mbedtls_ssl_free(&ssl); mbedtls_ssl_config_free(&conf);
                close(sock); goto release_mutex;
            }
            mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
            mbedtls_ssl_conf_ca_chain(&conf, esp_crt_bundle_get_bundle(NULL), NULL);
            mbedtls_ssl_setup(&ssl, &conf);
            mbedtls_ssl_set_bio(&ssl, &net_fd, mbedtls_net_send, mbedtls_net_recv, NULL);
            char hostbuf[64];
            snprintf(hostbuf, sizeof(hostbuf), "%s", s_cfg.smtp_server);
            mbedtls_ssl_set_hostname(&ssl, hostbuf);
            int r;
            while ((r = mbedtls_ssl_handshake(&ssl)) != 0) {
                if (r != MBEDTLS_ERR_SSL_WANT_READ && r != MBEDTLS_ERR_SSL_WANT_WRITE) {
                    mbedtls_ssl_free(&ssl); mbedtls_ssl_config_free(&conf);
                    close(sock); goto release_mutex;
                }
            }
            tls_active = true;
        }

        // Helper macros for line-oriented SMTP I/O.
        char line[256];
        int code = 0;

        #define SMTP_RECV(expected) do { \
            code = tls_active ? -1 : smtp_read_reply(sock, line, sizeof(line)); \
            if (tls_active) { \
                size_t read = 0; \
                while (read + 1 < sizeof(line)) { \
                    int r = mbedtls_ssl_read(&ssl, (unsigned char *)(line + read), 1); \
                    if (r <= 0) break; \
                    read += (size_t)r; \
                    if (line[read - 1] == '\n') break; \
                } \
                line[read] = '\0'; \
                while (read >= 4 && line[3] == '-') { \
                    size_t start = read; \
                    while (read + 1 < sizeof(line)) { \
                        int rr = mbedtls_ssl_read(&ssl, (unsigned char *)(line + read), 1); \
                        if (rr <= 0) break; \
                        read += (size_t)rr; \
                        if (line[read - 1] == '\n') break; \
                    } \
                    line[read] = '\0'; \
                    if (read - start >= 4 && line[start + 3] == ' ') break; \
                    if (read + 1 >= sizeof(line)) break; \
                } \
                code = atoi(line); \
            } \
            if (code / 100 != (expected) / 100) goto smtp_fail; \
        } while (0)

        #define SMTP_SEND(s) do { \
            if (tls_active) { \
                const char *p = (s); size_t ln = strlen(p); \
                while (ln > 0) { \
                    int w = mbedtls_ssl_write(&ssl, (const unsigned char *)p, ln); \
                    if (w <= 0) goto smtp_fail; \
                    p += w; ln -= (size_t)w; \
                } \
                int w = mbedtls_ssl_write(&ssl, (const unsigned char *)"\r\n", 2); \
                if (w != 2) goto smtp_fail; \
            } else { \
                if (smtp_send_line(sock, (s)) != 0) goto smtp_fail; \
            } \
        } while (0)

        SMTP_RECV(220);
        SMTP_SEND("EHLO hb-rf-eth-ng");
        SMTP_RECV(250);

        if (use_starttls) {
            SMTP_SEND("STARTTLS");
            SMTP_RECV(220);
            // Upgrade to TLS.
            mbedtls_ssl_init(&ssl);
            mbedtls_ssl_config_init(&conf);
            mbedtls_net_init(&net_fd);
            net_fd.fd = sock;
            if (mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
                                            MBEDTLS_SSL_TRANSPORT_STREAM,
                                            MBEDTLS_SSL_PRESET_DEFAULT) != 0) goto smtp_fail;
            mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
            mbedtls_ssl_conf_ca_chain(&conf, esp_crt_bundle_get_bundle(NULL), NULL);
            mbedtls_ssl_setup(&ssl, &conf);
            mbedtls_ssl_set_bio(&ssl, &net_fd, mbedtls_net_send, mbedtls_net_recv, NULL);
            char hostbuf[64];
            snprintf(hostbuf, sizeof(hostbuf), "%s", s_cfg.smtp_server);
            mbedtls_ssl_set_hostname(&ssl, hostbuf);
            int r;
            while ((r = mbedtls_ssl_handshake(&ssl)) != 0) {
                if (r != MBEDTLS_ERR_SSL_WANT_READ && r != MBEDTLS_ERR_SSL_WANT_WRITE) goto smtp_fail;
            }
            tls_active = true;
            SMTP_SEND("EHLO hb-rf-eth-ng");
            SMTP_RECV(250);
        }

        // AUTH LOGIN (base64) — only if user is configured.
        if (s_cfg.smtp_user[0]) {
            SMTP_SEND("AUTH LOGIN");
            SMTP_RECV(334);
            unsigned char obuf[128];
            size_t olen = 0;
            mbedtls_base64_encode(obuf, sizeof(obuf), &olen,
                                  (const unsigned char *)s_cfg.smtp_user, strlen(s_cfg.smtp_user));
            obuf[olen] = '\0';
            SMTP_SEND((const char *)obuf);
            SMTP_RECV(334);
            mbedtls_base64_encode(obuf, sizeof(obuf), &olen,
                                  (const unsigned char *)s_cfg.smtp_password, strlen(s_cfg.smtp_password));
            obuf[olen] = '\0';
            SMTP_SEND((const char *)obuf);
            SMTP_RECV(235);
        }

        snprintf(line, sizeof(line), "MAIL FROM:<%s>", s_cfg.smtp_from);
        SMTP_SEND(line);
        SMTP_RECV(250);
        snprintf(line, sizeof(line), "RCPT TO:<%s>", s_cfg.smtp_to);
        SMTP_SEND(line);
        SMTP_RECV(250);
        SMTP_SEND("DATA");
        SMTP_RECV(354);

        // Build the message headers + body. Sent line by line via SMTP_SEND.
        snprintf(line, sizeof(line), "From: %s", s_cfg.smtp_from);
        SMTP_SEND(line);
        snprintf(line, sizeof(line), "To: %s", s_cfg.smtp_to);
        SMTP_SEND(line);
        snprintf(line, sizeof(line), "Subject: [%s] %s", s_cfg.smtp_from, m.default_msg);
        SMTP_SEND(line);
        SMTP_SEND("Content-Type: text/plain; charset=utf-8");
        SMTP_SEND("");

        snprintf(line, sizeof(line), "%s: %s",
                 m.default_msg,
                 e.detail[0] ? e.detail : "");
        SMTP_SEND(line);

        SMTP_SEND(".");
        SMTP_RECV(250);
        SMTP_SEND("QUIT");

        ok = true;
    smtp_fail:
        if (tls_active) {
            mbedtls_ssl_close_notify(&ssl);
            mbedtls_ssl_free(&ssl);
            mbedtls_ssl_config_free(&conf);
            // mbedtls_net_free closes the underlying socket.
            mbedtls_net_free(&net_fd);
        } else {
            close(sock);
        }
    release_mutex:
        if (g_net_fetch_mutex) xSemaphoreGive(g_net_fetch_mutex);
    }
    return ok;
}

// ---------------------------------------------------------------------------
// Worker task.
// ---------------------------------------------------------------------------
static void events_task(void *)
{
    ESP_LOGI(TAG, "event worker started");
    while (s_running.load()) {
        EventEntry e;
        if (xQueueReceive(s_queue, &e, pdMS_TO_TICKS(500)) != pdTRUE) continue;

        if ((int)e.id >= 0 && (int)e.id < MAX_EVENT_ID) {
            int64_t now = esp_timer_get_time();
            int64_t cooldown_us = (int64_t)s_cfg.cooldown_seconds * 1000000LL;
            if (cooldown_us > 0 && (now - s_last_sent[e.id]) < cooldown_us) {
                g_suppressed_total.inc();
                continue;
            }
            s_last_sent[e.id] = now;
        }

        const EventMeta &m = meta_for(e.id);

        // Snapshot the channels bitmask
        uint8_t channels = s_cfg.channels;
        bool any_ok = false;

        if (channels & NOTIFY_CHANNEL_WEBHOOK) {
            if (send_webhook(e, m)) any_ok = true;
        }
        if (channels & NOTIFY_CHANNEL_TELEGRAM) {
            if (send_telegram(e, m)) any_ok = true;
        }
        if (channels & NOTIFY_CHANNEL_EMAIL) {
            if (send_email(e, m)) any_ok = true;
        }

        if (any_ok) g_sent_total.inc();
        else        g_failed_total.inc();
    }
    ESP_LOGI(TAG, "event worker stopped");
    s_running.store(false);
    s_task.store(NULL);
    vTaskDelete(NULL);
}

// ---------------------------------------------------------------------------
// Public API.
// ---------------------------------------------------------------------------
void events_init(void)
{
    if (s_cfg_mutex == NULL) s_cfg_mutex = xSemaphoreCreateMutex();
    if (s_queue == NULL) {
        s_queue = xQueueCreate(QUEUE_DEPTH, sizeof(EventEntry));
    }
}

esp_err_t events_start(const notify_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;
    if (!config->enabled) {
        // Stop the worker if it's running, since we just got disabled.
        if (s_running.load()) events_stop();
        return ESP_OK;
    }
    events_init();

    if (s_cfg_mutex) {
        xSemaphoreTake(s_cfg_mutex, portMAX_DELAY);
        memcpy(&s_cfg, config, sizeof(s_cfg));
        xSemaphoreGive(s_cfg_mutex);
    } else {
        memcpy(&s_cfg, config, sizeof(s_cfg));
    }

    if (s_running.load()) {
        return ESP_OK;  // already running, just refreshed config
    }

    s_running.store(true);
    TaskHandle_t h = NULL;
    // 8 KB stack — SMTP path uses mbedtls + base64 helpers.
    if (xTaskCreate(events_task, "events", 8192, NULL, 3, &h) != pdPASS) {
        ESP_LOGE(TAG, "task create failed");
        s_running.store(false);
        return ESP_FAIL;
    }
    s_task.store(h);
    return ESP_OK;
}

esp_err_t events_stop(void)
{
    if (!s_running.load()) return ESP_OK;
    s_running.store(false);
    // Wake the worker if it's blocked on the queue.
    EventEntry dummy = {};
    if (s_queue) xQueueSend(s_queue, &dummy, 0);

    for (int i = 0; i < 30 && s_task.load() != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    TaskHandle_t h = s_task.load();
    if (h) {
        ESP_LOGW(TAG, "worker did not exit cleanly, force-deleting");
        s_task.store(NULL);
        vTaskDelete(h);
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
