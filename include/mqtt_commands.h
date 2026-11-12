/*
 *  mqtt_commands.h is part of the HB-RF-ETH firmware v2.0
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

#ifndef MQTT_COMMANDS_H
#define MQTT_COMMANDS_H

#include <stdbool.h>
#include <stddef.h>

#include "cJSON.h"
#include "monitoring.h"

// Stage-1 "Home Assistant full control" MQTT command surface.
//
// Goal: after initial device setup (IP, admin password, MQTT broker) every
// day-to-day operation can be driven from Home Assistant so the WebUI is no
// longer required for routine work.
//
// Command protocol (topic <prefix>/command/<name>):
//   * value-carrying commands expect a JSON object payload:
//       {"token":"<command_token>","value":"<new value>"}
//     The token field is mandatory whenever a command token is configured
//     (same shared secret as the restart button). Without a configured token
//     the field may be omitted - broker ACLs then provide the protection.
//   * action commands (restart, update_install_*) keep the legacy form: the
//     whole payload must equal the command token, because Home Assistant
//     buttons and update entities send a fixed payload_press/payload_install
//     string.
//
// Security policy (deliberate, see docs/WIKI.md "MQTT-API-Referenz"):
//   * Factory reset is NOT reachable via MQTT. A misfiring HA automation
//     must never be able to wipe the device. It stays bound to the WebUI,
//     the /recovery page and the physical button.
//   * MQTT broker settings themselves (server, user, password, TLS
//     material, command token, discovery flags) are not MQTT-writable:
//     breaking them would orphan the very control surface being used.
//   * All other configuration writes reuse the exact validation and
//     persistence paths of the REST API (Settings::save(),
//     monitoring_schedule_update_config()).

/**
 * @brief Try to dispatch one command topic message.
 *
 * Called by the MQTT event handler before the legacy restart handling.
 * Performs its own token authentication (JSON "token" field) so value
 * payloads are not rejected by the legacy byte-compare.
 *
 * @return true when the command name was known and consumed (regardless of
 *         the outcome - rejections are reported via event topics, not by
 *         falling through to "unknown command").
 */
bool mqtt_commands_try_handle(const char *command, const char *payload, int payload_len);

/**
 * @brief Publish the HA discovery configs for all controllable entities.
 *
 * Appends to the device's discovery tree. Write entities (switch, number,
 * select, text, button, update) are only announced when commands are
 * enabled - the device ignores command payloads otherwise, and clickable
 * entities that silently do nothing are worse than none.
 *
 * @param cfg      snapshot of the active MQTT configuration (prefixes,
 *                 command_enabled, command_token)
 * @param serial   device serial number (entity unique-id component)
 * @param device   shared HA discovery device block (borrowed, duplicated
 *                 per entity)
 */
void mqtt_commands_publish_discovery(const mqtt_config_t *cfg, const char *serial,
                                     const cJSON *device);

/**
 * @brief Publish retained state topics for every controllable setting
 *        (<prefix>/status/settings/<key>) and the update entity states.
 *
 * Called on broker connect, after every accepted MQTT command, and on a
 * slow periodic cycle so WebUI-side changes converge too.
 */
void mqtt_commands_publish_states(const mqtt_config_t *cfg);

/**
 * @brief Publish only the update entity state topics
 *        (<prefix>/status/update_firmware|webui).
 *
 * Cheaper than the full settings batch; called every status cycle so
 * install progress ("in_progress") tracks the installer at 60 s resolution.
 */
void mqtt_commands_publish_update_states(const mqtt_config_t *cfg);

#endif // MQTT_COMMANDS_H
