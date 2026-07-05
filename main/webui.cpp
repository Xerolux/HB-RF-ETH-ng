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
#include <inttypes.h>
#include <sys/param.h>
#include <atomic>
#include <new>
#include <stdarg.h>
#include "webui.h"
#include "esp_log.h"
#include "esp_system.h"
#include "cJSON.h"
#include "esp_ota_ops.h"
#include "mbedtls/md.h"
#include "mbedtls/base64.h"
#include "monitoring_api.h"
#include "monitoring.h"
#include "rate_limiter.h"
#include "security_headers.h"
#include "secure_utils.h"
#include "log_manager.h"
#include "reset_info.h"
#include "system_reset.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "ota_config.h"
#include "semver.h"
#include "validation.h"
// #include "prometheus.h"

static const char *TAG = "WebUI";

// Safe cJSON number accessor: returns the integer value only when the item
// exists and is actually a number. Prevents undefined behaviour when the
// frontend sends an unexpected type.
static inline int cJSON_GetIntValueSafe(cJSON *item, int defaultValue)
{
    return (item && cJSON_IsNumber(item)) ? item->valueint : defaultValue;
}

#define EMBED_HANDLER(_uri, _resource, _contentType)                   \
    extern const char _resource[] asm("_binary_" #_resource "_start"); \
    extern const size_t _resource##_length asm(#_resource "_length");  \
    esp_err_t _resource##_handler_func(httpd_req_t *req)               \
    {                                                                  \
        add_security_headers(req);                                     \
        httpd_resp_set_type(req, _contentType);                        \
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");           \
        httpd_resp_send(req, _resource, _resource##_length);           \
        return ESP_OK;                                                 \
    };                                                                 \
    httpd_uri_t _resource##_handler = {                                \
        .uri = _uri,                                                   \
        .method = HTTP_GET,                                            \
        .handler = _resource##_handler_func,                           \
        .user_ctx = NULL};

EMBED_HANDLER("/*", index_html_gz, "text/html")
EMBED_HANDLER("/main.js", main_js_gz, "application/javascript")
EMBED_HANDLER("/main.css", main_css_gz, "text/css")
EMBED_HANDLER("/favicon.ico", favicon_ico_gz, "image/x-icon")

static Settings *_settings;
static LED *_statusLED;
static SysInfo *_sysInfo;
static UpdateCheck *_updateCheck;
static Ethernet *_ethernet;
static RawUartUdpListener *_rawUartUdpListener;
static RadioModuleConnector *_radioModuleConnector;
static RadioModuleDetector *_radioModuleDetector;
static char _token[46];

static void emit_log_enable_snapshot();

void generateToken()
{
    // Try persisted token first (survives reboots — keeps "remember me" valid
    // across firmware updates and restarts).
    if (_settings && _settings->loadAdminToken(_token, sizeof(_token))) {
        return;
    }

    char tokenBase[21];
    uint32_t rnd[2] = {esp_random(), esp_random()};
    memcpy(tokenBase, rnd, sizeof(rnd));
    strncpy(tokenBase + 2 * sizeof(uint32_t), _sysInfo->getSerialNumber(), sizeof(tokenBase) - 2 * sizeof(uint32_t) - 1);
    tokenBase[sizeof(tokenBase) - 1] = '\0';

    unsigned char shaResult[32];

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (unsigned char *)tokenBase, 20);
    mbedtls_md_finish(&ctx, shaResult);
    mbedtls_md_free(&ctx);

    size_t tokenLength;
    mbedtls_base64_encode((unsigned char *)_token, sizeof(_token), &tokenLength, shaResult, sizeof(shaResult));
    _token[tokenLength] = 0;

    if (_settings) {
        _settings->saveAdminToken(_token);
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
    char auth[60] = {0};
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth, sizeof(auth)) != ESP_OK)
        return ESP_FAIL;

    if (strncmp(auth, "Token ", 6) != 0)
        return ESP_FAIL;

    if (secure_strcmp(auth + 6, _token) != 0)
        return ESP_FAIL;

    return ESP_OK;
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

    // Check rate limit, but allow whitelisted IP
    // Prioritize manual setting, fallback to dynamic raw uart listener
    ip4_addr_t ccu_ip = {0};
    const char* storedCCUIP = _settings->getCCUIP();

    if (storedCCUIP && strlen(storedCCUIP) > 0) {
        ip4addr_aton(storedCCUIP, &ccu_ip);
    } else {
        ccu_ip = _rawUartUdpListener->getConnectedRemoteAddress();
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

esp_err_t get_sysinfo_json_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");

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

    cJSON *root = cJSON_CreateObject();
    cJSON *sysInfo = cJSON_AddObjectToObject(root, "sysInfo");

    cJSON_AddStringToObject(sysInfo, "serial", _sysInfo->getSerialNumber());
    cJSON_AddStringToObject(sysInfo, "hostname", _settings->getHostname());
    cJSON_AddStringToObject(sysInfo, "currentVersion", _sysInfo->getCurrentVersion());
    cJSON_AddStringToObject(sysInfo, "latestVersion", _updateCheck->getLatestVersion());
    cJSON_AddNumberToObject(sysInfo, "memoryUsage", _sysInfo->getMemoryUsage());
    cJSON_AddNumberToObject(sysInfo, "cpuUsage", _sysInfo->getCpuUsage());
    // The HB-RF-ETH board exposes neither a supply-voltage divider on GPIO37
    // nor a temperature sensor. Keep the legacy keys for API compatibility,
    // but never report floating ADC readings or sentinel temperatures.
    cJSON_AddNullToObject(sysInfo, "supplyVoltage");
    cJSON_AddNullToObject(sysInfo, "temperature");
    cJSON_AddNumberToObject(sysInfo, "uptimeSeconds", _sysInfo->getUptimeSeconds());
    cJSON_AddStringToObject(sysInfo, "boardRevision", _sysInfo->getBoardRevisionString());
    cJSON_AddStringToObject(sysInfo, "resetReason", _sysInfo->getResetReason());
    cJSON_AddBoolToObject(sysInfo, "ethernetConnected", _ethernet->isConnected());
    cJSON_AddNumberToObject(sysInfo, "ethernetSpeed", _ethernet->getLinkSpeedMbps());
    cJSON_AddStringToObject(sysInfo, "ethernetDuplex", _ethernet->getDuplexMode());

    // Current live network configuration (DHCP-assigned or static).
    ip4_addr_t currentIP, currentNM, currentGW, currentDNS1, currentDNS2;
    _ethernet->getNetworkSettings(&currentIP, &currentNM, &currentGW, &currentDNS1, &currentDNS2);
    cJSON_AddStringToObject(sysInfo, "localIP", ip2str(currentIP));
    cJSON_AddStringToObject(sysInfo, "netmask", ip2str(currentNM));
    cJSON_AddStringToObject(sysInfo, "gateway", ip2str(currentGW));
    cJSON_AddStringToObject(sysInfo, "dns1", ip2str(currentDNS1));
    cJSON_AddStringToObject(sysInfo, "dns2", ip2str(currentDNS2));

    // IPv6 addresses (link-local + global)
    char ipv6_addrs[4][48];
    int ipv6_count = _ethernet->getIPv6AddressStrings(ipv6_addrs, 4);
    cJSON *ipv6Array = cJSON_AddArrayToObject(sysInfo, "ipv6Addresses");
    for (int i = 0; i < ipv6_count; i++) {
        cJSON_AddItemToArray(ipv6Array, cJSON_CreateString(ipv6_addrs[i]));
    }

    cJSON_AddStringToObject(sysInfo, "rawUartRemoteAddress", ip2str(_rawUartUdpListener->getConnectedRemoteAddress()));
    cJSON_AddStringToObject(sysInfo, "radioModuleType", radioModuleTypeStr);
    cJSON_AddStringToObject(sysInfo, "radioModuleSerial", _radioModuleDetector->getSerial());
    cJSON_AddStringToObject(sysInfo, "radioModuleFirmwareVersion", fwVersionStr);
    cJSON_AddStringToObject(sysInfo, "radioModuleBidCosRadioMAC", bidCosMAC);
    cJSON_AddStringToObject(sysInfo, "radioModuleHmIPRadioMAC", hmIPMAC);
    cJSON_AddStringToObject(sysInfo, "radioModuleSGTIN", _radioModuleDetector->getSGTIN());

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

httpd_uri_t get_sysinfo_json_handler = {
    .uri = "/sysinfo.json",
    .method = HTTP_GET,
    .handler = get_sysinfo_json_handler_func,
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

    cJSON_AddBoolToObject(settings, "betaChannel", _settings->getBetaChannel());
    cJSON_AddBoolToObject(settings, "systemLogEnabled", _settings->getSystemLogEnabled());
    cJSON_AddBoolToObject(settings, "flashPause", _settings->getFlashPause());
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

void delayed_restart_task(void *pvParameter) {
    vTaskDelay(pdMS_TO_TICKS(3000));
    full_system_restart();
    vTaskDelete(NULL);
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

    char *buffer = (char *)malloc(4096);
    if (!buffer) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
    }

    int len = recv_full_body(req, buffer, 4096);

    if (len <= 0) {
        free(buffer);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data received");
    }

    cJSON *root = cJSON_Parse(buffer);
    free(buffer);
    buffer = NULL;

    if (!root) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    char *hostname = cJSON_GetStringValue(cJSON_GetObjectItem(root, "hostname"));
    bool useDHCP = cJSON_GetBoolValue(cJSON_GetObjectItem(root, "useDHCP"));
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
    if (cJSON_GetObjectItem(root, "ledBrightness") != NULL) {
        LED::setBrightness(ledBrightness);
    }

    cJSON *ledPrograms = cJSON_GetObjectItem(root, "ledPrograms");
    if (ledPrograms) {
        cJSON *item;
        int value;
        item = cJSON_GetObjectItem(ledPrograms, "idle");
        value = cJSON_GetIntValueSafe(item, -1);
        if (value >= 0) {
            _settings->setLedProgram(LED_PROG_IDLE, value);
            LED::setProgram(LED_PROG_IDLE, (led_state_t)value);
        }
        item = cJSON_GetObjectItem(ledPrograms, "ccu_disconnected");
        value = cJSON_GetIntValueSafe(item, -1);
        if (value >= 0) {
            _settings->setLedProgram(LED_PROG_CCU_DISCONNECTED, value);
            LED::setProgram(LED_PROG_CCU_DISCONNECTED, (led_state_t)value);
        }
        item = cJSON_GetObjectItem(ledPrograms, "ccu_connected");
        value = cJSON_GetIntValueSafe(item, -1);
        if (value >= 0) {
            _settings->setLedProgram(LED_PROG_CCU_CONNECTED, value);
            LED::setProgram(LED_PROG_CCU_CONNECTED, (led_state_t)value);
        }
        item = cJSON_GetObjectItem(ledPrograms, "update_available");
        value = cJSON_GetIntValueSafe(item, -1);
        if (value >= 0) {
            _settings->setLedProgram(LED_PROG_UPDATE_AVAILABLE, value);
            LED::setProgram(LED_PROG_UPDATE_AVAILABLE, (led_state_t)value);
        }
        item = cJSON_GetObjectItem(ledPrograms, "error");
        value = cJSON_GetIntValueSafe(item, -1);
        if (value >= 0) {
            _settings->setLedProgram(LED_PROG_ERROR, value);
            LED::setProgram(LED_PROG_ERROR, (led_state_t)value);
        }
        item = cJSON_GetObjectItem(ledPrograms, "booting");
        value = cJSON_GetIntValueSafe(item, -1);
        if (value >= 0) {
            _settings->setLedProgram(LED_PROG_BOOTING, value);
            LED::setProgram(LED_PROG_BOOTING, (led_state_t)value);
        }
        item = cJSON_GetObjectItem(ledPrograms, "update_in_progress");
        value = cJSON_GetIntValueSafe(item, -1);
        if (value >= 0) {
            _settings->setLedProgram(LED_PROG_UPDATE_IN_PROGRESS, value);
            LED::setProgram(LED_PROG_UPDATE_IN_PROGRESS, (led_state_t)value);
        }
    }

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

    if (adminUsername && adminUsername[0] != '\0') {
        if (!_settings->setAdminUsername(adminUsername)) {
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                       "Username must be 1-32 characters using letters, numbers, dot, dash, or underscore");
        }
    }

    if (adminPassword && adminPassword[0] != '\0') {
        if (!validateAdminPassword(adminPassword) || !_settings->setAdminPassword(adminPassword)) {
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                       "Password must be 8-32 characters with uppercase, lowercase, and numbers");
        }
    }

    if (hostname) {
        if (!_settings->setNetworkSettings(hostname, useDHCP, localIP, netmask, gateway, dns1, dns2)) {
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid network settings");
        }
    }
    _settings->setTimesource(timesource);
    _settings->setDcfOffset(dcfOffset);
    _settings->setGpsBaudrate(gpsBaudrate);
    if (ntpServer) {
        _settings->setNtpServer(ntpServer);
    }
    _settings->setLEDBrightness(ledBrightness);

    // Handle IPv6 when any IPv6-related field is present so toggling
    // enableIPv6 to false without sending ipv6Mode still persists.
    if (cJSON_GetObjectItem(root, "enableIPv6") ||
        cJSON_GetObjectItem(root, "ipv6Mode") ||
        cJSON_GetObjectItem(root, "ipv6Address") ||
        cJSON_GetObjectItem(root, "ipv6PrefixLength") ||
        cJSON_GetObjectItem(root, "ipv6Gateway") ||
        cJSON_GetObjectItem(root, "ipv6Dns1") ||
        cJSON_GetObjectItem(root, "ipv6Dns2")) {
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

    // Beta update channel toggle. Optional - omitted when not changed.
    cJSON *betaChannelItem = cJSON_GetObjectItem(root, "betaChannel");
    if (betaChannelItem && cJSON_IsBool(betaChannelItem)) {
        _settings->setBetaChannel(cJSON_IsTrue(betaChannelItem));
    }

    cJSON *systemLogEnabledItem = cJSON_GetObjectItem(root, "systemLogEnabled");
    if (systemLogEnabledItem && cJSON_IsBool(systemLogEnabledItem)) {
        _settings->setSystemLogEnabled(cJSON_IsTrue(systemLogEnabledItem));
        if (cJSON_IsTrue(systemLogEnabledItem)) {
            LogManager::begin(8192);
            if (LogManager::instance().isEnabled()) {
                emit_log_enable_snapshot();
            }
        } else {
            LogManager::stop();
        }

    cJSON *flashPauseItem = cJSON_GetObjectItem(root, "flashPause");
    if (flashPauseItem && cJSON_IsBool(flashPauseItem)) {
        _settings->setFlashPause(cJSON_IsTrue(flashPauseItem));
        set_flash_pause_enabled(cJSON_IsTrue(flashPauseItem));
    }
    }

    _settings->save();

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
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"settings.json\"");

    cJSON *root = cJSON_CreateObject();

    // NOTE: Admin password is intentionally excluded from backup for security.
    // The admin username is included because it is not secret and is needed to
    // restore password-manager friendly login settings across devices.
    // On restore, the current password will be preserved unless explicitly changed.

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

esp_err_t post_restore_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    char *buffer = (char*)malloc(4096);
    if (!buffer) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
    }

    int len = recv_full_body(req, buffer, 4096);

    if (len <= 0)
    {
        free(buffer);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data received");
    }

    cJSON *root = cJSON_Parse(buffer);
    free(buffer);

    if (!root) {
         return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    char *adminUsername = cJSON_GetStringValue(cJSON_GetObjectItem(root, "adminUsername"));
    char *adminPassword = cJSON_GetStringValue(cJSON_GetObjectItem(root, "adminPassword"));

    char *hostname = cJSON_GetStringValue(cJSON_GetObjectItem(root, "hostname"));
    bool useDHCP = cJSON_GetBoolValue(cJSON_GetObjectItem(root, "useDHCP"));
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
    if (cJSON_GetObjectItem(root, "ledBrightness") != NULL) {
        LED::setBrightness(ledBrightness);
    }

    cJSON *ledPrograms = cJSON_GetObjectItem(root, "ledPrograms");
    if (ledPrograms) {
        cJSON *item;
        int value;
        item = cJSON_GetObjectItem(ledPrograms, "idle");
        value = cJSON_GetIntValueSafe(item, -1);
        if (value >= 0) _settings->setLedProgram(LED_PROG_IDLE, value);
        item = cJSON_GetObjectItem(ledPrograms, "ccu_disconnected");
        value = cJSON_GetIntValueSafe(item, -1);
        if (value >= 0) _settings->setLedProgram(LED_PROG_CCU_DISCONNECTED, value);
        item = cJSON_GetObjectItem(ledPrograms, "ccu_connected");
        value = cJSON_GetIntValueSafe(item, -1);
        if (value >= 0) _settings->setLedProgram(LED_PROG_CCU_CONNECTED, value);
        item = cJSON_GetObjectItem(ledPrograms, "update_available");
        value = cJSON_GetIntValueSafe(item, -1);
        if (value >= 0) _settings->setLedProgram(LED_PROG_UPDATE_AVAILABLE, value);
        item = cJSON_GetObjectItem(ledPrograms, "error");
        value = cJSON_GetIntValueSafe(item, -1);
        if (value >= 0) _settings->setLedProgram(LED_PROG_ERROR, value);
        item = cJSON_GetObjectItem(ledPrograms, "booting");
        value = cJSON_GetIntValueSafe(item, -1);
        if (value >= 0) _settings->setLedProgram(LED_PROG_BOOTING, value);
        item = cJSON_GetObjectItem(ledPrograms, "update_in_progress");
        value = cJSON_GetIntValueSafe(item, -1);
        if (value >= 0) _settings->setLedProgram(LED_PROG_UPDATE_IN_PROGRESS, value);
    }

    // IPv6
    bool enableIPv6 = cJSON_GetBoolValue(cJSON_GetObjectItem(root, "enableIPv6"));
    char *ipv6Mode = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Mode"));
    char *ipv6Address = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Address"));
    int ipv6PrefixLength = cJSON_GetIntValueSafe(
        cJSON_GetObjectItem(root, "ipv6PrefixLength"), 64);
    char *ipv6Gateway = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Gateway"));
    char *ipv6Dns1 = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Dns1"));
    char *ipv6Dns2 = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Dns2"));

    if (adminUsername && adminUsername[0] != '\0') {
        if (!_settings->setAdminUsername(adminUsername)) {
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                       "Username must be 1-32 characters using letters, numbers, dot, dash, or underscore");
        }
    }

    if (adminPassword && adminPassword[0] != '\0') {
        if (!validateAdminPassword(adminPassword) || !_settings->setAdminPassword(adminPassword)) {
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                       "Password must be 8-32 characters with uppercase, lowercase, and numbers");
        }
    }

    if (hostname) {
        if (!_settings->setNetworkSettings(hostname, useDHCP, localIP, netmask, gateway, dns1, dns2)) {
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid network settings");
        }
    }
    _settings->setTimesource(timesource);
    _settings->setDcfOffset(dcfOffset);
    _settings->setGpsBaudrate(gpsBaudrate);
    if (ntpServer) {
        _settings->setNtpServer(ntpServer);
    }
    _settings->setLEDBrightness(ledBrightness);

    if (cJSON_GetObjectItem(root, "enableIPv6") ||
        cJSON_GetObjectItem(root, "ipv6Mode") ||
        cJSON_GetObjectItem(root, "ipv6Address") ||
        cJSON_GetObjectItem(root, "ipv6PrefixLength") ||
        cJSON_GetObjectItem(root, "ipv6Gateway") ||
        cJSON_GetObjectItem(root, "ipv6Dns1") ||
        cJSON_GetObjectItem(root, "ipv6Dns2")) {
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

    char *ccuIP = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ccuIP"));
    if (ccuIP) {
        _settings->setCCUIP(ccuIP);
    }

    // Beta update channel toggle (optional in backup payload).
    cJSON *betaChannelItem = cJSON_GetObjectItem(root, "betaChannel");
    if (betaChannelItem && cJSON_IsBool(betaChannelItem)) {
        _settings->setBetaChannel(cJSON_IsTrue(betaChannelItem));
    }

    cJSON *systemLogEnabledItem = cJSON_GetObjectItem(root, "systemLogEnabled");
    if (systemLogEnabledItem && cJSON_IsBool(systemLogEnabledItem)) {
        _settings->setSystemLogEnabled(cJSON_IsTrue(systemLogEnabledItem));
    }

    cJSON *flashPauseItem = cJSON_GetObjectItem(root, "flashPause");
    if (flashPauseItem && cJSON_IsBool(flashPauseItem)) {
        _settings->setFlashPause(cJSON_IsTrue(flashPauseItem));
        set_flash_pause_enabled(cJSON_IsTrue(flashPauseItem));
    }

    _settings->save();

    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\":true}");

    // Restart
    vTaskDelay(pdMS_TO_TICKS(1000));
    full_system_restart();

    return ESP_OK;
}

httpd_uri_t post_restore_handler = {
    .uri = "/api/restore",
    .method = HTTP_POST,
    .handler = post_restore_handler_func,
    .user_ctx = NULL};

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

// OTA status tracking - shared between the push upload handler (/ota_update)
// and the URL download handler (/api/ota_url) so the two cannot write the
// update partition concurrently.
enum ota_status_t {
    OTA_IDLE = 0,
    OTA_DOWNLOADING,
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

    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    if (!_updateCheck->tryBeginOtaOperation())
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "OTA update already in progress");
        return ESP_OK;
    }
    _ota_status = OTA_DOWNLOADING;
    _ota_progress = 0;
    set_ota_error("");

    esp_ota_handle_t ota_handle = 0;
    bool ota_begun = false;

    char *ota_buff = (char *)malloc(OTA_BUFFER_SIZE);
    if (!ota_buff) {
        ESP_LOGE(TAG, "Failed to allocate OTA buffer");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        _ota_status = OTA_FAILED;
        _updateCheck->finishOtaOperation();
        return ESP_FAIL;
    }

    int content_length = req->content_len;
    int content_received = 0;
    int recv_len;
    int timeout_retries = 5;
    bool is_req_body_started = false;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    const esp_partition_t *running = NULL;

    // Validate update partition exists
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA update partition found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition available");
        goto err;
    }

    ESP_LOGI(TAG, "Starting OTA update, partition: %s, size: %d bytes", update_partition->label, content_length);

    do
    {
        if ((recv_len = httpd_req_recv(req, ota_buff, MIN(content_length - content_received, OTA_BUFFER_SIZE))) < 0)
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
                ESP_LOGE(TAG, "OTA socket error %d, received %d of %d bytes", recv_len, content_received, content_length);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Network error during upload");
                goto err;
            }
        }
        else if (recv_len == 0)
        {
            // Connection closed by client
            ESP_LOGE(TAG, "OTA connection closed prematurely, received %d of %d bytes", content_received, content_length);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Incomplete upload");
            goto err;
        }

        if (!is_req_body_started)
        {
            is_req_body_started = true;

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
            ESP_LOGW(TAG, "Begin OTA Update to partition %s, File Size: %d", update_partition->label, content_length);
            _statusLED->setState(LED_STATE_BLINK_FAST);

            OTA_CHECK(esp_ota_write(ota_handle, ota_buff, recv_len) == ESP_OK, "Error writing OTA");
            content_received += recv_len;
            _ota_progress = (int)((int64_t)content_received * 100 / content_length);
            ESP_LOGI(TAG, "OTA progress: %d / %d bytes (%d%%)", content_received, content_length, (content_received * 100) / content_length);
        }
        else
        {
            OTA_CHECK(esp_ota_write(ota_handle, ota_buff, recv_len) == ESP_OK, "Error writing OTA");
            content_received += recv_len;
            _ota_progress = (int)((int64_t)content_received * 100 / content_length);
            ESP_LOGI(TAG, "OTA progress: %d / %d bytes (%d%%)", content_received, content_length, (content_received * 100) / content_length);
        }
    } while (content_received < content_length);

    // Verify complete firmware was received
    if (content_received != content_length) {
        ESP_LOGE(TAG, "Incomplete firmware: received %d of %d bytes", content_received, content_length);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Incomplete firmware upload");
        goto err;
    }

    // Validate and finalize OTA
    OTA_CHECK(esp_ota_end(ota_handle) == ESP_OK, "Error finalizing OTA");
    ota_begun = false;  // Successfully ended, don't abort

    // Verify the firmware image before setting boot partition
    ESP_LOGI(TAG, "Validating firmware image...");
    running = esp_ota_get_running_partition();

    if (update_partition == running) {
        ESP_LOGE(TAG, "Cannot update running partition!");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid OTA partition");
        goto err;
    }

    OTA_CHECK(esp_ota_set_boot_partition(update_partition) == ESP_OK, "Error setting boot partition");

    ESP_LOGI(TAG, "OTA finished successfully, restarting in 3 seconds to activate new firmware.");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\":true,\"message\":\"Firmware update completed, restarting in 3 seconds...\"}");

    _statusLED->setState(LED_STATE_OFF);
    _ota_progress = 100;
    _ota_status = OTA_SUCCESS;

    // Store reset reason for successful firmware update
    ResetInfo::storeResetReason(RESET_REASON_FIRMWARE_UPDATE);

    // Automatic restart after successful OTA update
    if (xTaskCreate(delayed_restart_task, "restart_task", 2048, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Could not create delayed restart task after successful OTA");
        _updateCheck->finishOtaOperation();
    }

    free(ota_buff);
    return ESP_OK;

err:
    if (ota_buff) free(ota_buff);
    _statusLED->setState(LED_STATE_OFF);
    _ota_status = OTA_FAILED;

    // Abort OTA if it was started but not completed
    if (ota_begun) {
        ESP_LOGW(TAG, "Aborting OTA operation due to error");
        esp_ota_abort(ota_handle);
    }

    // Store reset reason for failed firmware update
    ResetInfo::storeResetReason(RESET_REASON_UPDATE_FAILED);
    _updateCheck->finishOtaOperation();
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

    // Restart after a short delay to allow response to be sent
    vTaskDelay(pdMS_TO_TICKS(1000));
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

    httpd_resp_set_type(req, "application/json");
    /* CORS header removed - same-origin requests only */
    httpd_resp_sendstr(req, "{\"success\":true}");

    // Store reset reason before factory reset
    ResetInfo::storeResetReason(RESET_REASON_FACTORY_RESET);

    // Perform factory reset - clear settings
    _settings->clear();

    // Restart after a short delay to allow response to be sent
    vTaskDelay(pdMS_TO_TICKS(1000));
    full_system_restart();

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

    cJSON *root = cJSON_CreateObject();

    const char *status_str;
    const ota_status_t status = _ota_status.load();
    switch (status) {
        case OTA_DOWNLOADING: status_str = "downloading"; break;
        case OTA_SUCCESS:     status_str = "success"; break;
        case OTA_FAILED:      status_str = "failed"; break;
        default:              status_str = "idle"; break;
    }

    cJSON_AddStringToObject(root, "status", status_str);
    cJSON_AddNumberToObject(root, "progress", _ota_progress.load());
    if (status == OTA_FAILED) {
        char error[sizeof(_ota_error)];
        copy_ota_error(error, sizeof(error));
        if (error[0] != '\0') {
            cJSON_AddStringToObject(root, "error", error);
        }
    }

    char *json_string = cJSON_Print(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    if (json_string)
    {
        httpd_resp_sendstr(req, json_string);
        free(json_string);
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON allocation failed");
    }

    return ESP_OK;
}

httpd_uri_t get_ota_status_handler = {
    .uri = "/api/ota_status",
    .method = HTTP_GET,
    .handler = get_ota_status_handler_func,
    .user_ctx = NULL};

// OTA update from URL handler
static esp_err_t post_ota_url_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }

    // Read request body to get URL
    char buffer[512];
    int len = recv_full_body(req, buffer, sizeof(buffer));

    if (len <= 0)
    {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"Invalid request\"}");
        return ESP_OK;
    }

    cJSON *root = cJSON_Parse(buffer);
    if (root == NULL)
    {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return ESP_OK;
    }

    char *url_json = cJSON_GetStringValue(cJSON_GetObjectItem(root, "url"));

    // Copy URL before freeing JSON to avoid use-after-free
    char url_buf[256] = {0};
    if (url_json != NULL && strlen(url_json) > 0) {
        // Firmware downloads must be authenticated by TLS.
        if (strncmp(url_json, "https://", 8) != 0) {
            cJSON_Delete(root);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"Firmware URL must use HTTPS\"}");
            return ESP_OK;
        }
        if (strlen(url_json) >= sizeof(url_buf)) {
            cJSON_Delete(root);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"URL too long\"}");
            return ESP_OK;
        }
        strncpy(url_buf, url_json, sizeof(url_buf) - 1);
    }

    cJSON_Delete(root);

    if (url_buf[0] == '\0')
    {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"URL is required\"}");
        return ESP_OK;
    }

    // Reject concurrent OTA starts - two tasks writing the same update
    // partition would corrupt the image.
    if (!_updateCheck->tryBeginOtaOperation())
    {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"OTA update already in progress\"}");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting OTA update from URL: %s", url_buf);

    // Mark as downloading before the response is sent so a second request
    // arriving immediately afterwards is rejected.
    _ota_status = OTA_DOWNLOADING;
    _ota_progress = 0;

    // Create a task to perform the update (it's blocking)
    struct TaskArgs {
        char* url;
        LED* statusLED;
    };

    TaskArgs* args = new (std::nothrow) TaskArgs();
    if (args == NULL) {
        ESP_LOGE(TAG, "Failed to allocate OTA task arguments");
        _ota_status = OTA_IDLE;
        _updateCheck->finishOtaOperation();
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }
    args->url = strdup(url_buf);
    if (args->url == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for URL");
        delete args;
        _ota_status = OTA_IDLE;
        _updateCheck->finishOtaOperation();
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }
    args->statusLED = _statusLED;

    BaseType_t ret = xTaskCreate([](void* p) {
        TaskArgs* a = (TaskArgs*)p;

        ESP_LOGI(TAG, "OTA task started, downloading from: %s", a->url);

        _ota_status = OTA_DOWNLOADING;
        _ota_progress = 0;
        set_ota_error("");

        bool net_locked = false;
        if (g_net_fetch_mutex) {
            if (xSemaphoreTake(g_net_fetch_mutex, pdMS_TO_TICKS(30000)) != pdTRUE) {
                ESP_LOGE(TAG, "OTA URL update could not start: HTTPS subsystem busy");
                set_ota_error("HTTPS subsystem busy");
                _ota_status = OTA_FAILED;
                a->statusLED->setState(LED_STATE_ON);
                free(a->url);
                delete a;
                _updateCheck->finishOtaOperation();
                vTaskDelete(NULL);
                return;
            }
            net_locked = true;
        }

        esp_http_client_config_t config = {};
        configure_ota_http_client(config, a->url);
        config.timeout_ms = 30000;
        config.buffer_size = 4096;

        esp_https_ota_config_t ota_config = {};
        ota_config.http_config = &config;

        a->statusLED->setState(LED_STATE_BLINK_FAST);

        // Use advanced OTA API for progress tracking
        esp_https_ota_handle_t ota_handle = NULL;
        esp_err_t ret = esp_https_ota_begin(&ota_config, &ota_handle);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(ret));
            set_ota_error("OTA begin failed: %s", esp_err_to_name(ret));
            _ota_status = OTA_FAILED;
            a->statusLED->setState(LED_STATE_ON);
            if (net_locked) xSemaphoreGive(g_net_fetch_mutex);
            free(a->url);
            delete a;
            _updateCheck->finishOtaOperation();
            vTaskDelete(NULL);
            return;
        }

        int image_size = esp_https_ota_get_image_size(ota_handle);
        ESP_LOGI(TAG, "OTA image size: %d bytes", image_size);

        while (true) {
            ret = esp_https_ota_perform(ota_handle);
            if (ret != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
                break;
            }
            // Update progress
            if (image_size > 0) {
                int downloaded = esp_https_ota_get_image_len_read(ota_handle);
                _ota_progress = (int)((int64_t)downloaded * 100 / image_size);
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        bool complete = esp_https_ota_is_complete_data_received(ota_handle);
        if (ret == ESP_OK && complete) {
            _ota_progress = 100;
            ret = esp_https_ota_finish(ota_handle);
        } else {
            esp_https_ota_abort(ota_handle);
            if (ret == ESP_OK) {
                ret = ESP_ERR_INVALID_SIZE;
                set_ota_error("OTA download incomplete");
            }
        }

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "OTA Update successful, restarting...");
            _ota_status = OTA_SUCCESS;
            ResetInfo::storeResetReason(RESET_REASON_FIRMWARE_UPDATE);
            a->statusLED->setState(LED_STATE_OFF);
            if (net_locked) xSemaphoreGive(g_net_fetch_mutex);
            free(a->url);
            delete a;
            _updateCheck->finishOtaOperation();
            vTaskDelay(pdMS_TO_TICKS(1000));
            full_system_restart();
        } else {
            ESP_LOGE(TAG, "OTA Update failed: %s", esp_err_to_name(ret));
            set_ota_error("OTA failed: %s", esp_err_to_name(ret));
            _ota_status = OTA_FAILED;
            ResetInfo::storeResetReason(RESET_REASON_UPDATE_FAILED);
            a->statusLED->setState(LED_STATE_ON);
            if (net_locked) xSemaphoreGive(g_net_fetch_mutex);
            free(a->url);
            delete a;
            _updateCheck->finishOtaOperation();
        }
        vTaskDelete(NULL);
    }, "ota_url_update", 16384, args, 5, NULL);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA update task");
        free(args->url);
        delete args;
        _ota_status = OTA_IDLE;
        _updateCheck->finishOtaOperation();
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not start OTA task");
    }

    // Report success only after all allocations succeeded and the worker task
    // is running. The previous ordering returned success even on OOM.
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\":true,\"message\":\"OTA update started\"}");

    return ESP_OK;
}

httpd_uri_t post_ota_url_handler = {
    .uri = "/api/ota_url",
    .method = HTTP_POST,
    .handler = post_ota_url_handler_func,
    .user_ctx = NULL};

esp_err_t post_change_password_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }

    char buffer[512];
    int len = recv_full_body(req, buffer, sizeof(buffer));

    if (len <= 0)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
    }

    cJSON *root = cJSON_Parse(buffer);
    if (root == NULL)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    char *currentPassword = cJSON_GetStringValue(cJSON_GetObjectItem(root, "currentPassword"));
    char *newPassword = cJSON_GetStringValue(cJSON_GetObjectItem(root, "newPassword"));

    // Require the current password for re-authentication so a briefly
    // unlocked session cannot be used to change the password permanently.
    if (currentPassword == NULL || currentPassword[0] == '\0')
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Current password is required");
    }
    if (secure_strcmp(currentPassword, _settings->getAdminPassword()) != 0)
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Current password is incorrect");
    }

    if (!validateAdminPassword(newPassword))
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Password must be 8-32 characters with uppercase, lowercase, and numbers");
    }

    if (!_settings->setAdminPassword(newPassword))
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid password");
    }
    _settings->save();

    cJSON_Delete(root);

    // Re-generate token for security (clear old persisted one first so
    // generateToken() creates a fresh token and persists it).
    _settings->clearAdminToken();
    generateToken();

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


// ---- Async proxy for external HTTPS fetches --------------------------------
// /api/check_update, /api/changelog and /api/firmware_archive relay content
// from external servers.
// The fetch (DNS + TLS handshake + download, up to 10 s) must not run inside
// the httpd task: esp_http_server is single-threaded, so every other request
// (login, sysinfo polling, OTA status) would stall for the duration. The
// handler detaches the request with the async handler API and a short-lived
// worker task streams the upstream body to the client.

struct AsyncProxyJob {
    httpd_req_t *req;          // async copy of the request
    const char *url;
    const char *content_type;
    const char *error_message; // sent to the client if the upstream fetch fails
    bool failed;
    bool header_sent;
};

// Only one upstream fetch at a time - each worker needs ~9 KB task stack for
// the TLS handshake and these requests are rare (manual checks, 24 h timer).
static std::atomic<bool> _proxy_busy{false};

static esp_err_t _proxy_http_event_handler(esp_http_client_event_t *evt)
{
    AsyncProxyJob *job = (AsyncProxyJob *)evt->user_data;

    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (!job->failed && esp_http_client_get_status_code(evt->client) == 200) {
                esp_err_t err = httpd_resp_send_chunk(job->req, (const char *)evt->data, evt->data_len);
                if (err != ESP_OK) {
                    job->failed = true;
                    return ESP_FAIL;
                }
                job->header_sent = true;
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void _async_proxy_task(void *arg)
{
    AsyncProxyJob *job = (AsyncProxyJob *)arg;

    // Serialize with UpdateCheck background fetch so two TLS handshakes never
    // exhaust the heap at once. Never continue unlocked after a timeout.
    bool net_locked = false;
    if (g_net_fetch_mutex) {
        if (xSemaphoreTake(g_net_fetch_mutex, pdMS_TO_TICKS(15000)) != pdTRUE) {
            ESP_LOGW(TAG, "External proxy rejected: HTTPS subsystem busy");
            httpd_resp_set_status(job->req, "503 Service Unavailable");
            httpd_resp_set_type(job->req, "text/plain");
            httpd_resp_sendstr(job->req, "HTTPS subsystem busy, try again shortly");
            httpd_req_async_handler_complete(job->req);
            free(job);
            _proxy_busy.store(false);
            vTaskDelete(NULL);
            return;
        }
        net_locked = true;
    }

    esp_http_client_config_t config = {};
    configure_ota_http_client(config, job->url);
    config.event_handler = _proxy_http_event_handler;
    config.user_data = job;
    config.timeout_ms = 10000;
    config.buffer_size = 4096;

    httpd_resp_set_type(job->req, job->content_type);
    httpd_resp_set_hdr(job->req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        client = esp_http_client_init(&config);
    }
    esp_err_t err = client ? esp_http_client_perform(client) : ESP_ERR_NO_MEM;
    int status_code = client ? esp_http_client_get_status_code(client) : 0;

    if ((err == ESP_OK && status_code == 200) || job->header_sent) {
        httpd_resp_send_chunk(job->req, NULL, 0);
        if (err != ESP_OK || status_code != 200) {
            ESP_LOGE(TAG, "%s (%s, HTTP %d)", job->error_message, esp_err_to_name(err), status_code);
        }
    } else {
        ESP_LOGE(TAG, "%s (%s, HTTP %d)", job->error_message, esp_err_to_name(err), status_code);
        httpd_resp_send_err(job->req, HTTPD_500_INTERNAL_SERVER_ERROR, job->error_message);
    }

    if (client) {
        esp_http_client_cleanup(client);
    }
    if (net_locked) xSemaphoreGive(g_net_fetch_mutex);

    httpd_req_async_handler_complete(job->req);
    free(job);
    _proxy_busy.store(false);
    vTaskDelete(NULL);
}

static esp_err_t start_async_proxy(httpd_req_t *req, const char *url, const char *content_type, const char *error_message)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK) {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    bool expected = false;
    if (!_proxy_busy.compare_exchange_strong(expected, true)) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "Another external fetch is in progress, try again shortly");
        return ESP_OK;
    }

    AsyncProxyJob *job = (AsyncProxyJob *)calloc(1, sizeof(AsyncProxyJob));
    if (!job) {
        _proxy_busy.store(false);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }
    job->url = url;
    job->content_type = content_type;
    job->error_message = error_message;

    // The async copy carries the already-set security headers (the response
    // header block is duplicated by httpd_req_async_handler_begin).
    if (httpd_req_async_handler_begin(req, &job->req) != ESP_OK) {
        free(job);
        _proxy_busy.store(false);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    if (xTaskCreate(_async_proxy_task, "ext_proxy", 9216, job, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create proxy task");
        httpd_resp_send_err(job->req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        httpd_req_async_handler_complete(job->req);
        free(job);
        _proxy_busy.store(false);
    }
    return ESP_OK;
}

// Build and send a JSON snapshot of the currently cached GitHub release.
// Used by both GET (cached) and POST (after refresh) variants of
// /api/check_update so the response shape is identical.
static void send_release_info_response(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");

    ReleaseInfo info = _updateCheck->getReleaseInfo();
    const char *currentVersion = _sysInfo->getCurrentVersion();

    bool updateAvailable = false;
    if (info.valid && currentVersion && strcmp(info.version, "n/a") != 0) {
        updateAvailable = (compareVersions(currentVersion, info.version) < 0);
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "currentVersion", currentVersion ? currentVersion : "");
    cJSON_AddStringToObject(root, "latestVersion", info.valid ? info.version : "n/a");
    cJSON_AddBoolToObject(root, "updateAvailable", updateAvailable);
    cJSON_AddBoolToObject(root, "isPrerelease", info.isPrerelease);
    cJSON_AddStringToObject(root, "releaseNotes", info.valid ? info.body : "");
    cJSON_AddStringToObject(root, "releaseUrl", info.releaseUrl);
    cJSON_AddStringToObject(root, "downloadUrl", info.downloadUrl);
    cJSON_AddStringToObject(root, "sha256", info.sha256);
    cJSON_AddStringToObject(root, "publishedAt", info.publishedAt);
    cJSON_AddNumberToObject(root, "fetchedAt", (double)info.fetchedAtMs);
    cJSON_AddBoolToObject(root, "betaChannel", _settings->getBetaChannel());
    cJSON_AddBoolToObject(root, "fetchInProgress", _updateCheck->isFetchInProgress());
    if (!info.valid && info.error[0]) {
        cJSON_AddStringToObject(root, "error", info.error);
    } else {
        cJSON_AddNullToObject(root, "error");
    }

    const char *json = cJSON_PrintUnformatted(root);
    if (json) {
        httpd_resp_sendstr(req, json);
        free((void *)json);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON alloc failed");
    }
    cJSON_Delete(root);
}

esp_err_t get_check_update_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    // GET returns the cached snapshot only - no network fetch. The WebUI
    // uses POST /api/check_update to trigger a refresh; this keeps GET cheap
    // for polling and avoids redundant manifest requests.
    send_release_info_response(req);
    return ESP_OK;
}

httpd_uri_t get_check_update_handler = {
    .uri = "/api/check_update",
    .method = HTTP_GET,
    .handler = get_check_update_handler_func,
    .user_ctx = NULL};

// POST /api/check_update: triggers an immediate update-manifest fetch.
// Runs in a detached task because the fetch can take up to 10 s and the
// httpd task must stay responsive.
struct RefreshJob {
    httpd_req_t *req;
};

static void _refresh_task(void *arg)
{
    RefreshJob *job = (RefreshJob *)arg;

    // refresh() is non-blocking on contention - if a fetch is already
    // running (background timer or another "Check now"), it returns
    // false immediately. We then poll isFetchInProgress() until the
    // in-flight request finishes so the client still gets fresh data.
    _updateCheck->refresh();

    for (int i = 0; i < 30 && _updateCheck->isFetchInProgress(); i++) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    add_security_headers(job->req);
    send_release_info_response(job->req);

    httpd_req_async_handler_complete(job->req);
    free(job);
    vTaskDelete(NULL);
}

esp_err_t post_check_update_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    httpd_req_t *async_req;
    if (httpd_req_async_handler_begin(req, &async_req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    RefreshJob *job = (RefreshJob *)calloc(1, sizeof(RefreshJob));
    if (!job) {
        httpd_req_async_handler_complete(async_req);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }
    job->req = async_req;

    if (xTaskCreate(_refresh_task, "rel_refresh", 16384, job, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create refresh task");
        httpd_resp_send_err(async_req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        httpd_req_async_handler_complete(async_req);
        free(job);
    }
    return ESP_OK;
}

httpd_uri_t post_check_update_handler = {
    .uri = "/api/check_update",
    .method = HTTP_POST,
    .handler = post_check_update_handler_func,
    .user_ctx = NULL};

esp_err_t get_changelog_handler_func(httpd_req_t *req)
{
    return start_async_proxy(req,
                             "https://raw.githubusercontent.com/Xerolux/HB-RF-ETH-ng/main/CHANGELOG.md",
                             "text/markdown",
                             "Failed to fetch changelog from GitHub");
}

httpd_uri_t get_changelog_handler = {
    .uri = "/api/changelog",
    .method = HTTP_GET,
    .handler = get_changelog_handler_func,
    .user_ctx = NULL};

esp_err_t get_firmware_archive_handler_func(httpd_req_t *req)
{
    return start_async_proxy(req,
                             "https://raw.githubusercontent.com/Xerolux/HB-RF-ETH-ng/main/archive.json",
                             "application/json",
                             "Failed to fetch firmware archive from GitHub");
}

httpd_uri_t get_firmware_archive_handler = {
    .uri = "/api/firmware_archive",
    .method = HTTP_GET,
    .handler = get_firmware_archive_handler_func,
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

    size_t offset = 0;
    char query[32];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char param[16];
        if (httpd_query_key_value(query, "offset", param, sizeof(param)) == ESP_OK) {
            offset = strtoul(param, NULL, 10);
        }
    }

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");

    // Send totalWritten so frontend can sync its offset after ring buffer overflow
    size_t totalWritten = LogManager::instance().getTotalWritten();
    char totalWrittenStr[24];
    snprintf(totalWrittenStr, sizeof(totalWrittenStr), "%zu", totalWritten);
    httpd_resp_set_hdr(req, "X-Log-Total", totalWrittenStr);

    std::string content = LogManager::instance().getLogContent(offset);
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

    LogManager::begin(8192);
    if (LogManager::instance().isEnabled()) {
        if (_settings) {
            _settings->setSystemLogEnabled(true);
            _settings->save();
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

    LogManager::stop();
    if (_settings) {
        _settings->setSystemLogEnabled(false);
        _settings->save();
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

    std::string content = LogManager::instance().getLogContent();
    httpd_resp_send(req, content.c_str(), content.length());

    return ESP_OK;
}

httpd_uri_t get_log_download_handler = {
    .uri = "/api/log/download",
    .method = HTTP_GET,
    .handler = get_log_download_handler_func,
    .user_ctx = NULL};

// ---- Share log to paste.blueml.eu ----

struct ShareLogJob
{
    httpd_req_t *req;
};

static void _share_log_task(void *arg)
{
    ShareLogJob *job = (ShareLogJob *)arg;
    char result[512];
    result[0] = 0;

    std::string logContent = LogManager::instance().getLogContent();
    std::string report;
    report.reserve(4096 + logContent.length());

    // ---- Header ----
    report += "============================================================\n";
    report += "HB-RF-ETH-ng System Report\n";
    report += "============================================================\n\n";
    report += "Version: ";
    report += _sysInfo->getCurrentVersion();
    report += "\nSerial:  ";
    report += _sysInfo->getSerialNumber();
    report += "\nUptime:  ";
    {
        uint32_t s = _sysInfo->getUptimeSeconds();
        char ut[48];
        snprintf(ut, sizeof(ut), "%lud %luh %lum %lus",
                 (unsigned long)(s / 86400),
                 (unsigned long)((s % 86400) / 3600),
                 (unsigned long)((s % 3600) / 60),
                 (unsigned long)(s % 60));
        report += ut;
    }
    report += "\n\n";

    // ---- System Info ----
    report += "--- System Info ---\n";
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "CPU: %.0f%%\n", _sysInfo->getCpuUsage()); report += buf;
        snprintf(buf, sizeof(buf), "Memory: %.0f%%\n", _sysInfo->getMemoryUsage()); report += buf;
        report += "Supply Voltage: unavailable (no board sensor)\n";
        report += "Temperature: unavailable (no board sensor)\n";
        report += "Board Revision: "; report += _sysInfo->getBoardRevisionString(); report += "\n";
        report += "Reset Reason: "; report += _sysInfo->getResetReason(); report += "\n";
        report += "Ethernet: ";
        report += _ethernet->isConnected() ? "Connected" : "Disconnected";
        if (_ethernet->isConnected()) {
            snprintf(buf, sizeof(buf), ", %d Mbps, %s",
                     _ethernet->getLinkSpeedMbps(),
                     _ethernet->getDuplexMode());
            report += buf;
        }
        report += "\n";
    }
    report += "\n";

    // ---- Radio Module ----
    report += "--- Radio Module ---\n";
    {
        const char *typeStr = "-";
        switch (_radioModuleDetector->getRadioModuleType()) {
            case RADIO_MODULE_HM_MOD_RPI_PCB: typeStr = "HM-MOD-RPI-PCB"; break;
            case RADIO_MODULE_RPI_RF_MOD:     typeStr = "RPI-RF-MOD";     break;
            default: break;
        }
        report += "Type: "; report += typeStr; report += "\n";
        report += "Serial: "; report += _radioModuleDetector->getSerial(); report += "\n";
        const uint8_t *fw = _radioModuleDetector->getFirmwareVersion();
        char fwBuf[16];
        snprintf(fwBuf, sizeof(fwBuf), "%d.%d.%d", fw[0], fw[1], fw[2]);
        report += "Firmware: "; report += fwBuf; report += "\n";
        char macBuf[16];
        formatRadioMAC(_radioModuleDetector->getBidCosRadioMAC(), macBuf, sizeof(macBuf));
        report += "BidCos MAC: "; report += macBuf; report += "\n";
        formatRadioMAC(_radioModuleDetector->getHmIPRadioMAC(), macBuf, sizeof(macBuf));
        report += "HmIP MAC: "; report += macBuf; report += "\n";
        report += "SGTIN: "; report += _radioModuleDetector->getSGTIN(); report += "\n";
    }
    report += "\n";

    // ---- Network ----
    report += "--- Network ---\n";
    {
        report += "Hostname: "; report += _settings->getHostname(); report += "\n";
        report += "Use DHCP: "; report += _settings->getUseDHCP() ? "Yes" : "No"; report += "\n";
        ip4_addr_t ip, nm, gw, dns1, dns2;
        _ethernet->getNetworkSettings(&ip, &nm, &gw, &dns1, &dns2);
        report += "Local IP: "; report += ip2str(_settings->getLocalIP(), ip); report += "\n";
        report += "Netmask: "; report += ip2str(_settings->getNetmask(), nm); report += "\n";
        report += "Gateway: "; report += ip2str(_settings->getGateway(), gw); report += "\n";
        report += "DNS1: "; report += ip2str(_settings->getDns1(), dns1); report += "\n";
        report += "DNS2: "; report += ip2str(_settings->getDns2(), dns2); report += "\n";
        report += "CCU IP: "; report += _settings->getCCUIP(); report += "\n";
        report += "CCU Connected: ";
        ip4_addr_t ccuAddr = _rawUartUdpListener->getConnectedRemoteAddress();
        report += (ccuAddr.addr != IPADDR_ANY) ? "Yes" : "No";
        report += "\n";
        if (ccuAddr.addr != IPADDR_ANY) {
            report += "CCU Address: "; report += ip2str(ccuAddr); report += "\n";
        }
        report += "IPv6: "; report += _settings->getEnableIPv6() ? "Enabled" : "Disabled"; report += "\n";
        if (_settings->getEnableIPv6()) {
            report += "IPv6 Mode: "; report += _settings->getIPv6Mode(); report += "\n";
            report += "IPv6 Address: "; report += _settings->getIPv6Address(); report += "\n";
            char pfx[8];
            snprintf(pfx, sizeof(pfx), "%d", _settings->getIPv6PrefixLength());
            report += "IPv6 Prefix: /"; report += pfx; report += "\n";
            report += "IPv6 Gateway: "; report += _settings->getIPv6Gateway(); report += "\n";
        }
        report += "NTP Server: "; report += _settings->getNtpServer(); report += "\n";
        char tsBuf[16];
        snprintf(tsBuf, sizeof(tsBuf), "%d", _settings->getTimesource());
        report += "Timesource: "; report += tsBuf; report += "\n";
    }
    report += "\n";

    // ---- Monitoring Config (passwords redacted) ----
    report += "--- Monitoring ---\n";
    {
        monitoring_config_t monCfg;
        if (monitoring_get_config(&monCfg) == ESP_OK) {
            report += "MQTT Enabled: "; report += monCfg.mqtt.enabled ? "Yes" : "No"; report += "\n";
            if (monCfg.mqtt.enabled) {
                report += "MQTT Server: "; report += monCfg.mqtt.server; report += "\n";
                char portBuf[8];
                snprintf(portBuf, sizeof(portBuf), "%u", monCfg.mqtt.port);
                report += "MQTT Port: "; report += portBuf; report += "\n";
                report += "MQTT User: "; report += monCfg.mqtt.user[0] ? monCfg.mqtt.user : "(none)"; report += "\n";
                report += "MQTT Password: ";
                report += monCfg.mqtt.password[0] ? "****" : "(none)";
                report += "\n";
                report += "MQTT Topic Prefix: "; report += monCfg.mqtt.topic_prefix; report += "\n";
                report += "HA Discovery: "; report += monCfg.mqtt.ha_discovery_enabled ? "Yes" : "No"; report += "\n";
                if (monCfg.mqtt.ha_discovery_enabled) {
                    report += "HA Discovery Prefix: "; report += monCfg.mqtt.ha_discovery_prefix; report += "\n";
                }
                report += "MQTT TLS: "; report += monCfg.mqtt.tls_enable ? "Yes" : "No"; report += "\n";
                if (monCfg.mqtt.tls_enable) {
                    report += "TLS Skip Verify: "; report += monCfg.mqtt.tls_skip_verify ? "Yes (INSECURE)" : "No"; report += "\n";
                    report += "TLS CA Certs: "; report += monCfg.mqtt.tls_ca_certs[0] ? "<set>" : "<empty>"; report += "\n";
                    report += "TLS Client Cert: "; report += monCfg.mqtt.tls_certfile[0] ? "<set>" : "<empty>"; report += "\n";
                    report += "TLS Client Key: "; report += monCfg.mqtt.tls_keyfile[0] ? "<set>" : "<empty>"; report += "\n";
                }
                report += "Command Token: ";
                report += monCfg.mqtt.command_token[0] ? "****" : "(none)";
                report += "\n";
            }
            report += "CheckMK Enabled: "; report += monCfg.checkmk.enabled ? "Yes" : "No"; report += "\n";
            if (monCfg.checkmk.enabled) {
                char ckPort[8];
                snprintf(ckPort, sizeof(ckPort), "%u", monCfg.checkmk.port);
                report += "CheckMK Port: "; report += ckPort; report += "\n";
                report += "CheckMK Allowed Hosts: "; report += monCfg.checkmk.allowed_hosts; report += "\n";
            }
        } else {
            report += "(monitoring config unavailable)\n";
        }
    }
    report += "\n";

    // ---- Update ----
    report += "--- Update ---\n";
    {
        report += "Beta Channel: "; report += _settings->getBetaChannel() ? "Yes" : "No"; report += "\n";
        report += "Latest Version: "; report += _updateCheck->getLatestVersion(); report += "\n";
    }
    report += "\n";

    // ---- LED Programs ----
    report += "--- LED Configuration ---\n";
    {
        char ledBuf[32];
        report += "Brightness: ";
        snprintf(ledBuf, sizeof(ledBuf), "%d", _settings->getLEDBrightness());
        report += ledBuf; report += "\n";
        const char *progNames[] = {"Idle","CCU Discon.","CCU Con.","Update Avail.","Error","Booting","Update Prog."};
        int progVals[] = {
            _settings->getLedProgram(LED_PROG_IDLE),
            _settings->getLedProgram(LED_PROG_CCU_DISCONNECTED),
            _settings->getLedProgram(LED_PROG_CCU_CONNECTED),
            _settings->getLedProgram(LED_PROG_UPDATE_AVAILABLE),
            _settings->getLedProgram(LED_PROG_ERROR),
            _settings->getLedProgram(LED_PROG_BOOTING),
            _settings->getLedProgram(LED_PROG_UPDATE_IN_PROGRESS)};
        for (int i = 0; i < 7; i++) {
            snprintf(ledBuf, sizeof(ledBuf), "  %-16s: %d\n", progNames[i], progVals[i]);
            report += ledBuf;
        }
    }
    report += "\n";

    // ---- System Log ----
    report += "--- System Log ---\n";
    report += logContent;
    report += "\n";

    if (report.length() < 10)
    {
        snprintf(result, sizeof(result),
                 "{\"success\":false,\"error\":\"Report is empty\"}");
        goto respond;
    }

    {
        // MicroBin (the software behind paste.blueml.eu) only accepts
        // multipart/form-data uploads on /upload — it does not implement
        // the classic application/x-www-form-urlencoded POST to "/".
        static const char boundary[] = "----HBRFETHngBoundary7331";

        auto appendField = [&](std::string &body, const char *name, const char *value) {
            body += "--"; body += boundary; body += "\r\n";
            body += "Content-Disposition: form-data; name=\""; body += name; body += "\"\r\n\r\n";
            body += value;
            body += "\r\n";
        };

        std::string body;
        body.reserve(report.length() + 512);
        appendField(body, "content", report.c_str());
        appendField(body, "expiration", "24hour");
        appendField(body, "burn_after", "0");
        appendField(body, "syntax_highlight", "none");
        appendField(body, "privacy", "unlisted");
        body += "--"; body += boundary; body += "--\r\n";

        // Event handler captures the Location response header from the 302
        // redirect. ESP-IDF's esp_http_client_get_header() reads REQUEST
        // headers, not response headers — so we MUST use the event callback.
        struct PasteCtx {
            char location[256];
            bool hasLocation;
        };
        PasteCtx pctx = {};
        pctx.hasLocation = false;

        esp_http_client_config_t config = {};
        config.url = "https://paste.blueml.eu/upload";
        config.method = HTTP_METHOD_POST;
        // The global ESP-TLS insecure fallback is disabled. Attach the CA
        // bundle explicitly so log sharing remains functional and authenticated.
        config.crt_bundle_attach = esp_crt_bundle_attach;
        config.keep_alive_enable = false;
        config.disable_auto_redirect = true;
        config.timeout_ms = 20000;
        config.event_handler = [](esp_http_client_event_t *evt) -> esp_err_t {
            PasteCtx *ctx = (PasteCtx *)evt->user_data;
            if (!ctx) return ESP_OK;
            if (evt->event_id == HTTP_EVENT_ON_HEADER) {
                if (strcasecmp(evt->header_key, "Location") == 0) {
                    strncpy(ctx->location, evt->header_value, sizeof(ctx->location) - 1);
                    ctx->location[sizeof(ctx->location) - 1] = 0;
                    ctx->hasLocation = true;
                }
            }
            return ESP_OK;
        };
        config.user_data = &pctx;

        // Serialize with UpdateCheck / changelog proxy so two TLS handshakes
        // never run at once - concurrent mbedtls SSL context allocation is
        // what exhausts the ESP32 heap and makes mbedtls_ssl_setup() fail.
        bool net_locked = false;
        if (g_net_fetch_mutex) {
            if (xSemaphoreTake(g_net_fetch_mutex, pdMS_TO_TICKS(15000)) == pdTRUE) {
                net_locked = true;
            } else {
                snprintf(result, sizeof(result),
                         "{\"success\":false,\"error\":\"Network busy, please try again later\"}");
                goto respond;
            }
        }

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client)
        {
            snprintf(result, sizeof(result),
                     "{\"success\":false,\"error\":\"HTTP client init failed\"}");
            if (net_locked) xSemaphoreGive(g_net_fetch_mutex);
            goto respond;
        }

        std::string contentType = "multipart/form-data; boundary=";
        contentType += boundary;
        esp_http_client_set_header(client, "User-Agent", "HB-RF-ETH-ng");
        esp_http_client_set_header(client, "Content-Type", contentType.c_str());
        esp_http_client_set_post_field(client, body.c_str(), body.length());

        esp_err_t err = esp_http_client_perform(client);

        if (err == ESP_OK)
        {
            int status = esp_http_client_get_status_code(client);
            if ((status == 303 || status == 302 || status == 301) && pctx.hasLocation)
            {
                // MicroBin returns Location: https://paste.blueml.eu/upload/<id>
                // but the paste view URL is https://paste.blueml.eu/p/<id>.
                char* upload_ptr = strstr(pctx.location, "/upload");
                if (upload_ptr) {
                    memmove(upload_ptr + 2, upload_ptr + 7, strlen(upload_ptr + 7) + 1);
                    upload_ptr[0] = '/';
                    upload_ptr[1] = 'p';
                }
                snprintf(result, sizeof(result),
                         "{\"success\":true,\"url\":\"%s\"}", pctx.location);
            }
            else
            {
                snprintf(result, sizeof(result),
                         "{\"success\":false,\"error\":\"Paste service returned HTTP %d\"}", status);
            }
        }
        else
        {
            snprintf(result, sizeof(result),
                     "{\"success\":false,\"error\":\"Request failed: %s\"}", esp_err_to_name(err));
        }

        esp_http_client_cleanup(client);
        if (net_locked) xSemaphoreGive(g_net_fetch_mutex);
    }

respond:
    add_security_headers(job->req);
    httpd_resp_set_type(job->req, "application/json");
    httpd_resp_sendstr(job->req, result);
    httpd_req_async_handler_complete(job->req);
    free(job);
    vTaskDelete(NULL);
}

esp_err_t post_share_log_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    httpd_req_t *async_req;
    if (httpd_req_async_handler_begin(req, &async_req) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    ShareLogJob *job = (ShareLogJob *)calloc(1, sizeof(ShareLogJob));
    if (!job)
    {
        httpd_req_async_handler_complete(async_req);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }
    job->req = async_req;

    if (xTaskCreate(_share_log_task, "share_log", 12288, job, 3, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create share log task");
        httpd_resp_send_err(async_req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        httpd_req_async_handler_complete(async_req);
        free(job);
    }
    return ESP_OK;
}

httpd_uri_t post_share_log_handler = {
    .uri = "/api/log/share",
    .method = HTTP_POST,
    .handler = post_share_log_handler_func,
    .user_ctx = NULL};

// Prometheus metrics disabled - feature code available in prometheus.cpp.disabled

WebUI::WebUI(Settings *settings, LED *statusLED, SysInfo *sysInfo, UpdateCheck *updateCheck, Ethernet *ethernet, RawUartUdpListener *rawUartUdpListener, RadioModuleConnector *radioModuleConnector, RadioModuleDetector *radioModuleDetector)
{
    _settings = settings;
    _statusLED = statusLED;
    _sysInfo = sysInfo;
    _ethernet = ethernet;
    _updateCheck = updateCheck;
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
    config.max_uri_handlers = 32;
    config.uri_match_fn = httpd_uri_match_wildcard;
    // Increase stack: POST handlers allocate content buffers + config structs
    // that together exceed the default 4096-byte stack, causing overflow/corruption.
    config.stack_size = 8192;

    _httpd_handle = NULL;

    if (httpd_start(&_httpd_handle, &config) == ESP_OK)
    {
        httpd_register_uri_handler(_httpd_handle, &post_login_json_handler);
        httpd_register_uri_handler(_httpd_handle, &get_sysinfo_json_handler);
        httpd_register_uri_handler(_httpd_handle, &get_settings_json_handler);
        httpd_register_uri_handler(_httpd_handle, &post_settings_json_handler);
        httpd_register_uri_handler(_httpd_handle, &post_ota_update_handler);
        httpd_register_uri_handler(_httpd_handle, &post_restart_handler);
        httpd_register_uri_handler(_httpd_handle, &post_factory_reset_handler);
        httpd_register_uri_handler(_httpd_handle, &post_ota_url_handler);
        httpd_register_uri_handler(_httpd_handle, &get_ota_status_handler);
        httpd_register_uri_handler(_httpd_handle, &post_change_password_handler);
        httpd_register_uri_handler(_httpd_handle, &get_monitoring_handler);
        httpd_register_uri_handler(_httpd_handle, &post_monitoring_handler);
        httpd_register_uri_handler(_httpd_handle, &get_monitoring_test_handler);

        httpd_register_uri_handler(_httpd_handle, &get_backup_handler);
        httpd_register_uri_handler(_httpd_handle, &post_restore_handler);
        httpd_register_uri_handler(_httpd_handle, &get_check_update_handler);
        httpd_register_uri_handler(_httpd_handle, &post_check_update_handler);
        httpd_register_uri_handler(_httpd_handle, &get_changelog_handler);
        httpd_register_uri_handler(_httpd_handle, &get_firmware_archive_handler);
        httpd_register_uri_handler(_httpd_handle, &get_log_handler);
        httpd_register_uri_handler(_httpd_handle, &get_log_status_handler);
        httpd_register_uri_handler(_httpd_handle, &post_log_enable_handler);
        httpd_register_uri_handler(_httpd_handle, &post_log_disable_handler);
        httpd_register_uri_handler(_httpd_handle, &get_log_download_handler);
        httpd_register_uri_handler(_httpd_handle, &post_share_log_handler);

        httpd_register_uri_handler(_httpd_handle, &main_js_gz_handler);
        httpd_register_uri_handler(_httpd_handle, &main_css_gz_handler);
        httpd_register_uri_handler(_httpd_handle, &favicon_ico_gz_handler);
        httpd_register_uri_handler(_httpd_handle, &index_html_gz_handler);
    }
}

void WebUI::stop()
{
    httpd_stop(_httpd_handle);
}
