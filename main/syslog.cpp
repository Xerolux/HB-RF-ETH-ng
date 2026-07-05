/*
 *  syslog.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "syslog.h"
#include "log_manager.h"
#include "monitoring.h"
#include "settings.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "mbedtls/ssl.h"
#include "mbedtls/net_sockets.h"
#include "esp_crt_bundle.h"
#include <atomic>
#include <string.h>
#include <stdio.h>
#include <errno.h>

extern Settings *monitoring_get_settings(void);
extern SemaphoreHandle_t g_net_fetch_mutex;

static const char *TAG = "syslog";

// Forwarder configuration snapshot. Copied under s_mutex on start().
static syslog_config_t s_cfg = {};
static SemaphoreHandle_t s_cfg_mutex = NULL;

static std::atomic<bool>         s_running{false};
static std::atomic<TaskHandle_t> s_task{NULL};
static QueueHandle_t             s_queue = NULL;

// Each queued entry is a self-contained RFC 5424 message ready to send.
struct syslog_entry {
    char  buf[512];   // PRI + version + timestamp + host + app + msg
    size_t len;
};

static constexpr int QUEUE_DEPTH = 32;

// ---------------------------------------------------------------------------
// ESP-IDF log line parsing.
//
// Default IDF format:  "I (12345) TAG: user message"
//   - pos 0:   level letter V/D/I/W/E/?
//   - then " (<digits>) "
//   - then TAG up to ": "
//   - then user message (may contain spaces)
//
// Returns false if the line could not be parsed (in which case we fall back
// to severity INFO with tag "fw" and the whole line as message).
// ---------------------------------------------------------------------------
static bool parse_idf_line(const char *line, size_t len,
                           int *severity_out, char *tag_out, size_t tag_cap,
                           const char **msg_out, size_t *msg_len_out)
{
    if (len < 6 || line[1] != ' ' || line[2] != '(') return false;

    char lvl = line[0];
    int sev;
    switch (lvl) {
        case 'E': sev = 3; break;  // ERROR
        case 'W': sev = 4; break;  // WARNING
        case 'I': sev = 5; break;  // NOTICE/INFO
        case 'D': sev = 7; break;  // DEBUG
        case 'V': sev = 7; break;  // VERBOSE -> DEBUG
        default:  sev = 5; break;
    }
    *severity_out = sev;

    // Find the closing paren of "(<timestamp>)"
    size_t i = 3;
    while (i < len && line[i] != ')') i++;
    if (i >= len) return false;
    i++;                       // skip ')'
    if (i >= len || line[i] != ' ') return false;
    i++;                       // skip ' '

    // Tag: everything up to ": "
    size_t tag_start = i;
    while (i + 1 < len && !(line[i] == ':' && line[i + 1] == ' ')) i++;
    if (i + 1 >= len) return false;
    size_t tag_len = i - tag_start;
    if (tag_len == 0 || tag_len >= tag_cap) tag_len = tag_cap - 1;
    memcpy(tag_out, line + tag_start, tag_len);
    tag_out[tag_len] = '\0';

    i += 2;  // skip ": "
    *msg_out = line + i;
    *msg_len_out = len - i;
    return true;
}

static void format_rfc5424(char *out, size_t cap, size_t *out_len,
                           int severity, const char *tag,
                           const char *msg, size_t msg_len)
{
    // Facility = 1 (user-level). PRI = facility*8 + severity.
    int pri = 8 + severity;

    // ISO-8601 UTC timestamp from esp_timer. The device does not always have
    // wall-clock time early in boot, so fall back to monotonic uptime.
    int64_t now_us = esp_timer_get_time();
    time_t secs = (time_t)(now_us / 1000000ULL);
    struct tm tmv;
    gmtime_r(&secs, &tmv);
    char ts[24];
    // If the system clock has been synchronised (NTP/GPS/DCF), use it.
    // Otherwise gmtime_r will produce something around 1970 — syslog servers
    // accept that.
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tmv);

    const char *host = "hb-rf-eth-ng";
    Settings *s = monitoring_get_settings();
    if (s) {
        const char *h = s->getHostname();
        if (h && *h) host = h;
    }

    // Cap msg_len to avoid blowing the fixed buffer.
    if (msg_len > 384) msg_len = 384;

    int n = snprintf(out, cap, "<%d>1 %s %s fw %s - - %.*s\n",
                     pri, ts, host, tag ? tag : "fw",
                     (int)msg_len, msg ? msg : "");
    if (n < 0) {
        *out_len = 0;
        return;
    }
    if ((size_t)n >= cap) n = cap - 1;
    *out_len = (size_t)n;
}

// ---------------------------------------------------------------------------
// Subscriber hook called from LogManager::write().
// ---------------------------------------------------------------------------
void syslog_subscriber(const char *line, size_t len)
{
    if (!s_running.load() || !s_queue) return;

    // Severity filter (cheap pre-check before formatting)
    if (s_cfg.min_severity < 7 && line && len > 0) {
        char lvl = line[0];
        int sev;
        switch (lvl) {
            case 'E': sev = 3; break;
            case 'W': sev = 4; break;
            case 'I': sev = 5; break;
            case 'D': sev = 7; break;
            case 'V': sev = 7; break;
            default:  sev = 5; break;
        }
        if (sev > s_cfg.min_severity) return;
    }

    struct syslog_entry e;
    int sev = 6;
    char tag[32] = "fw";
    const char *msg = line;
    size_t msg_len = len;
    parse_idf_line(line, len, &sev, tag, sizeof(tag), &msg, &msg_len);
    format_rfc5424(e.buf, sizeof(e.buf), &e.len, sev, tag, msg, msg_len);
    if (e.len == 0) return;

    // Non-blocking enqueue. Drop on full (best-effort, ring buffer remains
    // authoritative).
    if (xQueueSend(s_queue, &e, 0) != pdPASS) {
        // No metric here — would re-enter LogManager via ESP_LOGW.
    }
}

// ---------------------------------------------------------------------------
// Transport.
// ---------------------------------------------------------------------------
static int resolve_and_connect_udp(const char *host, uint16_t port,
                                    struct sockaddr_in *out_addr)
{
    memset(out_addr, 0, sizeof(*out_addr));

    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    struct addrinfo *res = NULL;
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", port);
    if (getaddrinfo(host, port_str, &hints, &res) != ESP_OK || !res) {
        return -1;
    }
    memcpy(out_addr, res->ai_addr, sizeof(*out_addr));
    freeaddrinfo(res);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    return sock;
}

static int resolve_and_connect_tcp(const char *host, uint16_t port)
{
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res = NULL;
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", port);
    if (getaddrinfo(host, port_str, &hints, &res) != ESP_OK || !res) {
        return -1;
    }

    int sock = -1;
    for (struct addrinfo *a = res; a; a = a->ai_next) {
        sock = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (sock < 0) continue;

        // 3 s connect timeout via non-blocking + select
        int flags = fcntl(sock, F_GETFL, 0);
        if (flags >= 0) fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        int r = connect(sock, a->ai_addr, a->ai_addrlen);
        if (r == 0) break;

        if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
            fd_set wset;
            FD_ZERO(&wset);
            FD_SET(sock, &wset);
            struct timeval tv = { .tv_sec = 3, .tv_usec = 0 };
            r = select(sock + 1, NULL, &wset, NULL, &tv);
            if (r > 0) {
                int soerr = 0; socklen_t sl = sizeof(soerr);
                if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &soerr, &sl) == 0 && soerr == 0) {
                    // Restore blocking for downstream send()
                    if (flags >= 0) fcntl(sock, F_SETFL, flags);
                    break;
                }
            }
        }
        close(sock);
        sock = -1;
    }
    freeaddrinfo(res);
    return sock;
}

// TLS-over-TCP send. Serialised via g_net_fetch_mutex (see CLAUDE.md: any
// outbound TLS must take this mutex to avoid heap exhaustion from concurrent
// handshakes).
static bool send_tls(const char *host, uint16_t port, const char *buf, size_t len)
{
    if (!g_net_fetch_mutex) return false;
    if (xSemaphoreTake(g_net_fetch_mutex, pdMS_TO_TICKS(15000)) != pdTRUE) return false;

    bool ok = false;
    int sock = resolve_and_connect_tcp(host, port);
    if (sock < 0) goto done;

    {
        mbedtls_ssl_context ssl;
        mbedtls_ssl_config  conf;
        mbedtls_net_context server_fd;
        size_t written = 0;
        mbedtls_ssl_init(&ssl);
        mbedtls_ssl_config_init(&conf);
        mbedtls_net_init(&server_fd);

        server_fd.fd = sock;

        if (mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
                                        MBEDTLS_SSL_TRANSPORT_STREAM,
                                        MBEDTLS_SSL_PRESET_DEFAULT) != 0) goto teardown;
        mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
        esp_crt_bundle_attach(&conf);
        mbedtls_ssl_setup(&ssl, &conf);
        mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

        // hostname check
        mbedtls_ssl_set_hostname(&ssl, host);

        int r;
        while ((r = mbedtls_ssl_handshake(&ssl)) != 0) {
            if (r != MBEDTLS_ERR_SSL_WANT_READ && r != MBEDTLS_ERR_SSL_WANT_WRITE) goto teardown;
        }

        while (written < len) {
            int w = mbedtls_ssl_write(&ssl, (const unsigned char *)buf + written, len - written);
            if (w == MBEDTLS_ERR_SSL_WANT_READ || w == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
            if (w <= 0) break;
            written += (size_t)w;
        }
        ok = (written == len);

    teardown:
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&conf);
        // mbedtls_net_free closes the socket; do not double-close.
        mbedtls_net_free(&server_fd);
    }

done:
    if (g_net_fetch_mutex) xSemaphoreGive(g_net_fetch_mutex);
    return ok;
}

// ---------------------------------------------------------------------------
// Worker task.
// ---------------------------------------------------------------------------
static void syslog_task(void *pv)
{
    ESP_LOGI(TAG, "syslog forwarder started -> %s:%u transport=%u",
             s_cfg.server, s_cfg.port, s_cfg.transport);

    // Persistent TCP/TLS socket for non-UDP transports.
    int tcp_sock = -1;

    while (s_running.load()) {
        struct syslog_entry e;
        if (xQueueReceive(s_queue, &e, pdMS_TO_TICKS(500)) != pdTRUE) {
            continue;
        }
        if (e.len == 0) continue;

        if (s_cfg.transport == 0) {
            // UDP — connection-less, no need to keep a socket open.
            struct sockaddr_in dst;
            int sock = resolve_and_connect_udp(s_cfg.server, s_cfg.port, &dst);
            if (sock < 0) continue;
            sendto(sock, e.buf, e.len, 0, (struct sockaddr *)&dst, sizeof(dst));
            close(sock);
        } else if (s_cfg.transport == 1) {
            // TCP — reconnect lazily and reuse the socket.
            if (tcp_sock < 0) {
                tcp_sock = resolve_and_connect_tcp(s_cfg.server, s_cfg.port);
            }
            if (tcp_sock >= 0) {
                ssize_t w = send(tcp_sock, e.buf, e.len, 0);
                if (w <= 0) {
                    close(tcp_sock);
                    tcp_sock = -1;
                }
            }
        } else {
            // TLS — fresh handshake per message under the net-fetch mutex.
            // Syslog volumes are modest; a persistent TLS session is not
            // worth the heap on this device.
            send_tls(s_cfg.server, s_cfg.port, e.buf, e.len);
        }
    }

    if (tcp_sock >= 0) close(tcp_sock);
    ESP_LOGI(TAG, "syslog forwarder stopped");
    s_running.store(false);
    s_task.store(NULL);
    vTaskDelete(NULL);
}

// ---------------------------------------------------------------------------
// Public API.
// ---------------------------------------------------------------------------
esp_err_t syslog_start(const syslog_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;
    if (!config->enabled) return ESP_OK;
    if (s_running.load()) {
        ESP_LOGW(TAG, "already running");
        return ESP_OK;
    }
    if (config->server[0] == '\0') {
        ESP_LOGE(TAG, "no server configured");
        return ESP_ERR_INVALID_ARG;
    }

    if (s_cfg_mutex == NULL) s_cfg_mutex = xSemaphoreCreateMutex();
    if (s_cfg_mutex) {
        xSemaphoreTake(s_cfg_mutex, portMAX_DELAY);
        memcpy(&s_cfg, config, sizeof(s_cfg));
        xSemaphoreGive(s_cfg_mutex);
    }

    if (s_queue == NULL) {
        s_queue = xQueueCreate(QUEUE_DEPTH, sizeof(struct syslog_entry));
        if (!s_queue) {
            ESP_LOGE(TAG, "queue create failed");
            return ESP_ERR_NO_MEM;
        }
    }

    s_running.store(true);
    LogManager::instance().addSubscriber(syslog_subscriber);

    TaskHandle_t h = NULL;
    // 6 KB stack: TLS path uses mbedtls which is stack-hungry.
    if (xTaskCreate(syslog_task, "syslog", 6144, NULL, 4, &h) != pdPASS) {
        ESP_LOGE(TAG, "task create failed");
        LogManager::instance().removeSubscriber(syslog_subscriber);
        s_running.store(false);
        return ESP_FAIL;
    }
    s_task.store(h);
    return ESP_OK;
}

esp_err_t syslog_stop(void)
{
    if (!s_running.load()) return ESP_OK;
    s_running.store(false);
    LogManager::instance().removeSubscriber(syslog_subscriber);

    // Wake the worker by sending a no-op so it exits its queue wait.
    struct syslog_entry empty = {};
    if (s_queue) xQueueSend(s_queue, &empty, 0);

    for (int i = 0; i < 30 && s_task.load() != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    TaskHandle_t h = s_task.load();
    if (h) {
        ESP_LOGW(TAG, "task did not exit cleanly, force-deleting");
        s_task.store(NULL);
        vTaskDelete(h);
    }
    return ESP_OK;
}

bool syslog_is_running(void) { return s_running.load(); }
