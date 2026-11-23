/*
 *  webui_route_gate.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "webui_route_gate.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "route_gate";

// Set by webui_route_gate_release() and never cleared, so the bit is level
// triggered: every session after the release passes without a handshake.
#define ROUTE_GATE_READY_BIT BIT0

// Allocated at compile time: the gate has to exist before the server starts,
// and there is nothing sensible to do if that allocation could fail.
static StaticEventGroup_t s_routes_ready_storage;
static EventGroupHandle_t s_routes_ready = NULL;
static bool s_released                   = false;

void webui_route_gate_init(void)
{
    s_routes_ready = xEventGroupCreateStatic(&s_routes_ready_storage);
    s_released     = false;
}

esp_err_t webui_route_gate_open_session(httpd_handle_t server, int fd)
{
    (void)server;
    (void)fd;

    if (s_routes_ready == NULL) {
        // Not initialised: behave exactly like the ungated server.
        return ESP_OK;
    }

    // Runs on the server's only worker thread, inside httpd_sess_new(). No
    // request can be dispatched while this call blocks, which is the point.
    const EventBits_t bits = xEventGroupWaitBits(s_routes_ready, ROUTE_GATE_READY_BIT, pdFALSE,
                                                 pdTRUE, pdMS_TO_TICKS(WEBUI_ROUTE_GATE_WAIT_MS));
    if ((bits & ROUTE_GATE_READY_BIT) == 0) {
        // Registration did not finish in time. Serve anyway: refusing the
        // session would only make the client retry into the same state.
        ESP_LOGW(TAG, "Route table still incomplete after %d ms, serving session anyway",
                 WEBUI_ROUTE_GATE_WAIT_MS);
    }
    return ESP_OK;
}

void webui_route_gate_release(void)
{
    s_released = true;
    if (s_routes_ready != NULL) {
        (void)xEventGroupSetBits(s_routes_ready, ROUTE_GATE_READY_BIT);
    }
}

bool webui_route_gate_released(void)
{
    return s_released;
}
