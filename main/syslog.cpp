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
#include "crash_blackbox.h"
#include "settings.h"
#include "esp_log.h"
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
#include <time.h>

extern Settings *monitoring_get_settings(void);
extern SemaphoreHandle_t g_net_fetch_mutex;

static const char *TAG = "syslog";

// Forwarder configuration snapshot. Copied under the lifecycle mutex on start.
static syslog_config_t s_cfg = {};
// A restart requested while the old worker is still unwinding must not
// overwrite s_cfg: the old cycle may still use its server/port/TLS fields.
// The worker promotes this snapshot only after all old I/O is gone.
static syslog_config_t s_pending_cfg = {};
static StaticSemaphore_t s_lifecycle_mutex_buffer;

static SemaphoreHandle_t syslog_mutex()
{
    static SemaphoreHandle_t mutex =
        xSemaphoreCreateMutexStatic(&s_lifecycle_mutex_buffer);
    return mutex;
}

static std::atomic<bool>         s_running{false};
static std::atomic<bool>         s_restart_requested{false};
static std::atomic<TaskHandle_t> s_task{NULL};
static std::atomic<uint32_t>     s_min_severity{7};
static QueueHandle_t             s_queue = NULL;

// The logging hot path only copies one bounded raw line. Parsing, wall-clock
// access, hostname selection and RFC 5424 formatting all happen in the worker.
static constexpr size_t SYSLOG_RAW_LINE_MAX = 384;
struct syslog_entry {
    char  line[SYSLOG_RAW_LINE_MAX];
    size_t len;
};

// Queue depth: 16 bounded raw entries. Halved from the original 32 to ease
// heap pressure on the WROOM-32 (no PSRAM)
// when syslog is enabled. Syslog forwarding is best-effort UDP; a full
// queue already drops new lines via xQueueSend(..., 0) in enqueue(), so
// the lower depth trades burst capacity for ~8 KB of freed heap that is
// better spent on other TLS handshakes or a manual firmware upload. 16 still
// covers the typical smart-home log volume.
static constexpr int QUEUE_DEPTH = 16;

static void normalise_config(syslog_config_t *dst,
                             const syslog_config_t *src)
{
    memcpy(dst, src, sizeof(*dst));
    dst->server[sizeof(dst->server) - 1] = '\0';
    dst->hostname[sizeof(dst->hostname) - 1] = '\0';
    if (dst->min_severity > 7) dst->min_severity = 7;

    if (dst->hostname[0] == '\0') {
        Settings *settings = monitoring_get_settings();
        const char *hostname = settings ? settings->getHostname() : NULL;
        if (hostname && hostname[0]) {
            snprintf(dst->hostname, sizeof(dst->hostname), "%s", hostname);
        }
    }
    if (dst->hostname[0] == '\0') {
        snprintf(dst->hostname, sizeof(dst->hostname), "%s",
                 "hb-rf-eth-ng");
    }
}

static int severity_from_level(char level)
{
    switch (level) {
        case 'E': return 3;  // ERROR
        case 'W': return 4;  // WARNING
        case 'I': return 5;  // NOTICE/INFO
        case 'D': return 7;  // DEBUG
        case 'V': return 7;  // VERBOSE -> DEBUG
        default:  return 5;
    }
}

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

    *severity_out = severity_from_level(line[0]);

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
                           const char *msg, size_t msg_len,
                           const char *hostname)
{
    // Facility = 1 (user-level). PRI = facility*8 + severity.
    int pri = 8 + severity;

    // Wall-clock work belongs to the worker, never the LogManager callback.
    // If time conversion fails, RFC 5424 permits NILVALUE ("-").
    time_t secs = time(NULL);
    struct tm tmv;
    char ts[24];
    if (gmtime_r(&secs, &tmv) == NULL ||
        strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tmv) == 0) {
        strcpy(ts, "-");
    }

    // Cap msg_len to avoid blowing the fixed buffer.
    if (msg_len > 384) msg_len = 384;

    int n = snprintf(out, cap, "<%d>1 %s %s fw %s - - %.*s\n",
                     pri, ts,
                     (hostname && hostname[0]) ? hostname : "hb-rf-eth-ng",
                     tag ? tag : "fw",
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
void syslog_subscriber(const char *line, size_t len, uint64_t end_offset)
{
    (void)end_offset;
    if (!line || len == 0 ||
        !s_running.load(std::memory_order_acquire) || !s_queue) return;

    const int severity = severity_from_level(line[0]);
    if (severity > static_cast<int>(
            s_min_severity.load(std::memory_order_relaxed))) return;

    // Heap-allocate: syslog_entry is ~392 bytes and this hook runs on
    // whichever task issued ESP_LOG — including the 4 KB timer-service
    // task. xQueueSend copies into the queue's internal storage, so the
    // allocation is transient and freed immediately.
    struct syslog_entry *e = (struct syslog_entry *)malloc(sizeof(*e));
    if (!e) return;
    e->len = len < sizeof(e->line) ? len : sizeof(e->line);
    memcpy(e->line, line, e->len);

    xQueueSend(s_queue, e, 0);
    free(e);
}

// ---------------------------------------------------------------------------
// Transport.
// ---------------------------------------------------------------------------
static int resolve_and_connect_udp(const char *host, uint16_t port,
                                    struct sockaddr_in *out_addr)
{
    if (!s_running.load(std::memory_order_acquire)) return -1;
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
    if (!s_running.load(std::memory_order_acquire)) {
        freeaddrinfo(res);
        return -1;
    }
    memcpy(out_addr, res->ai_addr, sizeof(*out_addr));
    freeaddrinfo(res);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    return sock;
}

static int resolve_and_connect_tcp(const char *host, uint16_t port)
{
    if (!s_running.load(std::memory_order_acquire)) return -1;
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res = NULL;
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", port);
    if (getaddrinfo(host, port_str, &hints, &res) != ESP_OK || !res) {
        return -1;
    }
    if (!s_running.load(std::memory_order_acquire)) {
        freeaddrinfo(res);
        return -1;
    }

    int sock = -1;
    for (struct addrinfo *a = res; a; a = a->ai_next) {
        if (!s_running.load(std::memory_order_acquire)) break;
        sock = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (sock < 0) continue;

        // 3 s connect timeout via non-blocking + select
        int flags = fcntl(sock, F_GETFL, 0);
        if (flags >= 0) fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        int r = connect(sock, a->ai_addr, a->ai_addrlen);
        if (r == 0) {
            if (flags >= 0) fcntl(sock, F_SETFL, flags);
            break;
        }

        if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
            if (sock < 0 || sock >= FD_SETSIZE) {
                ESP_LOGE(TAG, "socket fd %d exceeds FD_SETSIZE %d", sock, FD_SETSIZE);
                close(sock);
                sock = -1;
                break;
            }
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

    if (sock >= 0) {
        // Bound every downstream TCP/TLS read and write. This prevents a dead
        // peer from pinning the best-effort worker (and syslog_stop) forever.
        struct timeval io_timeout = { .tv_sec = 3, .tv_usec = 0 };
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                   &io_timeout, sizeof(io_timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
                   &io_timeout, sizeof(io_timeout));
    }
    return sock;
}

// ---------------------------------------------------------------------------
// Persistent TLS session for the syslog forwarder.
//
// Originally the TLS transport did a fresh mbedtls_ssl_setup + handshake for
// every single log line. Each cycle allocates ~6-8 KB for the SSL context,
// returns it, allocates again on the next line — textbook small-block heap
// fragmentation. On the WROOM-32 (no PSRAM, ~250 KB internal heap, allocator
// with limited coalescing) this drives the *largest contiguous free block*
// steadily downward even though total free heap looks fine, until the next
// another TLS consumer cannot satisfy mbedtls_ssl_setup and panics
// deep inside the handshake. The heap watchdog eventually catches the
// sustained pressure and restarts the device.
//
// This wrapper keeps the SSL context alive across messages (mirroring how the
// TCP transport already keeps its socket). Reconnect/rehandshake happens only
// on write failure or idle timeout. The g_net_fetch_mutex is still taken per
// send to keep the cross-subsystem handshake-serialisation invariant.
// ---------------------------------------------------------------------------
struct syslog_tls_session {
    bool                 initialised = false;   // ssl/conf/net init done
    bool                 handshake_ok = false;  // ready to write
    mbedtls_ssl_context  ssl;
    mbedtls_ssl_config   conf;
    mbedtls_net_context  net_fd;
    TickType_t           last_use_tick = 0;
};

static constexpr TickType_t SYSLOG_TLS_HANDSHAKE_TIMEOUT =
    pdMS_TO_TICKS(10000);
static constexpr TickType_t SYSLOG_TLS_WRITE_TIMEOUT =
    pdMS_TO_TICKS(5000);
static constexpr int SYSLOG_STOP_TIMEOUT_MS = 15000;

static bool tick_timeout_elapsed(TickType_t start, TickType_t timeout)
{
    return (TickType_t)(xTaskGetTickCount() - start) >= timeout;
}

// Tear down everything but keep the struct alive (caller frees the struct).
static void syslog_tls_teardown(syslog_tls_session *s)
{
    if (!s || !s->initialised) return;
    // close_notify can itself consume a complete socket timeout. During a
    // requested stop the peer is not entitled to delay local resource release;
    // mbedtls_net_free still closes the connection cleanly from our side.
    if (s->handshake_ok && s_running.load(std::memory_order_acquire)) {
        mbedtls_ssl_close_notify(&s->ssl);
    }
    mbedtls_ssl_free(&s->ssl);
    mbedtls_ssl_config_free(&s->conf);
    // mbedtls_net_free closes the underlying socket.
    mbedtls_net_free(&s->net_fd);
    s->handshake_ok = false;
    s->initialised = false;
}

// Bring up (or re-bring-up) the session. Returns true on a usable session.
static bool syslog_tls_connect(syslog_tls_session *s, const char *host, uint16_t port)
{
    if (!s) return false;
    syslog_tls_teardown(s);

    int sock = resolve_and_connect_tcp(host, port);
    if (sock < 0) return false;

    mbedtls_ssl_init(&s->ssl);
    mbedtls_ssl_config_init(&s->conf);
    mbedtls_net_init(&s->net_fd);
    s->initialised = true;
    s->net_fd.fd = sock;

    if (mbedtls_ssl_config_defaults(&s->conf, MBEDTLS_SSL_IS_CLIENT,
                                    MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
        syslog_tls_teardown(s);
        return false;
    }
    // VERIFY_REQUIRED, not OPTIONAL: with OPTIONAL, mbedtls_ssl_handshake()
    // returns 0 (success) even for an invalid/expired/hostname-mismatched
    // peer certificate — the failure only shows up in
    // mbedtls_ssl_get_verify_result(), which this code never checked. That
    // silently accepted a MITM'd syslog TLS session. REQUIRED makes the
    // handshake itself fail closed on a bad certificate.
    mbedtls_ssl_conf_authmode(&s->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    esp_crt_bundle_attach(&s->conf);
    if (mbedtls_ssl_setup(&s->ssl, &s->conf) != 0) {
        syslog_tls_teardown(s);
        return false;
    }
    mbedtls_ssl_set_bio(&s->ssl, &s->net_fd, mbedtls_net_send, mbedtls_net_recv, NULL);
    mbedtls_ssl_set_hostname(&s->ssl, host);

    const TickType_t handshake_start = xTaskGetTickCount();
    int r;
    for (;;) {
        if (!s_running.load(std::memory_order_acquire) ||
            tick_timeout_elapsed(handshake_start,
                                 SYSLOG_TLS_HANDSHAKE_TIMEOUT)) {
            syslog_tls_teardown(s);
            return false;
        }
        r = mbedtls_ssl_handshake(&s->ssl);
        if (r == 0) break;
        if (r != MBEDTLS_ERR_SSL_WANT_READ && r != MBEDTLS_ERR_SSL_WANT_WRITE) {
            syslog_tls_teardown(s);
            return false;
        }
    }
    s->handshake_ok = true;
    s->last_use_tick = xTaskGetTickCount();
    return true;
}

static bool syslog_tls_send(syslog_tls_session *s, const char *host, uint16_t port,
                            const char *buf, size_t len)
{
    if (!s) return false;

    if (!s->handshake_ok) {
        if (!syslog_tls_connect(s, host, port)) return false;
    }

    const TickType_t write_start = xTaskGetTickCount();
    size_t written = 0;
    while (written < len) {
        if (!s_running.load(std::memory_order_acquire) ||
            tick_timeout_elapsed(write_start, SYSLOG_TLS_WRITE_TIMEOUT)) {
            syslog_tls_teardown(s);
            return false;
        }
        int w = mbedtls_ssl_write(&s->ssl, (const unsigned char *)buf + written, len - written);
        if (w == MBEDTLS_ERR_SSL_WANT_READ || w == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (w <= 0) {
            // Session is broken (peer reset, idle close, alert). Drop it so
            // the next send rebuilds a fresh one.
            syslog_tls_teardown(s);
            return false;
        }
        written += (size_t)w;
    }
    s->last_use_tick = xTaskGetTickCount();
    return true;
}

// Idle close threshold for a kept-alive TLS session. Without idle teardown
// the server side (or a stateful firewall) will eventually drop the session
// silently and the next write returns a fatal error — which syslog_tls_send
// already handles by tearing down and reconnecting, so this is only an
// optimisation to free ~6-8 KB of heap when syslog has been quiet.
static constexpr TickType_t SYSLOG_TLS_IDLE_CLOSE_TICKS =
    pdMS_TO_TICKS(5 * 60 * 1000);  // 5 min

// ---------------------------------------------------------------------------
// Worker task.
// ---------------------------------------------------------------------------
static void syslog_task(void *pv)
{
  for (;;) {
    ESP_LOGI(TAG, "syslog forwarder started -> %s:%u transport=%u",
             s_cfg.server, s_cfg.port, s_cfg.transport);

    // Persistent TCP socket for the TCP transport.
    int tcp_sock = -1;
    // Persistent UDP socket + resolved destination for the UDP transport.
    // Reused across log lines so we don't open/close a socket (and re-resolve
    // via getaddrinfo, which allocates) on every single message — that lwIP /
    // getaddrinfo churn adds up under high log volume and contributes to heap
    // fragmentation on the WROOM-32. Mirrors how the TCP path already keeps
    // its socket; rebuilt lazily only after a send failure.
    int udp_sock = -1;
    struct sockaddr_in udp_dst;
    memset(&udp_dst, 0, sizeof(udp_dst));

    // Persistent TLS session for the TLS transport. Allocated once on the
    // worker's stack; mbedtls contexts inside it are set up lazily.
    syslog_tls_session tls;

    while (s_running.load()) {
        struct syslog_entry e;
        if (xQueueReceive(s_queue, &e, pdMS_TO_TICKS(500)) != pdTRUE) {
            // Idle: opportunistically release the TLS session's mbedtls
            // contexts if it has been quiet for a while, so the ~6-8 KB
            // returns to the heap. Re-connected on the next log line.
            if (tls.handshake_ok) {
                const TickType_t now = xTaskGetTickCount();
                if ((TickType_t)(now - tls.last_use_tick) >=
                    SYSLOG_TLS_IDLE_CLOSE_TICKS) {
                    syslog_tls_teardown(&tls);
                }
            }
            continue;
        }
        if (e.len == 0) continue;

        int severity = 6;
        char tag[32] = "fw";
        const char *message = e.line;
        size_t message_len = e.len;
        parse_idf_line(e.line, e.len, &severity, tag, sizeof(tag),
                       &message, &message_len);

        char wire[512];
        size_t wire_len = 0;
        format_rfc5424(wire, sizeof(wire), &wire_len, severity, tag,
                       message, message_len, s_cfg.hostname);
        if (wire_len == 0) continue;

        if (s_cfg.transport == 0) {
            // UDP — keep a persistent socket + resolved destination (analogous
            // to the TCP path) instead of open/close + getaddrinfo per line.
            if (udp_sock < 0 &&
                s_running.load(std::memory_order_acquire)) {
                udp_sock = resolve_and_connect_udp(s_cfg.server, s_cfg.port, &udp_dst);
            }
            if (udp_sock >= 0) {
                ssize_t w = sendto(udp_sock, wire, wire_len, 0,
                                   (struct sockaddr *)&udp_dst, sizeof(udp_dst));
                if (w < 0) {
                    // Socket went bad (e.g. interface cycled) — drop and rebuild
                    // on the next line. Best-effort, like the TCP path.
                    close(udp_sock);
                    udp_sock = -1;
                }
            }
        } else if (s_cfg.transport == 1) {
            // TCP — reconnect lazily and reuse the socket.
            if (tcp_sock < 0 &&
                s_running.load(std::memory_order_acquire)) {
                tcp_sock = resolve_and_connect_tcp(s_cfg.server, s_cfg.port);
            }
            if (tcp_sock >= 0) {
                ssize_t w = send(tcp_sock, wire, wire_len, 0);
                if (w <= 0) {
                    close(tcp_sock);
                    tcp_sock = -1;
                }
            }
        } else {
            // TLS — persistent session, reconnect on failure.
            //
            // Skip while a manual firmware upload is in progress: the upload
            // owns g_net_fetch_mutex while writing, and contending
            // for it (or opening a second TLS context) risks starving the
            // upload of heap. The log line is dropped; the queue keeps moving.
            if (!net_fetch_ota_active() && g_net_fetch_mutex) {
                if (xSemaphoreTake(g_net_fetch_mutex, 0) == pdTRUE) {
                    crash_blackbox_net_op_begin("syslog_tls");
                    // Stop may race with the non-blocking mutex acquisition.
                    // Recheck after ownership so no TLS setup begins while the
                    // lifecycle is already unwinding.
                    if (s_running.load(std::memory_order_acquire)) {
                        syslog_tls_send(&tls, s_cfg.server, s_cfg.port,
                                        wire, wire_len);
                    }
                    crash_blackbox_net_op_end();
                    xSemaphoreGive(g_net_fetch_mutex);
                }
            }
        }
    }

    if (tcp_sock >= 0) close(tcp_sock);
    if (udp_sock >= 0) close(udp_sock);
    syslog_tls_teardown(&tls);
    ESP_LOGI(TAG, "syslog forwarder stopped");
    SemaphoreHandle_t mutex = syslog_mutex();
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (s_restart_requested.exchange(false, std::memory_order_acq_rel)) {
        // A caller tried to restore the service after stop() timed out. All
        // sockets/TLS state are gone now, so reuse this task and its stack
        // instead of racing a second task against the old cleanup path.
        memcpy(&s_cfg, &s_pending_cfg, sizeof(s_cfg));
        s_min_severity.store(s_cfg.min_severity, std::memory_order_release);
        if (s_queue) xQueueReset(s_queue);
        s_running.store(true, std::memory_order_release);
        LogManager::instance().addSubscriber(syslog_subscriber);
        xSemaphoreGive(mutex);
        ESP_LOGI(TAG, "syslog deferred restart completed");
        continue;
    }
    s_running.store(false, std::memory_order_release);
    s_task.store(NULL, std::memory_order_release);
    xSemaphoreGive(mutex);
    break;
  }
    vTaskDelete(NULL);
}

// ---------------------------------------------------------------------------
// Public API.
// ---------------------------------------------------------------------------
esp_err_t syslog_start(const syslog_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;
    if (!config->enabled) return ESP_OK;
    if (config->server[0] == '\0') {
        ESP_LOGE(TAG, "no server configured");
        return ESP_ERR_INVALID_ARG;
    }

    SemaphoreHandle_t mutex = syslog_mutex();
    if (!mutex) {
        ESP_LOGE(TAG, "lifecycle mutex create failed");
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(mutex, portMAX_DELAY);

    if (s_task.load(std::memory_order_acquire) != NULL) {
        const bool running = s_running.load(std::memory_order_acquire);
        if (!running) {
            normalise_config(&s_pending_cfg, config);
            s_restart_requested.store(true, std::memory_order_release);
        }
        xSemaphoreGive(mutex);
        if (running) {
            ESP_LOGW(TAG, "already running");
            return ESP_OK;
        }
        ESP_LOGW(TAG, "syslog restart queued until cleanup completes");
        return ESP_OK;
    }

    normalise_config(&s_cfg, config);
    s_min_severity.store(s_cfg.min_severity, std::memory_order_release);

    if (s_queue == NULL) {
        s_queue = xQueueCreate(QUEUE_DEPTH, sizeof(struct syslog_entry));
        if (!s_queue) {
            xSemaphoreGive(mutex);
            ESP_LOGE(TAG, "queue create failed");
            return ESP_ERR_NO_MEM;
        }
    }
    xQueueReset(s_queue);

    s_restart_requested.store(false, std::memory_order_release);
    s_running.store(true, std::memory_order_release);
    LogManager::instance().addSubscriber(syslog_subscriber);

    TaskHandle_t h = NULL;
    // 6 KB stack: TLS path uses mbedtls which is stack-hungry.
    if (xTaskCreate(syslog_task, "syslog", 6144, NULL, 4, &h) != pdPASS) {
        ESP_LOGE(TAG, "task create failed");
        LogManager::instance().removeSubscriber(syslog_subscriber);
        s_running.store(false, std::memory_order_release);
        xSemaphoreGive(mutex);
        return ESP_FAIL;
    }
    s_task.store(h, std::memory_order_release);
    xSemaphoreGive(mutex);
    return ESP_OK;
}

esp_err_t syslog_stop(void)
{
    SemaphoreHandle_t mutex = syslog_mutex();
    if (!mutex) return ESP_ERR_NO_MEM;

    xSemaphoreTake(mutex, portMAX_DELAY);
    TaskHandle_t task = s_task.load(std::memory_order_acquire);
    // A deliberate stop supersedes any restore queued by a previous caller.
    s_restart_requested.store(false, std::memory_order_release);
    if (!task) {
        s_running.store(false, std::memory_order_release);
        xSemaphoreGive(mutex);
        return ESP_OK;
    }

    s_running.store(false, std::memory_order_release);
    LogManager::instance().removeSubscriber(syslog_subscriber);

    // Wake the worker by sending a no-op so it exits its queue wait.
    struct syslog_entry empty = {};
    if (s_queue) xQueueSend(s_queue, &empty, 0);
    xSemaphoreGive(mutex);

    // Connect and socket I/O are bounded to 3 s; handshake/write loops also
    // observe s_running and cleanup skips close_notify during stop. Fifteen
    // seconds therefore covers the longest TLS unwind with scheduler margin.
    for (int i = 0; i < SYSLOG_STOP_TIMEOUT_MS / 100 &&
         s_task.load(std::memory_order_acquire) != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (s_task.load(std::memory_order_acquire) != NULL) {
        // Never delete a task which may own g_net_fetch_mutex or live mbedTLS
        // state. Socket timeouts bound its eventual self-cleanup; start() stays
        // blocked until the worker publishes s_task == NULL.
        ESP_LOGW(TAG, "worker still stopping after timeout");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

bool syslog_is_running(void) { return s_running.load(); }
