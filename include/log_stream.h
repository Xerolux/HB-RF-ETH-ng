/*
 *  log_stream.h is part of the HB-RF-ETH firmware v2.0
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

#ifndef LOG_STREAM_H
#define LOG_STREAM_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_http_server.h"

// WebSocket-backed live log stream (Phase E). Multiple browser tabs can be
// subscribed at the same time; each receives every log line the moment
// LogManager::write() captures it.

// Initialise internal state. Idempotent; safe to call once at boot. The
// publish queue, LogManager subscription and worker remain dormant until the
// first WebSocket client connects.
void log_stream_init(void);

// GET /api/log/stream — handler for frames received after the authenticated
// WebSocket handshake. Pre/post callbacks on log_stream_ws_uri authenticate
// and register the subscriber before this handler can run.
esp_err_t log_stream_handler(httpd_req_t *req);

// HTTP server close callback. It unregisters WebSocket subscribers before the
// socket is closed, then performs the close required by ESP-IDF's close_fn
// contract. Safe for non-WebSocket HTTP sessions as well.
void log_stream_close_socket(httpd_handle_t handle, int fd);

// Queue a captured log line for every connected subscriber. `end_offset` is
// the absolute LogManager ring-stream position immediately after the line.
// A full queue forces clients to reconnect and recover from the ring snapshot
// instead of silently losing data.
void log_stream_publish(const char *message, size_t len, uint64_t end_offset);

// Number of currently snapshot-synchronised WebSocket subscribers. Exposed via
// /api/log/status so the UI can show "live" indicator.
int log_stream_subscriber_count(void);

// URI descriptor for httpd registration. Defined in log_stream.cpp.
extern httpd_uri_t log_stream_ws_uri;

#endif // LOG_STREAM_H
