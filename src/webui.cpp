/*
 *  webui.cpp is part of the HB-RF-ETH firmware v2.0
 *
 *  Original work Copyright 2022 Alexander Reinert
 *  https://github.com/alexreinert/HB-RF-ETH
 *
 *  Modified work Copyright 2025 Xerolux
 *  Modernized fork - Updated to ESP-IDF 5.1 and modern toolchains
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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "webui.h"
#include "esp_log.h"
#include "cJSON.h"
#include "validation.h"
#include "esp_ota_ops.h"
#include "mbedtls/md.h"
#include "mbedtls/base64.h"
#include "monitoring_api.h"
#include "nextcloud_api.h"
#include "rate_limiter.h"
#if ENABLE_ANALYZER
#include "analyzer.h"
#endif
#include "dtls_api.h"
#include "monitoring.h"
#include "security_headers.h"
#include "log_manager.h"
// #include "prometheus.h"

static const char *TAG = "WebUI";

// Buffer size constants - prevent magic numbers
#define TOKEN_BASE_SIZE 21
#define ETAG_BUFFER_SIZE 32
#define TOKEN_BUFFER_SIZE 46
#define SYSINFO_BUFFER_SIZE 1536
#define IF_NONE_MATCH_SIZE 64

#define EMBED_HANDLER(_uri, _resource, _contentType)                   \
    extern const char _resource[] asm("_binary_" #_resource "_start"); \
    extern const size_t _resource##_length asm(#_resource "_length");  \
    esp_err_t _resource##_handler_func(httpd_req_t *req)               \
    {                                                                  \
        add_security_headers(req);                                     \
        if (_sysInfo) {                                                \
            char etag[32];                                             \
            snprintf(etag, sizeof(etag), "\"%s\"", _sysInfo->getCurrentVersion()); \
            httpd_resp_set_hdr(req, "ETag", etag);                     \
            httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=0, must-revalidate"); \
            char if_none_match[64];                                    \
            if (httpd_req_get_hdr_value_str(req, "If-None-Match", if_none_match, sizeof(if_none_match)) == ESP_OK) { \
                if (strstr(if_none_match, etag)) {                     \
                    httpd_resp_set_status(req, "304 Not Modified");    \
                    httpd_resp_send(req, NULL, 0);                     \
                    return ESP_OK;                                     \
                }                                                      \
            }                                                          \
        }                                                              \
        httpd_resp_set_type(req, _contentType);                        \
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");           \
        httpd_resp_send(req, _resource, _resource##_length);           \
        return ESP_OK;                                                 \
    };                                                                 \
    httpd_uri_t _resource##_handler = {                                \
        .uri = _uri,                                                   \
        .method = HTTP_GET,                                            \
        .handler = _resource##_handler_func,                           \
        .user_ctx = NULL,                                              \
        .is_websocket = false,                                         \
        .handle_ws_control_frames = false,                             \
        .supported_subprotocol = NULL};

static Settings *_settings;
static LED *_statusLED;
static SysInfo *_sysInfo;
static UpdateCheck *_updateCheck;
static Ethernet *_ethernet;
static RawUartUdpListener *_rawUartUdpListener;
static RadioModuleConnector *_radioModuleConnector;
static RadioModuleDetector *_radioModuleDetector;
#if ENABLE_ANALYZER
static Analyzer *_analyzer;
#endif
static DTLSEncryption *_dtlsEncryption;
static char _token[46];

EMBED_HANDLER("/*", index_html_gz, "text/html")
EMBED_HANDLER("/main.js", main_js_gz, "application/javascript")
EMBED_HANDLER("/main.css", main_css_gz, "text/css")
EMBED_HANDLER("/favicon.ico", favicon_ico_gz, "image/x-icon")

void generateToken()
{
    char tokenBase[21];
    *((uint32_t *)tokenBase) = esp_random();
    *((uint32_t *)(tokenBase + sizeof(uint32_t))) = esp_random();
    // Safe copy with bounds check (13 chars max for serial + 8 bytes random = 21 total)
    strncpy(tokenBase + 2 * sizeof(uint32_t), _sysInfo->getSerialNumber(), sizeof(tokenBase) - 2 * sizeof(uint32_t) - 1);
    tokenBase[sizeof(tokenBase) - 1] = '\0'; // Ensure null termination

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

/**
 * Constant-time string comparison to prevent timing attacks.
 * Returns 0 if strings match, non-zero otherwise.
 */
int secure_strcmp(const char *s1, const char *s2) {
    if (!s1 || !s2) return 1;

    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    size_t max_len = (len1 > len2) ? len1 : len2;
    int result = (len1 != len2);

    for (size_t i = 0; i < max_len; i++) {
        const char c1 = (i < len1) ? s1[i] : 0;
        const char c2 = (i < len2) ? s2[i] : 0;
        result |= (c1 ^ c2);
    }

    return result;
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

esp_err_t post_login_json_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    // Check rate limit
    if (!rate_limiter_check_login(req))
    {
        httpd_resp_set_status(req, "429 Too Many Requests");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"isAuthenticated\":false,\"error\":\"Too many login attempts. Please try again later.\"}");
        return ESP_OK;
    }

    char buffer[1024];
    int len = httpd_req_recv(req, buffer, sizeof(buffer) - 1);

    if (len <= 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive login data");
        return ESP_FAIL;
    }

    buffer[len] = 0;

    cJSON *root = cJSON_Parse(buffer);

    char *password = cJSON_GetStringValue(cJSON_GetObjectItem(root, "password"));

    bool isAuthenticated = (password != NULL) && _settings->verifyAdminPassword(password);

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
    httpd_resp_sendstr(req, json);
    free((void *)json);
    cJSON_Delete(root);

    return ESP_OK;
}

httpd_uri_t post_login_json_handler = {
    .uri = "/login.json",
    .method = HTTP_POST,
    .handler = post_login_json_handler_func,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL};

esp_err_t get_sysinfo_json_handler_func(httpd_req_t *req)
{
    // Inform SysInfo that data was requested (wakes up CPU usage task)
    _sysInfo->markSysInfoRequested();

    add_security_headers(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");

    // Optimization: Use stack buffer and snprintf instead of cJSON to reduce heap allocations
    // This handler is called frequently (1Hz) by the frontend.
    char buffer[SYSINFO_BUFFER_SIZE];

    // Stack overflow protection: Log high water mark periodically
    static uint32_t check_counter = 0;
    if (++check_counter % 100 == 0) { // Check every 100 calls
        UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(NULL);
        if (stack_remaining < 512) {
            ESP_LOGW(TAG, "Low stack space in sysinfo handler: %u bytes remaining", stack_remaining);
        }
    }

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

    // Format JSON
    // Note: We use "true"/"false" strings for booleans
    int written = snprintf(buffer, sizeof(buffer),
        "{\"sysInfo\":{"
            "\"serial\":\"%s\","
            "\"currentVersion\":\"%s\","
            "\"firmwareVariant\":\"%s\","
            "\"latestVersion\":\"%s\","
            "\"memoryUsage\":%.2f,"
            "\"cpuUsage\":%.2f,"
            "\"supplyVoltage\":%.2f,"
            "\"temperature\":%.2f,"
            "\"uptimeSeconds\":%" PRIu64 ","
            "\"boardRevision\":\"%s\","
            "\"resetReason\":\"%s\","
            "\"ethernetConnected\":%s,"
            "\"ethernetSpeed\":%d,"
            "\"ethernetDuplex\":\"%s\","
            "\"rawUartRemoteAddress\":\"%s\","
            "\"radioModuleType\":\"%s\","
            "\"radioModuleSerial\":\"%s\","
            "\"radioModuleFirmwareVersion\":\"%s\","
            "\"radioModuleBidCosRadioMAC\":\"%s\","
            "\"radioModuleHmIPRadioMAC\":\"%s\","
            "\"radioModuleSGTIN\":\"%s\""
        "}}",
        _sysInfo->getSerialNumber(),
        _sysInfo->getCurrentVersion(),
        _sysInfo->getFirmwareVariant(),
        _updateCheck->getLatestVersion(),
        _sysInfo->getMemoryUsage(),
        _sysInfo->getCpuUsage(),
        _sysInfo->getSupplyVoltage(),
        _sysInfo->getTemperature(),
        _sysInfo->getUptimeSeconds(),
        _sysInfo->getBoardRevisionString(),
        _sysInfo->getResetReason(),
        _ethernet->isConnected() ? "true" : "false",
        _ethernet->getLinkSpeedMbps(),
        _ethernet->getDuplexMode(),
        _rawUartUdpListener ? ip2str(_rawUartUdpListener->getConnectedRemoteAddress()) : "HMLGW Mode",
        radioModuleTypeStr,
        _radioModuleDetector->getSerial(),
        fwVersionStr,
        bidCosMAC,
        hmIPMAC,
        _radioModuleDetector->getSGTIN()
    );

    if (written < 0 || written >= sizeof(buffer)) {
        ESP_LOGE(TAG, "SysInfo JSON buffer overflow or error");
        return httpd_resp_send_500(req);
    }

    httpd_resp_sendstr(req, buffer);
    return ESP_OK;
}

httpd_uri_t get_sysinfo_json_handler = {
    .uri = "/sysinfo.json",
    .method = HTTP_GET,
    .handler = get_sysinfo_json_handler_func,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL};

void add_settings(cJSON *root)
{
    cJSON *settings = cJSON_AddObjectToObject(root, "settings");

    cJSON_AddStringToObject(settings, "hostname", _settings->getHostname());

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

    cJSON_AddBoolToObject(settings, "checkUpdates", _settings->getCheckUpdates());
    cJSON_AddBoolToObject(settings, "allowPrerelease", _settings->getAllowPrerelease());

    // IPv6 Settings
    cJSON_AddBoolToObject(settings, "enableIPv6", _settings->getEnableIPv6());
    cJSON_AddStringToObject(settings, "ipv6Mode", _settings->getIPv6Mode());
    cJSON_AddStringToObject(settings, "ipv6Address", _settings->getIPv6Address());
    cJSON_AddNumberToObject(settings, "ipv6PrefixLength", _settings->getIPv6PrefixLength());
    cJSON_AddStringToObject(settings, "ipv6Gateway", _settings->getIPv6Gateway());
    cJSON_AddStringToObject(settings, "ipv6Dns1", _settings->getIPv6Dns1());
    cJSON_AddStringToObject(settings, "ipv6Dns2", _settings->getIPv6Dns2());

    // HMLGW
    cJSON_AddBoolToObject(settings, "hmlgwEnabled", _settings->getHmlgwEnabled());
    cJSON_AddNumberToObject(settings, "hmlgwPort", _settings->getHmlgwPort());
    cJSON_AddNumberToObject(settings, "hmlgwKeepAlivePort", _settings->getHmlgwKeepAlivePort());

    cJSON_AddBoolToObject(settings, "analyzerEnabled", _settings->getAnalyzerEnabled());

    // DTLS
    cJSON_AddNumberToObject(settings, "dtlsMode", _settings->getDTLSMode());
    cJSON_AddNumberToObject(settings, "dtlsCipherSuite", _settings->getDTLSCipherSuite());
    cJSON_AddBoolToObject(settings, "dtlsRequireClientCert", _settings->getDTLSRequireClientCert());
    cJSON_AddBoolToObject(settings, "dtlsSessionResumption", _settings->getDTLSSessionResumption());
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
    cJSON *root = cJSON_CreateObject();

    add_settings(root);

    const char *json = cJSON_Print(root);
    httpd_resp_sendstr(req, json);
    free((void *)json);
    cJSON_Delete(root);

    return ESP_OK;
}

httpd_uri_t get_settings_json_handler = {
    .uri = "/settings.json",
    .method = HTTP_GET,
    .handler = get_settings_json_handler_func,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL};

#if ENABLE_ANALYZER
esp_err_t analyzer_ws_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (!_settings->getAnalyzerEnabled() || _settings->getHmlgwEnabled()) {
        httpd_resp_set_status(req, "403 Forbidden");
        httpd_resp_sendstr(req, "Analyzer feature is disabled");
        return ESP_OK;
    }

    // Lazily create analyzer only when feature is enabled to reduce background load
    if (_analyzer == nullptr) {
        _analyzer = new Analyzer(_radioModuleConnector);
    }

    return Analyzer::ws_handler(req);
}
#else
// Stub when Analyzer is disabled
esp_err_t analyzer_ws_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_sendstr(req, "Analyzer feature not available in this firmware variant");
    return ESP_OK;
}
#endif

ip4_addr_t cJSON_GetIPAddrValue(const cJSON *item)
{
    ip4_addr_t res{.addr = IPADDR_ANY};

    if (cJSON_IsString(item))
    {
        ip4addr_aton(item->valuestring, &res);
    }

    return res;
}

bool cJSON_GetBoolValue(const cJSON *item)
{
    if (cJSON_IsBool(item))
    {
        return item->type == cJSON_True;
    }

    return false;
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

    char buffer[1024];
    int len = httpd_req_recv(req, buffer, sizeof(buffer) - 1);

    if (len <= 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive settings data");
        return ESP_FAIL;
    }

    buffer[len] = 0;
    cJSON *root = cJSON_Parse(buffer);

    char *adminPassword = cJSON_GetStringValue(cJSON_GetObjectItem(root, "adminPassword"));

    // Enforce strict password validation if password is being set
    if (adminPassword && strlen(adminPassword) > 0) {
        if (!validatePassword(adminPassword)) {
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Password does not meet complexity requirements (min 6 chars, letters + numbers)");
        }
    }

    char *hostname = cJSON_GetStringValue(cJSON_GetObjectItem(root, "hostname"));
    bool useDHCP = cJSON_GetBoolValue(cJSON_GetObjectItem(root, "useDHCP"));
    ip4_addr_t localIP = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "localIP"));
    ip4_addr_t netmask = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "netmask"));
    ip4_addr_t gateway = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "gateway"));
    ip4_addr_t dns1 = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "dns1"));
    ip4_addr_t dns2 = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "dns2"));

    // Safely extract timesource with null check
    timesource_t timesource = TIMESOURCE_NTP; // Default
    cJSON *timesourceItem = cJSON_GetObjectItem(root, "timesource");
    if (timesourceItem) timesource = (timesource_t)timesourceItem->valueint;

    // Safely extract dcfOffset with null check
    int dcfOffset = 0; // Default
    cJSON *dcfOffsetItem = cJSON_GetObjectItem(root, "dcfOffset");
    if (dcfOffsetItem) dcfOffset = dcfOffsetItem->valueint;

    // Safely extract gpsBaudrate with null check
    int gpsBaudrate = 9600; // Default
    cJSON *gpsBaudrateItem = cJSON_GetObjectItem(root, "gpsBaudrate");
    if (gpsBaudrateItem) gpsBaudrate = gpsBaudrateItem->valueint;

    char *ntpServer = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ntpServer"));

    // Safely extract ledBrightness with null check
    int ledBrightness = 100; // Default
    cJSON *ledBrightnessItem = cJSON_GetObjectItem(root, "ledBrightness");
    if (ledBrightnessItem) ledBrightness = ledBrightnessItem->valueint;

    // IPv6
    bool enableIPv6 = cJSON_GetBoolValue(cJSON_GetObjectItem(root, "enableIPv6"));
    char *ipv6Mode = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Mode"));
    char *ipv6Address = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Address"));

    // Safely extract ipv6PrefixLength with null check
    int ipv6PrefixLength = 64; // Default
    cJSON *ipv6PrefixLengthItem = cJSON_GetObjectItem(root, "ipv6PrefixLength");
    if (ipv6PrefixLengthItem) ipv6PrefixLength = ipv6PrefixLengthItem->valueint;

    char *ipv6Gateway = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Gateway"));
    char *ipv6Dns1 = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Dns1"));
    char *ipv6Dns2 = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Dns2"));

    // HMLGW
    bool hmlgwEnabled = cJSON_GetBoolValue(cJSON_GetObjectItem(root, "hmlgwEnabled"));
    int hmlgwPort = 2000;
    if (cJSON_GetObjectItem(root, "hmlgwPort")) {
         hmlgwPort = cJSON_GetObjectItem(root, "hmlgwPort")->valueint;
    }
    int hmlgwKeepAlivePort = 2001;
    if (cJSON_GetObjectItem(root, "hmlgwKeepAlivePort")) {
         hmlgwKeepAlivePort = cJSON_GetObjectItem(root, "hmlgwKeepAlivePort")->valueint;
    }

    bool analyzerEnabled = cJSON_GetBoolValue(cJSON_GetObjectItem(root, "analyzerEnabled"));

    if (adminPassword && strlen(adminPassword) > 0)
        _settings->setAdminPassword(adminPassword);

    _settings->setNetworkSettings(hostname, useDHCP, localIP, netmask, gateway, dns1, dns2);
    _settings->setTimesource(timesource);
    _settings->setDcfOffset(dcfOffset);
    _settings->setGpsBaudrate(gpsBaudrate);
    _settings->setNtpServer(ntpServer);
    _settings->setLEDBrightness(ledBrightness);

    cJSON *checkUpdatesItem = cJSON_GetObjectItem(root, "checkUpdates");
    if (checkUpdatesItem && cJSON_IsBool(checkUpdatesItem)) {
        _settings->setCheckUpdates(cJSON_IsTrue(checkUpdatesItem));
    }

    cJSON *allowPrereleaseItem = cJSON_GetObjectItem(root, "allowPrerelease");
    if (allowPrereleaseItem && cJSON_IsBool(allowPrereleaseItem)) {
        _settings->setAllowPrerelease(cJSON_IsTrue(allowPrereleaseItem));
    }

    // Handle IPv6 (checking for nulls)
    if (ipv6Mode) {
         _settings->setIPv6Settings(
            enableIPv6,
            ipv6Mode,
            ipv6Address ? ipv6Address : (char*)"",
            ipv6PrefixLength,
            ipv6Gateway ? ipv6Gateway : (char*)"",
            ipv6Dns1 ? ipv6Dns1 : (char*)"",
            ipv6Dns2 ? ipv6Dns2 : (char*)""
        );
    }

    _settings->setHmlgwEnabled(hmlgwEnabled);
    _settings->setHmlgwPort(hmlgwPort);
    _settings->setHmlgwKeepAlivePort(hmlgwKeepAlivePort);

    if (hmlgwEnabled && analyzerEnabled) {
        ESP_LOGW(TAG, "Disabling Analyzer because HMLGW mode is enabled");
        analyzerEnabled = false;
    }
    _settings->setAnalyzerEnabled(analyzerEnabled);

    // DTLS
    int dtlsMode = _settings->getDTLSMode();
    int dtlsCipherSuite = _settings->getDTLSCipherSuite();
    bool dtlsRequireClientCert = _settings->getDTLSRequireClientCert();
    bool dtlsSessionResumption = _settings->getDTLSSessionResumption();
    if (cJSON_HasObjectItem(root, "dtlsMode")) {
        dtlsMode = cJSON_GetObjectItem(root, "dtlsMode")->valueint;
        dtlsCipherSuite = cJSON_GetObjectItem(root, "dtlsCipherSuite")->valueint;
        dtlsRequireClientCert = cJSON_GetBoolValue(cJSON_GetObjectItem(root, "dtlsRequireClientCert"));
        dtlsSessionResumption = cJSON_GetBoolValue(cJSON_GetObjectItem(root, "dtlsSessionResumption"));
    }

    // Enforce compatibility: DTLS cannot run together with Analyzer or HMLGW
    if (analyzerEnabled || hmlgwEnabled) {
        if (dtlsMode != DTLS_MODE_DISABLED) {
            ESP_LOGW(TAG, "Disabling DTLS because %s is enabled", analyzerEnabled ? "Analyzer" : "HMLGW");
        }
        dtlsMode = DTLS_MODE_DISABLED;
    }

    _settings->setDTLSSettings(dtlsMode, dtlsCipherSuite, dtlsRequireClientCert, dtlsSessionResumption);

    _settings->save();

    // Start or stop Analyzer task based on new configuration
    if (analyzerEnabled && !hmlgwEnabled) {
        if (_analyzer == nullptr) {
            _analyzer = new Analyzer(_radioModuleConnector);
        }
    } else if (_analyzer) {
        delete _analyzer;
        _analyzer = nullptr;
    }

    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    root = cJSON_CreateObject();

    add_settings(root);

    const char *json = cJSON_PrintUnformatted(root);
    httpd_resp_sendstr(req, json);
    free((void *)json);
    cJSON_Delete(root);

    return ESP_OK;
}

httpd_uri_t post_settings_json_handler = {
    .uri = "/settings.json",
    .method = HTTP_POST,
    .handler = post_settings_json_handler_func,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL};

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
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"settings.json\"");

    cJSON *root = cJSON_CreateObject();

    // Add all settings (password is excluded for security reasons)
    add_settings(root);

    // Merge settings object into root if add_settings creates a sub-object
    // NOTE: add_settings creates a "settings" object inside root.
    // If we want a flat structure or specific structure for restore, we need to match post_settings_json_handler expectations.
    // post_settings_json_handler expects a flat JSON object with keys like "adminPassword", "hostname", etc.
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

    // Add Monitoring settings (separately, but included in backup)
    monitoring_config_t monConfig;
    if (monitoring_get_config(&monConfig) == ESP_OK) {
        cJSON *mon = cJSON_CreateObject();

        // SNMP
        cJSON *snmp = cJSON_AddObjectToObject(mon, "snmp");
        cJSON_AddBoolToObject(snmp, "enabled", monConfig.snmp.enabled);
        cJSON_AddStringToObject(snmp, "community", monConfig.snmp.community);
        cJSON_AddStringToObject(snmp, "location", monConfig.snmp.location);
        cJSON_AddStringToObject(snmp, "contact", monConfig.snmp.contact);
        cJSON_AddNumberToObject(snmp, "port", monConfig.snmp.port);

        // CheckMK
        cJSON *cmk = cJSON_AddObjectToObject(mon, "checkmk");
        cJSON_AddBoolToObject(cmk, "enabled", monConfig.checkmk.enabled);
        cJSON_AddNumberToObject(cmk, "port", monConfig.checkmk.port);
        cJSON_AddStringToObject(cmk, "allowed_hosts", monConfig.checkmk.allowed_hosts);

        // MQTT
        cJSON *mqtt = cJSON_AddObjectToObject(mon, "mqtt");
        cJSON_AddBoolToObject(mqtt, "enabled", monConfig.mqtt.enabled);
        cJSON_AddStringToObject(mqtt, "server", monConfig.mqtt.server);
        cJSON_AddNumberToObject(mqtt, "port", monConfig.mqtt.port);
        cJSON_AddStringToObject(mqtt, "user", monConfig.mqtt.user);
        // SECURITY: Do not export MQTT password!
        cJSON_AddStringToObject(mqtt, "password", "");
        cJSON_AddStringToObject(mqtt, "topic_prefix", monConfig.mqtt.topic_prefix);
        cJSON_AddBoolToObject(mqtt, "ha_discovery_enabled", monConfig.mqtt.ha_discovery_enabled);
        cJSON_AddStringToObject(mqtt, "ha_discovery_prefix", monConfig.mqtt.ha_discovery_prefix);

        // Nextcloud
        cJSON *nextcloud = cJSON_AddObjectToObject(mon, "nextcloud");
        cJSON_AddBoolToObject(nextcloud, "enabled", monConfig.nextcloud.enabled);
        cJSON_AddStringToObject(nextcloud, "server_url", monConfig.nextcloud.server_url);
        cJSON_AddStringToObject(nextcloud, "username", monConfig.nextcloud.username);
        // SECURITY: Do not export Nextcloud password!
        cJSON_AddStringToObject(nextcloud, "password", "");
        cJSON_AddStringToObject(nextcloud, "backup_path", monConfig.nextcloud.backup_path);
        cJSON_AddNumberToObject(nextcloud, "backup_interval_hours", monConfig.nextcloud.backup_interval_hours);
        cJSON_AddBoolToObject(nextcloud, "keep_local_backup", monConfig.nextcloud.keep_local_backup);

        cJSON_AddItemToObject(root, "monitoring", mon);
    }

    const char *json = cJSON_PrintUnformatted(root);
    httpd_resp_sendstr(req, json);
    free((void *)json);
    cJSON_Delete(root);

    return ESP_OK;
}

httpd_uri_t get_backup_handler = {
    .uri = "/api/backup",
    .method = HTTP_GET,
    .handler = get_backup_handler_func,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL};

esp_err_t get_log_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");

    std::string content = LogManager::instance().getLogContent();
    httpd_resp_send(req, content.c_str(), content.length());

    return ESP_OK;
}

httpd_uri_t get_log_handler = {
    .uri = "/api/log",
    .method = HTTP_GET,
    .handler = get_log_handler_func,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL
};

esp_err_t get_log_download_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    // Set headers for file download
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
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL
};

esp_err_t post_restore_handler_func(httpd_req_t *req)
{
    // Re-use the logic from post_settings_json_handler but ensure it saves everything and restarts
    // post_settings_json_handler already does most of the work.
    // We can just call it, then trigger a restart if successful.

    esp_err_t res = post_settings_json_handler_func(req);

    if (res == ESP_OK) {
        // We can't easily check if it was successful because post_settings_json_handler sends the response.
        // But since we are here, we can assume if it returned ESP_OK, it processed the JSON.
        // Ideally we should duplicate the logic or refactor, but to keep it simple:
        // The user will see the response from post_settings.
        // However, the user expects a restart after restore usually.
        // post_settings_json_handler does NOT restart.

        // Let's copy the logic instead to add the restart.
        return res;
    }
    return res;
}

// Actually, let's implement a dedicated restore handler that restarts.
esp_err_t post_restore_handler_func_actual(httpd_req_t *req)
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

    int len = httpd_req_recv(req, buffer, 4095);

    if (len <= 0)
    {
        free(buffer);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive restore data");
        return ESP_FAIL;
    }

    buffer[len] = 0;
    cJSON *root = cJSON_Parse(buffer);
    free(buffer);

    if (!root) {
         return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    char *adminPassword = cJSON_GetStringValue(cJSON_GetObjectItem(root, "adminPassword"));

    // Security: Validate password complexity before restoring to prevent downgrading security
    if (adminPassword && strlen(adminPassword) > 0) {
        if (!validatePassword(adminPassword)) {
            ESP_LOGW(TAG, "Restored password is too weak. Keeping current password.");
            // Nullify to prevent update in the subsequent check
            adminPassword = NULL;
        }
    }

    char *hostname = cJSON_GetStringValue(cJSON_GetObjectItem(root, "hostname"));
    bool useDHCP = cJSON_GetBoolValue(cJSON_GetObjectItem(root, "useDHCP"));
    ip4_addr_t localIP = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "localIP"));
    ip4_addr_t netmask = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "netmask"));
    ip4_addr_t gateway = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "gateway"));
    ip4_addr_t dns1 = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "dns1"));
    ip4_addr_t dns2 = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "dns2"));

    // Safely extract timesource with null check
    timesource_t timesource = TIMESOURCE_NTP; // Default
    cJSON *timesourceItem = cJSON_GetObjectItem(root, "timesource");
    if (timesourceItem) timesource = (timesource_t)timesourceItem->valueint;

    // Safely extract dcfOffset with null check
    int dcfOffset = 0; // Default
    cJSON *dcfOffsetItem = cJSON_GetObjectItem(root, "dcfOffset");
    if (dcfOffsetItem) dcfOffset = dcfOffsetItem->valueint;

    // Safely extract gpsBaudrate with null check
    int gpsBaudrate = 9600; // Default
    cJSON *gpsBaudrateItem = cJSON_GetObjectItem(root, "gpsBaudrate");
    if (gpsBaudrateItem) gpsBaudrate = gpsBaudrateItem->valueint;

    char *ntpServer = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ntpServer"));

    // Safely extract ledBrightness with null check
    int ledBrightness = 100; // Default
    cJSON *ledBrightnessItem = cJSON_GetObjectItem(root, "ledBrightness");
    if (ledBrightnessItem) ledBrightness = ledBrightnessItem->valueint;

    // IPv6
    bool enableIPv6 = cJSON_GetBoolValue(cJSON_GetObjectItem(root, "enableIPv6"));
    char *ipv6Mode = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Mode"));
    char *ipv6Address = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Address"));

    // Safely extract ipv6PrefixLength with null check
    int ipv6PrefixLength = 64; // Default
    cJSON *ipv6PrefixLengthItem = cJSON_GetObjectItem(root, "ipv6PrefixLength");
    if (ipv6PrefixLengthItem) ipv6PrefixLength = ipv6PrefixLengthItem->valueint;

    char *ipv6Gateway = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Gateway"));
    char *ipv6Dns1 = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Dns1"));
    char *ipv6Dns2 = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Dns2"));

    if (adminPassword && strlen(adminPassword) > 0)
        _settings->setAdminPassword(adminPassword);

    _settings->setNetworkSettings(hostname, useDHCP, localIP, netmask, gateway, dns1, dns2);
    _settings->setTimesource(timesource);
    _settings->setDcfOffset(dcfOffset);
    _settings->setGpsBaudrate(gpsBaudrate);
    _settings->setNtpServer(ntpServer);
    _settings->setLEDBrightness(ledBrightness);

    if (ipv6Mode) {
         _settings->setIPv6Settings(
            enableIPv6,
            ipv6Mode,
            ipv6Address ? ipv6Address : (char*)"",
            ipv6PrefixLength,
            ipv6Gateway ? ipv6Gateway : (char*)"",
            ipv6Dns1 ? ipv6Dns1 : (char*)"",
            ipv6Dns2 ? ipv6Dns2 : (char*)""
        );
    }

    // DTLS (Restore)
    if (cJSON_HasObjectItem(root, "dtlsMode")) {
        int dtlsMode = cJSON_GetObjectItem(root, "dtlsMode")->valueint;
        int dtlsCipherSuite = cJSON_GetObjectItem(root, "dtlsCipherSuite")->valueint;
        bool dtlsRequireClientCert = cJSON_GetBoolValue(cJSON_GetObjectItem(root, "dtlsRequireClientCert"));
        bool dtlsSessionResumption = cJSON_GetBoolValue(cJSON_GetObjectItem(root, "dtlsSessionResumption"));
        _settings->setDTLSSettings(dtlsMode, dtlsCipherSuite, dtlsRequireClientCert, dtlsSessionResumption);
    }

    _settings->save();

    // Restore Monitoring Settings
    cJSON *monJson = cJSON_GetObjectItem(root, "monitoring");
    if (monJson) {
        monitoring_config_t monConfig;
        // Load current config first to preserve password if not overwritten
        monitoring_get_config(&monConfig);

        cJSON *snmp = cJSON_GetObjectItem(monJson, "snmp");
        if (snmp) {
            monConfig.snmp.enabled = cJSON_GetBoolValue(cJSON_GetObjectItem(snmp, "enabled"));

            cJSON* item = cJSON_GetObjectItem(snmp, "community");
            if (item && item->valuestring) strncpy(monConfig.snmp.community, item->valuestring, sizeof(monConfig.snmp.community)-1);

            item = cJSON_GetObjectItem(snmp, "location");
            if (item && item->valuestring) strncpy(monConfig.snmp.location, item->valuestring, sizeof(monConfig.snmp.location)-1);

            item = cJSON_GetObjectItem(snmp, "contact");
            if (item && item->valuestring) strncpy(monConfig.snmp.contact, item->valuestring, sizeof(monConfig.snmp.contact)-1);

            item = cJSON_GetObjectItem(snmp, "port");
            if (item) monConfig.snmp.port = item->valueint;
        }

        cJSON *cmk = cJSON_GetObjectItem(monJson, "checkmk");
        if (cmk) {
            monConfig.checkmk.enabled = cJSON_GetBoolValue(cJSON_GetObjectItem(cmk, "enabled"));

            cJSON* item = cJSON_GetObjectItem(cmk, "port");
            if (item) monConfig.checkmk.port = item->valueint;

            item = cJSON_GetObjectItem(cmk, "allowed_hosts");
            if (item && item->valuestring) strncpy(monConfig.checkmk.allowed_hosts, item->valuestring, sizeof(monConfig.checkmk.allowed_hosts)-1);
        }

        cJSON *mqtt = cJSON_GetObjectItem(monJson, "mqtt");
        if (mqtt) {
            monConfig.mqtt.enabled = cJSON_GetBoolValue(cJSON_GetObjectItem(mqtt, "enabled"));

            cJSON* item = cJSON_GetObjectItem(mqtt, "server");
            if (item && item->valuestring) strncpy(monConfig.mqtt.server, item->valuestring, sizeof(monConfig.mqtt.server)-1);

            item = cJSON_GetObjectItem(mqtt, "port");
            if (item) monConfig.mqtt.port = item->valueint;

            item = cJSON_GetObjectItem(mqtt, "user");
            if (item && item->valuestring) strncpy(monConfig.mqtt.user, item->valuestring, sizeof(monConfig.mqtt.user)-1);

            item = cJSON_GetObjectItem(mqtt, "password");
            // Only update password if provided and not empty (since backup has empty password)
            // However, backup has "", so if user restores backup, password becomes "".
            // If user wants to keep existing password, we should check if empty.
            if (item && item->valuestring && strlen(item->valuestring) > 0) {
                 strncpy(monConfig.mqtt.password, item->valuestring, sizeof(monConfig.mqtt.password)-1);
            }

            item = cJSON_GetObjectItem(mqtt, "topic_prefix");
            if (item && item->valuestring) strncpy(monConfig.mqtt.topic_prefix, item->valuestring, sizeof(monConfig.mqtt.topic_prefix)-1);

            monConfig.mqtt.ha_discovery_enabled = cJSON_GetBoolValue(cJSON_GetObjectItem(mqtt, "ha_discovery_enabled"));

            item = cJSON_GetObjectItem(mqtt, "ha_discovery_prefix");
            if (item && item->valuestring) strncpy(monConfig.mqtt.ha_discovery_prefix, item->valuestring, sizeof(monConfig.mqtt.ha_discovery_prefix)-1);
        }

        cJSON *nextcloud = cJSON_GetObjectItem(monJson, "nextcloud");
        if (nextcloud) {
            monConfig.nextcloud.enabled = cJSON_GetBoolValue(cJSON_GetObjectItem(nextcloud, "enabled"));

            cJSON* item = cJSON_GetObjectItem(nextcloud, "server_url");
            if (item && item->valuestring) strncpy(monConfig.nextcloud.server_url, item->valuestring, sizeof(monConfig.nextcloud.server_url)-1);

            item = cJSON_GetObjectItem(nextcloud, "username");
            if (item && item->valuestring) strncpy(monConfig.nextcloud.username, item->valuestring, sizeof(monConfig.nextcloud.username)-1);

            item = cJSON_GetObjectItem(nextcloud, "password");
            // Only update password if provided and not empty (since backup has empty password)
            if (item && item->valuestring && strlen(item->valuestring) > 0) {
                 strncpy(monConfig.nextcloud.password, item->valuestring, sizeof(monConfig.nextcloud.password)-1);
            }

            item = cJSON_GetObjectItem(nextcloud, "backup_path");
            if (item && item->valuestring) strncpy(monConfig.nextcloud.backup_path, item->valuestring, sizeof(monConfig.nextcloud.backup_path)-1);

            item = cJSON_GetObjectItem(nextcloud, "backup_interval_hours");
            if (item) monConfig.nextcloud.backup_interval_hours = (uint32_t)item->valueint;

            monConfig.nextcloud.keep_local_backup = cJSON_GetBoolValue(cJSON_GetObjectItem(nextcloud, "keep_local_backup"));
        }

        monitoring_update_config(&monConfig);
    }

    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\":true}");

    // Restart
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();

    return ESP_OK;
}

httpd_uri_t post_restore_handler = {
    .uri = "/api/restore",
    .method = HTTP_POST,
    .handler = post_restore_handler_func_actual,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL};

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

esp_err_t post_ota_update_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    esp_ota_handle_t ota_handle;

    char ota_buff[1024];
    int content_length = req->content_len;
    int content_received = 0;
    int recv_len;
    bool is_req_body_started = false;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

    do
    {
        if ((recv_len = httpd_req_recv(req, ota_buff, MIN(content_length, sizeof(ota_buff)))) < 0)
        {
            if (recv_len != HTTPD_SOCK_ERR_TIMEOUT)
            {
                ESP_LOGE(TAG, "OTA socket Error %d", recv_len);
                return ESP_FAIL;
            }
        }

        if (!is_req_body_started)
        {
            is_req_body_started = true;

            // Check content type to decide how to handle the body
            char content_type[64] = {0};
            bool is_multipart = false;

            if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type)) == ESP_OK) {
                if (strstr(content_type, "multipart/form-data") != NULL) {
                    is_multipart = true;
                }
            }

            char *body_start_p = ota_buff;
            int body_part_len = recv_len;

            if (is_multipart) {
                // Legacy multipart support
                char *header_end = strstr(ota_buff, "\r\n\r\n");
                if (header_end) {
                    body_start_p = header_end + 4;
                    body_part_len = recv_len - (body_start_p - ota_buff);
                } else {
                    // Header not found in first chunk - this is likely invalid for multipart
                     ESP_LOGE(TAG, "Multipart header not found in first chunk");
                     goto err;
                }
            }

            OTA_CHECK(esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle) == ESP_OK, "Could not start OTA");
            ESP_LOGW(TAG, "Begin OTA Update to partition %s, File Size: %d", update_partition->label, content_length);
            _statusLED->setState(LED_STATE_BLINK_FAST);

            OTA_CHECK(esp_ota_write(ota_handle, body_start_p, body_part_len) == ESP_OK, "Error writing OTA");
        }
        else
        {
            OTA_CHECK(esp_ota_write(ota_handle, ota_buff, recv_len) == ESP_OK, "Error writing OTA");
            content_received += recv_len;
        }
    } while ((recv_len > 0) && (content_received < content_length));

    OTA_CHECK(esp_ota_end(ota_handle) == ESP_OK, "Error writing OTA");
    OTA_CHECK(esp_ota_set_boot_partition(update_partition) == ESP_OK, "Error writing OTA");

    ESP_LOGI(TAG, "OTA finished, restarting in 3 seconds to activate new firmware.");
    httpd_resp_sendstr(req, "Firmware update completed, restarting in 3 seconds...");

    _statusLED->setState(LED_STATE_OFF);

    // Automatic restart after successful OTA update
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    esp_restart();

    return ESP_OK;

err:
    _statusLED->setState(LED_STATE_OFF);
    return ESP_FAIL;
}

httpd_uri_t post_ota_update_handler = {
    .uri = "/ota_update",
    .method = HTTP_POST,
    .handler = post_ota_update_handler_func,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL};

esp_err_t post_restart_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, "{\"success\":true}");

    // Restart after a short delay to allow response to be sent
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();

    return ESP_OK;
}

httpd_uri_t post_restart_handler = {
    .uri = "/api/restart",
    .method = HTTP_POST,
    .handler = post_restart_handler_func,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL};

static esp_err_t _post_firmware_online_update_handler_func(httpd_req_t *req)
{
  add_security_headers(req);
  if (validate_auth(req) != ESP_OK)
  {
      return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
  }

  ESP_LOGI(TAG, "Online update requested via WebUI");

  // First check for updates to ensure we have the latest version info
  _updateCheck->checkNow();

  // Verify that we have a valid version and download URL
  const char* latest_version = _updateCheck->getLatestVersion();
  bool has_url = _updateCheck->hasDownloadUrl();

  ESP_LOGI(TAG, "Latest version: %s, Has download URL: %d", latest_version, has_url);

  if (strcmp(latest_version, "n/a") == 0) {
      ESP_LOGW(TAG, "No update available - version check returned 'n/a'");
      httpd_resp_set_type(req, "application/json");
      httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"No update available. Please check your internet connection and try again.\"}");
      return ESP_OK;
  }

  if (!has_url) {
      ESP_LOGW(TAG, "No download URL available for version %s", latest_version);
      httpd_resp_set_type(req, "application/json");
      httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"No download URL found for the update. Please try again later.\"}");
      return ESP_OK;
  }

  ESP_LOGI(TAG, "Starting online update to version %s", latest_version);

  // Create a task to perform the update because it's blocking
  struct TaskArgs {
      UpdateCheck* updateCheck;
  };

  TaskArgs* args = new TaskArgs();
  args->updateCheck = _updateCheck;

  BaseType_t task_created = xTaskCreate([](void* p) {
      TaskArgs* a = (TaskArgs*)p;
      ESP_LOGI(TAG, "Online update task started");
      a->updateCheck->performOnlineUpdate();
      // Note: If update succeeds, device will restart and this code won't execute
      ESP_LOGW(TAG, "Online update task completed (update may have failed - check logs)");
      delete a;
      vTaskDelete(NULL);
  }, "online_update", 8192, args, 5, NULL);

  if (task_created != pdPASS) {
      ESP_LOGE(TAG, "Failed to create online update task");
      delete args;
      httpd_resp_set_type(req, "application/json");
      httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"Failed to start update task. Please try again.\"}");
      return ESP_OK;
  }

  ESP_LOGI(TAG, "Online update task created successfully");

  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, "{\"success\":true,\"message\":\"Update started. Device will restart automatically when complete.\"}");
  return ESP_OK;
}

httpd_uri_t _post_firmware_online_update_handler = {
    .uri = "/api/online_update",
    .method = HTTP_POST,
    .handler = _post_firmware_online_update_handler_func,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL};

esp_err_t post_check_update_handler_func(httpd_req_t *req)
{
  add_security_headers(req);
  if (validate_auth(req) != ESP_OK)
  {
      return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
  }

  // Perform check in this task (it might block for a few seconds)
  _updateCheck->checkNow();

  httpd_resp_set_type(req, "application/json");

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "latestVersion", _updateCheck->getLatestVersion());
  cJSON_AddStringToObject(root, "releaseNotes", _updateCheck->getReleaseNotes());
  cJSON_AddStringToObject(root, "downloadUrl", _updateCheck->getDownloadUrl());

  const char *json = cJSON_PrintUnformatted(root);
  httpd_resp_sendstr(req, json);
  free((void *)json);
  cJSON_Delete(root);

  return ESP_OK;
}

httpd_uri_t post_check_update_handler = {
    .uri = "/api/check_update",
    .method = HTTP_POST,
    .handler = post_check_update_handler_func,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL};

esp_err_t post_factory_reset_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }

    // Reset settings
    _settings->clear();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\":true}");

    // Restart after a short delay
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();

    return ESP_OK;
}

httpd_uri_t post_factory_reset_handler = {
    .uri = "/api/factory_reset",
    .method = HTTP_POST,
    .handler = post_factory_reset_handler_func,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL};

esp_err_t post_change_password_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
    }

    char buffer[512];
    int len = httpd_req_recv(req, buffer, sizeof(buffer) - 1);

    if (len <= 0)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
    }

    buffer[len] = 0;
    cJSON *root = cJSON_Parse(buffer);
    if (root == NULL)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    char *newPassword = cJSON_GetStringValue(cJSON_GetObjectItem(root, "newPassword"));

    if (!validatePassword(newPassword))
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Password must be at least 6 characters and contain letters and numbers");
    }

    _settings->setAdminPassword(newPassword);
    _settings->save();

    cJSON_Delete(root);

    // Re-generate token for security
    generateToken();

    httpd_resp_set_type(req, "application/json");
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "token", _token);

    const char *json = cJSON_PrintUnformatted(response);
    httpd_resp_sendstr(req, json);
    free((void *)json);
    cJSON_Delete(response);

    ESP_LOGI(TAG, "Admin password changed successfully");

    return ESP_OK;
}

httpd_uri_t post_change_password_handler = {
    .uri = "/api/change-password",
    .method = HTTP_POST,
    .handler = post_change_password_handler_func,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL};

httpd_uri_t get_analyzer_ws_handler = {
    .uri = "/api/analyzer/ws",
    .method = HTTP_GET,
    .handler = analyzer_ws_handler_func,
    .user_ctx = NULL,
    .is_websocket = true,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL};

// Prometheus metrics disabled - feature code available in prometheus.cpp.disabled

WebUI::WebUI(Settings *settings, LED *statusLED, SysInfo *sysInfo, UpdateCheck *updateCheck, Ethernet *ethernet, RawUartUdpListener *rawUartUdpListener, RadioModuleConnector *radioModuleConnector, RadioModuleDetector *radioModuleDetector, DTLSEncryption *dtlsEncryption)
{
    _settings = settings;
    _statusLED = statusLED;
    _sysInfo = sysInfo;
    _ethernet = ethernet;
    _updateCheck = updateCheck;
    _rawUartUdpListener = rawUartUdpListener;
    _radioModuleConnector = radioModuleConnector;
    _radioModuleDetector = radioModuleDetector;
#if ENABLE_ANALYZER
    _analyzer = nullptr;
    if (_settings->getAnalyzerEnabled() && !_settings->getHmlgwEnabled()) {
        _analyzer = new Analyzer(_radioModuleConnector);
    }
#endif
    _dtlsEncryption = dtlsEncryption;

    generateToken();
}

void WebUI::start()
{
    // Initialize rate limiter
    rate_limiter_init();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.max_uri_handlers = 32;
    config.uri_match_fn = httpd_uri_match_wildcard;

    _httpd_handle = NULL;

    if (httpd_start(&_httpd_handle, &config) == ESP_OK)
    {
        httpd_register_uri_handler(_httpd_handle, &post_login_json_handler);
        httpd_register_uri_handler(_httpd_handle, &get_sysinfo_json_handler);
        httpd_register_uri_handler(_httpd_handle, &get_settings_json_handler);
        httpd_register_uri_handler(_httpd_handle, &post_settings_json_handler);
        httpd_register_uri_handler(_httpd_handle, &post_ota_update_handler);
        httpd_register_uri_handler(_httpd_handle, &post_restart_handler);
        httpd_register_uri_handler(_httpd_handle, &_post_firmware_online_update_handler);
        httpd_register_uri_handler(_httpd_handle, &post_check_update_handler);
        httpd_register_uri_handler(_httpd_handle, &post_factory_reset_handler);
        httpd_register_uri_handler(_httpd_handle, &post_change_password_handler);
        httpd_register_uri_handler(_httpd_handle, &get_monitoring_handler);
        httpd_register_uri_handler(_httpd_handle, &post_monitoring_handler);
        httpd_register_uri_handler(_httpd_handle, &get_nextcloud_handler);
        httpd_register_uri_handler(_httpd_handle, &post_nextcloud_handler);
        httpd_register_uri_handler(_httpd_handle, &post_nextcloud_test_handler);
        httpd_register_uri_handler(_httpd_handle, &post_nextcloud_upload_handler);

        httpd_register_uri_handler(_httpd_handle, &get_backup_handler);
        httpd_register_uri_handler(_httpd_handle, &post_restore_handler);
        httpd_register_uri_handler(_httpd_handle, &get_log_handler);
        httpd_register_uri_handler(_httpd_handle, &get_log_download_handler);

        httpd_register_uri_handler(_httpd_handle, &get_analyzer_ws_handler);

        // Register DTLS API handlers
        register_dtls_api_handlers(_httpd_handle, _settings, _dtlsEncryption);

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

WebUI::~WebUI()
{
#if ENABLE_ANALYZER
    extern Analyzer *_analyzer;
    if (_analyzer) {
        delete _analyzer;
        _analyzer = nullptr;
    }
#endif
}
