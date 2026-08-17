/*
 *  webui_internal.h is part of the HB-RF-ETH firmware v2.0
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

// Shared surface between the WebUI request-handler translation units.
//
// webui.cpp had grown past 3700 lines and held every HTTP handler in the
// firmware. This header is the seam that let the two largest self-contained
// groups — settings backup/restore and firmware upload — move into their own
// files without turning the module's internals into public API. It is
// deliberately internal to main/ and is not installed alongside webui.h.
//
// webui.cpp owns the definitions; the extracted units only consume them.

#include "cJSON.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "monitoring.h"
#include "led.h"
#include "settings.h"

// --- Shared context ---------------------------------------------------------
//
// The handlers need the objects WebUI was constructed with. These are
// accessors rather than exported globals so the extracted units cannot
// accidentally rebind them; webui.cpp remains the only writer.
Settings *webui_settings();
LED *webui_status_led();

// --- Request helpers --------------------------------------------------------

// Returns ESP_OK when the request carries a valid admin token.
esp_err_t validate_auth(httpd_req_t *req);

// Emit `{"success":false,"error":<code>[,"field":<field>]}` with an explicit
// HTTP status line. Every JSON-returning handler uses this so error shapes stay
// identical across endpoints.
esp_err_t send_json_error(httpd_req_t *req, const char *status, const char *code,
                          const char *field = NULL);

// Serialize the current device settings into `root`. Shared by /settings.json
// and the backup export so the two can never drift apart.
void add_settings(cJSON *root);

bool cJSON_GetBoolValue(const cJSON *item);
ip4_addr_t cJSON_GetIPAddrValue(const cJSON *item);
bool parse_ipv4_json(const cJSON *item, bool optional, ip4_addr_t *address);
bool valid_admin_username(const char *value);

// Read a complete request body into `buf`, honouring Content-Length and
// looping over partial reads. Returns the byte count, or a negative value on
// error/oversized body.
int recv_full_body(httpd_req_t *req, char *buf, size_t buf_size);

// Issue a new admin token, invalidating any existing session. Called after a
// restore replaces the stored credentials.
esp_err_t rotate_admin_token();

// Push the persisted flash-pause preference into the restart-sync helper.
// Called before any deliberate restart so the WebUI countdown matches what the
// device is about to do.
bool refresh_restart_sync_from_settings();

static inline int cJSON_GetIntValueSafe(cJSON *item, int defaultValue)
{
    return (item && cJSON_IsNumber(item)) ? item->valueint : defaultValue;
}

// --- Exclusive device operations --------------------------------------------

// RAII holder for the global "one disruptive operation at a time" reservation
// shared with the monitoring heap watchdog. While held, the watchdog will not
// restart the device for low heap — a firmware write legitimately drives the
// heap down, and rebooting mid-write would brick the upgrade.
class ScopedOperationReservation
{
public:
    ScopedOperationReservation() : held(ota_operation_try_begin()) {}
    ~ScopedOperationReservation()
    {
        if (held) ota_operation_finish();
    }
    explicit operator bool() const { return held; }

    ScopedOperationReservation(const ScopedOperationReservation &)            = delete;
    ScopedOperationReservation &operator=(const ScopedOperationReservation &) = delete;

private:
    bool held;
};

// --- Routes defined outside webui.cpp ---------------------------------------
//
// WebUI::start() registers every route, so the structs the extracted units own
// have to be visible to it.
extern httpd_uri_t get_backup_handler;         // webui_backup.cpp
extern httpd_uri_t post_restore_handler;       // webui_backup.cpp
extern httpd_uri_t post_ota_update_handler;    // webui_ota.cpp
extern httpd_uri_t post_restart_handler;       // webui_ota.cpp
extern httpd_uri_t post_factory_reset_handler; // webui_ota.cpp
extern httpd_uri_t get_ota_status_handler;     // webui_ota.cpp
