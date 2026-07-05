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

// Initialise internal state. Idempotent; safe to call once at boot.
void log_stream_init(void);

// GET /api/log/stream — WebSocket upgrade handler. Auth is checked via the
// same validate_auth() helper used by the other endpoints; the token is
// passed either as ?token=... in the upgrade URL (front-end fallback) or as
// the standard Sec-WebSocket-Protocol header.
esp_err_t log_stream_handler(httpd_req_t *req);

// Push a single log line to every connected subscriber. Non-blocking; if a
// subscriber's send queue is full the line is dropped for that subscriber
// (live log is best-effort; the persisted ring buffer remains authoritative).
//
// `level` is the ESP-IDF log level (0..4), `tag` the ESP log tag, `message`
// is a single NUL-terminated line without trailing LF.
void log_stream_publish(int level, const char *tag, const char *message);

// Number of currently connected WebSocket subscribers. Exposed via
// /api/log/status so the UI can show "live" indicator.
int log_stream_subscriber_count(void);

// URI descriptor for httpd registration. Defined in log_stream.cpp.
extern httpd_uri_t log_stream_ws_uri;

#endif // LOG_STREAM_H
