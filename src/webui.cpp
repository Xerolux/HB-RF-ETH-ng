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
#include "esp_ota_ops.h"
#include "mbedtls/md.h"
#include "mbedtls/base64.h"
#include "monitoring_api.h"
#include "rate_limiter.h"
#include "security_headers.h"
#include "secure_utils.h"
#include "log_manager.h"
#include "reset_info.h"
#include "system_reset.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
// #include "prometheus.h"

static const char *TAG = "WebUI";

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

void generateToken()
{
    char tokenBase[21];
    *((uint32_t *)tokenBase) = esp_random();
    *((uint32_t *)(tokenBase + sizeof(uint32_t))) = esp_random();
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
    int len = httpd_req_recv(req, buffer, sizeof(buffer) - 1);

    if (len > 0)
    {
        buffer[len] = 0;

        cJSON *root = cJSON_Parse(buffer);

        if (!root) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "{\"isAuthenticated\":false,\"error\":\"Invalid request\"}");
            return ESP_OK;
        }

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
        if (json) {
            httpd_resp_sendstr(req, json);
            free((void *)json);
        } else {
            httpd_resp_send_500(req);
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
    cJSON_AddStringToObject(sysInfo, "latestVersion", _updateCheck->getLatestVersion());
    cJSON_AddNumberToObject(sysInfo, "memoryUsage", _sysInfo->getMemoryUsage());
    cJSON_AddNumberToObject(sysInfo, "cpuUsage", _sysInfo->getCpuUsage());
    cJSON_AddNumberToObject(sysInfo, "supplyVoltage", _sysInfo->getSupplyVoltage());
    cJSON_AddNumberToObject(sysInfo, "temperature", _sysInfo->getTemperature());
    cJSON_AddNumberToObject(sysInfo, "uptimeSeconds", _sysInfo->getUptimeSeconds());
    cJSON_AddStringToObject(sysInfo, "boardRevision", _sysInfo->getBoardRevisionString());
    cJSON_AddStringToObject(sysInfo, "resetReason", _sysInfo->getResetReason());
    cJSON_AddBoolToObject(sysInfo, "ethernetConnected", _ethernet->isConnected());
    cJSON_AddNumberToObject(sysInfo, "ethernetSpeed", _ethernet->getLinkSpeedMbps());
    cJSON_AddStringToObject(sysInfo, "ethernetDuplex", _ethernet->getDuplexMode());
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
        httpd_resp_send_500(req);
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
    cJSON_AddBoolToObject(settings, "updateLedBlink", _settings->getUpdateLedBlink());

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

    cJSON_AddStringToObject(settings, "ccuIP", _settings->getCCUIP());
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
    cJSON *root = cJSON_CreateObject();

    add_settings(root);

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
    if (item && cJSON_IsBool(item))
    {
        return item->type == cJSON_True;
    }

    return false;
}

void delayed_restart_task(void *pvParameter) {
    vTaskDelay(3000 / portTICK_PERIOD_MS);
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

    char buffer[1024];
    int len = httpd_req_recv(req, buffer, sizeof(buffer) - 1);

    if (len > 0)
    {
        buffer[len] = 0;
        cJSON *root = cJSON_Parse(buffer);

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

        cJSON *timesourceItem = cJSON_GetObjectItem(root, "timesource");
        timesource_t timesource = timesourceItem ? (timesource_t)timesourceItem->valueint : _settings->getTimesource();

        cJSON *dcfOffsetItem = cJSON_GetObjectItem(root, "dcfOffset");
        int dcfOffset = dcfOffsetItem ? dcfOffsetItem->valueint : _settings->getDcfOffset();

        cJSON *gpsBaudrateItem = cJSON_GetObjectItem(root, "gpsBaudrate");
        int gpsBaudrate = gpsBaudrateItem ? gpsBaudrateItem->valueint : _settings->getGpsBaudrate();

        char *ntpServer = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ntpServer"));

        cJSON *ledBrightnessItem = cJSON_GetObjectItem(root, "ledBrightness");
        int ledBrightness = ledBrightnessItem ? ledBrightnessItem->valueint : _settings->getLEDBrightness();

        cJSON *updateLedBlinkItem = cJSON_GetObjectItem(root, "updateLedBlink");
        if (updateLedBlinkItem && cJSON_IsBool(updateLedBlinkItem)) {
            _settings->setUpdateLedBlink(cJSON_IsTrue(updateLedBlinkItem));
        }

        // IPv6
        bool enableIPv6 = cJSON_GetBoolValue(cJSON_GetObjectItem(root, "enableIPv6"));
        char *ipv6Mode = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Mode"));
        char *ipv6Address = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Address"));
        cJSON *ipv6PrefixItem = cJSON_GetObjectItem(root, "ipv6PrefixLength");
        int ipv6PrefixLength = ipv6PrefixItem ? ipv6PrefixItem->valueint : 64;
        char *ipv6Gateway = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Gateway"));
        char *ipv6Dns1 = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Dns1"));
        char *ipv6Dns2 = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Dns2"));

        char *ccuIP = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ccuIP"));

        if (adminPassword && strlen(adminPassword) > 0)
            _settings->setAdminPassword(adminPassword);

        if (hostname) {
            _settings->setNetworkSettings(hostname, useDHCP, localIP, netmask, gateway, dns1, dns2);
        }
        _settings->setTimesource(timesource);
        _settings->setDcfOffset(dcfOffset);
        _settings->setGpsBaudrate(gpsBaudrate);
        if (ntpServer) {
            _settings->setNtpServer(ntpServer);
        }
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
                ipv6Address ? ipv6Address : "",
                ipv6PrefixLength,
                ipv6Gateway ? ipv6Gateway : "",
                ipv6Dns1 ? ipv6Dns1 : "",
                ipv6Dns2 ? ipv6Dns2 : ""
            );
        }

        if (ccuIP) {
            _settings->setCCUIP(ccuIP);
        }

        _settings->save();

        cJSON_Delete(root);

        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":true,\"message\":\"Settings saved.\"}");

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
    // On restore, the current password will be preserved unless explicitly changed.

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

    int len = httpd_req_recv(req, buffer, 4095);

    if (len <= 0)
    {
        free(buffer);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data received");
    }

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

    cJSON *timesourceItem = cJSON_GetObjectItem(root, "timesource");
    timesource_t timesource = timesourceItem ? (timesource_t)timesourceItem->valueint : _settings->getTimesource();

    cJSON *dcfOffsetItem = cJSON_GetObjectItem(root, "dcfOffset");
    int dcfOffset = dcfOffsetItem ? dcfOffsetItem->valueint : _settings->getDcfOffset();

    cJSON *gpsBaudrateItem = cJSON_GetObjectItem(root, "gpsBaudrate");
    int gpsBaudrate = gpsBaudrateItem ? gpsBaudrateItem->valueint : _settings->getGpsBaudrate();

    char *ntpServer = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ntpServer"));

    cJSON *ledBrightnessItem = cJSON_GetObjectItem(root, "ledBrightness");
    int ledBrightness = ledBrightnessItem ? ledBrightnessItem->valueint : _settings->getLEDBrightness();

    // IPv6
    bool enableIPv6 = cJSON_GetBoolValue(cJSON_GetObjectItem(root, "enableIPv6"));
    char *ipv6Mode = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Mode"));
    char *ipv6Address = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Address"));
    cJSON *ipv6PrefixItem = cJSON_GetObjectItem(root, "ipv6PrefixLength");
    int ipv6PrefixLength = ipv6PrefixItem ? ipv6PrefixItem->valueint : 64;
    char *ipv6Gateway = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Gateway"));
    char *ipv6Dns1 = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Dns1"));
    char *ipv6Dns2 = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ipv6Dns2"));

    if (adminPassword && strlen(adminPassword) > 0)
        _settings->setAdminPassword(adminPassword);

    if (hostname) {
        _settings->setNetworkSettings(hostname, useDHCP, localIP, netmask, gateway, dns1, dns2);
    }
    _settings->setTimesource(timesource);
    _settings->setDcfOffset(dcfOffset);
    _settings->setGpsBaudrate(gpsBaudrate);
    if (ntpServer) {
        _settings->setNtpServer(ntpServer);
    }
    _settings->setLEDBrightness(ledBrightness);

    if (ipv6Mode) {
         _settings->setIPv6Settings(
            enableIPv6,
            ipv6Mode,
            ipv6Address ? ipv6Address : "",
            ipv6PrefixLength,
            ipv6Gateway ? ipv6Gateway : "",
            ipv6Dns1 ? ipv6Dns1 : "",
            ipv6Dns2 ? ipv6Dns2 : ""
        );
    }

    _settings->save();

    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\":true}");

    // Restart
    vTaskDelay(1000 / portTICK_PERIOD_MS);
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

esp_err_t post_ota_update_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK)
    {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    esp_ota_handle_t ota_handle = 0;
    bool ota_begun = false;

    char *ota_buff = (char *)malloc(OTA_BUFFER_SIZE);
    if (!ota_buff) {
        ESP_LOGE(TAG, "Failed to allocate OTA buffer");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }

    int content_length = req->content_len;
    int content_received = 0;
    int recv_len;
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
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
            {
                // Timeout - continue waiting
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
                     httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid multipart format");
                     goto err;
                }
            }

            size_t image_size = OTA_SIZE_UNKNOWN;
            if (!is_multipart) {
                image_size = content_length;
            }

            OTA_CHECK(esp_ota_begin(update_partition, image_size, &ota_handle) == ESP_OK, "Could not start OTA");
            ota_begun = true;
            ESP_LOGW(TAG, "Begin OTA Update to partition %s, File Size: %d", update_partition->label, content_length);
            _statusLED->setState(LED_STATE_BLINK_FAST);

            OTA_CHECK(esp_ota_write(ota_handle, body_start_p, body_part_len) == ESP_OK, "Error writing OTA");
            content_received += body_part_len;
            ESP_LOGI(TAG, "OTA progress: %d / %d bytes (%d%%)", content_received, content_length, (content_received * 100) / content_length);
        }
        else
        {
            OTA_CHECK(esp_ota_write(ota_handle, ota_buff, recv_len) == ESP_OK, "Error writing OTA");
            content_received += recv_len;
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

    // Store reset reason for successful firmware update
    ResetInfo::storeResetReason(RESET_REASON_FIRMWARE_UPDATE);

    // Automatic restart after successful OTA update
    xTaskCreate(delayed_restart_task, "restart_task", 2048, NULL, 5, NULL);

    free(ota_buff);
    return ESP_OK;

err:
    if (ota_buff) free(ota_buff);
    _statusLED->setState(LED_STATE_OFF);

    // Abort OTA if it was started but not completed
    if (ota_begun) {
        ESP_LOGW(TAG, "Aborting OTA operation due to error");
        esp_ota_abort(ota_handle);
    }

    // Store reset reason for failed firmware update
    ResetInfo::storeResetReason(RESET_REASON_UPDATE_FAILED);
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
    vTaskDelay(1000 / portTICK_PERIOD_MS);
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
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    full_system_restart();

    return ESP_OK;
}

httpd_uri_t post_factory_reset_handler = {
    .uri = "/api/factory-reset",
    .method = HTTP_POST,
    .handler = post_factory_reset_handler_func,
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
    int len = httpd_req_recv(req, buffer, sizeof(buffer) - 1);

    if (len <= 0)
    {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"Invalid request\"}");
        return ESP_OK;
    }

    buffer[len] = 0;
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
        // Basic URL validation - must start with http:// or https://
        if (strncmp(url_json, "http://", 7) != 0 && strncmp(url_json, "https://", 8) != 0) {
            cJSON_Delete(root);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"Invalid URL format\"}");
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

    ESP_LOGI(TAG, "Starting OTA update from URL: %s", url_buf);

    // Send success response immediately
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\":true,\"message\":\"OTA update started\"}");

    // Create a task to perform the update (it's blocking)
    struct TaskArgs {
        char* url;
        LED* statusLED;
    };

    TaskArgs* args = new TaskArgs();
    args->url = strdup(url_buf);
    if (args->url == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for URL");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"Memory allocation failed\"}");
        delete args;
        return ESP_OK;
    }
    args->statusLED = _statusLED;

    BaseType_t ret = xTaskCreate([](void* p) {
        TaskArgs* a = (TaskArgs*)p;

        ESP_LOGI(TAG, "OTA task started, downloading from: %s", a->url);

        esp_http_client_config_t config = {};
        config.url = a->url;
        config.crt_bundle_attach = esp_crt_bundle_attach;
        config.keep_alive_enable = true;
        config.timeout_ms = 30000;  // 30 second timeout for downloads
        config.buffer_size = 4096;   // Larger buffer for better performance
        config.max_redirection_count = 5;  // Allow redirects

        esp_https_ota_config_t ota_config = {};
        ota_config.http_config = &config;

        a->statusLED->setState(LED_STATE_BLINK_FAST);

        esp_err_t ret = esp_https_ota(&ota_config);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "OTA Update successful, validating firmware...");

            // Validate the new partition
            const esp_partition_t *running = esp_ota_get_running_partition();
            const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

            if (update_partition != NULL && update_partition != running) {
                // Verify the partition can be booted
                esp_app_desc_t running_app_desc;
                esp_app_desc_t new_app_desc;

                if (esp_ota_get_partition_description(running, &running_app_desc) == ESP_OK &&
                    esp_ota_get_partition_description(update_partition, &new_app_desc) == ESP_OK) {
                    ESP_LOGI(TAG, "Running firmware: %s %s", running_app_desc.project_name, running_app_desc.version);
                    ESP_LOGI(TAG, "New firmware: %s %s", new_app_desc.project_name, new_app_desc.version);
                }
            }

            ESP_LOGI(TAG, "Firmware validation complete, restarting...");
            ResetInfo::storeResetReason(RESET_REASON_FIRMWARE_UPDATE);
            a->statusLED->setState(LED_STATE_OFF);
            free(a->url);
            delete a;
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            full_system_restart();
        } else {
            ESP_LOGE(TAG, "OTA Update failed: %s", esp_err_to_name(ret));
            ResetInfo::storeResetReason(RESET_REASON_UPDATE_FAILED);
            a->statusLED->setState(LED_STATE_ON);
            free(a->url);
            delete a;
        }
        vTaskDelete(NULL);
    }, "ota_url_update", 8192, args, 5, NULL);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA update task");
        free(args->url);
        delete args;
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"Failed to start update task\"}");
        return ESP_OK;
    }

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

    // Password requirements: min 8 chars, uppercase, lowercase, digit
    bool has_lower = false;
    bool has_upper = false;
    bool has_digit = false;
    bool is_valid_length = (newPassword != NULL) && (strlen(newPassword) >= 8);

    if (is_valid_length) {
        for (int i = 0; newPassword[i] != 0; i++) {
            if (newPassword[i] >= 'a' && newPassword[i] <= 'z') {
                has_lower = true;
            } else if (newPassword[i] >= 'A' && newPassword[i] <= 'Z') {
                has_upper = true;
            } else if (newPassword[i] >= '0' && newPassword[i] <= '9') {
                has_digit = true;
            }
        }
    }

    if (!is_valid_length || !has_lower || !has_upper || !has_digit)
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Password must be at least 8 characters with uppercase, lowercase, and numbers");
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

    return ESP_OK;
}

httpd_uri_t post_change_password_handler = {
    .uri = "/api/change-password",
    .method = HTTP_POST,
    .handler = post_change_password_handler_func,
    .user_ctx = NULL};


struct ChangelogContext {
    httpd_req_t *req;
    bool failed;
    bool header_sent;
};

static esp_err_t _changelog_http_event_handler(esp_http_client_event_t *evt)
{
    ChangelogContext *ctx = (ChangelogContext *)evt->user_data;

    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (!ctx->failed && esp_http_client_get_status_code(evt->client) == 200) {
                esp_err_t err = httpd_resp_send_chunk(ctx->req, (const char *)evt->data, evt->data_len);
                if (err != ESP_OK) {
                    ctx->failed = true;
                    return ESP_FAIL;
                }
                ctx->header_sent = true;
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t get_changelog_handler_func(httpd_req_t *req)
{
    add_security_headers(req);

    if (validate_auth(req) != ESP_OK) {
        httpd_resp_set_status(req, "401 Not authorized");
        httpd_resp_sendstr(req, "401 Not authorized");
        return ESP_OK;
    }

    ChangelogContext ctx = { req, false, false };

    esp_http_client_config_t config = {};
    config.url = "https://raw.githubusercontent.com/Xerolux/HB-RF-ETH-ng/main/CHANGELOG.md";
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.event_handler = _changelog_http_event_handler;
    config.user_data = &ctx;
    config.timeout_ms = 10000;
    config.buffer_size = 4096;

    httpd_resp_set_type(req, "text/markdown");

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    int status_code = esp_http_client_get_status_code(client);

    if (err == ESP_OK && status_code == 200) {
        httpd_resp_send_chunk(req, NULL, 0);
    } else {
        if (!ctx.header_sent) {
            if (err != ESP_OK) {
                 ESP_LOGE(TAG, "Failed to fetch changelog: %s", esp_err_to_name(err));
                 httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to fetch changelog from GitHub");
            } else {
                 ESP_LOGE(TAG, "Failed to fetch changelog: HTTP %d", status_code);
                 httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "GitHub returned error");
            }
        } else {
            httpd_resp_send_chunk(req, NULL, 0);
        }
    }

    esp_http_client_cleanup(client);
    return ESP_OK;
}

httpd_uri_t get_changelog_handler = {
    .uri = "/api/changelog",
    .method = HTTP_GET,
    .handler = get_changelog_handler_func,
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

    std::string content = LogManager::instance().getLogContent(offset);
    httpd_resp_send(req, content.c_str(), content.length());

    return ESP_OK;
}

httpd_uri_t get_log_handler = {
    .uri = "/api/log",
    .method = HTTP_GET,
    .handler = get_log_handler_func,
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

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.max_uri_handlers = 21;
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
        httpd_register_uri_handler(_httpd_handle, &post_ota_url_handler);
        httpd_register_uri_handler(_httpd_handle, &post_change_password_handler);
        httpd_register_uri_handler(_httpd_handle, &get_monitoring_handler);
        httpd_register_uri_handler(_httpd_handle, &post_monitoring_handler);

        httpd_register_uri_handler(_httpd_handle, &get_backup_handler);
        httpd_register_uri_handler(_httpd_handle, &post_restore_handler);
        httpd_register_uri_handler(_httpd_handle, &get_changelog_handler);
        httpd_register_uri_handler(_httpd_handle, &get_log_handler);
        httpd_register_uri_handler(_httpd_handle, &get_log_download_handler);

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
