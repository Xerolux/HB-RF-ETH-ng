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
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "webui.h"
#include "esp_log.h"
#include "cJSON.h"
#include "validation.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
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
#include "secure_utils.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

static const char *TAG = "WebUI";

// FIX: Use std::atomic instead of volatile for proper thread safety
static std::atomic<bool> _restart_requested{false};
static TaskHandle_t _restart_task_handle = NULL;

/**
 * Safe restart task - runs at highest priority to ensure clean shutdown
 *
 * This task:
 * 1. Gives time for HTTP response to be sent
 * 2. Waits for all pending operations to complete
 * 3. Calls esp_restart() with proper cleanup
 */
static void restart_task(void* pvParameters) {
    ESP_LOGI(TAG, "Restart task initiated - preparing for system restart");

    // Give the HTTP response time to be sent to client
    vTaskDelay(pdMS_TO_TICKS(500));

    // Log the restart reason
    ESP_LOGW(TAG, "System restart requested - esp_restart() will be called");

    // Disable watchdogs that might interfere with restart
    esp_task_wdt_reset();

    // Additional delay to ensure all buffers are flushed
    vTaskDelay(pdMS_TO_TICKS(200));

    // Now perform the restart
    ESP_LOGI(TAG, "Calling esp_restart()...");

    // Disable interrupts and restart
    esp_restart();

    // Should never reach here, but just in case:
    vTaskDelete(NULL);
}

/**
 * Trigger a system restart via a dedicated high-priority task
 *
 * This is the preferred method for restart as it:
 * - Ensures HTTP response is sent before restart
 * - Runs at high priority to prevent interference
 * - Allows ESP-IDF to properly cleanup resources
 */
static void trigger_restart() {
    // FIX: Use atomic exchange to prevent TOCTOU race between check and set
    bool expected = false;
    if (!_restart_requested.compare_exchange_strong(expected, true)) {
        ESP_LOGW(TAG, "Restart already requested, ignoring duplicate request");
        return;
    }

    ESP_LOGI(TAG, "Scheduling system restart...");

    // Create restart task at very high priority (24, above almost everything)
    // This ensures the restart completes even if system is under load
    BaseType_t ret = xTaskCreate(
        restart_task,
        "restart_task",
        4096,           // Stack size
        NULL,           // Parameters
        24,             // Priority - very high, below ESP-IDF tasks (25-31)
        &_restart_task_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create restart task! Trying immediate restart...");
        // Fallback: immediate restart after short delay
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_restart();
    }
}

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

    secure_zero(tokenBase, sizeof(tokenBase));
    secure_zero(shaResult, sizeof(shaResult));
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
    if (json) {
        httpd_resp_sendstr(req, json);
        free((void *)json);
    } else {
        httpd_resp_send_500(req);
    }
    cJSON_Delete(root);

    secure_zero(buffer, sizeof(buffer));

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
    cJSON_AddStringToObject(sysInfo, "currentVersion", _sysInfo->getCurrentVersion());
    cJSON_AddStringToObject(sysInfo, "firmwareVariant", _sysInfo->getFirmwareVariant());
    cJSON_AddNumberToObject(sysInfo, "memoryUsage", _sysInfo->getMemoryUsage());
    cJSON_AddNumberToObject(sysInfo, "cpuUsage", _sysInfo->getCpuUsage());
    cJSON_AddNumberToObject(sysInfo, "uptimeSeconds", _sysInfo->getUptimeSeconds());
    cJSON_AddStringToObject(sysInfo, "boardRevision", _sysInfo->getBoardRevisionString());
    cJSON_AddStringToObject(sysInfo, "resetReason", _sysInfo->getResetReason());
    cJSON_AddBoolToObject(sysInfo, "ethernetConnected", _ethernet->isConnected());
    cJSON_AddNumberToObject(sysInfo, "ethernetSpeed", _ethernet->getLinkSpeedMbps());
    cJSON_AddStringToObject(sysInfo, "ethernetDuplex", _ethernet->getDuplexMode());
    cJSON_AddStringToObject(sysInfo, "rawUartRemoteAddress", _rawUartUdpListener ? ip2str(_rawUartUdpListener->getConnectedRemoteAddress()) : "HMLGW Mode");
    cJSON_AddStringToObject(sysInfo, "radioModuleType", radioModuleTypeStr);
    cJSON_AddStringToObject(sysInfo, "radioModuleSerial", _radioModuleDetector->getSerial());
    cJSON_AddStringToObject(sysInfo, "radioModuleFirmwareVersion", fwVersionStr);
    cJSON_AddStringToObject(sysInfo, "radioModuleBidCosRadioMAC", bidCosMAC);
    cJSON_AddStringToObject(sysInfo, "radioModuleHmIPRadioMAC", hmIPMAC);
    cJSON_AddStringToObject(sysInfo, "radioModuleSGTIN", _radioModuleDetector->getSGTIN());

    // Feature capabilities (compile-time)
#if ENABLE_HMLGW
    cJSON_AddBoolToObject(sysInfo, "enableHmlgw", true);
#else
    cJSON_AddBoolToObject(sysInfo, "enableHmlgw", false);
#endif
#if ENABLE_ANALYZER
    cJSON_AddBoolToObject(sysInfo, "enableAnalyzer", true);
#else
    cJSON_AddBoolToObject(sysInfo, "enableAnalyzer", false);
#endif

    const char *json = cJSON_PrintUnformatted(root);
    if (json) {
        httpd_resp_sendstr(req, json);
        free((void *)json);
    } else {
        httpd_resp_send_500(req);
    }
    cJSON_Delete(root);


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
    if (json) {
        httpd_resp_sendstr(req, json);
        free((void *)json);
    } else {
        httpd_resp_send_500(req);
    }
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
        // FIX: Check heap before creating Analyzer (uses ~40KB for buffer pool)
        uint32_t free_heap = esp_get_free_heap_size();
        if (free_heap < 60000) {
            ESP_LOGW(TAG, "Insufficient heap for Analyzer: %" PRIu32 " bytes free", free_heap);
            httpd_resp_set_status(req, "503 Service Unavailable");
            httpd_resp_sendstr(req, "Insufficient memory for Analyzer");
            return ESP_OK;
        }
        _analyzer = new Analyzer(_radioModuleConnector);
        if (!_analyzer || !_analyzer->isReady()) {
            ESP_LOGE(TAG, "Failed to initialize Analyzer");
            if (_analyzer) {
                delete _analyzer;
                _analyzer = nullptr;
            }
            httpd_resp_set_status(req, "503 Service Unavailable");
            httpd_resp_sendstr(req, "Failed to initialize Analyzer");
            return ESP_OK;
        }
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
#if ENABLE_ANALYZER
        if (_analyzer == nullptr) {
            _analyzer = new Analyzer(_radioModuleConnector);
        }
#endif
    } else {
#if ENABLE_ANALYZER
        if (_analyzer) {
            delete _analyzer;
            _analyzer = nullptr;
        }
#endif
    }

    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    root = cJSON_CreateObject();

    add_settings(root);

    const char *json = cJSON_PrintUnformatted(root);
    if (json) {
        httpd_resp_sendstr(req, json);
        free((void *)json);
    } else {
        httpd_resp_send_500(req);
    }
    cJSON_Delete(root);

    secure_zero(buffer, sizeof(buffer));
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
    if (json) {
        httpd_resp_sendstr(req, json);
        free((void *)json);
    } else {
        httpd_resp_send_500(req);
    }
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

    std::string content = LogManager::instance().getLogContent(offset);
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

// Paste proxy - uploads log content to paste.blueml.eu (MicroBin) from the backend
// to avoid CORS issues with direct browser-to-paste requests
esp_err_t post_log_paste_handler_func(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    // Get the log content
    std::string log_content = LogManager::instance().getLogContent();
    if (log_content.empty()) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"No log content available\"}");
        return ESP_OK;
    }

    // Build multipart form data for MicroBin upload
    const char *boundary = "----HbRfEthBoundary";

    // Build the multipart body
    std::string body;
    body.reserve(log_content.size() + 512);

    // content field
    body += "--"; body += boundary; body += "\r\n";
    body += "Content-Disposition: form-data; name=\"content\"\r\n\r\n";
    body += log_content;
    body += "\r\n";

    // expiration field
    body += "--"; body += boundary; body += "\r\n";
    body += "Content-Disposition: form-data; name=\"expiration\"\r\n\r\n";
    body += "1week\r\n";

    // privacy field
    body += "--"; body += boundary; body += "\r\n";
    body += "Content-Disposition: form-data; name=\"privacy\"\r\n\r\n";
    body += "unlisted\r\n";

    // syntax_highlight field
    body += "--"; body += boundary; body += "\r\n";
    body += "Content-Disposition: form-data; name=\"syntax_highlight\"\r\n\r\n";
    body += "\r\n";

    // Final boundary
    body += "--"; body += boundary; body += "--\r\n";

    // Configure HTTP client
    esp_http_client_config_t config = {};
    config.url = "https://paste.blueml.eu/upload";
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.buffer_size = 1536;
    config.buffer_size_tx = 512;
    config.timeout_ms = 15000;
    config.disable_auto_redirect = true;  // We handle redirects manually

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Paste: Failed to initialize HTTP client");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"Failed to connect to paste service\"}");
        return ESP_OK;
    }

    // Set headers
    char content_type[80];
    snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", boundary);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", content_type);
    esp_http_client_set_header(client, "User-Agent", "HB-RF-ETH-ng");
    esp_http_client_set_post_field(client, body.c_str(), body.size());

    ESP_LOGI(TAG, "Paste: Uploading %d bytes to paste.blueml.eu", (int)body.size());

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "Paste: HTTP status %d", status_code);

        if (status_code == 302 || status_code == 301 || status_code == 303) {
            // MicroBin redirects to the paste URL on success
            char location[256] = {0};
            // Read the Location header from the redirect response
            char *location_ptr = NULL;
            esp_http_client_get_header(client, "Location", &location_ptr);

            if (location_ptr && strlen(location_ptr) > 0) {
                // Location may be relative (e.g., "/paste/abc123") or absolute
                if (location_ptr[0] == '/') {
                    snprintf(location, sizeof(location), "https://paste.blueml.eu%s", location_ptr);
                } else {
                    strncpy(location, location_ptr, sizeof(location) - 1);
                }
                ESP_LOGI(TAG, "Paste: Success, URL: %s", location);

                char response[384];
                snprintf(response, sizeof(response), "{\"success\":true,\"url\":\"%s\"}", location);
                httpd_resp_set_type(req, "application/json");
                httpd_resp_sendstr(req, response);
            } else {
                ESP_LOGW(TAG, "Paste: Redirect but no Location header");
                httpd_resp_set_type(req, "application/json");
                httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"Paste service returned redirect without location\"}");
            }
        } else if (status_code == 200) {
            // Some MicroBin configs return 200 with the URL in the response body
            char resp_body[256] = {0};
            int read_len = esp_http_client_read(client, resp_body, sizeof(resp_body) - 1);
            if (read_len > 0) {
                resp_body[read_len] = '\0';
                ESP_LOGI(TAG, "Paste: 200 response: %s", resp_body);
            }
            // Return the final URL from the client
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "{\"success\":true,\"url\":\"https://paste.blueml.eu\"}");
        } else {
            ESP_LOGE(TAG, "Paste: Unexpected status code %d", status_code);
            char error_resp[128];
            snprintf(error_resp, sizeof(error_resp), "{\"success\":false,\"error\":\"Paste service returned status %d\"}", status_code);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, error_resp);
        }
    } else {
        ESP_LOGE(TAG, "Paste: HTTP request failed: %s", esp_err_to_name(err));
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"Failed to connect to paste service\"}");
    }

    esp_http_client_cleanup(client);
    return ESP_OK;
}

httpd_uri_t post_log_paste_handler = {
    .uri = "/api/log/paste",
    .method = HTTP_POST,
    .handler = post_log_paste_handler_func,
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
    const size_t RESTORE_BUFFER_SIZE = 4096;

    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    char *buffer = (char*)malloc(RESTORE_BUFFER_SIZE);
    if (!buffer) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
    }

    int len = httpd_req_recv(req, buffer, RESTORE_BUFFER_SIZE - 1);

    if (len <= 0)
    {
        free(buffer);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive restore data");
        return ESP_FAIL;
    }

    buffer[len] = 0;
    cJSON *root = cJSON_Parse(buffer);
    secure_zero(buffer, RESTORE_BUFFER_SIZE);
    free(buffer);

    if (!root) {
         return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    char *adminPassword = cJSON_GetStringValue(cJSON_GetObjectItem(root, "adminPassword"));

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
    httpd_resp_sendstr(req, "{\"success\":true,\"message\":\"Settings restored. System is restarting...\"}");

    // Use trigger_restart() for more reliable reboot
    trigger_restart();

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

    ESP_LOGI(TAG, "OTA finished successfully! Partition: %s, Size: %d bytes",
             update_partition->label, content_received);
    httpd_resp_sendstr(req, "Firmware update completed! System is restarting...");

    _statusLED->setState(LED_STATE_OFF);

    // Use trigger_restart() for more reliable reboot
    // This ensures all connections are properly closed before restart
    trigger_restart();

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
    httpd_resp_sendstr(req, "{\"success\":true,\"message\":\"System is restarting...\"}");

    // Trigger restart via dedicated task (more reliable than direct esp_restart from HTTP handler)
    trigger_restart();

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
    httpd_resp_sendstr(req, "{\"success\":true,\"message\":\"Factory reset complete. System is restarting...\"}");

    // Use trigger_restart() for more reliable reboot
    trigger_restart();

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
    if (json) {
        httpd_resp_sendstr(req, json);
        free((void *)json);
    } else {
        httpd_resp_send_500(req);
    }
    cJSON_Delete(response);

    ESP_LOGI(TAG, "Admin password changed successfully");

    secure_zero(buffer, sizeof(buffer));

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

WebUI::WebUI(Settings *settings, LED *statusLED, SysInfo *sysInfo, Ethernet *ethernet, RawUartUdpListener *rawUartUdpListener, RadioModuleConnector *radioModuleConnector, RadioModuleDetector *radioModuleDetector, DTLSEncryption *dtlsEncryption)
{
    _settings = settings;
    _statusLED = statusLED;
    _sysInfo = sysInfo;
    _ethernet = ethernet;
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
    config.stack_size = 8192;
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
        httpd_register_uri_handler(_httpd_handle, &post_log_paste_handler);

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
