/*
 *  mqtt_handler.h is part of the HB-RF-ETH firmware v2.0
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

#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include "monitoring.h"
#include "esp_err.h"

// Initialize MQTT subsystem
esp_err_t mqtt_handler_init(void);

// Start MQTT client
esp_err_t mqtt_handler_start(const mqtt_config_t *config);

// Stop MQTT client
esp_err_t mqtt_handler_stop(void);

// True when the MQTT client is logged in to the broker. Used by the
// Prometheus exporter (`hbrfeth_mqtt_connected`) and by event/notification
// sinks to avoid emitting a "disconnected" notification on every transient
// reconnect attempt.
bool mqtt_handler_is_connected(void);

// Publish status update (status sensors + LWT "online" marker)
void mqtt_handler_publish_status(void);

// Publish a one-shot event payload under <prefix>/<subtopic> (non-retained).
// Used for transient events like OTA started / finished / command rejected.
void mqtt_handler_publish_event(const char *subtopic, const char *payload);

// Force a single immediate status publish cycle (e.g. after a setting changed
// or after OTA state changed). Safe to call from any task; returns immediately
// and schedules the publish on the periodic task if it is running.
void mqtt_handler_trigger_status_publish(void);

#endif // MQTT_HANDLER_H
