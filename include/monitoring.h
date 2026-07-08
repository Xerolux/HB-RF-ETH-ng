/*
 *  monitoring.h is part of the HB-RF-ETH firmware v2.0
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

#ifndef MONITORING_H
#define MONITORING_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "prometheus.h"

// Forward declarations
class Settings;
class SysInfo;
class UpdateCheck;
class Ethernet;
class RadioModuleDetector;
class SystemClock;

// CheckMK Agent Configuration
typedef struct {
    bool enabled;
    uint16_t port;  // default 6556
    char allowed_hosts[256];  // comma-separated list of IPs
} checkmk_config_t;

// MQTT Configuration
typedef struct {
    bool enabled;
    char server[65];
    uint16_t port;
    char user[33];
    char password[33];
    char topic_prefix[65];
    bool ha_discovery_enabled;
    char ha_discovery_prefix[65];
    bool tls_enable;            // True = connect via TLS (MQTT over TLS) (default: false)
    bool tls_skip_verify;       // Disables TLS cert + hostname verification (INSECURE; self-signed / lab only; default: false)
    char tls_ca_certs[2048];    // MQTT TLS CA bundle (PEM); empty = use built-in ESP-IDF CA bundle
    char tls_certfile[2048];    // MQTT TLS client cert (mTLS), PEM; empty = no client cert
    char tls_keyfile[2048];     // MQTT TLS client key (mTLS), PEM; empty = no client key
    // --- Command topic security (Phase A) ---
    // When false, the device will not subscribe to <prefix>/command/# at all
    // and ignores any command message (default: true to preserve previous
    // behaviour where commands were tied to HA discovery being enabled).
    bool command_enabled;
    // Optional shared-secret that callers must place in the command payload
    // (or in a "token" JSON field) for a command to be accepted. Empty means
    // "no token required" - in that case rely on broker-side ACL + TLS/mTLS.
    char command_token[65];
} mqtt_config_t;

// Syslog forwarding configuration (Phase B). When enabled, every line written
// to the LogManager ring buffer is also forwarded to the configured remote
// syslog server using RFC 5424 framing.
typedef struct {
    bool enabled;
    char server[65];        // host or IP
    uint16_t port;          // default 514
    uint8_t transport;      // 0 = UDP, 1 = TCP, 2 = TLS (TCP+TLS)
    uint8_t min_severity;   // 0=EMERG .. 7=DEBUG (default 6 = INFO)
    char hostname[32];      // override; empty = Settings::getHostname()
} syslog_config_t;

// Event notification configuration (Phase C/D). Multi-channel: any subset of
// webhook / telegram / email can be enabled via the `channels` bitmask.
typedef struct {
    bool enabled;
    uint8_t channels;          // bitmask: 1=webhook, 2=telegram, 4=email
    char webhook_url[193];     // https://... (192 + NUL)
    char webhook_secret[65];   // sent as X-HB-RF-ETH-Secret header
    char telegram_token[65];   // botXXXXXXX:YYYYYYYY
    char telegram_chatid[33];  // numeric chat id
    char smtp_server[65];
    uint16_t smtp_port;        // default 587
    uint8_t smtp_tls;          // 0 = plaintext, 1 = STARTTLS, 2 = implicit TLS
    char smtp_user[49];
    char smtp_password[49];
    char smtp_from[49];
    char smtp_to[49];
    uint16_t cooldown_seconds; // per-event-type debounce window (default 300)
} notify_config_t;

// Monitoring configuration
typedef struct {
    checkmk_config_t checkmk;
    mqtt_config_t mqtt;
    prometheus_config_t prometheus;
    syslog_config_t syslog;
    notify_config_t notify;
} monitoring_config_t;

// Initialize monitoring subsystem
esp_err_t monitoring_init(const monitoring_config_t *config, SysInfo* sysInfo, UpdateCheck* updateCheck);

// Register additional data providers so MQTT can publish richer status topics
// (Ethernet link/IP, radio module info, system clock / NTP sync state).
// All pointers are optional; pass NULL for any provider that is unavailable.
// Must be called BEFORE monitoring_init() so the MQTT publish task can use
// them from the very first status cycle.
void monitoring_set_providers(Ethernet* ethernet,
                              RadioModuleDetector* radioModuleDetector,
                              SystemClock* systemClock);

// Register the Settings instance so MQTT-initiated factory-reset can use
// Settings::clear() instead of direct NVS calls. Must be called before any
// MQTT command that could trigger a factory reset.
void monitoring_set_settings(Settings* settings);
Settings* monitoring_get_settings(void);

// Update configuration (synchronous - blocks caller)
esp_err_t monitoring_update_config(const monitoring_config_t *config);

// Schedule configuration update asynchronously (returns immediately, applies in background task)
esp_err_t monitoring_schedule_update_config(const monitoring_config_t *config);

// Temporarily stop heap-heavy monitoring workers before OTA. Returns an
// opaque bitmask that must be passed to monitoring_resume_after_ota() when the
// OTA fails and the device stays online. On OTA success the device reboots, so
// resuming is unnecessary.
uint32_t monitoring_pause_for_ota(void);
void monitoring_resume_after_ota(uint32_t paused_mask);

// Get current configuration
esp_err_t monitoring_get_config(monitoring_config_t *config);

// Run a lightweight connectivity/self-test for a configured monitoring target.
// Supported targets: "checkmk", "mqtt"
//
// On success the function fills:
//   * ok       – whether the probe succeeded
//   * code     – a stable, machine-readable key (e.g. "monitoring.diag.mqtt.tcp_failed")
//                that the WebUI maps to a translated string. Always set when ESP_OK
//                is returned. Backwards compatibility: older frontends still read
//                the English `message` fallback.
//   * message  – English human-readable fallback text
//   * host     – optional target host (empty when not applicable), exposed so the
//   * port     – optional target port,       frontend can interpolate translations
//                with the exact server/port the probe used.
//   * tls_enabled – mirror of the MQTT TLS flag so the UI can append a TLS note.
esp_err_t monitoring_run_diagnostic(const char *target, bool *ok,
                                    char *code, size_t code_len,
                                    char *message, size_t message_len,
                                    char *host, size_t host_len,
                                    uint16_t *port,
                                    bool *tls_enabled);
// CheckMK Agent functions
esp_err_t checkmk_start(const checkmk_config_t *config);
esp_err_t checkmk_stop(void);

// Serialize external HTTPS requests (update-check + changelog proxy) so two
// TLS connections never occupy the heap at once on memory-constrained devices.
extern SemaphoreHandle_t g_net_fetch_mutex;

// OTA download activity flag. Set by both OTA paths (WebUI URL download and
// MQTT-triggered update) around their TLS download so that lower-priority
// outbound TLS consumers (event notifications, syslog forwarding) can defer
// and leave the mutex uncontested for the duration of a firmware download.
// Without this, an SMTP notification fired by ota_started can hold
// g_net_fetch_mutex for 20-40s and starve the OTA itself.
void net_fetch_set_ota_active(bool active);
bool net_fetch_ota_active(void);

#endif // MONITORING_H
