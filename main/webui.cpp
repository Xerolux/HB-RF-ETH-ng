/*
 *  webui.cpp is part of the HB-RF-ETH firmware v2.0
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <sys/param.h>
#include <atomic>
#include <memory>
#include <new>
#include <stdarg.h>
#include "webui.h"
#include "webui_internal.h"
#include "ping_service.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include "esp_ota_ops.h"
#include "mbedtls/md.h"
#include "mbedtls/base64.h"
#include "monitoring_api.h"
#include "monitoring.h"
#include "events.h"
#include "rate_limiter.h"
#include "security_headers.h"
#include "secure_utils.h"
#include "log_manager.h"
#include "reset_info.h"
#include "nvs_storage_lock.h"
#include "crash_blackbox.h"
#include "system_reset.h"
#include "system_overview_api.h"
#include "theme_api.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "ota_config.h"
#include "semver.h"
#include "validation.h"
#include "log_stream.h"
#include "pins.h"

static const char *TAG = "WebUI";

// Safe cJSON number accessor: returns the integer value only when the item
// exists and is actually a number. Prevents undefined behaviour when the
// frontend sends an unexpected type.
#define EMBED_HANDLER(_uri, _resource, _contentType, _contentEncoding) \
    extern const char _resource[] asm("_binary_" #_resource "_start"); \
    extern const size_t _resource##_length asm(#_resource "_length");  \
    esp_err_t _resource##_handler_func(httpd_req_t *req)               \
    {                                                                  \
        add_security_headers(req);                                     \
        httpd_resp_set_type(req, _contentType);                        \
        httpd_resp_set_hdr(req, "Content-Encoding", _contentEncoding); \
        httpd_resp_send(req, _resource, _resource##_length);           \
        return ESP_OK;                                                 \
    };                                                                 \
    httpd_uri_t _resource##_handler = {                                \
        .uri = _uri,                                                   \
        .method = HTTP_GET,                                            \
        .handler = _resource##_handler_func,                           \
        .user_ctx = NULL};

EMBED_HANDLER("/*", index_html_gz, "text/html", "gzip")
EMBED_HANDLER("/main.js", main_js_gz, "application/javascript", "gzip")
EMBED_HANDLER("/main.css", main_css_gz, "text/css", "gzip")
EMBED_HANDLER("/favicon.ico", favicon_ico_gz, "image/x-icon", "gzip")
// PWA assets — make the WebUI installable. The single icon-256.png serves as
// the standard icon, the maskable icon (declared in manifest.webmanifest with
// purpose "any maskable"), and the iOS apple-touch-icon (referenced from the
// apple-touch-icon <link> in index.html). Embedding one file instead of three
// copies saves ~188 KB of flash on the memory-constrained WROOM-32.
EMBED_HANDLER("/manifest.webmanifest", manifest_webmanifest_gz, "application/manifest+json", "gzip")
EMBED_HANDLER("/icon-256.png", icon_256_png_gz, "image/png", "gzip")

static Settings *_settings;
static LED *_statusLED;
static SysInfo *_sysInfo;
static Ethernet *_ethernet;
static RawUartUdpListener *_rawUartUdpListener;
static RadioModuleConnector *_radioModuleConnector;
static RadioModuleDetector *_radioModuleDetector;
static char _token[46];

// Read-only handles for the handler units extracted out of this file
// (webui_backup.cpp, webui_ota.cpp). They stay static here so this
// translation unit remains the only writer; see webui_internal.h.
Settings *webui_settings() { return _settings; }
LED *webui_status_led() { return _statusLED; }

static void emit_log_enable_snapshot();
int recv_full_body(httpd_req_t *req, char *buf, size_t buf_size);

static constexpr int64_t PASSWORD_RESET_WINDOW_US = 90LL * 1000LL * 1000LL;
static bool s_password_reset_active = false;
static bool s_password_reset_confirmed = false;
static bool s_password_reset_wait_release = false;
static int64_t s_password_reset_deadline_us = 0;
static char s_password_reset_token[17] = {0};

static bool board_button_pressed()
{
    return gpio_get_level(HM_BTN_PIN) == 0;
}

static void reset_password_reset_state()
{
    s_password_reset_active = false;
    s_password_reset_confirmed = false;
    s_password_reset_wait_release = false;
    s_password_reset_deadline_us = 0;
    memset(s_password_reset_token, 0, sizeof(s_password_reset_token));
}

static int password_reset_remaining_seconds()
{
    if (!s_password_reset_active) return 0;
    int64_t remaining_us = s_password_reset_deadline_us - esp_timer_get_time();
    if (remaining_us <= 0) return 0;
    return (int)((remaining_us + 999999) / 1000000);
}

static void generate_password_reset_token()
{
    uint32_t rnd[2] = {esp_random(), esp_random()};
    snprintf(s_password_reset_token, sizeof(s_password_reset_token), "%08" PRIx32 "%08" PRIx32, rnd[0], rnd[1]);
}

static bool password_reset_token_matches(const char *token)
{
    return token && s_password_reset_token[0] && secure_strcmp(token, s_password_reset_token) == 0;
}

static bool password_reset_refresh_state()
{
    if (!s_password_reset_active) return false;

    if (esp_timer_get_time() >= s_password_reset_deadline_us)
    {
        reset_password_reset_state();
        return false;
    }

    if (!s_password_reset_confirmed)
    {
        const bool pressed = board_button_pressed();
        if (s_password_reset_wait_release)
        {
            s_password_reset_wait_release = pressed;
        }
        else if (pressed)
        {
            s_password_reset_confirmed = true;
            ESP_LOGW(TAG, "Password reset confirmed by physical board button");
        }
    }

    return true;
}

static bool read_password_reset_token(httpd_req_t *req, char *token, size_t token_size)
{
    char buffer[128];
    int len = recv_full_body(req, buffer, sizeof(buffer));
    if (len <= 0) return false;

    cJSON *root = cJSON_Parse(buffer);
    if (!root) return false;

    const char *token_value = cJSON_GetStringValue(cJSON_GetObjectItem(root, "token"));
    bool ok = token_value && token_value[0] && strlen(token_value) < token_size;
    if (ok)
    {
        strncpy(token, token_value, token_size);
        token[token_size - 1] = '\0';
    }
    cJSON_Delete(root);
    return ok;
}


static esp_err_t generate_fresh_admin_token(char *out, size_t out_size)
{
    if (!out || out_size == 0 || !_sysInfo) return ESP_ERR_INVALID_ARG;
    out[0] = '\0';

    char tokenBase[21] = {};
    uint32_t rnd[2] = {esp_random(), esp_random()};
    memcpy(tokenBase, rnd, sizeof(rnd));
    const char *serial_number = _sysInfo->getSerialNumber();
    if (!serial_number) serial_number = "";
    strncpy(tokenBase + 2 * sizeof(uint32_t), serial_number,
            sizeof(tokenBase) - 2 * sizeof(uint32_t) - 1);
    tokenBase[sizeof(tokenBase) - 1] = '\0';

    unsigned char shaResult[32] = {};

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    const mbedtls_md_info_t *md_info =
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    int md_result = md_info ? mbedtls_md_setup(&ctx, md_info, 0) : -1;
    if (md_result == 0) md_result = mbedtls_md_starts(&ctx);
    if (md_result == 0) {
        md_result = mbedtls_md_update(
            &ctx, reinterpret_cast<unsigned char *>(tokenBase), 20);
    }
    if (md_result == 0) md_result = mbedtls_md_finish(&ctx, shaResult);
    mbedtls_md_free(&ctx);
    if (md_result != 0) return ESP_FAIL;

    size_t tokenLength = 0;
    const int encode_result = mbedtls_base64_encode(
        reinterpret_cast<unsigned char *>(out), out_size, &tokenLength,
        shaResult, sizeof(shaResult));
    if (encode_result != 0 || tokenLength >= out_size) {
        out[0] = '\0';
        return ESP_ERR_INVALID_SIZE;
    }
    out[tokenLength] = '\0';
    return ESP_OK;
}

esp_err_t rotate_admin_token()
{
    if (!_settings) return ESP_ERR_INVALID_STATE;
    char fresh_token[sizeof(_token)] = {};
    esp_err_t result =
        generate_fresh_admin_token(fresh_token, sizeof(fresh_token));
    if (result == ESP_OK) result = _settings->saveAdminToken(fresh_token);
    // Runtime authorization changes only after the new token is durable.
    if (result == ESP_OK) {
        memcpy(_token, fresh_token, sizeof(_token));
        _token[sizeof(_token) - 1] = '\0';
    }
    memset(fresh_token, 0, sizeof(fresh_token));
    return result;
}

void generateToken()
{
    // Try persisted token first (survives reboots — keeps "remember me" valid
    // across firmware updates and restarts).
    if (_settings && _settings->loadAdminToken(_token, sizeof(_token))) return;

    _token[0] = '\0';
    const esp_err_t result = rotate_admin_token();
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Could not create a durable admin token: %s",
                 esp_err_to_name(result));
    }
}

const char *ip2str(ip4_addr_t addr, ip4_addr_t fallback)
{
    if (addr.addr == IPADDR_ANY || addr.addr == IPADDR_NONE)
    {
        if (fallback.addr == IPADDR_ANY || fallback.addr == IPADDR_NONE)
        {
            return "";
        }
        return ip4addr_ntoa(&fallback);
    }
    return ip4addr_ntoa(&addr);
}

const char *ip2str(ip4_addr_t addr)
{
    if (addr.addr == IPADDR_ANY || addr.addr == IPADDR_NONE)
    {
        return "";
    }
    return ip4addr_ntoa(&addr);
}

void formatRadioMAC(uint32_t radioMAC, char *buf, size_t bufSize)
{
    if (radioMAC == 0)
    {
        snprintf(buf, bufSize, "n/a");
    }
    else
    {
        snprintf(buf, bufSize, "0x%06" PRIX32, radioMAC);
    }
}

esp_err_t validate_auth(httpd_req_t *req)
{
    if (_token[0] == '\0') return ESP_FAIL;
    char auth[60] = {0};
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth, sizeof(auth)) != ESP_OK)
        return ESP_FAIL;

    if (strncmp(auth, "Token ", 6) != 0)
        return ESP_FAIL;

    if (secure_strcmp(auth + 6, _token) != 0)
        return ESP_FAIL;

    return ESP_OK;
}

// Bearer-token check for callers that do not have an httpd_req_t — most
// notably the WebSocket upgrade handler, which cannot rely on the
// Authorization header because browsers do not let the WebSocket client
// API set custom headers. The token is compared in constant time.
bool check_admin_token(const char *token)
{
    if (!token || !token[0]) return false;
    return secure_strcmp(token, _token) == 0;
}

// Receive the complete request body into buf (NUL-terminated).
// httpd_req_recv() performs a single socket read and may return fewer bytes
// than requested when the body spans multiple TCP segments, so loop until
// content_len bytes have arrived. Returns the body length, or -1 if the body
// does not fit into buf or the connection failed.
int recv_full_body(httpd_req_t *req, char *buf, size_t buf_size)
{
    if (req->content_len >= buf_size)
        return -1;

    size_t received = 0;
    int timeout_retries = 3;
    while (received < req->content_len)
    {
        int ret = httpd_req_recv(req, buf + received, req->content_len - received);
        if (ret == HTTPD_SOCK_ERR_TIMEOUT && timeout_retries-- > 0)
            continue;
        if (ret <= 0)
            return -1;
        received += ret;
    }
    buf[received] = '\0';
    return (int)received;
}

esp_err_t post_login_json_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    // Check rate limit, but allow the whitelisted CCU IP. Only an explicitly,
    // authenticated-admin-configured static CCU address is trusted here.
    // The previous fallback to _rawUartUdpListener->getConnectedRemoteAddress()
    // let any LAN host bypass the login brute-force throttle for free: that
    // address is set by the unauthenticated raw-uart protocol (port 3008)
    // whenever ANY host sends a 5-byte "connect" packet — no credential or
    // proof of being the real CCU required. Trusting it for a security
    // control was an unauthenticated-network-signal-based bypass.
    ip4_addr_t ccu_ip = {0};
    const char* storedCCUIP = _settings->getCCUIP();

    if (storedCCUIP && strlen(storedCCUIP) > 0) {
        ip4addr_aton(storedCCUIP, &ccu_ip);
    }

    if (!rate_limiter_is_whitelisted(req, &ccu_ip) && !rate_limiter_check_login(req))
    {
        httpd_resp_set_status(req, "429 Too Many Requests");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"isAuthenticated\":false,\"error\":\"Too many login attempts. Please try again later.\"}");
        return ESP_OK;
    }

    char buffer[1024];
    int len = recv_full_body(req, buffer, sizeof(buffer));

    if (len > 0)
    {
        cJSON *root = cJSON_Parse(buffer);

        if (!root) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "{\"isAuthenticated\":false,\"error\":\"Invalid request\"}");
            return ESP_OK;
        }

        char *username = cJSON_GetStringValue(cJSON_GetObjectItem(root, "username"));
        char *password = cJSON_GetStringValue(cJSON_GetObjectItem(root, "password"));

        bool isAuthenticated = (username != NULL) &&
                               (password != NULL) &&
                               (secure_strcmp(username, _settings->getAdminUsername()) == 0) &&
                               (secure_strcmp(password, _settings->getAdminPassword()) == 0);

        cJSON_Delete(root);

        httpd_resp_set_type(req, "application/json");
        root = cJSON_CreateObject();

        cJSON_AddBoolToObject(root, "isAuthenticated", isAuthenticated);
        if (isAuthenticated)
        {
            // Successful login - reset rate limit for this IP
            rate_limiter_reset_ip(req);
            cJSON_AddStringToObject(root, "token", _token);
            cJSON_AddBoolToObject(root, "passwordChanged", _settings->getPasswordChanged());
        }

        const char *json = cJSON_PrintUnformatted(root);
        if (json) {
            httpd_resp_sendstr(req, json);
            free((void *)json);
        } else {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON alloc failed");
        }
        cJSON_Delete(root);

        return ESP_OK;
    }

    return ESP_FAIL;
}

httpd_uri_t post_login_json_handler = {
    .uri = "/login.json",
    .method = HTTP_POST,
    .handler = post_login_json_handler_func,
    .user_ctx = NULL};

esp_err_t post_password_reset_start_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    generate_password_reset_token();
    s_password_reset_active = true;
    s_password_reset_confirmed = false;
    s_password_reset_wait_release = board_button_pressed();
    s_password_reset_deadline_us = esp_timer_get_time() + PASSWORD_RESET_WINDOW_US;

    ESP_LOGW(TAG, "Password reset armed; waiting for physical board button confirmation");

    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "token", s_password_reset_token);
    cJSON_AddNumberToObject(response, "expiresIn", password_reset_remaining_seconds());
    cJSON_AddStringToObject(response, "button", "HM button (GPIO34)");

    httpd_resp_set_type(req, "application/json");
    const char *json = cJSON_PrintUnformatted(response);
    if (json) {
        httpd_resp_sendstr(req, json);
        free((void *)json);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON alloc failed");
    }
    cJSON_Delete(response);
    return ESP_OK;
}

httpd_uri_t post_password_reset_start_handler = {
    .uri = "/api/password-reset/start",
    .method = HTTP_POST,
    .handler = post_password_reset_start_handler_func,
    .user_ctx = NULL};

esp_err_t post_password_reset_status_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    char token[sizeof(s_password_reset_token)] = {0};
    if (!read_password_reset_token(req, token, sizeof(token)) || !password_reset_token_matches(token))
    {
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Invalid reset token");
    }

    const bool active = password_reset_refresh_state();

    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "active", active);
    cJSON_AddBoolToObject(response, "confirmed", active && s_password_reset_confirmed);
    cJSON_AddNumberToObject(response, "expiresIn", password_reset_remaining_seconds());

    httpd_resp_set_type(req, "application/json");
    const char *json = cJSON_PrintUnformatted(response);
    if (json) {
        httpd_resp_sendstr(req, json);
        free((void *)json);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON alloc failed");
    }
    cJSON_Delete(response);
    return ESP_OK;
}

httpd_uri_t post_password_reset_status_handler = {
    .uri = "/api/password-reset/status",
    .method = HTTP_POST,
    .handler = post_password_reset_status_handler_func,
    .user_ctx = NULL};

esp_err_t post_password_reset_complete_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    char buffer[512];
    int len = recv_full_body(req, buffer, sizeof(buffer));
    if (len <= 0)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
    }

    cJSON *root = cJSON_Parse(buffer);
    if (!root)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    const char *token = cJSON_GetStringValue(cJSON_GetObjectItem(root, "token"));
    const char *newPassword = cJSON_GetStringValue(cJSON_GetObjectItem(root, "newPassword"));

    if (!password_reset_token_matches(token) || !password_reset_refresh_state() || !s_password_reset_confirmed)
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Physical confirmation required");
    }

    if (!validateAdminPassword(newPassword))
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Password must be 8-32 characters with uppercase, lowercase, and numbers");
    }

    settings_snapshot_t previous_settings = {};
    _settings->snapshot(&previous_settings);
    const esp_err_t token_result = rotate_admin_token();
    if (token_result != ESP_OK) {
        ESP_LOGE(TAG, "Physical password reset token rotation failed: %s",
                 esp_err_to_name(token_result));
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Authentication token could not be persisted");
    }
    if (!_settings->setAdminPassword(newPassword))
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid password");
    }
    if (_settings->save() != ESP_OK) {
        _settings->restoreSnapshot(&previous_settings);
        const esp_err_t rollback_result = _settings->save();
        if (rollback_result != ESP_OK) {
            ESP_LOGE(TAG, "Physical password reset rollback failed: %s",
                     esp_err_to_name(rollback_result));
        }
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Password could not be persisted");
    }
    cJSON_Delete(root);

    reset_password_reset_state();

    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "token", _token);

    httpd_resp_set_type(req, "application/json");
    const char *json = cJSON_PrintUnformatted(response);
    if (json) {
        httpd_resp_sendstr(req, json);
        free((void *)json);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON alloc failed");
    }
    cJSON_Delete(response);

    ESP_LOGW(TAG, "Admin password reset via physical board button");
    return ESP_OK;
}
httpd_uri_t post_password_reset_complete_handler = {
    .uri = "/api/password-reset/complete",
    .method = HTTP_POST,
    .handler = post_password_reset_complete_handler_func,
    .user_ctx = NULL};

esp_err_t get_sysinfo_json_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");

    // Determine Radio Module Type String
    const char* radioModuleTypeStr = "-";
    switch (_radioModuleDetector->getRadioModuleType())
    {
    case RADIO_MODULE_HM_MOD_RPI_PCB:
        radioModuleTypeStr = "HM-MOD-RPI-PCB";
        break;
    case RADIO_MODULE_RPI_RF_MOD:
        radioModuleTypeStr = "RPI-RF-MOD";
        break;
    default:
        break;
    }

    // Format Radio MACs
    char bidCosMAC[16];
    char hmIPMAC[16];
    formatRadioMAC(_radioModuleDetector->getBidCosRadioMAC(), bidCosMAC, sizeof(bidCosMAC));
    formatRadioMAC(_radioModuleDetector->getHmIPRadioMAC(), hmIPMAC, sizeof(hmIPMAC));

    // Format Firmware Version
    const uint8_t *fwVersion = _radioModuleDetector->getFirmwareVersion();
    char fwVersionStr[16];
    snprintf(fwVersionStr, sizeof(fwVersionStr), "%d.%d.%d", fwVersion[0], fwVersion[1], fwVersion[2]);

    char buf[256];

    // Start sysInfo object
    snprintf(buf, sizeof(buf), "{\"sysInfo\":{");
    httpd_resp_send_chunk(req, buf, strlen(buf));

    snprintf(buf, sizeof(buf), "\"serial\":\"%s\",\"hostname\":\"%s\",\"currentVersion\":\"%s\",",
             _sysInfo->getSerialNumber(), _settings->getHostname(), _sysInfo->getCurrentVersion());
    httpd_resp_send_chunk(req, buf, strlen(buf));

    snprintf(buf, sizeof(buf), "\"latestVersion\":\"%s\",\"memoryUsage\":%.1f,\"cpuUsage\":%.1f,",
             "n/a", _sysInfo->getMemoryUsage(), _sysInfo->getCpuUsage());
    httpd_resp_send_chunk(req, buf, strlen(buf));

    snprintf(buf, sizeof(buf), "\"supplyVoltage\":null,\"temperature\":null,\"uptimeSeconds\":%" PRIu32 ",",
             (uint32_t)_sysInfo->getUptimeSeconds());
    httpd_resp_send_chunk(req, buf, strlen(buf));

    snprintf(buf, sizeof(buf), "\"boardRevision\":\"%s\",\"boardSenseVoltage\":%" PRIu32 ",\"resetReason\":\"%s\",",
             _sysInfo->getBoardRevisionString().c_str(), (uint32_t)_sysInfo->getBoardSenseVoltage(), _sysInfo->getResetReason());
    httpd_resp_send_chunk(req, buf, strlen(buf));

    snprintf(buf, sizeof(buf), "\"ethernetConnected\":%s,\"ethernetSpeed\":%d,\"ethernetDuplex\":\"%s\",",
             _ethernet->isConnected() ? "true" : "false", _ethernet->getLinkSpeedMbps(), _ethernet->getDuplexMode());
    httpd_resp_send_chunk(req, buf, strlen(buf));

    ip4_addr_t currentIP, currentNM, currentGW, currentDNS1, currentDNS2;
    _ethernet->getNetworkSettings(&currentIP, &currentNM, &currentGW, &currentDNS1, &currentDNS2);

    // Format IPs individually to avoid lwIP ip4addr_ntoa static buffer overlap
    snprintf(buf, sizeof(buf), "\"localIP\":\"%s\",", ip2str(currentIP));
    httpd_resp_send_chunk(req, buf, strlen(buf));
    snprintf(buf, sizeof(buf), "\"netmask\":\"%s\",", ip2str(currentNM));
    httpd_resp_send_chunk(req, buf, strlen(buf));
    snprintf(buf, sizeof(buf), "\"gateway\":\"%s\",", ip2str(currentGW));
    httpd_resp_send_chunk(req, buf, strlen(buf));
    snprintf(buf, sizeof(buf), "\"dns1\":\"%s\",", ip2str(currentDNS1));
    httpd_resp_send_chunk(req, buf, strlen(buf));
    snprintf(buf, sizeof(buf), "\"dns2\":\"%s\",", ip2str(currentDNS2));
    httpd_resp_send_chunk(req, buf, strlen(buf));

    // ipv6Array
    snprintf(buf, sizeof(buf), "\"ipv6Addresses\":[");
    httpd_resp_send_chunk(req, buf, strlen(buf));
    
    char ipv6_addrs[4][48];
    int ipv6_count = _ethernet->getIPv6AddressStrings(ipv6_addrs, 4);
    for (int i = 0; i < ipv6_count; i++) {
        snprintf(buf, sizeof(buf), "%s\"%s\"", (i == 0) ? "" : ",", ipv6_addrs[i]);
        httpd_resp_send_chunk(req, buf, strlen(buf));
    }
    
    snprintf(buf, sizeof(buf), "],");
    httpd_resp_send_chunk(req, buf, strlen(buf));

    // radio module
    snprintf(buf, sizeof(buf), "\"rawUartRemoteAddress\":\"%s\",\"radioModuleType\":\"%s\",\"radioModuleSerial\":\"%s\",",
             ip2str(_rawUartUdpListener->getConnectedRemoteAddress()), radioModuleTypeStr, _radioModuleDetector->getSerial());
    httpd_resp_send_chunk(req, buf, strlen(buf));

    snprintf(buf, sizeof(buf), "\"radioModuleFirmwareVersion\":\"%s\",\"radioModuleBidCosRadioMAC\":\"%s\",",
             fwVersionStr, bidCosMAC);
    httpd_resp_send_chunk(req, buf, strlen(buf));

    snprintf(buf, sizeof(buf), "\"radioModuleHmIPRadioMAC\":\"%s\",\"radioModuleSGTIN\":\"%s\"",
             hmIPMAC, _radioModuleDetector->getSGTIN());
    httpd_resp_send_chunk(req, buf, strlen(buf));

    // Task stack high-water marks. The summary string can exceed the 256-byte
    // buf above, so emit it as its own chunk with a dedicated buffer. Spaces
    // are legal in JSON strings; only escape quotes and backslashes (defensive
    // — task names are bounded ASCII).
    //
    // Sized from the string rather than a fixed bound: the list is sorted
    // danger-first, so a fixed buffer silently drops the over-provisioned
    // tasks at the tail, which are exactly the ones worth reclaiming stack
    // from. Falls back to a truncated-but-valid chunk if the allocation
    // fails, so a low-heap device still answers /sysinfo.json.
    {
        std::string stacks = _sysInfo->getTaskStackInfo();
        const size_t prefix_len = sizeof(",\"taskStacks\":\"") - 1;
        // Worst case every character needs an escape, plus the closing quote
        // and the NUL.
        const size_t stack_buf_size = prefix_len + stacks.size() * 2 + 2;
        char *stack_buf             = (char *)malloc(stack_buf_size);
        if (stack_buf) {
            size_t pos   = 0;
            stack_buf[0] = 0;
            pos += snprintf(stack_buf + pos, stack_buf_size - pos, ",\"taskStacks\":\"");
            for (const char *p = stacks.c_str(); *p && pos + 4 < stack_buf_size; p++) {
                if (*p == '"' || *p == '\\') {
                    stack_buf[pos++] = '\\';
                }
                stack_buf[pos++] = *p;
            }
            stack_buf[pos++] = '"';
            stack_buf[pos]   = 0;
            httpd_resp_send_chunk(req, stack_buf, pos);
            free(stack_buf);
        } else {
            static const char empty[] = ",\"taskStacks\":\"alloc-failed\"";
            httpd_resp_send_chunk(req, empty, sizeof(empty) - 1);
        }
    }

    // Close sysInfo and root
    snprintf(buf, sizeof(buf), "}}");
    httpd_resp_send_chunk(req, buf, strlen(buf));

    // End chunked response
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

httpd_uri_t get_sysinfo_json_handler = {
    .uri = "/sysinfo.json",
    .method = HTTP_GET,
    .handler = get_sysinfo_json_handler_func,
    .user_ctx = NULL};

esp_err_t post_ping_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    // Ping is an authenticated action: it sends ICMP from the device to an
    // arbitrary target (SSRF surface) and blocks an httpd worker for up to 4s.
    // Without this guard any LAN peer could probe internal/external hosts via
    // the device and exhaust the httpd worker pool. Mirrors the auth check on
    // the other state-changing POST handlers.
    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    char buf[128];
    if (req->content_len <= 0 || req->content_len >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too large");
        return ESP_FAIL;
    }

    const int ret = recv_full_body(req, buf, sizeof(buf));
    if (ret <= 0) {
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *target_json = cJSON_GetObjectItem(root, "target");
    const char *target = target_json && cJSON_IsString(target_json) ? target_json->valuestring : "";
    ip4_addr_t literal_addr = {};
    const bool literal_ipv4 = ip4addr_aton(target, &literal_addr) != 0;
    if (strlen(target) == 0 ||
        (!literal_ipv4 && !validateHostname(target))) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req,
            "{\"success\":false,\"error\":\"invalid_target\"}");
    }

    int latency = ping_service_ping(target, 4000); // 4 seconds timeout
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    char resp[128];
    if (latency >= 0) {
        snprintf(resp, sizeof(resp), "{\"success\":true,\"latency_ms\":%d}", latency);
    } else if (latency == PING_SERVICE_DNS_ERROR) {
        snprintf(resp, sizeof(resp),
                 "{\"success\":false,\"error\":\"dns_failed\"}");
    } else if (latency == PING_SERVICE_TIMEOUT) {
        snprintf(resp, sizeof(resp),
                 "{\"success\":false,\"error\":\"timeout\"}");
    } else {
        httpd_resp_set_status(req, "500 Internal Server Error");
        snprintf(resp, sizeof(resp),
                 "{\"success\":false,\"error\":\"internal\"}");
    }
    httpd_resp_sendstr(req, resp);
    return ESP_OK;
}

httpd_uri_t post_ping_handler = {
    .uri = "/api/ping",
    .method = HTTP_POST,
    .handler = post_ping_handler_func,
    .user_ctx = NULL};

void add_settings(cJSON *root)
{
    cJSON *settings = cJSON_AddObjectToObject(root, "settings");

    cJSON_AddStringToObject(settings, "hostname", _settings->getHostname());
    cJSON_AddStringToObject(settings, "adminUsername", _settings->getAdminUsername());

    cJSON_AddBoolToObject(settings, "useDHCP", _settings->getUseDHCP());

    ip4_addr_t currentIP, currentNetmask, currentGateway, currentDNS1, currentDNS2;
    _ethernet->getNetworkSettings(&currentIP, &currentNetmask, &currentGateway, &currentDNS1, &currentDNS2);
    cJSON_AddStringToObject(settings, "localIP", ip2str(_settings->getLocalIP(), currentIP));
    cJSON_AddStringToObject(settings, "netmask", ip2str(_settings->getNetmask(), currentNetmask));
    cJSON_AddStringToObject(settings, "gateway", ip2str(_settings->getGateway(), currentGateway));
    cJSON_AddStringToObject(settings, "dns1", ip2str(_settings->getDns1(), currentDNS1));
    cJSON_AddStringToObject(settings, "dns2", ip2str(_settings->getDns2(), currentDNS2));

    cJSON_AddNumberToObject(settings, "timesource", _settings->getTimesource());

    cJSON_AddNumberToObject(settings, "dcfOffset", _settings->getDcfOffset());

    cJSON_AddNumberToObject(settings, "gpsBaudrate", _settings->getGpsBaudrate());

    cJSON_AddStringToObject(settings, "ntpServer", _settings->getNtpServer());

    cJSON_AddNumberToObject(settings, "ledBrightness", _settings->getLEDBrightness());

    cJSON *ledPrograms = cJSON_AddObjectToObject(settings, "ledPrograms");
    cJSON_AddNumberToObject(ledPrograms, "idle", _settings->getLedProgram(LED_PROG_IDLE));
    cJSON_AddNumberToObject(ledPrograms, "ccu_disconnected", _settings->getLedProgram(LED_PROG_CCU_DISCONNECTED));
    cJSON_AddNumberToObject(ledPrograms, "ccu_connected", _settings->getLedProgram(LED_PROG_CCU_CONNECTED));
    cJSON_AddNumberToObject(ledPrograms, "update_available", _settings->getLedProgram(LED_PROG_UPDATE_AVAILABLE));
    cJSON_AddNumberToObject(ledPrograms, "error", _settings->getLedProgram(LED_PROG_ERROR));
    cJSON_AddNumberToObject(ledPrograms, "booting", _settings->getLedProgram(LED_PROG_BOOTING));
    cJSON_AddNumberToObject(ledPrograms, "update_in_progress", _settings->getLedProgram(LED_PROG_UPDATE_IN_PROGRESS));

    // IPv6 Settings
    cJSON_AddBoolToObject(settings, "enableIPv6", _settings->getEnableIPv6());
    cJSON_AddStringToObject(settings, "ipv6Mode", _settings->getIPv6Mode());
    cJSON_AddStringToObject(settings, "ipv6Address", _settings->getIPv6Address());
    cJSON_AddNumberToObject(settings, "ipv6PrefixLength", _settings->getIPv6PrefixLength());
    cJSON_AddStringToObject(settings, "ipv6Gateway", _settings->getIPv6Gateway());
    cJSON_AddStringToObject(settings, "ipv6Dns1", _settings->getIPv6Dns1());
    cJSON_AddStringToObject(settings, "ipv6Dns2", _settings->getIPv6Dns2());

    cJSON_AddStringToObject(settings, "ccuIP", _settings->getCCUIP());

    cJSON_AddBoolToObject(settings, "systemLogEnabled", _settings->getSystemLogEnabled());
    cJSON_AddBoolToObject(settings, "flashPause", _settings->getFlashPause());
    cJSON_AddBoolToObject(settings, "testDesignEnabled", _settings->getTestDesignEnabled());
}

esp_err_t get_settings_json_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    cJSON *root = cJSON_CreateObject();

    add_settings(root);

    const char *json = cJSON_PrintUnformatted(root);
    if (json) {
        httpd_resp_sendstr(req, json);
        free((void *)json);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON alloc failed");
    }
    cJSON_Delete(root);

    return ESP_OK;
}

httpd_uri_t get_settings_json_handler = {
    .uri = "/settings.json",
    .method = HTTP_GET,
    .handler = get_settings_json_handler_func,
    .user_ctx = NULL};

ip4_addr_t cJSON_GetIPAddrValue(const cJSON *item)
{
    ip4_addr_t res{.addr = IPADDR_ANY};

    if (item && cJSON_IsString(item))
    {
        ip4addr_aton(item->valuestring, &res);
    }

    return res;
}

bool cJSON_GetBoolValue(const cJSON *item)
{
    return (item && cJSON_IsBool(item)) ? cJSON_IsTrue(item) : false;
}

esp_err_t send_json_error(httpd_req_t *req, const char *status,
                          const char *code, const char *field)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "JSON allocation failed");
    }
    cJSON_AddBoolToObject(root, "success", false);
    cJSON_AddStringToObject(root, "error", code);
    if (field) cJSON_AddStringToObject(root, "field", field);
    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "JSON allocation failed");
    }
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    const esp_err_t result = httpd_resp_sendstr(req, body);
    free(body);
    return result;
}

bool valid_admin_username(const char *value)
{
    if (!value) return false;
    const size_t length = strlen(value);
    if (length < 1 || length > 32) return false;
    for (size_t index = 0; index < length; ++index) {
        const char c = value[index];
        if (!isalnum(static_cast<unsigned char>(c)) &&
            c != '-' && c != '_' && c != '.') {
            return false;
        }
    }
    return true;
}

bool parse_ipv4_json(const cJSON *item, bool optional,
                     ip4_addr_t *address)
{
    if (!item || !cJSON_IsString(item) || !address) return false;
    const char *value = cJSON_GetStringValue(item);
    if (optional && value && value[0] == '\0') {
        address->addr = IPADDR_ANY;
        return true;
    }
    return value && ip4addr_aton(value, address) != 0;
}

bool refresh_restart_sync_from_settings()
{
    const bool enabled = _settings && _settings->getFlashPause();
    set_flash_pause_enabled(enabled);
    return enabled;
}

esp_err_t post_settings_json_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    // Keep the 8 KiB request limit, but allocate only what this request needs.
    // A normal settings payload is ~700 bytes; reserving a fixed contiguous
    // 8 KiB block made saves fail on fragmented/low-memory ESP32 devices even
    // though the body itself was small.
    constexpr size_t maxSettingsBodySize = 8192;
    if (req->content_len == 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data received");
    }
    if (req->content_len >= maxSettingsBodySize) {
        return httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Request body too large");
    }

    const size_t bufferSize = req->content_len + 1;
    char *buffer = (char *)malloc(bufferSize);
    if (!buffer) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
    }

    int len = recv_full_body(req, buffer, bufferSize);

    if (len < 0) {
        free(buffer);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive request body");
    }

    cJSON *root = cJSON_Parse(buffer);
    free(buffer);
    buffer = NULL;

    if (!root) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    char *hostname = cJSON_GetStringValue(cJSON_GetObjectItem(root, "hostname"));
    // Presence guard mirrors the IPv6 fields below: without it,
    // cJSON_GetBoolValue silently returns false on a missing key, and a
    // partial payload reaching setNetworkSettings() would turn DHCP off.
    // buildSettingsPayload() always sends useDHCP so this rarely matters,
    // but the guard makes the contract explicit and future-proof.
    bool useDHCP = cJSON_GetObjectItem(root, "useDHCP")
                       ? cJSON_GetBoolValue(cJSON_GetObjectItem(root, "useDHCP"))
                       : _settings->getUseDHCP();
    ip4_addr_t localIP = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "localIP"));
    ip4_addr_t netmask = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "netmask"));
    ip4_addr_t gateway = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "gateway"));
    ip4_addr_t dns1 = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "dns1"));
    ip4_addr_t dns2 = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "dns2"));

    timesource_t timesource = (timesource_t)cJSON_GetIntValueSafe(
        cJSON_GetObjectItem(root, "timesource"), (int)_settings->getTimesource());

    int dcfOffset = cJSON_GetIntValueSafe(
        cJSON_GetObjectItem(root, "dcfOffset"), _settings->getDcfOffset());

    int gpsBaudrate = cJSON_GetIntValueSafe(
        cJSON_GetObjectItem(root, "gpsBaudrate"), _settings->getGpsBaudrate());

    char *ntpServer = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ntpServer"));

    int ledBrightness = cJSON_GetIntValueSafe(
        cJSON_GetObjectItem(root, "ledBrightness"), _settings->getLEDBrightness());
    cJSON *ledPrograms = cJSON_GetObjectItem(root, "ledPrograms");

    // IPv6
    bool enableIPv6 = cJSON_GetBoolValue(cJSON_GetObjectItem(root, "enableIPv6"));
    char *ipv6Mode = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Mode"));
    char *ipv6Address = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Address"));
    int ipv6PrefixLength = cJSON_GetIntValueSafe(
        cJSON_GetObjectItem(root, "ipv6PrefixLength"), 64);
    char *ipv6Gateway = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Gateway"));
    char *ipv6Dns1 = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Dns1"));
    char *ipv6Dns2 = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Dns2"));

    char *ccuIP = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ccuIP"));

    char *adminUsername = cJSON_GetStringValue(cJSON_GetObjectItem(root, "adminUsername"));
    char *adminPassword = cJSON_GetStringValue(cJSON_GetObjectItem(root, "adminPassword"));
    char *currentPassword = cJSON_GetStringValue(
        cJSON_GetObjectItem(root, "currentPassword"));

    // Validate the complete request before changing any live setting. This
    // prevents a later invalid NTP/network value from producing a misleading
    // success response after earlier fields were already applied.
    if (adminUsername && !valid_admin_username(adminUsername)) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request", "invalid_username",
                               "adminUsername");
    }
    bool admin_password_change_requested = false;
    if (adminPassword && adminPassword[0] != '\0') {
        // A payload may echo the already-active password. Treat that as an
        // unchanged field (including legacy/default passwords) and neither
        // demand re-authentication nor rotate the session token.
        if (strlen(adminPassword) >= 33) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_password",
                                   "adminPassword");
        }
        admin_password_change_requested =
            secure_strcmp(adminPassword,
                          _settings->getAdminPassword()) != 0;
        if (admin_password_change_requested &&
            !validateAdminPassword(adminPassword)) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_password",
                                   "adminPassword");
        }
    }
    if (admin_password_change_requested) {
        if (currentPassword == NULL || currentPassword[0] == '\0') {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request",
                                   "current_password_required",
                                   "currentPassword");
        }
        if (secure_strcmp(currentPassword,
                          _settings->getAdminPassword()) != 0) {
            cJSON_Delete(root);
            return send_json_error(req, "403 Forbidden",
                                   "current_password_incorrect",
                                   "currentPassword");
        }
    }
    if (hostname && !validateHostname(hostname)) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request", "invalid_hostname",
                               "hostname");
    }
    if (hostname && !useDHCP) {
        const bool local_ok = parse_ipv4_json(
            cJSON_GetObjectItem(root, "localIP"), false, &localIP);
        const bool netmask_ok = parse_ipv4_json(
            cJSON_GetObjectItem(root, "netmask"), false, &netmask);
        const bool gateway_ok = parse_ipv4_json(
            cJSON_GetObjectItem(root, "gateway"), false, &gateway);
        if (!local_ok || localIP.addr == IPADDR_ANY ||
            !validateIPAddress(localIP)) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_ipv4",
                                   "localIP");
        }
        if (!netmask_ok || netmask.addr == IPADDR_ANY ||
            !validateNetmask(netmask)) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_netmask",
                                   "netmask");
        }
        if (!gateway_ok || !validateIPAddress(gateway)) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_ipv4",
                                   "gateway");
        }
    }
    if (hostname) {
        const bool dns1_ok = parse_ipv4_json(
            cJSON_GetObjectItem(root, "dns1"), true, &dns1);
        const bool dns2_ok = parse_ipv4_json(
            cJSON_GetObjectItem(root, "dns2"), true, &dns2);
        if (!dns1_ok || (dns1.addr != IPADDR_ANY &&
                        !validateIPAddress(dns1))) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_ipv4",
                                   "dns1");
        }
        if (!dns2_ok || (dns2.addr != IPADDR_ANY &&
                        !validateIPAddress(dns2))) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_ipv4",
                                   "dns2");
        }
    }
    if (timesource < TIMESOURCE_NTP || timesource > TIMESOURCE_GPS) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request", "invalid_time_source",
                               "timesource");
    }
    if (ntpServer && !validateNtpServer(ntpServer)) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request", "invalid_ntp_server",
                               "ntpServer");
    }
    if (!validateDcfOffset(dcfOffset)) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request", "invalid_dcf_offset",
                               "dcfOffset");
    }
    if (!validateGpsBaudrate(gpsBaudrate)) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request", "invalid_gps_baudrate",
                               "gpsBaudrate");
    }
    if (!validateLEDBrightness(ledBrightness)) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request", "invalid_led_brightness",
                               "ledBrightness");
    }
    if (ccuIP && ccuIP[0] != '\0' && !validateCcuAddress(ccuIP)) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request", "invalid_ccu_address",
                               "ccuIP");
    }

    const bool has_ipv6 = cJSON_GetObjectItem(root, "enableIPv6") ||
        cJSON_GetObjectItem(root, "ipv6Mode") ||
        cJSON_GetObjectItem(root, "ipv6Address") ||
        cJSON_GetObjectItem(root, "ipv6PrefixLength") ||
        cJSON_GetObjectItem(root, "ipv6Gateway") ||
        cJSON_GetObjectItem(root, "ipv6Dns1") ||
        cJSON_GetObjectItem(root, "ipv6Dns2");
    if (has_ipv6) {
        const char *effective_mode = ipv6Mode ? ipv6Mode : _settings->getIPv6Mode();
        const char *effective_address = ipv6Address ? ipv6Address : _settings->getIPv6Address();
        if (!effective_mode ||
            (strcmp(effective_mode, "auto") != 0 &&
             strcmp(effective_mode, "static") != 0 &&
             strcmp(effective_mode, "disabled") != 0)) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_ipv6_mode",
                                   "ipv6Mode");
        }
        if (ipv6PrefixLength < 1 || ipv6PrefixLength > 128) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_ipv6_prefix",
                                   "ipv6PrefixLength");
        }
        if (enableIPv6 && strcmp(effective_mode, "static") == 0 &&
            (!effective_address || effective_address[0] == '\0' ||
             !validateIPv6Address(effective_address))) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_ipv6",
                                   "ipv6Address");
        }
        const struct {
            const char *value;
            const char *field;
        } optional_ipv6[] = {
            {ipv6Gateway, "ipv6Gateway"},
            {ipv6Dns1, "ipv6Dns1"},
            {ipv6Dns2, "ipv6Dns2"},
        };
        for (const auto &entry : optional_ipv6) {
            if (entry.value && entry.value[0] != '\0' &&
                !validateIPv6Address(entry.value)) {
                cJSON_Delete(root);
                return send_json_error(req, "400 Bad Request", "invalid_ipv6",
                                       entry.field);
            }
        }
    }

    std::unique_ptr<settings_snapshot_t> previous_settings(
        new (std::nothrow) settings_snapshot_t{});
    if (!previous_settings) {
        cJSON_Delete(root);
        return send_json_error(req, "500 Internal Server Error",
                               "settings_snapshot_allocation", "settings");
    }
    _settings->snapshot(previous_settings.get());

    if (adminUsername && adminUsername[0] != '\0') {
        if (!_settings->setAdminUsername(adminUsername)) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_username",
                                   "adminUsername");
        }
    }

    if (admin_password_change_requested) {
        const esp_err_t token_result = rotate_admin_token();
        if (token_result != ESP_OK) {
            _settings->restoreSnapshot(previous_settings.get());
            ESP_LOGE(TAG, "Settings password token rotation failed: %s",
                     esp_err_to_name(token_result));
            cJSON_Delete(root);
            return send_json_error(req, "500 Internal Server Error",
                                   "token_rotation_failed", "adminPassword");
        }
        if (!_settings->setAdminPassword(adminPassword)) {
            _settings->restoreSnapshot(previous_settings.get());
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_password",
                                   "adminPassword");
        }
    }

    if (hostname) {
        if (!_settings->setNetworkSettings(hostname, useDHCP, localIP, netmask, gateway, dns1, dns2)) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_network",
                                   "network");
        }
    }
    _settings->setTimesource(timesource);
    _settings->setDcfOffset(dcfOffset);
    _settings->setGpsBaudrate(gpsBaudrate);
    if (ntpServer) {
        _settings->setNtpServer(ntpServer);
    }
    _settings->setLEDBrightness(ledBrightness);

    if (ledPrograms) {
        const struct {
            const char *key;
            led_program_t program;
        } programs[] = {
            {"idle", LED_PROG_IDLE},
            {"ccu_disconnected", LED_PROG_CCU_DISCONNECTED},
            {"ccu_connected", LED_PROG_CCU_CONNECTED},
            {"update_available", LED_PROG_UPDATE_AVAILABLE},
            {"error", LED_PROG_ERROR},
            {"booting", LED_PROG_BOOTING},
            {"update_in_progress", LED_PROG_UPDATE_IN_PROGRESS},
        };
        for (const auto &program : programs) {
            const int value = cJSON_GetIntValueSafe(
                cJSON_GetObjectItem(ledPrograms, program.key), -1);
            if (value >= 0 && value <= 10) {
                _settings->setLedProgram(program.program, value);
            }
        }
    }

    // Handle IPv6 when any IPv6-related field is present so toggling
    // enableIPv6 to false without sending ipv6Mode still persists.
    if (has_ipv6) {
        _settings->setIPv6Settings(
            cJSON_GetObjectItem(root, "enableIPv6") ? enableIPv6 : _settings->getEnableIPv6(),
            ipv6Mode ? ipv6Mode : _settings->getIPv6Mode(),
            ipv6Address ? ipv6Address : _settings->getIPv6Address(),
            cJSON_GetObjectItem(root, "ipv6PrefixLength") ? ipv6PrefixLength : _settings->getIPv6PrefixLength(),
            ipv6Gateway ? ipv6Gateway : _settings->getIPv6Gateway(),
            ipv6Dns1 ? ipv6Dns1 : _settings->getIPv6Dns1(),
            ipv6Dns2 ? ipv6Dns2 : _settings->getIPv6Dns2()
        );
    }

    if (ccuIP) {
        _settings->setCCUIP(ccuIP);
    }

    cJSON *systemLogEnabledItem = cJSON_GetObjectItem(root, "systemLogEnabled");
    if (systemLogEnabledItem && cJSON_IsBool(systemLogEnabledItem)) {
        _settings->setSystemLogEnabled(cJSON_IsTrue(systemLogEnabledItem));
    }

    cJSON *flashPauseItem = cJSON_GetObjectItem(root, "flashPause");
    if (flashPauseItem && cJSON_IsBool(flashPauseItem)) {
        _settings->setFlashPause(cJSON_IsTrue(flashPauseItem));
    }
    cJSON *testDesignItem = cJSON_GetObjectItem(root, "testDesignEnabled");
    if (testDesignItem && cJSON_IsBool(testDesignItem)) {
        _settings->setTestDesignEnabled(cJSON_IsTrue(testDesignItem));
    }

    const esp_err_t settings_result = _settings->save();
    if (settings_result != ESP_OK) {
        _settings->restoreSnapshot(previous_settings.get());
        const esp_err_t rollback_result = _settings->save();
        if (rollback_result != ESP_OK) {
            ESP_LOGE(TAG, "Could not roll back failed settings save: %s",
                     esp_err_to_name(rollback_result));
        }
        cJSON_Delete(root);
        return send_json_error(req, "507 Insufficient Storage",
                               "settings_persist_failed", "settings");
    }

    // Apply visible/runtime side effects only after durable persistence.
    if (cJSON_GetObjectItem(root, "ledBrightness") != NULL) {
        LED::setBrightness(ledBrightness);
    }
    if (ledPrograms) {
        const struct {
            const char *key;
            led_program_t program;
        } programs[] = {
            {"idle", LED_PROG_IDLE},
            {"ccu_disconnected", LED_PROG_CCU_DISCONNECTED},
            {"ccu_connected", LED_PROG_CCU_CONNECTED},
            {"update_available", LED_PROG_UPDATE_AVAILABLE},
            {"error", LED_PROG_ERROR},
            {"booting", LED_PROG_BOOTING},
            {"update_in_progress", LED_PROG_UPDATE_IN_PROGRESS},
        };
        for (const auto &program : programs) {
            const int value = cJSON_GetIntValueSafe(
                cJSON_GetObjectItem(ledPrograms, program.key), -1);
            if (value >= 0 && value <= 10) {
                LED::setProgram(program.program, (led_state_t)value);
            }
        }
    }
    if (systemLogEnabledItem && cJSON_IsBool(systemLogEnabledItem)) {
        if (cJSON_IsTrue(systemLogEnabledItem)) {
            LogManager::begin();
            if (LogManager::instance().isEnabled()) {
                emit_log_enable_snapshot();
            }
        } else {
            LogManager::stop();
        }
    }
    if (flashPauseItem && cJSON_IsBool(flashPauseItem)) {
        set_flash_pause_enabled(cJSON_IsTrue(flashPauseItem));
    }

    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\":true,\"message\":\"Settings saved.\"}");

    return ESP_OK;
}

httpd_uri_t post_settings_json_handler = {
    .uri = "/settings.json",
    .method = HTTP_POST,
    .handler = post_settings_json_handler_func,
    .user_ctx = NULL};

esp_err_t post_change_password_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK)
    {
        return send_json_error(req, "401 Unauthorized", "unauthorized");
    }

    char buffer[512];
    int len = recv_full_body(req, buffer, sizeof(buffer));

    if (len <= 0)
    {
        return send_json_error(req, "400 Bad Request", "invalid_request");
    }

    cJSON *root = cJSON_Parse(buffer);
    if (root == NULL)
    {
        return send_json_error(req, "400 Bad Request", "invalid_request");
    }

    char *currentPassword = cJSON_GetStringValue(cJSON_GetObjectItem(root, "currentPassword"));
    char *newPassword = cJSON_GetStringValue(cJSON_GetObjectItem(root, "newPassword"));

    // Require the current password for re-authentication so a briefly
    // unlocked session cannot be used to change the password permanently.
    if (currentPassword == NULL || currentPassword[0] == '\0')
    {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request",
                               "current_password_required",
                               "currentPassword");
    }
    if (secure_strcmp(currentPassword, _settings->getAdminPassword()) != 0)
    {
        cJSON_Delete(root);
        return send_json_error(req, "403 Forbidden",
                               "current_password_incorrect",
                               "currentPassword");
    }

    if (!validateAdminPassword(newPassword))
    {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request",
                               "invalid_new_password", "newPassword");
    }

    settings_snapshot_t previous_settings = {};
    _settings->snapshot(&previous_settings);
    const esp_err_t token_result = rotate_admin_token();
    if (token_result != ESP_OK) {
        ESP_LOGE(TAG, "Password-change token rotation failed: %s",
                 esp_err_to_name(token_result));
        cJSON_Delete(root);
        return send_json_error(req, "500 Internal Server Error",
                               "token_rotation_failed", "newPassword");
    }
    if (!_settings->setAdminPassword(newPassword))
    {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request",
                               "invalid_new_password", "newPassword");
    }
    if (_settings->save() != ESP_OK) {
        _settings->restoreSnapshot(&previous_settings);
        (void)_settings->save();
        cJSON_Delete(root);
        return send_json_error(req, "507 Insufficient Storage",
                               "password_persist_failed", "newPassword");
    }

    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "token", _token);

    const char *json = cJSON_PrintUnformatted(response);
    if (json) {
        httpd_resp_sendstr(req, json);
        free((void *)json);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON alloc failed");
    }
    cJSON_Delete(response);

    ESP_LOGI(TAG, "Admin password changed successfully");

    return ESP_OK;
}

httpd_uri_t post_change_password_handler = {
    .uri = "/api/change-password",
    .method = HTTP_POST,
    .handler = post_change_password_handler_func,
    .user_ctx = NULL};


esp_err_t get_log_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    uint64_t offset = 0;
    char query[32];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char param[24];
        if (httpd_query_key_value(query, "offset", param, sizeof(param)) == ESP_OK) {
            offset = strtoull(param, NULL, 10);
        }
    }

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");

    // Body and absolute end offset come from one locked snapshot. A log line
    // arriving between two independent reads must not make the client skip or
    // duplicate bytes on its next request.
    uint64_t totalWritten = 0;
    std::string content = LogManager::instance().getLogSnapshot(offset, &totalWritten);
    char totalWrittenStr[24];
    snprintf(totalWrittenStr, sizeof(totalWrittenStr), "%" PRIu64, totalWritten);
    httpd_resp_set_hdr(req, "X-Log-Total", totalWrittenStr);

    httpd_resp_send(req, content.c_str(), content.length());

    return ESP_OK;
}

httpd_uri_t get_log_handler = {
    .uri = "/api/log",
    .method = HTTP_GET,
    .handler = get_log_handler_func,
    .user_ctx = NULL};

// GET /api/log/status - whether the in-memory log ring buffer is active.
// When enabled from the WebUI, the preference is persisted and capture starts
// again on the next boot.
esp_err_t get_log_status_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    char body[64];
    snprintf(body, sizeof(body), "{\"enabled\":%s,\"persistent\":%s}",
             LogManager::instance().isEnabled() ? "true" : "false",
             (_settings && _settings->getSystemLogEnabled()) ? "true" : "false");
    httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_uri_t get_log_status_handler = {
    .uri = "/api/log/status",
    .method = HTTP_GET,
    .handler = get_log_status_handler_func,
    .user_ctx = NULL};

// /api/crash_log — returns the persisted log tail captured just before the
// last watchdog/panic restart (see LogManager::saveCrashTailNvs). Returns a
// JSON object {"available":bool, "tail":string}. The snapshot is cleared
// from NVS after the first successful read so a normal reboot does not show
// stale data. This is the answer to "the user has no log because the device
// restarted": the few hundred bytes that matter now survive the reboot.
esp_err_t get_crash_log_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    std::string tail = LogManager::loadCrashTailNvs();
    httpd_resp_set_type(req, "application/json");

    if (tail.empty()) {
        httpd_resp_sendstr(req, "{\"available\":false,\"tail\":\"\"}");
        return ESP_OK;
    }

    // JSON-escape the tail. It is plain text from the log ring buffer, so
    // backslash and double-quote must be escaped and control chars dropped.
    std::string esc;
    esc.reserve(tail.size() + 16);
    for (char c : tail) {
        switch (c) {
            case '\\': esc += "\\\\"; break;
            case '"':  esc += "\\\""; break;
            case '\r': break;  // ignore
            case '\n': esc += "\\n"; break;
            case '\t': esc += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) break;  // drop other ctrl
                esc += c;
        }
    }

    std::string body;
    body.reserve(esc.size() + 32);
    body += "{\"available\":true,\"tail\":\"";
    body += esc;
    body += "\"}";
    httpd_resp_send(req, body.data(), (ssize_t)body.size());

    // Best-effort erase: the snapshot has now been delivered to the UI; we
    // do not want it to reappear on every reload.
    NvsStorageLock storage_lock(portMAX_DELAY, "webui.crash_tail_erase");
    if (storage_lock) {
        nvs_handle_t h;
        if (nvs_open("reset_info", NVS_READWRITE, &h) == ESP_OK) {
            nvs_erase_key(h, "clog");
            nvs_commit(h);
            nvs_close(h);
        }
    }
    return ESP_OK;
}

httpd_uri_t get_crash_log_handler = {
    .uri = "/api/crash_log",
    .method = HTTP_GET,
    .handler = get_crash_log_handler_func,
    .user_ctx = NULL};

static void emit_log_enable_snapshot()
{
    ESP_LOGI(TAG, "System log capture enabled via WebUI");
    ESP_LOGW(TAG, "Boot logs before activation are not available because the log buffer is allocated on demand");

    if (_sysInfo) {
        ESP_LOGI(TAG, "Snapshot: version=%s, serial=%s, uptime=%lus, free_heap=%u",
                 _sysInfo->getCurrentVersion(),
                 _sysInfo->getSerialNumber(),
                 (unsigned long)_sysInfo->getUptimeSeconds(),
                 (unsigned int)esp_get_free_heap_size());
        ESP_LOGI(TAG, "Snapshot: cpu=%.0f%%, memory=%.0f%%, reset_reason=%s",
                 _sysInfo->getCpuUsage(),
                 _sysInfo->getMemoryUsage(),
                 _sysInfo->getResetReason());
    } else {
        ESP_LOGW(TAG, "Snapshot: system info unavailable");
    }

    if (_ethernet) {
        ip4_addr_t ip, nm, gw, dns1, dns2;
        _ethernet->getNetworkSettings(&ip, &nm, &gw, &dns1, &dns2);
        char linkBuf[40] = "";
        if (_ethernet->isConnected()) {
            snprintf(linkBuf, sizeof(linkBuf), ", speed=%dMbps, duplex=%s",
                     _ethernet->getLinkSpeedMbps(),
                     _ethernet->getDuplexMode());
        }
        ip4_addr_t unsetIp = {};
        unsetIp.addr = IPADDR_ANY;
        ESP_LOGI(TAG, "Snapshot: ethernet=%s%s, ip=%s, gateway=%s, dns1=%s",
                 _ethernet->isConnected() ? "connected" : "disconnected",
                 linkBuf,
                 ip2str(_settings ? _settings->getLocalIP() : unsetIp, ip),
                 ip2str(_settings ? _settings->getGateway() : unsetIp, gw),
                 ip2str(_settings ? _settings->getDns1() : unsetIp, dns1));
    } else {
        ESP_LOGW(TAG, "Snapshot: ethernet unavailable");
    }

    if (_radioModuleDetector) {
        const char *typeStr = "unknown";
        switch (_radioModuleDetector->getRadioModuleType()) {
            case RADIO_MODULE_HM_MOD_RPI_PCB: typeStr = "HM-MOD-RPI-PCB"; break;
            case RADIO_MODULE_RPI_RF_MOD:     typeStr = "RPI-RF-MOD";     break;
            case RADIO_MODULE_HMIP_RFUSB:     typeStr = "HMIP-RFUSB";     break;
            default: break;
        }

        const uint8_t *fw = _radioModuleDetector->getFirmwareVersion();
        ESP_LOGI(TAG, "Snapshot: radio=%s, serial=%s, firmware=%u.%u.%u, bidcos=0x%06" PRIX32 ", hmip=0x%06" PRIX32,
                 typeStr,
                 _radioModuleDetector->getSerial(),
                 fw[0], fw[1], fw[2],
                 _radioModuleDetector->getBidCosRadioMAC(),
                 _radioModuleDetector->getHmIPRadioMAC());
    } else {
        ESP_LOGW(TAG, "Snapshot: radio module detector unavailable");
    }

    if (_rawUartUdpListener) {
        ip4_addr_t ccuAddr = _rawUartUdpListener->getConnectedRemoteAddress();
        ESP_LOGI(TAG, "Snapshot: ccu_connected=%s%s%s",
                 ccuAddr.addr != IPADDR_ANY ? "yes" : "no",
                 ccuAddr.addr != IPADDR_ANY ? ", ccu_address=" : "",
                 ccuAddr.addr != IPADDR_ANY ? ip2str(ccuAddr) : "");
    }
}

// POST /api/log/enable - allocate the ring buffer and start capturing logs.
esp_err_t post_log_enable_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    // Drain any (empty) request body so keep-alive stays consistent.
    if (req->content_len > 0) {
        char discard[64];
        size_t remaining = req->content_len;
        while (remaining > 0) {
            int n = httpd_req_recv(req, discard, remaining < sizeof(discard) ? remaining : sizeof(discard));
            if (n <= 0) break;
            remaining -= (size_t)n;
        }
    }

    const bool previous_enabled =
        _settings ? _settings->getSystemLogEnabled() : false;
    LogManager::begin();
    if (LogManager::instance().isEnabled()) {
        if (_settings) {
            _settings->setSystemLogEnabled(true);
            if (_settings->save() != ESP_OK) {
                _settings->setSystemLogEnabled(previous_enabled);
                (void)_settings->save();
                if (!previous_enabled) LogManager::stop();
                return httpd_resp_send_err(
                    req, HTTPD_500_INTERNAL_SERVER_ERROR,
                    "Log setting could not be persisted");
            }
        }
        emit_log_enable_snapshot();
    }
    httpd_resp_set_type(req, "application/json");
    char body[32];
    snprintf(body, sizeof(body), "{\"enabled\":%s}",
             LogManager::instance().isEnabled() ? "true" : "false");
    httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_uri_t post_log_enable_handler = {
    .uri = "/api/log/enable",
    .method = HTTP_POST,
    .handler = post_log_enable_handler_func,
    .user_ctx = NULL};

// POST /api/log/disable - free the ring buffer and stop capturing.
esp_err_t post_log_disable_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    if (req->content_len > 0) {
        char discard[64];
        size_t remaining = req->content_len;
        while (remaining > 0) {
            int n = httpd_req_recv(req, discard, remaining < sizeof(discard) ? remaining : sizeof(discard));
            if (n <= 0) break;
            remaining -= (size_t)n;
        }
    }

    const bool previous_enabled =
        _settings ? _settings->getSystemLogEnabled() : false;
    LogManager::stop();
    if (_settings) {
        _settings->setSystemLogEnabled(false);
        if (_settings->save() != ESP_OK) {
            _settings->setSystemLogEnabled(previous_enabled);
            (void)_settings->save();
            if (previous_enabled) LogManager::begin();
            return httpd_resp_send_err(
                req, HTTPD_500_INTERNAL_SERVER_ERROR,
                "Log setting could not be persisted");
        }
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"enabled\":false}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_uri_t post_log_disable_handler = {
    .uri = "/api/log/disable",
    .method = HTTP_POST,
    .handler = post_log_disable_handler_func,
    .user_ctx = NULL};

esp_err_t get_log_download_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"hb-rf-eth-log.txt\"");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");

    const uint64_t snapshot_end = LogManager::instance().getTotalWritten();
    uint64_t absolute_offset = 0;
    constexpr size_t CHUNK_SIZE = 1024;
    char *chunk = static_cast<char *>(malloc(CHUNK_SIZE));
    if (!chunk)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Could not allocate log download buffer");
    }

    esp_err_t result = ESP_OK;
    while (absolute_offset < snapshot_end)
    {
        const uint64_t remaining = snapshot_end - absolute_offset;
        const size_t requested = remaining < CHUNK_SIZE
            ? static_cast<size_t>(remaining)
            : CHUNK_SIZE;
        const size_t count = LogManager::instance().readChunk(
            &absolute_offset, chunk, requested);
        if (count == 0) break;

        result = httpd_resp_send_chunk(req, chunk, count);
        if (result != ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    free(chunk);
    if (result == ESP_OK)
    {
        result = httpd_resp_send_chunk(req, nullptr, 0);
    }
    return result;
}

httpd_uri_t get_log_download_handler = {
    .uri = "/api/log/download",
    .method = HTTP_GET,
    .handler = get_log_download_handler_func,
    .user_ctx = NULL};

// Prometheus metrics disabled - feature code available in prometheus.cpp.disabled

WebUI::WebUI(Settings *settings, LED *statusLED, SysInfo *sysInfo, Ethernet *ethernet, RawUartUdpListener *rawUartUdpListener, RadioModuleConnector *radioModuleConnector, RadioModuleDetector *radioModuleDetector)
{
    _settings = settings;
    _statusLED = statusLED;
    _sysInfo = sysInfo;
    _ethernet = ethernet;
    _rawUartUdpListener = rawUartUdpListener;
    _radioModuleConnector = radioModuleConnector;
    _radioModuleDetector = radioModuleDetector;

    generateToken();
}

void WebUI::start()
{
    // Initialize rate limiter
    rate_limiter_init();

    // Suppress noisy httpd warnings
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);
    // Certificate validation per-request log (happens 3+ times per boot)
    esp_log_level_set("esp-x509-crt-bundle", ESP_LOG_WARN);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    // Reserve capacity for modular diagnostics/theme APIs.
    config.max_uri_handlers = 44;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.close_fn = log_stream_close_socket;
    // Increase stack: POST handlers allocate content buffers + config structs
    // that together exceed the default 4096-byte stack, causing overflow/corruption.
    config.stack_size = 8192;

    _httpd_handle = NULL;

    if (httpd_start(&_httpd_handle, &config) == ESP_OK)
    {
        // Make the WebSocket log stream available as soon as the server starts
        // so subscribers can connect before any monitoring backend is enabled.
        log_stream_init();
        httpd_register_uri_handler(_httpd_handle, &log_stream_ws_uri);
        system_overview_api_register(_httpd_handle);
        theme_api_register(_httpd_handle);

        httpd_register_uri_handler(_httpd_handle, &post_login_json_handler);
        httpd_register_uri_handler(_httpd_handle, &post_password_reset_start_handler);
        httpd_register_uri_handler(_httpd_handle, &post_password_reset_status_handler);
        httpd_register_uri_handler(_httpd_handle, &post_password_reset_complete_handler);
        httpd_register_uri_handler(_httpd_handle, &get_sysinfo_json_handler);
        httpd_register_uri_handler(_httpd_handle, &get_settings_json_handler);
        httpd_register_uri_handler(_httpd_handle, &post_settings_json_handler);
        httpd_register_uri_handler(_httpd_handle, &post_ota_update_handler);
        httpd_register_uri_handler(_httpd_handle, &post_restart_handler);
        httpd_register_uri_handler(_httpd_handle, &post_factory_reset_handler);
        httpd_register_uri_handler(_httpd_handle, &get_ota_status_handler);
        httpd_register_uri_handler(_httpd_handle, &post_change_password_handler);
        httpd_register_uri_handler(_httpd_handle, &get_monitoring_handler);
        httpd_register_uri_handler(_httpd_handle, &post_monitoring_handler);
        httpd_register_uri_handler(_httpd_handle, &get_monitoring_test_handler);
        
        httpd_register_uri_handler(_httpd_handle, &post_ping_handler);

        httpd_register_uri_handler(_httpd_handle, &get_backup_handler);
        httpd_register_uri_handler(_httpd_handle, &post_restore_handler);
        httpd_register_uri_handler(_httpd_handle, &get_log_handler);
        httpd_register_uri_handler(_httpd_handle, &get_log_status_handler);
        httpd_register_uri_handler(_httpd_handle, &post_log_enable_handler);
        httpd_register_uri_handler(_httpd_handle, &post_log_disable_handler);
        httpd_register_uri_handler(_httpd_handle, &get_log_download_handler);
        httpd_register_uri_handler(_httpd_handle, &get_crash_log_handler);

        httpd_register_uri_handler(_httpd_handle, &main_js_gz_handler);
        httpd_register_uri_handler(_httpd_handle, &main_css_gz_handler);
        httpd_register_uri_handler(_httpd_handle, &favicon_ico_gz_handler);
        httpd_register_uri_handler(_httpd_handle, &index_html_gz_handler);
        // PWA assets
        httpd_register_uri_handler(_httpd_handle, &manifest_webmanifest_gz_handler);
        httpd_register_uri_handler(_httpd_handle, &icon_256_png_gz_handler);
    }
}

void WebUI::stop()
{
    httpd_stop(_httpd_handle);
}
