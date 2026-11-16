/*
 *  mqtt_diagnostics.h is part of the HB-RF-ETH firmware v2.0
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

#ifndef MQTT_DIAGNOSTICS_H
#define MQTT_DIAGNOSTICS_H

#include "cJSON.h"
#include "monitoring.h"

#include "event_journal.h"

// Stage-2 "Diagnose ohne WebUI" MQTT surface. Read-only diagnostics that
// previously required the WebUI: live device events (HA event entity), the
// reboot-surviving event journal, the system-log tail and the crash tail,
// plus the radio-UART / min-heap metrics that were already published as
// status topics but never announced to Home Assistant.

/**
 * @brief Publish the HA discovery configs for all diagnostic entities.
 *
 * Called from mqtt_handler_publish_ha_discovery(); the entity set does not
 * depend on commandEnabled because everything here is read-only (with one
 * explicit exception: the clear-crash-log button, which is only announced
 * while commands are enabled).
 */
void mqtt_diagnostics_publish_discovery(const mqtt_config_t *cfg, const char *serial,
                                        const cJSON *device);

/**
 * @brief Publish the retained diagnostic state topics (journal history,
 *        log tail, crash tail) on their slow convergence cycle.
 *
 * Called on broker connect and every ~6 minutes. Event-ENTITY payloads are
 * published immediately per event via the journal listener instead.
 */
void mqtt_diagnostics_publish_states(const mqtt_config_t *cfg);

/**
 * @brief Journal listener entry point (registered by mqtt_handler_init).
 *
 * Publishes the event immediately as a non-retained HA event-entity payload
 * on <prefix>/event/device_events and refreshes the journal history topic so
 * automations and dashboards see both the trigger and the updated history.
 *
 * Runs on whatever task emitted the event; must never block.
 */
void mqtt_diagnostics_on_journal_event(const event_journal_entry_t *entry);

/**
 * @brief Publish only the journal history state topic.
 *
 * Used by the mqtt_publish task after the listener noted a change (see
 * mqtt_diagnostics_note_journal_change) so dashboards converge within the
 * ~5 s status subdivision without stack pressure on emitting tasks.
 */
void mqtt_diagnostics_publish_journal_snapshot(const mqtt_config_t *cfg);

/**
 * @brief Listener-side flag: a journal change wants a snapshot refresh on
 *        the mqtt_publish task (mqtt_diagnostics_consume_journal_change).
 */
void mqtt_diagnostics_note_journal_change(void);
bool mqtt_diagnostics_consume_journal_change(void);

/**
 * @brief Erase the persisted crash tail (the "clog" NVS slot in the
 *        reset_info namespace), mirroring the WebUI read-and-forget rule.
 *
 * Exposed as the MQTT command `clear_crash_log`. Returns true when the slot
 * was erased or already empty.
 */
bool mqtt_diagnostics_clear_crash_log(void);

#endif // MQTT_DIAGNOSTICS_H
