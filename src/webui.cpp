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
#include "webui.h"
#include "esp_log.h"
#include "cJSON.h"
#include "validation.h"
#include "esp_ota_ops.h"
#include "mbedtls/md.h"
#include "mbedtls/base64.h"
#include "monitoring_api.h"
#include "rate_limiter.h"
#include "analyzer.h"
// #include "prometheus.h"

static const char *TAG = "WebUI";

#define EMBED_HANDLER(_uri, _resource, _contentType)                   \
    extern const char _resource[] asm("_binary_" #_resource "_start"); \
    extern const size_t _resource##_length asm(#_resource "_length");  \
    esp_err_t _resource##_handler_func(httpd_req_t *req)               \
    {                                                                  \
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
static Analyzer *_analyzer;
static char _token[46];

void generateToken()
{
    char tokenBase[21];
    *((uint32_t *)tokenBase) = esp_random();
    *((uint32_t *)(tokenBase + sizeof(uint32_t))) = esp_random();
    strcpy(tokenBase + 2 * sizeof(uint32_t), _sysInfo->getSerialNumber());

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
    size_t min_len = (len1 < len2) ? len1 : len2;
    int result = 0;

    if (len1 != len2) {
        result = 1;
    }

    for (size_t i = 0; i < min_len; i++) {
        result |= (s1[i] ^ s2[i]);
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

    if (len > 0)
    {
        buffer[len] = 0;

        cJSON *root = cJSON_Parse(buffer);

        char *password = cJSON_GetStringValue(cJSON_GetObjectItem(root, "password"));

        bool isAuthenticated = (password != NULL) && (secure_strcmp(password, _settings->getAdminPassword()) == 0);

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

    return ESP_FAIL;
}

httpd_uri_t post_login_json_handler = {
    .uri = "/login.json",
    .method = HTTP_POST,
    .handler = post_login_json_handler_func,
    .user_ctx = NULL};

esp_err_t get_sysinfo_json_handler_func(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");

    // Optimization: Use stack buffer and snprintf instead of cJSON to reduce heap allocations
    // This handler is called frequently (1Hz) by the frontend.
    char buffer[1536]; // Increased buffer to be safe, ESP32 stack usually allows this

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
        ip2str(_rawUartUdpListener->getConnectedRemoteAddress()),
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
    .user_ctx = NULL};

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
}

esp_err_t get_settings_json_handler_func(httpd_req_t *req)
{
    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
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
    .user_ctx = NULL};

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
    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    char buffer[1024];
    int len = httpd_req_recv(req, buffer, sizeof(buffer) - 1);

    if (len > 0)
    {
        buffer[len] = 0;
        cJSON *root = cJSON_Parse(buffer);

        char *adminPassword = cJSON_GetStringValue(cJSON_GetObjectItem(root, "adminPassword"));

    // Check for password length to prevent truncation/lockout
    if (adminPassword && strlen(adminPassword) > MAX_PASSWORD_LENGTH) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Password too long");
    }

        char *hostname = cJSON_GetStringValue(cJSON_GetObjectItem(root, "hostname"));
        bool useDHCP = cJSON_GetBoolValue(cJSON_GetObjectItem(root, "useDHCP"));
        ip4_addr_t localIP = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "localIP"));
        ip4_addr_t netmask = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "netmask"));
        ip4_addr_t gateway = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "gateway"));
        ip4_addr_t dns1 = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "dns1"));
        ip4_addr_t dns2 = cJSON_GetIPAddrValue(cJSON_GetObjectItem(root, "dns2"));

        timesource_t timesource = (timesource_t)cJSON_GetObjectItem(root, "timesource")->valueint;

        int dcfOffset = cJSON_GetObjectItem(root, "dcfOffset")->valueint;

        int gpsBaudrate = cJSON_GetObjectItem(root, "gpsBaudrate")->valueint;

        char *ntpServer = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ntpServer"));

        int ledBrightness = cJSON_GetObjectItem(root, "ledBrightness")->valueint;

        // IPv6
        bool enableIPv6 = cJSON_GetBoolValue(cJSON_GetObjectItem(root, "enableIPv6"));
        char *ipv6Mode = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Mode"));
        char *ipv6Address = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Address"));
        int ipv6PrefixLength = cJSON_GetObjectItem(root, "ipv6PrefixLength")->valueint;
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

        _settings->save();

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

    return ESP_FAIL;
}

httpd_uri_t post_settings_json_handler = {
    .uri = "/settings.json",
    .method = HTTP_POST,
    .handler = post_settings_json_handler_func,
    .user_ctx = NULL};

esp_err_t get_backup_handler_func(httpd_req_t *req)
{
    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"settings.json\"");

    cJSON *root = cJSON_CreateObject();

    // Add all settings including admin password (for full restore)
    cJSON_AddStringToObject(root, "adminPassword", _settings->getAdminPassword());

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
    .user_ctx = NULL};

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

    if (len > 0)
    {
        buffer[len] = 0;
        cJSON *root = cJSON_Parse(buffer);
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

        timesource_t timesource = (timesource_t)cJSON_GetObjectItem(root, "timesource")->valueint;

        int dcfOffset = cJSON_GetObjectItem(root, "dcfOffset")->valueint;

        int gpsBaudrate = cJSON_GetObjectItem(root, "gpsBaudrate")->valueint;

        char *ntpServer = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ntpServer"));

        int ledBrightness = cJSON_GetObjectItem(root, "ledBrightness")->valueint;

        // IPv6
        bool enableIPv6 = cJSON_GetBoolValue(cJSON_GetObjectItem(root, "enableIPv6"));
        char *ipv6Mode = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Mode"));
        char *ipv6Address = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Address"));
        int ipv6PrefixLength = cJSON_GetObjectItem(root, "ipv6PrefixLength")->valueint;
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

        _settings->save();

        cJSON_Delete(root);

        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":true}");

        // Restart
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_restart();

        return ESP_OK;
    }

    return ESP_FAIL;
}

httpd_uri_t post_restore_handler = {
    .uri = "/api/restore",
    .method = HTTP_POST,
    .handler = post_restore_handler_func_actual,
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

esp_err_t post_ota_update_handler_func(httpd_req_t *req)
{
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
    .user_ctx = NULL};

esp_err_t post_restart_handler_func(httpd_req_t *req)
{
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
    .user_ctx = NULL};

static esp_err_t _post_firmware_online_update_handler_func(httpd_req_t *req)
{
  if (validate_auth(req) != ESP_OK)
  {
      return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, NULL);
  }

  // Create a task to perform the update because it's blocking
  struct TaskArgs {
      UpdateCheck* updateCheck;
  };

  TaskArgs* args = new TaskArgs();
  args->updateCheck = _updateCheck;

  xTaskCreate([](void* p) {
      TaskArgs* a = (TaskArgs*)p;
      a->updateCheck->performOnlineUpdate();
      delete a;
      vTaskDelete(NULL);
  }, "online_update", 8192, args, 5, NULL);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, "{\"success\":true}");
  return ESP_OK;
}

httpd_uri_t _post_firmware_online_update_handler = {
    .uri = "/api/online_update",
    .method = HTTP_POST,
    .handler = _post_firmware_online_update_handler_func,
    .user_ctx = NULL};

esp_err_t post_change_password_handler_func(httpd_req_t *req)
{
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

    // Regex: ^(?=.*[A-Za-z])(?=.*\d)[A-Za-z\d]{6,}$ - approximated with manual checks
    bool has_letter = false;
    bool has_digit = false;
    bool is_valid_length = (newPassword != NULL) && (strlen(newPassword) >= 6) && (strlen(newPassword) <= MAX_PASSWORD_LENGTH);

    if (is_valid_length) {
        for (int i = 0; newPassword[i] != 0; i++) {
            if ((newPassword[i] >= 'a' && newPassword[i] <= 'z') || (newPassword[i] >= 'A' && newPassword[i] <= 'Z')) {
                has_letter = true;
            } else if (newPassword[i] >= '0' && newPassword[i] <= '9') {
                has_digit = true;
            }
        }
    }

    if (!is_valid_length || !has_letter || !has_digit)
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
    .user_ctx = NULL};

httpd_uri_t get_analyzer_ws_handler = {
    .uri = "/api/analyzer/ws",
    .method = HTTP_GET,
    .handler = Analyzer::ws_handler,
    .user_ctx = NULL,
    .is_websocket = true
};

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
    _analyzer = new Analyzer(_radioModuleConnector);

    generateToken();
}

void WebUI::start()
{
    // Initialize rate limiter
    rate_limiter_init();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.max_uri_handlers = 20;
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
        httpd_register_uri_handler(_httpd_handle, &post_change_password_handler);
        httpd_register_uri_handler(_httpd_handle, &get_monitoring_handler);
        httpd_register_uri_handler(_httpd_handle, &post_monitoring_handler);

        httpd_register_uri_handler(_httpd_handle, &get_backup_handler);
        httpd_register_uri_handler(_httpd_handle, &post_restore_handler);

        httpd_register_uri_handler(_httpd_handle, &get_analyzer_ws_handler);

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
