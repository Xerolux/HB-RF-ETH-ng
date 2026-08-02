/*
 *  syslog.h is part of the HB-RF-ETH firmware v2.0
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

#ifndef SYSLOG_H
#define SYSLOG_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "monitoring.h"

// Start the syslog forwarder. Spawns a background task that drains a queue
// filled by syslog_subscriber(). Also registers syslog_subscriber() with
// the LogManager. No-op if config->enabled is false.
esp_err_t syslog_start(const syslog_config_t *config);

// Stop the forwarder and unregister from LogManager. Waits up to 15 seconds
// for bounded connect/TLS I/O and worker-owned cleanup; returns ESP_ERR_TIMEOUT
// if that lifecycle is still in progress. The worker is never force-deleted;
// a start requested during cleanup is queued and reuses the existing task.
esp_err_t syslog_stop(void);

// Returns true while the forwarder task is running.
bool syslog_is_running(void);

// Subscriber hook compatible with log_line_subscriber_t. Called by
// LogManager::write() for every captured line. It only applies the severity
// filter and non-blockingly queues one bounded raw line. Parsing, timestamp /
// hostname lookup and RFC 5424 formatting are performed by the worker.
void syslog_subscriber(const char *line, size_t len, uint64_t end_offset);

#endif // SYSLOG_H
