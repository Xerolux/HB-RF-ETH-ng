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
static inline int cJSON_GetIntValueSafe(cJSON *item, int defaultValue)
{
    return (item && cJSON_IsNumber(item)) ? item->valueint : defaultValue;
}

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

static esp_err_t rotate_admin_token()
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
    {
        std::string stacks = _sysInfo->getTaskStackInfo();
        char stack_buf[640];
        size_t pos = 0;
        stack_buf[0] = 0;
        pos += snprintf(stack_buf + pos, sizeof(stack_buf) - pos,
                        ",\"taskStacks\":\"");
        for (const char *p = stacks.c_str(); *p && pos + 4 < sizeof(stack_buf); p++) {
            if (*p == '"' || *p == '\\') {
                stack_buf[pos++] = '\\';
            }
            stack_buf[pos++] = *p;
        }
        stack_buf[pos++] = '"';
        stack_buf[pos] = 0;
        httpd_resp_send_chunk(req, stack_buf, pos);
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

static esp_err_t send_json_error(httpd_req_t *req, const char *status,
                                 const char *code, const char *field = NULL)
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

static bool valid_admin_username(const char *value)
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

static bool parse_ipv4_json(const cJSON *item, bool optional,
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

static bool refresh_restart_sync_from_settings()
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

static bool backup_get_bool(const cJSON *object, const char *key, bool *value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsBool(item) || !value) return false;
    *value = cJSON_IsTrue(item);
    return true;
}

static bool backup_get_uint(const cJSON *object, const char *key,
                            unsigned maximum, unsigned *value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsNumber(item) || !value ||
        item->valuedouble < 0 || item->valuedouble > maximum ||
        item->valuedouble != item->valueint) {
        return false;
    }
    *value = static_cast<unsigned>(item->valueint);
    return true;
}

static bool backup_get_string(const cJSON *object, const char *key,
                              char *value, size_t value_size)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsString(item) || !item->valuestring ||
        !value || value_size == 0 ||
        strlen(item->valuestring) >= value_size) {
        return false;
    }
    snprintf(value, value_size, "%s", item->valuestring);
    return true;
}

static esp_err_t add_monitoring_backup(cJSON *root)
{
    monitoring_config_t *config =
        new (std::nothrow) monitoring_config_t();
    if (!config) return ESP_ERR_NO_MEM;
    if (monitoring_get_config(config) != ESP_OK) {
        delete config;
        return ESP_ERR_INVALID_STATE;
    }

    cJSON *monitoring = cJSON_AddObjectToObject(root, "monitoring");
    cJSON *checkmk = monitoring
        ? cJSON_AddObjectToObject(monitoring, "checkmk") : NULL;
    cJSON *mqtt = monitoring
        ? cJSON_AddObjectToObject(monitoring, "mqtt") : NULL;
    cJSON *prometheus = monitoring
        ? cJSON_AddObjectToObject(monitoring, "prometheus") : NULL;
    cJSON *syslog = monitoring
        ? cJSON_AddObjectToObject(monitoring, "syslog") : NULL;
    cJSON *notify = monitoring
        ? cJSON_AddObjectToObject(monitoring, "notify") : NULL;
    if (!monitoring || !checkmk || !mqtt || !prometheus ||
        !syslog || !notify) {
        delete config;
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddBoolToObject(checkmk, "enabled", config->checkmk.enabled);
    cJSON_AddNumberToObject(checkmk, "port", config->checkmk.port);
    cJSON_AddStringToObject(checkmk, "allowedHosts",
                            config->checkmk.allowed_hosts);

    cJSON_AddBoolToObject(mqtt, "enabled", config->mqtt.enabled);
    cJSON_AddStringToObject(mqtt, "server", config->mqtt.server);
    cJSON_AddNumberToObject(mqtt, "port", config->mqtt.port);
    cJSON_AddStringToObject(mqtt, "user", config->mqtt.user);
    cJSON_AddStringToObject(mqtt, "password", config->mqtt.password);
    cJSON_AddStringToObject(mqtt, "topicPrefix", config->mqtt.topic_prefix);
    cJSON_AddBoolToObject(mqtt, "haDiscoveryEnabled",
                          config->mqtt.ha_discovery_enabled);
    cJSON_AddStringToObject(mqtt, "haDiscoveryPrefix",
                            config->mqtt.ha_discovery_prefix);
    cJSON_AddBoolToObject(mqtt, "tlsEnable", config->mqtt.tls_enable);
    cJSON_AddBoolToObject(mqtt, "tlsSkipVerify",
                          config->mqtt.tls_skip_verify);
    cJSON_AddStringToObject(mqtt, "tlsCaCerts",
                            config->mqtt.tls_ca_certs);
    cJSON_AddStringToObject(mqtt, "tlsCertfile",
                            config->mqtt.tls_certfile);
    cJSON_AddStringToObject(mqtt, "tlsKeyfile",
                            config->mqtt.tls_keyfile);
    cJSON_AddBoolToObject(mqtt, "commandEnabled",
                          config->mqtt.command_enabled);
    cJSON_AddStringToObject(mqtt, "commandToken",
                            config->mqtt.command_token);

    cJSON_AddBoolToObject(prometheus, "enabled",
                          config->prometheus.enabled);
    cJSON_AddNumberToObject(prometheus, "port", config->prometheus.port);
    cJSON_AddStringToObject(prometheus, "allowedHosts",
                            config->prometheus.allowed_hosts);

    cJSON_AddBoolToObject(syslog, "enabled", config->syslog.enabled);
    cJSON_AddStringToObject(syslog, "server", config->syslog.server);
    cJSON_AddNumberToObject(syslog, "port", config->syslog.port);
    cJSON_AddNumberToObject(syslog, "transport",
                            config->syslog.transport);
    cJSON_AddNumberToObject(syslog, "minSeverity",
                            config->syslog.min_severity);
    cJSON_AddStringToObject(syslog, "hostname",
                            config->syslog.hostname);

    cJSON_AddBoolToObject(notify, "enabled", config->notify.enabled);
    cJSON_AddNumberToObject(notify, "channels", config->notify.channels);
    cJSON_AddStringToObject(notify, "webhookUrl",
                            config->notify.webhook_url);
    cJSON_AddStringToObject(notify, "webhookSecret",
                            config->notify.webhook_secret);
    cJSON_AddStringToObject(notify, "telegramToken",
                            config->notify.telegram_token);
    cJSON_AddStringToObject(notify, "telegramChatId",
                            config->notify.telegram_chatid);
    cJSON_AddStringToObject(notify, "smtpServer",
                            config->notify.smtp_server);
    cJSON_AddNumberToObject(notify, "smtpPort",
                            config->notify.smtp_port);
    cJSON_AddNumberToObject(notify, "smtpTls",
                            config->notify.smtp_tls);
    cJSON_AddStringToObject(notify, "smtpUser",
                            config->notify.smtp_user);
    cJSON_AddStringToObject(notify, "smtpPassword",
                            config->notify.smtp_password);
    cJSON_AddStringToObject(notify, "smtpFrom",
                            config->notify.smtp_from);
    cJSON_AddStringToObject(notify, "smtpTo",
                            config->notify.smtp_to);
    cJSON_AddNumberToObject(notify, "cooldownSeconds",
                            config->notify.cooldown_seconds);
    cJSON_AddNumberToObject(notify, "eventMask", config->notify.event_mask);

    delete config;
    return ESP_OK;
}

static esp_err_t parse_monitoring_backup(
    const cJSON *root, monitoring_config_t **restored_config)
{
    if (!restored_config) return ESP_ERR_INVALID_ARG;
    *restored_config = NULL;

    const cJSON *monitoring =
        cJSON_GetObjectItemCaseSensitive(root, "monitoring");
    if (!monitoring) return ESP_OK; // Backwards-compatible legacy backup.
    if (!cJSON_IsObject(monitoring)) return ESP_ERR_INVALID_ARG;

    const cJSON *checkmk =
        cJSON_GetObjectItemCaseSensitive(monitoring, "checkmk");
    const cJSON *mqtt =
        cJSON_GetObjectItemCaseSensitive(monitoring, "mqtt");
    const cJSON *prometheus =
        cJSON_GetObjectItemCaseSensitive(monitoring, "prometheus");
    const cJSON *syslog =
        cJSON_GetObjectItemCaseSensitive(monitoring, "syslog");
    const cJSON *notify =
        cJSON_GetObjectItemCaseSensitive(monitoring, "notify");
    if (!cJSON_IsObject(checkmk) || !cJSON_IsObject(mqtt) ||
        !cJSON_IsObject(prometheus) || !cJSON_IsObject(syslog) ||
        !cJSON_IsObject(notify)) {
        return ESP_ERR_INVALID_ARG;
    }

    monitoring_config_t *config =
        new (std::nothrow) monitoring_config_t();
    if (!config) return ESP_ERR_NO_MEM;
    memset(config, 0, sizeof(*config));
    unsigned number = 0;

    bool valid =
        backup_get_bool(checkmk, "enabled", &config->checkmk.enabled) &&
        backup_get_uint(checkmk, "port", UINT16_MAX, &number);
    if (valid) config->checkmk.port = static_cast<uint16_t>(number);
    valid = valid && validatePort(config->checkmk.port) &&
        backup_get_string(checkmk, "allowedHosts",
                          config->checkmk.allowed_hosts,
                          sizeof(config->checkmk.allowed_hosts));

    valid = valid &&
        backup_get_bool(mqtt, "enabled", &config->mqtt.enabled) &&
        backup_get_string(mqtt, "server", config->mqtt.server,
                          sizeof(config->mqtt.server)) &&
        backup_get_uint(mqtt, "port", UINT16_MAX, &number);
    if (valid) config->mqtt.port = static_cast<uint16_t>(number);
    valid = valid && validatePort(config->mqtt.port) &&
        backup_get_string(mqtt, "user", config->mqtt.user,
                          sizeof(config->mqtt.user)) &&
        backup_get_string(mqtt, "password", config->mqtt.password,
                          sizeof(config->mqtt.password)) &&
        backup_get_string(mqtt, "topicPrefix", config->mqtt.topic_prefix,
                          sizeof(config->mqtt.topic_prefix)) &&
        backup_get_bool(mqtt, "haDiscoveryEnabled",
                        &config->mqtt.ha_discovery_enabled) &&
        backup_get_string(mqtt, "haDiscoveryPrefix",
                          config->mqtt.ha_discovery_prefix,
                          sizeof(config->mqtt.ha_discovery_prefix)) &&
        backup_get_bool(mqtt, "tlsEnable", &config->mqtt.tls_enable) &&
        backup_get_bool(mqtt, "tlsSkipVerify",
                        &config->mqtt.tls_skip_verify) &&
        backup_get_string(mqtt, "tlsCaCerts",
                          config->mqtt.tls_ca_certs,
                          sizeof(config->mqtt.tls_ca_certs)) &&
        backup_get_string(mqtt, "tlsCertfile",
                          config->mqtt.tls_certfile,
                          sizeof(config->mqtt.tls_certfile)) &&
        backup_get_string(mqtt, "tlsKeyfile",
                          config->mqtt.tls_keyfile,
                          sizeof(config->mqtt.tls_keyfile)) &&
        backup_get_bool(mqtt, "commandEnabled",
                        &config->mqtt.command_enabled) &&
        backup_get_string(mqtt, "commandToken",
                          config->mqtt.command_token,
                          sizeof(config->mqtt.command_token));
    if (valid && (config->mqtt.enabled || config->mqtt.server[0] != '\0')) {
        valid = validateServerAddress(config->mqtt.server,
                                      sizeof(config->mqtt.server) - 1);
    }
    if (valid && config->mqtt.command_token[0] != '\0') {
        valid = validateMqttCommandToken(config->mqtt.command_token);
    }

    valid = valid &&
        backup_get_bool(prometheus, "enabled",
                        &config->prometheus.enabled) &&
        backup_get_uint(prometheus, "port", UINT16_MAX, &number);
    if (valid) config->prometheus.port = static_cast<uint16_t>(number);
    valid = valid && validatePort(config->prometheus.port) &&
        backup_get_string(prometheus, "allowedHosts",
                          config->prometheus.allowed_hosts,
                          sizeof(config->prometheus.allowed_hosts));

    valid = valid &&
        backup_get_bool(syslog, "enabled", &config->syslog.enabled) &&
        backup_get_string(syslog, "server", config->syslog.server,
                          sizeof(config->syslog.server)) &&
        backup_get_uint(syslog, "port", UINT16_MAX, &number);
    if (valid) config->syslog.port = static_cast<uint16_t>(number);
    valid = valid && validatePort(config->syslog.port) &&
        backup_get_uint(syslog, "transport", 2, &number);
    if (valid) config->syslog.transport = static_cast<uint8_t>(number);
    valid = valid &&
        backup_get_uint(syslog, "minSeverity", 7, &number);
    if (valid) config->syslog.min_severity = static_cast<uint8_t>(number);
    valid = valid &&
        backup_get_string(syslog, "hostname", config->syslog.hostname,
                          sizeof(config->syslog.hostname));
    if (valid && (config->syslog.enabled || config->syslog.server[0] != '\0')) {
        valid = validateServerAddress(config->syslog.server,
                                      sizeof(config->syslog.server) - 1);
    }

    valid = valid &&
        backup_get_bool(notify, "enabled", &config->notify.enabled) &&
        backup_get_uint(notify, "channels", 7, &number);
    if (valid) config->notify.channels = static_cast<uint8_t>(number);
    valid = valid &&
        backup_get_string(notify, "webhookUrl",
                          config->notify.webhook_url,
                          sizeof(config->notify.webhook_url)) &&
        backup_get_string(notify, "webhookSecret",
                          config->notify.webhook_secret,
                          sizeof(config->notify.webhook_secret)) &&
        backup_get_string(notify, "telegramToken",
                          config->notify.telegram_token,
                          sizeof(config->notify.telegram_token)) &&
        backup_get_string(notify, "telegramChatId",
                          config->notify.telegram_chatid,
                          sizeof(config->notify.telegram_chatid)) &&
        backup_get_string(notify, "smtpServer",
                          config->notify.smtp_server,
                          sizeof(config->notify.smtp_server)) &&
        backup_get_uint(notify, "smtpPort", UINT16_MAX, &number);
    if (valid) config->notify.smtp_port = static_cast<uint16_t>(number);
    valid = valid && validatePort(config->notify.smtp_port) &&
        backup_get_uint(notify, "smtpTls", 2, &number);
    if (valid) config->notify.smtp_tls = static_cast<uint8_t>(number);
    valid = valid &&
        backup_get_string(notify, "smtpUser", config->notify.smtp_user,
                          sizeof(config->notify.smtp_user)) &&
        backup_get_string(notify, "smtpPassword",
                          config->notify.smtp_password,
                          sizeof(config->notify.smtp_password)) &&
        backup_get_string(notify, "smtpFrom", config->notify.smtp_from,
                          sizeof(config->notify.smtp_from)) &&
        backup_get_string(notify, "smtpTo", config->notify.smtp_to,
                          sizeof(config->notify.smtp_to)) &&
        backup_get_uint(notify, "cooldownSeconds", UINT16_MAX, &number);
    if (valid) {
        config->notify.cooldown_seconds = static_cast<uint16_t>(number);
    }
    // Optional on purpose: backups written before the event selection existed
    // have no such key, and requiring it would make every one of them fail to
    // restore the whole notify block. A missing key keeps the default, which
    // is the same "everything" behaviour those backups were taken under.
    unsigned event_mask = 0;
    if (backup_get_uint(notify, "eventMask", NOTIFY_EVENT_ALL, &event_mask)) {
        config->notify.event_mask = static_cast<uint16_t>(event_mask);
    }
    // Same CRLF/control-char rejection as the normal POST /api/monitoring
    // path (monitoring_api.cpp): these three fields are interpolated
    // verbatim into raw SMTP protocol lines / an HTTP header, so a crafted
    // or corrupted backup file must not be able to reintroduce header/command
    // injection through the restore path.
    if (valid && config->notify.webhook_secret[0] != '\0') {
        valid = validateNoControlChars(config->notify.webhook_secret);
    }
    if (valid && config->notify.smtp_from[0] != '\0') {
        valid = validateNoControlChars(config->notify.smtp_from);
    }
    if (valid && config->notify.smtp_to[0] != '\0') {
        valid = validateNoControlChars(config->notify.smtp_to);
    }

    if (!valid) {
        delete config;
        return ESP_ERR_INVALID_ARG;
    }
    *restored_config = config;
    return ESP_OK;
}

static esp_err_t add_theme_backup(cJSON *root)
{
    char scheme[8] = {};
    char color[8] = {};
    esp_err_t result = theme_api_get_config(
        scheme, sizeof(scheme), color, sizeof(color));
    if (result != ESP_OK) return result;
    cJSON *theme = cJSON_AddObjectToObject(root, "theme");
    if (!theme) return ESP_ERR_NO_MEM;
    cJSON_AddStringToObject(theme, "colorScheme", scheme);
    cJSON_AddStringToObject(theme, "primaryColor", color);
    return ESP_OK;
}

static esp_err_t parse_theme_backup(const cJSON *root,
                                    char scheme[8], char color[8],
                                    bool *has_theme)
{
    if (!scheme || !color || !has_theme) return ESP_ERR_INVALID_ARG;
    *has_theme = false;
    const cJSON *theme = cJSON_GetObjectItemCaseSensitive(root, "theme");
    if (!theme) return ESP_OK; // Backwards-compatible legacy backup.
    if (!cJSON_IsObject(theme) ||
        !backup_get_string(theme, "colorScheme", scheme, 8) ||
        !backup_get_string(theme, "primaryColor", color, 8)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (strcmp(scheme, "system") != 0 &&
        strcmp(scheme, "light") != 0 &&
        strcmp(scheme, "dark") != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(color) != 7 || color[0] != '#') return ESP_ERR_INVALID_ARG;
    for (size_t index = 1; index < 7; ++index) {
        if (!isxdigit(static_cast<unsigned char>(color[index]))) {
            return ESP_ERR_INVALID_ARG;
        }
    }
    *has_theme = true;
    return ESP_OK;
}

esp_err_t get_backup_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(
        req, "Content-Disposition",
        "attachment; filename=\"hb-rf-eth-ng-backup.json\"");
    httpd_resp_set_hdr(req, "Cache-Control",
                       "no-store, no-cache, must-revalidate, max-age=0");

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Out of memory");
    }

    cJSON_AddStringToObject(root, "_format", "hb-rf-eth-ng-backup");
    cJSON_AddNumberToObject(root, "_version", 2);

    // A complete, text-editable JSON backup necessarily contains secrets in
    // plaintext. Keep this warning inside the file so it remains visible even
    // when the backup is copied or opened outside the WebUI.
    cJSON *security = cJSON_AddObjectToObject(root, "_security");
    if (!security) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Out of memory");
    }
    cJSON_AddBoolToObject(security, "containsPlaintextSecrets", true);
    cJSON_AddStringToObject(
        security, "warning",
        "This unencrypted backup contains passwords, tokens, certificates and "
        "private keys in plaintext. Store it like a password and never publish it.");

    // Unknown metadata keys are intentionally ignored by the restore parser.
    // This makes the file useful as a manually edited multi-device template
    // without weakening validation of the actual configuration fields.
    cJSON *portability = cJSON_AddObjectToObject(root, "_portability");
    if (!portability) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Out of memory");
    }
    cJSON_AddBoolToObject(portability, "editable", true);
    cJSON_AddStringToObject(
        portability, "warning",
        "Before importing on another device, review hostname, static IP, "
        "administrator password and device-specific MQTT values.");

    add_settings(root);

    // Merge settings object into root if add_settings creates a sub-object
    // NOTE: add_settings creates a "settings" object inside root.
    // If we want a flat structure or specific structure for restore, we need to match post_settings_json_handler expectations.
    // post_settings_json_handler expects a flat JSON object with keys like "adminUsername", "adminPassword", "hostname", etc.
    // But add_settings creates { "settings": { "hostname": ... } }

    // We need to flatten it.
    cJSON *settingsObj = cJSON_GetObjectItem(root, "settings");
    if (settingsObj) {
        cJSON *child = settingsObj->child;
        while (child) {
            cJSON_AddItemToObject(root, child->string, cJSON_Duplicate(child, 1));
            child = child->next;
        }
        cJSON_DeleteItemFromObject(root, "settings");
    }

    // A complete backup intentionally contains credentials and private keys.
    // It is available only through the authenticated endpoint and marked
    // no-store, but the downloaded file itself must be protected by the user.
    cJSON_AddStringToObject(root, "adminPassword",
                            _settings->getAdminPassword());
    cJSON_AddBoolToObject(root, "passwordChanged",
                          _settings->getPasswordChanged());

    if (add_theme_backup(root) != ESP_OK ||
        add_monitoring_backup(root) != ESP_OK) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Could not create complete backup");
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

httpd_uri_t get_backup_handler = {
    .uri = "/api/backup",
    .method = HTTP_GET,
    .handler = get_backup_handler_func,
    .user_ctx = NULL};

// Serializes restore/factory-reset mutations with monitoring configuration,
// manual firmware writes and MQTT restarts.  Every early return releases the
// reservation automatically; a successful reserved restart never returns.
class ScopedOperationReservation {
public:
    ScopedOperationReservation() : held(ota_operation_try_begin()) {}
    ~ScopedOperationReservation()
    {
        if (held) ota_operation_finish();
    }
    explicit operator bool() const { return held; }

private:
    bool held;
};

esp_err_t post_restore_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    ScopedOperationReservation operation;
    if (!operation) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(
            req, "Another configuration or restart operation is active");
    }

    // Complete backups include MQTT TLS material and notification credentials,
    // so they can legitimately be larger than the old settings-only 8 KiB
    // payload.
    constexpr size_t maxRestoreBodySize = 24576;
    if (req->content_len == 0)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data received");
    }
    if (req->content_len >= maxRestoreBodySize)
    {
        return httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Request body too large");
    }

    const size_t bufferSize = req->content_len + 1;
    char *buffer = (char*)malloc(bufferSize);
    if (!buffer) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
    }

    int len = recv_full_body(req, buffer, bufferSize);

    if (len < 0)
    {
        free(buffer);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive request body");
    }

    cJSON *root = cJSON_Parse(buffer);
    free(buffer);

    if (!root) {
         return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    monitoring_config_t *parsed_monitoring = NULL;
    const esp_err_t monitoring_parse_result =
        parse_monitoring_backup(root, &parsed_monitoring);
    std::unique_ptr<monitoring_config_t> restored_monitoring(
        parsed_monitoring);
    char restored_theme_scheme[8] = {};
    char restored_theme_color[8] = {};
    bool has_restored_theme = false;
    const esp_err_t theme_parse_result =
        parse_theme_backup(root, restored_theme_scheme,
                           restored_theme_color, &has_restored_theme);
    if (monitoring_parse_result != ESP_OK ||
        theme_parse_result != ESP_OK) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request",
                               "invalid_backup", "backup");
    }

    char *adminUsername = cJSON_GetStringValue(cJSON_GetObjectItem(root, "adminUsername"));
    char *adminPassword = cJSON_GetStringValue(cJSON_GetObjectItem(root, "adminPassword"));
    char *currentPassword = cJSON_GetStringValue(
        cJSON_GetObjectItem(root, "currentPassword"));
    cJSON *passwordChangedItem =
        cJSON_GetObjectItem(root, "passwordChanged");

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
    bool enableIPv6 = cJSON_GetBoolValue(
        cJSON_GetObjectItem(root, "enableIPv6"));
    char *ipv6Mode = cJSON_GetStringValue(
        cJSON_GetObjectItem(root, "ipv6Mode"));
    char *ipv6Address = cJSON_GetStringValue(
        cJSON_GetObjectItem(root, "ipv6Address"));
    int ipv6PrefixLength = cJSON_GetIntValueSafe(
        cJSON_GetObjectItem(root, "ipv6PrefixLength"), 64);
    char *ipv6Gateway = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Gateway"));
    char *ipv6Dns1 = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Dns1"));
    char *ipv6Dns2 = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Dns2"));
    char *ccuIP = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ccuIP"));
    cJSON *systemLogEnabledItem =
        cJSON_GetObjectItem(root, "systemLogEnabled");
    cJSON *flashPauseItem = cJSON_GetObjectItem(root, "flashPause");
    cJSON *testDesignItem = cJSON_GetObjectItem(root, "testDesignEnabled");

    // Validate the entire backup and prove NVS capacity before changing any
    // live or persistent setting. A rejected restore must leave the running
    // configuration untouched.
    const bool has_password_changed_flag =
        passwordChangedItem && cJSON_IsBool(passwordChangedItem);
    if (adminUsername && adminUsername[0] != '\0' &&
        !valid_admin_username(adminUsername)) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request", "invalid_username",
                               "adminUsername");
    }
    bool admin_password_change_requested = false;
    if (adminPassword && adminPassword[0] != '\0') {
        const size_t password_len = strlen(adminPassword);
        if (password_len >= 33) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_password",
                                   "adminPassword");
        }
        admin_password_change_requested =
            secure_strcmp(adminPassword,
                          _settings->getAdminPassword()) != 0;
        // A same-password backup is configuration-only. For an actual
        // credential replacement, legacy backups with an explicit flag may
        // retain their historic password while new-format input must satisfy
        // the current password policy.
        const bool password_valid = !admin_password_change_requested ||
            (has_password_changed_flag
                ? password_len > 0
                : validateAdminPassword(adminPassword));
        if (!password_valid) {
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
            !validateIPAddress(localIP) ||
            !netmask_ok || netmask.addr == IPADDR_ANY ||
            !validateNetmask(netmask) ||
            !gateway_ok || !validateIPAddress(gateway)) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_network",
                                   "network");
        }
    }
    if (hostname) {
        const bool dns1_ok = parse_ipv4_json(
            cJSON_GetObjectItem(root, "dns1"), true, &dns1);
        const bool dns2_ok = parse_ipv4_json(
            cJSON_GetObjectItem(root, "dns2"), true, &dns2);
        if (!dns1_ok || !dns2_ok ||
            (dns1.addr != IPADDR_ANY && !validateIPAddress(dns1)) ||
            (dns2.addr != IPADDR_ANY && !validateIPAddress(dns2))) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_network",
                                   "dns");
        }
    }
    if (timesource < TIMESOURCE_NTP || timesource > TIMESOURCE_GPS ||
        !validateDcfOffset(dcfOffset) ||
        !validateGpsBaudrate(gpsBaudrate) ||
        !validateLEDBrightness(ledBrightness) ||
        (ntpServer && !validateNtpServer(ntpServer)) ||
        (ccuIP && ccuIP[0] != '\0' && !validateCcuAddress(ccuIP))) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request", "invalid_backup",
                               "settings");
    }

    const bool has_ipv6 =
        cJSON_GetObjectItem(root, "enableIPv6") ||
        cJSON_GetObjectItem(root, "ipv6Mode") ||
        cJSON_GetObjectItem(root, "ipv6Address") ||
        cJSON_GetObjectItem(root, "ipv6PrefixLength") ||
        cJSON_GetObjectItem(root, "ipv6Gateway") ||
        cJSON_GetObjectItem(root, "ipv6Dns1") ||
        cJSON_GetObjectItem(root, "ipv6Dns2");
    if (has_ipv6) {
        const bool effective_enabled = cJSON_GetObjectItem(root, "enableIPv6")
            ? enableIPv6 : _settings->getEnableIPv6();
        const char *effective_mode =
            ipv6Mode ? ipv6Mode : _settings->getIPv6Mode();
        const char *effective_address =
            ipv6Address ? ipv6Address : _settings->getIPv6Address();
        if (!effective_mode ||
            (strcmp(effective_mode, "auto") != 0 &&
             strcmp(effective_mode, "static") != 0 &&
             strcmp(effective_mode, "disabled") != 0) ||
            ipv6PrefixLength < 1 || ipv6PrefixLength > 128 ||
            (effective_enabled && strcmp(effective_mode, "static") == 0 &&
             (!effective_address || effective_address[0] == '\0' ||
              !validateIPv6Address(effective_address)))) {
            cJSON_Delete(root);
            return send_json_error(req, "400 Bad Request", "invalid_ipv6",
                                   "ipv6");
        }
        const char *optional_ipv6[] = {
            ipv6Gateway, ipv6Dns1, ipv6Dns2
        };
        for (const char *value : optional_ipv6) {
            if (value && value[0] != '\0' && !validateIPv6Address(value)) {
                cJSON_Delete(root);
                return send_json_error(req, "400 Bad Request", "invalid_ipv6",
                                       "ipv6");
            }
        }
    }

    static const char *LED_PROGRAM_KEYS[] = {
        "idle", "ccu_disconnected", "ccu_connected", "update_available",
        "error", "booting", "update_in_progress"
    };
    if (ledPrograms && !cJSON_IsObject(ledPrograms)) {
        cJSON_Delete(root);
        return send_json_error(req, "400 Bad Request", "invalid_backup",
                               "ledPrograms");
    }
    if (ledPrograms) {
        for (const char *key : LED_PROGRAM_KEYS) {
            cJSON *item = cJSON_GetObjectItem(ledPrograms, key);
            if (item && (!cJSON_IsNumber(item) ||
                         item->valuedouble != item->valueint ||
                         item->valueint < 0 || item->valueint > 10)) {
                cJSON_Delete(root);
                return send_json_error(req, "400 Bad Request",
                                       "invalid_backup", "ledPrograms");
            }
        }
    }

    std::unique_ptr<settings_snapshot_t> previous_settings(
        new (std::nothrow) settings_snapshot_t{});
    std::unique_ptr<monitoring_config_t> previous_monitoring;
    if (!previous_settings) {
        cJSON_Delete(root);
        return send_json_error(req, "500 Internal Server Error",
                               "restore_snapshot_allocation", "settings");
    }
    _settings->snapshot(previous_settings.get());
    if (restored_monitoring) {
        previous_monitoring.reset(
            new (std::nothrow) monitoring_config_t{});
        if (!previous_monitoring ||
            monitoring_get_config(previous_monitoring.get()) != ESP_OK) {
            cJSON_Delete(root);
            return send_json_error(req, "500 Internal Server Error",
                                   "restore_snapshot_allocation",
                                   "monitoring");
        }
    }

    // Keep every app-owned writer out of the shared 16 KiB NVS partition
    // from capacity preflight through the final commit. Nested helpers take
    // this recursive lock as well.
    NvsStorageLock restore_storage(portMAX_DELAY, "webui.restore");
    if (!restore_storage) {
        cJSON_Delete(root);
        return send_json_error(req, "503 Service Unavailable",
                               "restore_storage_busy", "backup");
    }

    if (restored_monitoring &&
        monitoring_validate_config_storage(restored_monitoring.get()) !=
            ESP_OK) {
        cJSON_Delete(root);
        return send_json_error(req, "507 Insufficient Storage",
                               "restore_monitoring_capacity", "monitoring");
    }

    // Snapshot the rollbackable theme before changing any persistent value.
    char previous_theme_scheme[8] = {};
    char previous_theme_color[8] = {};
    if (has_restored_theme) {
        if (theme_api_get_config(previous_theme_scheme,
                                 sizeof(previous_theme_scheme),
                                 previous_theme_color,
                                 sizeof(previous_theme_color)) != ESP_OK) {
            cJSON_Delete(root);
            return send_json_error(req, "500 Internal Server Error",
                                   "restore_theme_snapshot_failed", "theme");
        }
    }

    // Rotate before accepting the replacement password in RAM and before the
    // restore transaction captures its auth rollback image. The backup will
    // therefore contain the old durable password paired with the new token.
    if (admin_password_change_requested) {
        const esp_err_t token_result = rotate_admin_token();
        if (token_result != ESP_OK) {
            ESP_LOGE(TAG, "Restore password token rotation failed: %s",
                     esp_err_to_name(token_result));
            cJSON_Delete(root);
            return send_json_error(req, "500 Internal Server Error",
                                   "restore_token_rotation_failed",
                                   "adminPassword");
        }
    }

    // Apply the already-validated Settings candidate in RAM, then require a
    // successful NVS commit before touching the other namespaces.
    if (adminUsername && adminUsername[0] != '\0') {
        (void)_settings->setAdminUsername(adminUsername);
    }
    if (admin_password_change_requested) {
        if (has_password_changed_flag) {
            (void)_settings->restoreAdminPassword(
                adminPassword, cJSON_IsTrue(passwordChangedItem));
        } else {
            (void)_settings->setAdminPassword(adminPassword);
        }
    }
    if (hostname) {
        (void)_settings->setNetworkSettings(
            hostname, useDHCP, localIP, netmask, gateway, dns1, dns2);
    }
    _settings->setTimesource(timesource);
    _settings->setDcfOffset(dcfOffset);
    _settings->setGpsBaudrate(gpsBaudrate);
    if (ntpServer) _settings->setNtpServer(ntpServer);
    _settings->setLEDBrightness(ledBrightness);

    if (ledPrograms) {
        for (int index = 0; index < 7; ++index) {
            cJSON *item =
                cJSON_GetObjectItem(ledPrograms, LED_PROGRAM_KEYS[index]);
            if (item) {
                _settings->setLedProgram(index, item->valueint);
            }
        }
    }

    if (has_ipv6) {
        _settings->setIPv6Settings(
            cJSON_GetObjectItem(root, "enableIPv6")
                ? enableIPv6 : _settings->getEnableIPv6(),
            ipv6Mode ? ipv6Mode : _settings->getIPv6Mode(),
            ipv6Address ? ipv6Address : _settings->getIPv6Address(),
            cJSON_GetObjectItem(root, "ipv6PrefixLength")
                ? ipv6PrefixLength : _settings->getIPv6PrefixLength(),
            ipv6Gateway ? ipv6Gateway : _settings->getIPv6Gateway(),
            ipv6Dns1 ? ipv6Dns1 : _settings->getIPv6Dns1(),
            ipv6Dns2 ? ipv6Dns2 : _settings->getIPv6Dns2());
    }
    if (ccuIP) _settings->setCCUIP(ccuIP);
    if (systemLogEnabledItem && cJSON_IsBool(systemLogEnabledItem)) {
        _settings->setSystemLogEnabled(cJSON_IsTrue(systemLogEnabledItem));
    }
    if (flashPauseItem && cJSON_IsBool(flashPauseItem)) {
        _settings->setFlashPause(cJSON_IsTrue(flashPauseItem));
    }
    if (testDesignItem && cJSON_IsBool(testDesignItem)) {
        _settings->setTestDesignEnabled(cJSON_IsTrue(testDesignItem));
    }

    const esp_err_t settings_capacity_result =
        _settings->validateStorageCapacity();
    if (settings_capacity_result != ESP_OK) {
        _settings->restoreSnapshot(previous_settings.get());
        cJSON_Delete(root);
        if (settings_capacity_result == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
            return send_json_error(req, "507 Insufficient Storage",
                                   "restore_settings_capacity", "settings");
        }
        return send_json_error(req, "500 Internal Server Error",
                               "restore_settings_validation_failed",
                               "settings");
    }

    // Arm one durable marker before the first persistent Settings write. It
    // covers Settings, theme and monitoring as a single restore generation.
    // restore_storage remains held until the marker is cleared after all
    // writes, or after a fully successful rollback.
    const esp_err_t restore_begin_result =
        Settings::beginRestoreTransaction();
    if (restore_begin_result != ESP_OK) {
        _settings->restoreSnapshot(previous_settings.get());
        cJSON_Delete(root);
        if (restore_begin_result == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
            return send_json_error(req, "507 Insufficient Storage",
                                   "restore_storage_capacity", "backup");
        }
        return send_json_error(req, "500 Internal Server Error",
                               "restore_transaction_begin_failed", "backup");
    }

    auto finish_rollback_transaction = [](
        const char *context, esp_err_t settings_rollback,
        esp_err_t monitoring_rollback, esp_err_t theme_rollback) -> bool {
        if (settings_rollback != ESP_OK || monitoring_rollback != ESP_OK ||
            theme_rollback != ESP_OK) {
            ESP_LOGE(TAG,
                     "%s; restore marker retained: settings=%s monitoring=%s theme=%s",
                     context, esp_err_to_name(settings_rollback),
                     esp_err_to_name(monitoring_rollback),
                     esp_err_to_name(theme_rollback));
            return false;
        }
        const esp_err_t finish_result =
            Settings::finishRestoreTransaction();
        if (finish_result != ESP_OK) {
            ESP_LOGE(TAG, "%s; could not clear restore marker: %s",
                     context, esp_err_to_name(finish_result));
            return false;
        }
        return true;
    };

    const esp_err_t settings_save_result = _settings->save();
    if (settings_save_result != ESP_OK) {
        _settings->restoreSnapshot(previous_settings.get());
        const esp_err_t rollback_result = _settings->save();
        const bool rollback_complete = finish_rollback_transaction(
            "Could not roll back Settings after failed restore",
            rollback_result, ESP_OK, ESP_OK);
        cJSON_Delete(root);
        if (!rollback_complete) {
            return send_json_error(req, "500 Internal Server Error",
                                   "restore_rollback_failed", "backup");
        }
        if (settings_save_result == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
            return send_json_error(req, "507 Insufficient Storage",
                                   "restore_settings_failed", "settings");
        }
        return send_json_error(req, "500 Internal Server Error",
                               "restore_settings_failed", "settings");
    }

    bool theme_written = false;
    if (has_restored_theme) {
        const esp_err_t theme_result = theme_api_set_config(
            restored_theme_scheme, restored_theme_color);
        if (theme_result != ESP_OK) {
            const esp_err_t theme_rollback = theme_api_set_config(
                previous_theme_scheme, previous_theme_color);
            _settings->restoreSnapshot(previous_settings.get());
            const esp_err_t rollback_result = _settings->save();
            const bool rollback_complete = finish_rollback_transaction(
                "Could not fully roll back theme restore failure",
                rollback_result, ESP_OK, theme_rollback);
            cJSON_Delete(root);
            if (!rollback_complete) {
                return send_json_error(req, "500 Internal Server Error",
                                       "restore_rollback_failed", "backup");
            }
            if (theme_result == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
                return send_json_error(req, "507 Insufficient Storage",
                                       "restore_theme_failed", "theme");
            }
            return send_json_error(req, "500 Internal Server Error",
                                   "restore_theme_failed", "theme");
        }
        theme_written = true;
    }

    if (restored_monitoring) {
        // Settings and theme share the same physical NVS partition with
        // monitoring. Repeat the earlier preflight immediately before the
        // monitoring transaction, while restore_storage still excludes every
        // other app-owned writer. This catches capacity consumed by the two
        // preceding commits without ever erasing the known-good monitoring
        // generation.
        const esp_err_t final_monitoring_capacity =
            monitoring_validate_config_storage(restored_monitoring.get());
        if (final_monitoring_capacity != ESP_OK) {
            _settings->restoreSnapshot(previous_settings.get());
            const esp_err_t settings_rollback = _settings->save();
            esp_err_t theme_rollback = ESP_OK;
            if (theme_written) {
                theme_rollback = theme_api_set_config(
                    previous_theme_scheme, previous_theme_color);
            }
            const bool rollback_complete = finish_rollback_transaction(
                "Restore capacity rollback incomplete", settings_rollback,
                ESP_OK, theme_rollback);
            cJSON_Delete(root);
            if (!rollback_complete) {
                return send_json_error(req, "500 Internal Server Error",
                                       "restore_rollback_failed", "backup");
            }

            if (final_monitoring_capacity ==
                ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
                return send_json_error(req, "507 Insufficient Storage",
                                       "restore_monitoring_capacity",
                                       "monitoring");
            }
            ESP_LOGE(TAG, "Final monitoring capacity validation failed: %s",
                     esp_err_to_name(final_monitoring_capacity));
            return send_json_error(req, "500 Internal Server Error",
                                   "restore_monitoring_validation_failed",
                                   "monitoring");
        }

        const esp_err_t monitoring_result =
            monitoring_save_config_for_restore(restored_monitoring.get());
        if (monitoring_result != ESP_OK) {
            _settings->restoreSnapshot(previous_settings.get());
            const esp_err_t settings_rollback = _settings->save();
            const esp_err_t monitoring_rollback =
                monitoring_save_config_for_restore(previous_monitoring.get());
            esp_err_t theme_rollback = ESP_OK;
            if (theme_written) {
                theme_rollback = theme_api_set_config(
                    previous_theme_scheme, previous_theme_color);
            }
            const bool rollback_complete = finish_rollback_transaction(
                "Restore rollback incomplete", settings_rollback,
                monitoring_rollback, theme_rollback);
            cJSON_Delete(root);
            if (!rollback_complete) {
                return send_json_error(req, "500 Internal Server Error",
                                       "restore_rollback_failed", "backup");
            }
            if (monitoring_result == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
                return send_json_error(req, "507 Insufficient Storage",
                                       "restore_monitoring_capacity",
                                       "monitoring");
            }
            return send_json_error(req, "500 Internal Server Error",
                                   "restore_monitoring_failed", "monitoring");
        }
    }

    const esp_err_t restore_finish_result =
        Settings::finishRestoreTransaction();
    if (restore_finish_result != ESP_OK) {
        _settings->restoreSnapshot(previous_settings.get());
        const esp_err_t settings_rollback = _settings->save();
        esp_err_t monitoring_rollback = ESP_OK;
        if (restored_monitoring) {
            monitoring_rollback =
                monitoring_save_config_for_restore(previous_monitoring.get());
        }
        esp_err_t theme_rollback = ESP_OK;
        if (theme_written) {
            theme_rollback = theme_api_set_config(
                previous_theme_scheme, previous_theme_color);
        }
        const bool rollback_complete = finish_rollback_transaction(
            "Restore marker finalization failed", settings_rollback,
            monitoring_rollback, theme_rollback);
        cJSON_Delete(root);
        if (!rollback_complete) {
            return send_json_error(req, "500 Internal Server Error",
                                   "restore_rollback_failed", "backup");
        }
        return send_json_error(req, "500 Internal Server Error",
                               "restore_transaction_finish_failed", "backup");
    }

    // Only expose non-persistent side effects after every namespace committed.
    if (cJSON_GetObjectItem(root, "ledBrightness") != NULL) {
        LED::setBrightness(ledBrightness);
    }
    if (flashPauseItem && cJSON_IsBool(flashPauseItem)) {
        set_flash_pause_enabled(cJSON_IsTrue(flashPauseItem));
    }

    // Do not carry the storage lock into the restart cleanup: a late MQTT
    // callback may need to write its one-shot migration marker before the
    // cooperative MQTT stop can finish.
    restore_storage.release();

    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\":true}");

    // Restart
    vTaskDelay(pdMS_TO_TICKS(1000));
    refresh_restart_sync_from_settings();
    full_system_restart_with_reserved_operation();

    return ESP_OK;
}

httpd_uri_t post_restore_handler = {
    .uri = "/api/restore",
    .method = HTTP_POST,
    .handler = post_restore_handler_func,
    .user_ctx = NULL};

// Forward declaration: prepare_ota_heap() is defined further down (after the
// upload handler) but is now also used on the success path of the upload
// handler so that heap/network-active subsystems are stopped before the
// restart. Without this the upload handler would not compile.
static esp_err_t prepare_ota_heap(uint32_t *paused_monitoring);
struct ota_finalize_ctx {
    const esp_partition_t *update_partition;
    const esp_partition_t *running;
};
static void ota_finalize_task(void *pv);

#define OTA_CHECK(a, str, ...)                                                    \
    do                                                                            \
    {                                                                             \
        if (!(a))                                                                 \
        {                                                                         \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, str);       \
            goto err;                                                             \
        }                                                                         \
    } while (0)

#define OTA_BUFFER_SIZE 4096

// OTA status tracking for the remaining manual firmware upload endpoint
// (/ota_update). Automatic update checks and URL-based OTA were removed, but
// this gate still prevents two administrators from writing the update
// partition concurrently.
enum ota_status_t {
    OTA_IDLE = 0,
    OTA_DOWNLOADING,
    OTA_FINALIZING,
    OTA_SUCCESS,
    OTA_FAILED
};

static std::atomic<ota_status_t> _ota_status{OTA_IDLE};
static std::atomic<int> _ota_progress{0};  // 0-100
static char _ota_error[128] = {0};
static portMUX_TYPE _ota_error_mux = portMUX_INITIALIZER_UNLOCKED;

static void set_ota_error(const char *format, ...)
{
    char text[sizeof(_ota_error)];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    portENTER_CRITICAL(&_ota_error_mux);
    snprintf(_ota_error, sizeof(_ota_error), "%s", text);
    portEXIT_CRITICAL(&_ota_error_mux);
}

static void copy_ota_error(char *dest, size_t size)
{
    portENTER_CRITICAL(&_ota_error_mux);
    snprintf(dest, size, "%s", _ota_error);
    portEXIT_CRITICAL(&_ota_error_mux);
}

esp_err_t post_ota_update_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    bool net_gate_held = false;

    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    if (!ota_operation_try_begin())
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "OTA update already in progress");
        return ESP_OK;
    }
    _ota_status = OTA_DOWNLOADING;
    _ota_progress = 0;
    set_ota_error("");

    // The inbound upload itself is not TLS, but its 4 KiB buffer plus flash
    // erase/write pressure must not overlap an outbound TLS handshake on a
    // WROOM-32. Mark the operation before waiting so best-effort consumers
    // defer; hold the ownershipless gate only while bytes are streamed.
    net_fetch_set_ota_active(true);
    if (g_net_fetch_mutex != NULL) {
        if (xSemaphoreTake(g_net_fetch_mutex,
                           pdMS_TO_TICKS(20000)) != pdTRUE) {
            ESP_LOGE(TAG, "Could not reserve network/TLS gate for OTA upload");
            _ota_status = OTA_FAILED;
            set_ota_error("Network/TLS subsystem busy");
            net_fetch_set_ota_active(false);
            ota_operation_finish();
            return httpd_resp_send_err(req,
                                       HTTPD_500_INTERNAL_SERVER_ERROR,
                                       "Network/TLS subsystem busy");
        }
        net_gate_held = true;
        crash_blackbox_net_op_begin("webui_ota_upload");
    }

    esp_ota_handle_t ota_handle = 0;
    bool ota_begun = false;

    char *ota_buff = (char *)malloc(OTA_BUFFER_SIZE);
    if (!ota_buff) {
        ESP_LOGE(TAG, "Failed to allocate OTA buffer");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        _ota_status = OTA_FAILED;
        if (net_gate_held) { crash_blackbox_net_op_end(); xSemaphoreGive(g_net_fetch_mutex); }
        net_fetch_set_ota_active(false);
        ota_operation_finish();
        return ESP_FAIL;
    }

    const size_t content_length = req->content_len;
    if (content_length == 0x50000) {
        ESP_LOGW(TAG, "Rejected 320 KiB WebUI image on firmware endpoint");
        free(ota_buff);
        _ota_status = OTA_FAILED;
        if (net_gate_held) { crash_blackbox_net_op_end(); xSemaphoreGive(g_net_fetch_mutex); }
        net_fetch_set_ota_active(false);
        ota_operation_finish();
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
            "Falsche Datei: Das 327680-Byte-WebUI-/WWW-Image muss unter System -> WebUI installiert werden.");
    }
    size_t content_received = 0;
    int recv_len;
    int timeout_retries = 5;
    static constexpr int64_t OTA_UPLOAD_TOTAL_TIMEOUT_US =
        5LL * 60LL * 1000LL * 1000LL;
    static constexpr int64_t OTA_UPLOAD_NO_PROGRESS_TIMEOUT_US =
        30LL * 1000LL * 1000LL;
    const int64_t upload_started_us = esp_timer_get_time();
    int64_t last_progress_us = upload_started_us;
    bool is_req_body_started = false;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    const esp_partition_t *running = NULL;
    esp_err_t ota_end_result = ESP_OK;

    // Validate update partition exists
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA update partition found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition available");
        goto err;
    }

    // HTTP Content-Length is size_t. Narrowing it to int before MIN() could
    // turn a value above INT_MAX negative and then back into a huge size_t in
    // httpd_req_recv(), overflowing the fixed 4 KiB buffer. Also reject images
    // which cannot fit the selected application partition before reading a
    // single byte from the socket.
    if (content_length == 0 || content_length > update_partition->size ||
        content_length > static_cast<size_t>(INT32_MAX)) {
        ESP_LOGW(TAG, "Rejected OTA content length: %u (partition %u)",
                 (unsigned)content_length, (unsigned)update_partition->size);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "Invalid firmware image size");
        goto err;
    }

    ESP_LOGI(TAG, "Starting OTA update, partition: %s, size: %u bytes",
             update_partition->label, (unsigned)content_length);

    do
    {
        const int64_t now_us = esp_timer_get_time();
        if (now_us - upload_started_us > OTA_UPLOAD_TOTAL_TIMEOUT_US ||
            now_us - last_progress_us > OTA_UPLOAD_NO_PROGRESS_TIMEOUT_US) {
            ESP_LOGE(TAG,
                     "OTA upload deadline exceeded after %u of %u bytes",
                     (unsigned)content_received, (unsigned)content_length);
            set_ota_error("Firmware upload timed out");
            httpd_resp_set_status(req, "408 Request Timeout");
            httpd_resp_sendstr(req, "Firmware upload timed out");
            goto err;
        }

        const size_t receive_cap =
            MIN(content_length - content_received,
                static_cast<size_t>(OTA_BUFFER_SIZE));
        if ((recv_len = httpd_req_recv(req, ota_buff, receive_cap)) < 0)
        {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT && timeout_retries-- > 0)
            {
                // Transient timeout - retry a bounded number of times. An
                // unbounded retry loop would wedge the single httpd task
                // forever if the client stalls mid-upload.
                continue;
            }
            else
            {
                ESP_LOGE(TAG, "OTA socket error %d, received %u of %u bytes",
                         recv_len, (unsigned)content_received,
                         (unsigned)content_length);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Network error during upload");
                goto err;
            }
        }
        else if (recv_len == 0)
        {
            // Connection closed by client
            ESP_LOGE(TAG,
                     "OTA connection closed prematurely, received %u of %u bytes",
                     (unsigned)content_received, (unsigned)content_length);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Incomplete upload");
            goto err;
        }

        // A peer may trickle a few bytes before each socket timeout. Reset the
        // transient retry budget on real progress, while the independent
        // no-progress and total deadlines keep the single httpd task bounded.
        last_progress_us = esp_timer_get_time();
        timeout_retries = 5;

        if (!is_req_body_started)
        {
            is_req_body_started = true;

            if (recv_len <= 0 || static_cast<unsigned char>(ota_buff[0]) != 0xE9) {
                ESP_LOGW(TAG, "Rejected non-ESP firmware image (magic 0x%02x)",
                         recv_len > 0 ? static_cast<unsigned char>(ota_buff[0]) : 0);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                    "Falsche Datei: kein gueltiges ESP32-Firmware-Abbild. WebUI unter System -> WebUI installieren.");
                goto err;
            }

            // Only raw binary uploads are supported (the WebUI posts the file
            // as the request body). The previous multipart/form-data path was
            // broken by design: it compared stripped body bytes against the
            // full content length (loop never terminated) and wrote the
            // trailing boundary into flash.
            char content_type[64] = {0};
            if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type)) == ESP_OK &&
                strstr(content_type, "multipart/form-data") != NULL) {
                ESP_LOGE(TAG, "Multipart firmware uploads are not supported - send the raw binary as request body");
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Multipart uploads not supported, send raw binary body");
                goto err;
            }

            OTA_CHECK(esp_ota_begin(update_partition, content_length, &ota_handle) == ESP_OK, "Could not start OTA");
            ota_begun = true;
            ESP_LOGW(TAG, "Begin OTA Update to partition %s, File Size: %u",
                     update_partition->label, (unsigned)content_length);
            _statusLED->setState(LED_STATE_BLINK_FAST);

            OTA_CHECK(esp_ota_write(ota_handle, ota_buff, recv_len) == ESP_OK, "Error writing OTA");
            content_received += static_cast<size_t>(recv_len);
            _ota_progress = static_cast<int>(content_received * 100 /
                                             content_length);
            ESP_LOGI(TAG, "OTA progress: %u / %u bytes (%d%%)",
                     (unsigned)content_received, (unsigned)content_length,
                     _ota_progress.load());
        }
        else
        {
            OTA_CHECK(esp_ota_write(ota_handle, ota_buff, recv_len) == ESP_OK, "Error writing OTA");
            content_received += static_cast<size_t>(recv_len);
            _ota_progress = static_cast<int>(content_received * 100 /
                                             content_length);
            ESP_LOGI(TAG, "OTA progress: %u / %u bytes (%d%%)",
                     (unsigned)content_received, (unsigned)content_length,
                     _ota_progress.load());
        }
    } while (content_received < content_length);

    // Verify complete firmware was received
    if (content_received != content_length) {
        ESP_LOGE(TAG, "Incomplete firmware: received %u of %u bytes",
                 (unsigned)content_received, (unsigned)content_length);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Incomplete firmware upload");
        goto err;
    }

    // Validate and finalize OTA
    // esp_ota_end() consumes/removes the OTA handle even when image
    // verification fails. Clear the ownership flag before testing its result
    // so the error path never calls esp_ota_abort() on a stale handle.
    ota_end_result = esp_ota_end(ota_handle);
    ota_begun = false;
    OTA_CHECK(ota_end_result == ESP_OK, "Error finalizing OTA");

    // Verify the firmware image before setting boot partition
    ESP_LOGI(TAG, "Validating firmware image...");
    running = esp_ota_get_running_partition();

    if (running == NULL || update_partition == running) {
        ESP_LOGE(TAG, "Cannot update running partition!");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid OTA partition");
        goto err;
    }

    // The image is finalized and no longer needs the upload buffer. Release it
    // before stopping workers so a fragmented heap does not turn an otherwise
    // valid update into a cleanup timeout.
    free(ota_buff);
    ota_buff = NULL;

    // Release before stopping MQTT/other workers: an MQTT reconnect callback
    // may be waiting on this gate and its cooperative stop must be allowed to
    // finish. No more upload buffer or flash write is active beyond this point.
    if (net_gate_held) {
        crash_blackbox_net_op_end();
        xSemaphoreGive(g_net_fetch_mutex);
        net_gate_held = false;
    }
    net_fetch_set_ota_active(false);

    // Offload the post-upload sequence (worker stop → boot select → restart)
    // to a background task so the httpd thread stays free to serve status
    // polls during the potentially 90 s sequential worker-stop phase.
    _ota_status = OTA_FINALIZING;

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\":true,\"message\":\"Firmware received, finalizing and restarting...\"}");

    // Scope ctx so goto err (from the upload phase above) cannot jump over
    // its initialization — C++ forbids crossing a declaration with an
    // initializer even for pointer types.
    {
        ota_finalize_ctx *ctx = (ota_finalize_ctx *)calloc(1, sizeof(ota_finalize_ctx));
        if (!ctx) {
            ESP_LOGE(TAG, "Could not allocate OTA finalize context");
            _ota_status = OTA_FAILED;
            set_ota_error("Memory allocation failed during finalize");
            ota_operation_finish();
            return ESP_OK;
        }
        ctx->update_partition = update_partition;
        ctx->running = running;

        if (xTaskCreate(ota_finalize_task, "ota_finalize", 8192,
                        ctx, 2, NULL) != pdPASS) {
            ESP_LOGE(TAG, "Could not create OTA finalize task");
            _ota_status = OTA_FAILED;
            set_ota_error("Could not start finalize task");
            free(ctx);
            ota_operation_finish();
            return ESP_OK;
        }
    }

    return ESP_OK;

err:
    if (ota_buff) free(ota_buff);
    if (net_gate_held) { crash_blackbox_net_op_end(); xSemaphoreGive(g_net_fetch_mutex); }
    net_fetch_set_ota_active(false);
    _statusLED->setState(LED_STATE_OFF);
    _ota_status = OTA_FAILED;

    // Abort OTA if it was started but not completed
    if (ota_begun) {
        ESP_LOGW(TAG, "Aborting OTA operation due to error");
        esp_ota_abort(ota_handle);
    }

    // Store reset reason for failed firmware update
    ResetInfo::storeResetReason(RESET_REASON_UPDATE_FAILED);
    ota_operation_finish();
    return ESP_FAIL;
}

httpd_uri_t post_ota_update_handler = {
    .uri = "/ota_update",
    .method = HTTP_POST,
    .handler = post_ota_update_handler_func,
    .user_ctx = NULL};

esp_err_t post_restart_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }

    httpd_resp_set_type(req, "application/json");
    /* CORS header removed - same-origin requests only */
    httpd_resp_sendstr(req, "{\"success\":true}");

    // Store reset reason before restart
    ResetInfo::storeResetReason(RESET_REASON_USER_RESTART);

    // Queue the notification before the delay below so the events worker has
    // a chance to pick it up (queue poll: 500ms) and start delivery before
    // full_system_restart()'s monitoring_pause_for_ota() stops the worker —
    // that stop path waits up to 30s for an in-flight send to finish, but
    // only if the worker already dequeued the entry.
    events_emit(EVENT_RESTART, "manual restart via WebUI/API");

    // Restart after a short delay to allow response to be sent
    vTaskDelay(pdMS_TO_TICKS(1000));
    refresh_restart_sync_from_settings();
    full_system_restart();

    return ESP_OK;
}

httpd_uri_t post_restart_handler = {
    .uri = "/api/restart",
    .method = HTTP_POST,
    .handler = post_restart_handler_func,
    .user_ctx = NULL};

esp_err_t post_factory_reset_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }

    ScopedOperationReservation operation;
    if (!operation) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(
            req, "Another configuration or restart operation is active");
    }

    // Queue the notification against the still-live (pre-wipe) notify config
    // before anything is erased. Same delivery-window reasoning as
    // post_restart_handler_func: the events worker gets the 1s pre-restart
    // delay plus up to 30s inside monitoring_pause_for_ota() to actually
    // send it, as long as it dequeues the entry before that stop begins.
    events_emit(EVENT_FACTORY_RESET, "factory reset via WebUI/API");

    // Erase every user-controlled namespace first. reset_info is deliberately
    // part of Settings::clear(), so store the one allowed post-reset metadata
    // value and report success only after the complete wipe succeeded.
    const esp_err_t clear_result = _settings->clear();
    if (clear_result != ESP_OK) {
        ESP_LOGE(TAG, "Factory reset failed while clearing settings: %s",
                 esp_err_to_name(clear_result));
        return send_json_error(req, "500 Internal Server Error",
                               "factory_reset_failed", "settings");
    }
    ResetInfo::storeResetReason(RESET_REASON_FACTORY_RESET);

    httpd_resp_set_type(req, "application/json");
    /* CORS header removed - same-origin requests only */
    httpd_resp_sendstr(req, "{\"success\":true}");

    // Restart after a short delay to allow response to be sent
    vTaskDelay(pdMS_TO_TICKS(1000));
    refresh_restart_sync_from_settings();
    full_system_restart_with_reserved_operation();

    return ESP_OK;
}

httpd_uri_t post_factory_reset_handler = {
    .uri = "/api/factory-reset",
    .method = HTTP_POST,
    .handler = post_factory_reset_handler_func,
    .user_ctx = NULL};

static esp_err_t get_ota_status_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }

    const char *status_str;
    const ota_status_t status = _ota_status.load();
    switch (status) {
        case OTA_DOWNLOADING: status_str = "downloading"; break;
        case OTA_FINALIZING:  status_str = "finalizing"; break;
        case OTA_SUCCESS:     status_str = "success"; break;
        case OTA_FAILED:      status_str = "failed"; break;
        default:              status_str = "idle"; break;
    }

    const char* flashPause = (_settings && _settings->getFlashPause()) ? "true" : "false";
    // The escaped OTA error can use up to twice the 128-byte source buffer.
    // Leave enough room for the surrounding JSON without truncating it.
    char buf[384];

    if (status == OTA_FAILED) {
        char error[sizeof(_ota_error)];
        copy_ota_error(error, sizeof(error));
        if (error[0] != '\0') {
            // Escape any quotes in the error string just in case
            char esc_error[sizeof(_ota_error) * 2] = {0};
            int j = 0;
            for (int i = 0; error[i] && j < sizeof(esc_error) - 2; i++) {
                if (error[i] == '"' || error[i] == '\\') {
                    esc_error[j++] = '\\';
                }
                esc_error[j++] = error[i];
            }
            snprintf(buf, sizeof(buf), "{\"status\":\"%s\",\"progress\":%d,\"flashPause\":%s,\"error\":\"%s\"}",
                     status_str, _ota_progress.load(), flashPause, esc_error);
        } else {
            snprintf(buf, sizeof(buf), "{\"status\":\"%s\",\"progress\":%d,\"flashPause\":%s}",
                     status_str, _ota_progress.load(), flashPause);
        }
    } else {
        snprintf(buf, sizeof(buf), "{\"status\":\"%s\",\"progress\":%d,\"flashPause\":%s}",
                 status_str, _ota_progress.load(), flashPause);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, buf);

    return ESP_OK;
}

httpd_uri_t get_ota_status_handler = {
    .uri = "/api/ota_status",
    .method = HTTP_GET,
    .handler = get_ota_status_handler_func,
    .user_ctx = NULL};

// Free heap for the manual firmware upload restart by shutting down heap-heavy
// subsystems.
// The ESP32-WROOM-32 has no PSRAM; with MQTT/monitoring running, only
// ~60 KB heap can remain. Keeping those workers alive while finalizing the
// uploaded image and taking Ethernet down also creates avoidable contention.
// On OTA success the device restarts and
// everything comes back; on failure the returned mask is used to resume the
// paused monitoring workers without requiring a manual restart.
static esp_err_t prepare_ota_heap(uint32_t *paused_monitoring)
{
    if (!paused_monitoring) return ESP_ERR_INVALID_ARG;
    *paused_monitoring = 0;
    ESP_LOGI(TAG, "Preparing heap for manual OTA restart (current free: %u KB)",
             (unsigned)(esp_get_free_heap_size() / 1024));

    // Stop MQTT, CheckMK, Prometheus, Syslog and notification workers. Besides
    // TLS state, this can free several task stacks (6-8 KB each) before OTA.
    esp_err_t monitoring_pause_result =
        monitoring_pause_for_ota(paused_monitoring);
    if (monitoring_pause_result != ESP_OK) {
        ESP_LOGE(TAG, "Cannot continue OTA/restart while monitoring is stopping: %s",
                 esp_err_to_name(monitoring_pause_result));
        return monitoring_pause_result;
    }

    // Note: the automatic update-check feature (former UpdateCheck esp_timer)
    // was removed, so there is no background fetch task left to stop here.

    // Brief settle for heap de-fragmentation
    vTaskDelay(pdMS_TO_TICKS(200));
    return ESP_OK;
}

// Background finalize task: stop workers, select boot partition, restart.
// Runs on its own task so the httpd thread is free to serve status polls
// during the (potentially 90 s) sequential worker-stop phase.
static void ota_finalize_task(void *pv)
{
    ota_finalize_ctx *ctx = static_cast<ota_finalize_ctx *>(pv);
    uint32_t paused_monitoring = 0;

    esp_err_t prepare_result = prepare_ota_heap(&paused_monitoring);
    if (prepare_result != ESP_OK) {
        _statusLED->setState(LED_STATE_OFF);
        _ota_status = OTA_FAILED;
        set_ota_error("Restart deferred: background network cleanup failed");
        ResetInfo::storeResetReason(RESET_REASON_UPDATE_FAILED);
        monitoring_resume_after_ota(paused_monitoring);
        ota_operation_finish();
        free(ctx);
        vTaskDelete(NULL);
        return;
    }

    esp_err_t boot_result = esp_ota_set_boot_partition(ctx->update_partition);
    const esp_partition_t *selected_boot = esp_ota_get_boot_partition();
    if (selected_boot != ctx->update_partition) {
        ESP_LOGE(TAG, "Could not select uploaded OTA partition: set=%s selected=%s",
                 esp_err_to_name(boot_result),
                 selected_boot ? selected_boot->label : "unknown");

        bool running_restored = (selected_boot == ctx->running);
        for (int attempt = 0; !running_restored && attempt < 3; ++attempt) {
            esp_err_t restore_result = esp_ota_set_boot_partition(ctx->running);
            selected_boot = esp_ota_get_boot_partition();
            running_restored =
                (restore_result == ESP_OK && selected_boot == ctx->running);
            if (!running_restored) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }

        _statusLED->setState(LED_STATE_OFF);
        _ota_status = OTA_FAILED;
        set_ota_error(running_restored
                          ? "Could not activate uploaded firmware"
                          : "Boot selection uncertain; restarting safely");
        ResetInfo::storeResetReason(RESET_REASON_UPDATE_FAILED);

        if (running_restored) {
            monitoring_resume_after_ota(paused_monitoring);
            ota_operation_finish();
            free(ctx);
            vTaskDelete(NULL);
            return;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
        refresh_restart_sync_from_settings();
        full_system_restart_with_reserved_operation();
        free(ctx);
        return;
    }

    ESP_LOGI(TAG, "OTA finished successfully, restarting to activate new firmware.");
    _statusLED->setState(LED_STATE_OFF);
    _ota_progress = 100;
    _ota_status = OTA_SUCCESS;
    ResetInfo::storeResetReason(RESET_REASON_FIRMWARE_UPDATE);

    vTaskDelay(pdMS_TO_TICKS(3000));
    refresh_restart_sync_from_settings();
    full_system_restart_with_reserved_operation();
    free(ctx);
}

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
