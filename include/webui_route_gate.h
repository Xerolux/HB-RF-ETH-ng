/*
 *  webui_route_gate.h is part of the HB-RF-ETH firmware v2.0
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

#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

// Boot-time admission gate for the web server's sessions (issue #491).
//
// ESP-IDF's httpd_register_uri_handler() publishes a slot in the server's
// handler table before it initialises that slot:
//
//     hd->hd_calls[i] = malloc(sizeof(httpd_uri_t));   // published here ...
//     hd->hd_calls[i]->uri = strdup(uri_handler->uri); // ... usable from here
//
// The server's worker thread walks that table for every dispatched request and
// calls uri_match_fn(hd->hd_calls[i]->uri, ...), which dereferences the slot.
// WebUI::start() has to call httpd_start() before it can register anything, so
// for a short window at every boot a request that reaches the end of the table
// reads uninitialised heap memory through that pointer - a LoadProhibited panic
// inside httpd_uri_match_wildcard() that looks like random heap corruption.
//
// The gate closes that window without touching ESP-IDF: httpd_config_t::open_fn
// holds every new session until the registering task has entered the last route
// and calls webui_route_gate_release(). Sessions already queue in the TCP
// backlog are served right after that; none is ever refused by the gate.
//
// The gate only protects registrations that happen before the release. Adding
// a route later, while sessions are being served, reintroduces the race.
//
// Usage in WebUI::start():
//
//     webui_route_gate_init();
//     config.open_fn = webui_route_gate_open_session;
//     httpd_start(&server, &config);
//     ... httpd_register_uri_handler(...) ...
//     webui_route_gate_release();

// Longest an early session is held before it is served regardless. Registration
// normally completes in well under a second; waiting longer only delays a
// client without protecting anything once registration has evidently stopped.
#define WEBUI_ROUTE_GATE_WAIT_MS 5000

// Put the gate in the holding state. Call once, before httpd_start().
void webui_route_gate_init(void);

// httpd_config_t::open_fn. Blocks the calling server thread while routes are
// still being registered, then always returns ESP_OK so the session proceeds.
esp_err_t webui_route_gate_open_session(httpd_handle_t server, int fd);

// Announce that the last route is registered. Wakes held sessions and lets
// every later one pass without waiting. Idempotent.
void webui_route_gate_release(void);

// True once the route table is complete and the server is serving sessions.
bool webui_route_gate_released(void);

#ifdef __cplusplus
}
#endif
