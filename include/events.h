/*
 *  events.h is part of the HB-RF-ETH firmware v2.0
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

#ifndef EVENTS_H
#define EVENTS_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "monitoring.h"

// Channel bitmask values for notify_config_t.channels.
#define NOTIFY_CHANNEL_WEBHOOK   (1u << 0)
#define NOTIFY_CHANNEL_TELEGRAM  (1u << 1)
#define NOTIFY_CHANNEL_EMAIL     (1u << 2)

// Stable event identifiers. The numeric value is part of the wire format
// (webhook JSON, email subject) — do not renumber existing entries.
typedef enum : uint8_t {
    EVENT_ETH_LINK_DOWN     = 0,
    EVENT_ETH_LINK_UP       = 1,
    EVENT_RF_MODULE_LOST    = 2,
    EVENT_RF_MODULE_DETECTED= 3,
    EVENT_OTA_STARTED       = 4,
    EVENT_OTA_SUCCEEDED     = 5,
    EVENT_OTA_FAILED        = 6,
    EVENT_MQTT_DISCONNECTED = 7,
    EVENT_MQTT_RECONNECTED  = 8,
    EVENT_FACTORY_RESET     = 9,
    EVENT_RESTART           = 10,
    EVENT_TEST              = 254,  // emitted by the diagnostic endpoint
} Event;

// Initialise the event subsystem (queue + worker task). Idempotent.
// The worker task is created suspended; it only runs after events_start().
void events_init(void);

// Apply a new notification configuration. Spawns or stops the worker task
// depending on `config->enabled`. Safe to call repeatedly.
esp_err_t events_start(const notify_config_t *config);
esp_err_t events_stop(void);

// True while the notification worker is running.
bool events_is_running(void);

// Emit an event. Thread-safe, non-blocking. The optional `detail` string is
// a short human-readable context (e.g. "RF module type changed: RPI-RF-MOD").
// If the event type was emitted less than `cooldown_seconds` ago, the call
// is silently dropped.
//
// `detail` may be NULL. If provided, it must be NUL-terminated and shorter
// than 128 bytes (longer values are truncated).
void events_emit(Event event, const char *detail);

// Emit a test notification on every enabled channel. Used by the
// /api/monitoring/test?target=notify endpoint.
void events_emit_test(void);

#endif // EVENTS_H
