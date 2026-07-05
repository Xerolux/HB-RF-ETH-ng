/*
 *  prometheus.h is part of the HB-RF-ETH firmware v2.0
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

#ifndef PROMETHEUS_H
#define PROMETHEUS_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Mirrors checkmk_config_t semantics: Prometheus is a pull model, so we
// cannot rely on a token; restrict access by source IP instead.
typedef struct {
    bool enabled;
    uint16_t port;            // default 9100
    char allowed_hosts[256];  // comma-separated list, "*" = allow all
} prometheus_config_t;

// Start the Prometheus exporter task on the configured port. No-op if the
// config is disabled. Returns ESP_FAIL if the task or socket could not be
// created.
esp_err_t prometheus_start(const prometheus_config_t *config);

// Stop the exporter and close the listening socket. Blocks up to ~2.5 s
// waiting for the task to exit cleanly.
esp_err_t prometheus_stop(void);

// True while the exporter is accepting connections. Used by the diagnostic.
bool prometheus_is_running(void);
int  prometheus_listen_port(void);

#endif // PROMETHEUS_H
