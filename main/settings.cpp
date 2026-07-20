/*
 *  settings.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "settings.h"
#include "validation.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "esp_log.h"
#include <string.h>

Settings::Settings()
{
  _mutex = xSemaphoreCreateMutex();
  if (_mutex == NULL) {
    ESP_LOGE("Settings", "FAILED to create settings mutex - configuration will not be thread-safe");
  }
  load();
}

static const char *TAG = "Settings";
static const char *NVS_NAMESPACE = "HB-RF-ETH";
static const char *MONITORING_NVS_NAMESPACE = "monitoring";

static esp_err_t erase_nvs_namespace(const char *ns)
{
  nvs_handle_t handle = 0;
  esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGI(TAG, "NVS namespace '%s' not present during factory reset", ns);
    return ESP_OK;
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_open('%s') failed in clear(): %s", ns, esp_err_to_name(err));
    return err;
  }

  err = nvs_erase_all(handle);
  if (err == ESP_OK) {
    err = nvs_commit(handle);
  }
  nvs_close(handle);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to erase NVS namespace '%s': %s", ns, esp_err_to_name(err));
  } else {
    ESP_LOGI(TAG, "Erased NVS namespace '%s'", ns);
  }
  return err;
}

#define GET_IP_ADDR(handle, name, var, defaultValue)  \
  if (nvs_get_u32(handle, name, &var.addr) != ESP_OK) \
  {                                                   \
    var.addr = defaultValue;                          \
  }

#define GET_INT(handle, name, var, defaultValue) \
  if (nvs_get_i32(handle, name, &var) != ESP_OK) \
  {                                              \
    var = defaultValue;                          \
  }

#define GET_BOOL(handle, name, var, defaultValue)          \
  int8_t __##var##_temp;                                   \
  if (nvs_get_i8(handle, name, &__##var##_temp) != ESP_OK) \
  {                                                        \
    var = defaultValue;                                    \
  }                                                        \
  else                                                     \
  {                                                        \
    var = (__##var##_temp != 0);                           \
  }

#define SET_IP_ADDR(handle, name, var) ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_set_u32(handle, name, var.addr));
#define SET_INT(handle, name, var) ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_set_i32(handle, name, var));
#define SET_STR(handle, name, var) ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_set_str(handle, name, var));
#define SET_BOOL(handle, name, var) ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_set_i8(handle, name, var ? 1 : 0));

void Settings::load()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);

  uint32_t handle = 0;

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle));

  size_t adminUsernameLength = sizeof(_adminUsername);
  if (nvs_get_str(handle, "adminUsername", _adminUsername, &adminUsernameLength) != ESP_OK)
  {
    strncpy(_adminUsername, "admin", sizeof(_adminUsername) - 1);
    _adminUsername[sizeof(_adminUsername) - 1] = '\0';
    ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_set_str(handle, "adminUsername", _adminUsername));
    // Force one login after the username feature is introduced. Existing
    // passwords remain valid; only old browser tokens are invalidated.
    size_t tokenLength = 0;
    if (nvs_get_str(handle, "adminToken", NULL, &tokenLength) == ESP_OK)
    {
      ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_erase_key(handle, "adminToken"));
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_commit(handle));
  }
  else if (_adminUsername[0] == '\0')
  {
    strncpy(_adminUsername, "admin", sizeof(_adminUsername) - 1);
    _adminUsername[sizeof(_adminUsername) - 1] = '\0';
  }

  size_t adminPasswordLength = sizeof(_adminPassword);
  if (nvs_get_str(handle, "adminPassword", _adminPassword, &adminPasswordLength) != ESP_OK)
  {
    strncpy(_adminPassword, "admin", sizeof(_adminPassword) - 1);
    _passwordChanged = false;
  }
  else
  {
    // Check if password was changed (if it's not default "admin")
    _passwordChanged = (strcmp(_adminPassword, "admin") != 0);
  }

  // Also try to load explicit passwordChanged flag (overrides auto-detection)
  // We use a temporary variable to avoid self-assignment warning if key is missing
  {
      bool currentVal = _passwordChanged;
      GET_BOOL(handle, "passwordChanged", _passwordChanged, currentVal);
  }

  size_t hostnameLength = sizeof(_hostname);
  if (nvs_get_str(handle, "hostname", _hostname, &hostnameLength) != ESP_OK)
  {
    uint8_t baseMac[6];
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    snprintf(_hostname, sizeof(_hostname) - 1, "HB-RF-ETH-%02X%02X%02X", baseMac[3], baseMac[4], baseMac[5]);
  }
  else if (!validateHostname(_hostname))
  {
    ESP_LOGW(TAG, "Invalid hostname in NVS, restoring default");
    uint8_t baseMac[6];
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    snprintf(_hostname, sizeof(_hostname), "HB-RF-ETH-%02X%02X%02X", baseMac[3], baseMac[4], baseMac[5]);
  }

  GET_BOOL(handle, "useDHCP", _useDHCP, true);
  GET_IP_ADDR(handle, "localIP", _localIP, IPADDR_ANY);
  GET_IP_ADDR(handle, "netmask", _netmask, IPADDR_ANY);
  GET_IP_ADDR(handle, "gateway", _gateway, IPADDR_ANY);

  // Use Cloudflare DNS when no primary DNS was stored. Existing non-empty
  // user settings remain authoritative. Legacy 0.0.0.0 values are migrated
  // once so static IPv4 installations can resolve NTP and OTA hosts.
  ip4_addr_t defaultDns1;
  IP4_ADDR(&defaultDns1, 1, 1, 1, 1);
  GET_IP_ADDR(handle, "dns1", _dns1, defaultDns1.addr);
  GET_IP_ADDR(handle, "dns2", _dns2, IPADDR_ANY);
  if (_dns1.addr == IPADDR_ANY || _dns1.addr == IPADDR_NONE)
  {
    _dns1 = defaultDns1;
    ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_set_u32(handle, "dns1", _dns1.addr));
    ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_commit(handle));
  }

  GET_INT(handle, "timesource", _timesource, TIMESOURCE_NTP);
  if (_timesource < TIMESOURCE_NTP || _timesource > TIMESOURCE_GPS)
  {
    ESP_LOGW(TAG, "Invalid time source in NVS, restoring NTP");
    _timesource = TIMESOURCE_NTP;
  }
  
  GET_INT(handle, "dcfOffset", _dcfOffset, 40000);
  if (!validateDcfOffset(_dcfOffset))
  {
    _dcfOffset = 40000;
  }

  GET_INT(handle, "gpsBaudrate", _gpsBaudrate, 9600);
  if (!validateGpsBaudrate(_gpsBaudrate))
  {
    _gpsBaudrate = 9600;
  }

  size_t ntpServerLength = sizeof(_ntpServer);
  if (nvs_get_str(handle, "ntpServer", _ntpServer, &ntpServerLength) != ESP_OK)
  {
    strncpy(_ntpServer, "pool.ntp.org", sizeof(_ntpServer) - 1);
  }
  else if (!validateNtpServer(_ntpServer))
  {
    ESP_LOGW(TAG, "Invalid NTP server in NVS, restoring default");
    strncpy(_ntpServer, "pool.ntp.org", sizeof(_ntpServer) - 1);
  }

  // Initialize default LED programs
  int32_t defaults[7] = {
      1,  // IDLE: ON
      5,  // CCU_DISCONNECTED: BLINK_SLOW
      6,  // CCU_CONNECTED: BLINK_2X
      4,  // UPDATE_AVAILABLE: BLINK_FAST
      10, // ERROR: STROBE
      4,  // BOOTING: BLINK_FAST
      5   // UPDATE_IN_PROGRESS: BLINK_SLOW
  };

  // Check for legacy updateLedBlink setting to migrate preference
  bool updateLedBlink;
  GET_BOOL(handle, "updateLedBlink", updateLedBlink, true);
  if (!updateLedBlink) {
      defaults[3] = 0; // Set UPDATE_AVAILABLE to OFF if legacy setting was false
  }

  // Load LED programs
  char key[16];
  for (int i = 0; i < 7; i++) {
      snprintf(key, sizeof(key), "ledProg%d", i);
      GET_INT(handle, key, _ledPrograms[i], defaults[i]);
      if (_ledPrograms[i] < 0 || _ledPrograms[i] > 10)
      {
        ESP_LOGW(TAG, "Invalid LED program %d in NVS, restoring default", i);
        _ledPrograms[i] = defaults[i];
      }
  }

  GET_INT(handle, "ledBrightness", _ledBrightness, 100);
  if (!validateLEDBrightness(_ledBrightness))
  {
    _ledBrightness = 100;
  }

  // Load IPv6 settings
  GET_BOOL(handle, "enableIPv6", _enableIPv6, false);

  size_t len;
  len = sizeof(_ipv6Mode);
  if (nvs_get_str(handle, "ipv6Mode", _ipv6Mode, &len) != ESP_OK) strncpy(_ipv6Mode, "auto", sizeof(_ipv6Mode) - 1);

  len = sizeof(_ipv6Address);
  if (nvs_get_str(handle, "ipv6Address", _ipv6Address, &len) != ESP_OK) _ipv6Address[0] = 0;

  GET_INT(handle, "ipv6Prefix", _ipv6PrefixLength, 64);

  len = sizeof(_ipv6Gateway);
  if (nvs_get_str(handle, "ipv6Gateway", _ipv6Gateway, &len) != ESP_OK) _ipv6Gateway[0] = 0;

  len = sizeof(_ipv6Dns1);
  if (nvs_get_str(handle, "ipv6Dns1", _ipv6Dns1, &len) != ESP_OK) _ipv6Dns1[0] = 0;

  len = sizeof(_ipv6Dns2);
  if (nvs_get_str(handle, "ipv6Dns2", _ipv6Dns2, &len) != ESP_OK) _ipv6Dns2[0] = 0;

  len = sizeof(_ccuIP);
  if (nvs_get_str(handle, "ccuIP", _ccuIP, &len) != ESP_OK) _ccuIP[0] = 0;

  GET_BOOL(handle, "betaChannel", _betaChannel, false);
  // NVS key max length is 15; do not rename to "systemLogEnabled" (16) — it
  // silently fails with ESP_ERR_NVS_KEY_TOO_LONG and the toggle won't persist.
  GET_BOOL(handle, "sysLogEnabled", _systemLogEnabled, false);
  GET_BOOL(handle, "flashPause", _flashPause, false);
  GET_BOOL(handle, "testDesign", _testDesignEnabled, false);

  // Supporter key (optional, cosmetic). Stored verbatim — validation happens
  // on read so an expired or malformed key is harmless.
  {
    size_t skLen = sizeof(_supporterKey);
    if (nvs_get_str(handle, "supporterKey", _supporterKey, &skLen) != ESP_OK)
      _supporterKey[0] = 0;
  }

  nvs_close(handle);

  if (_mutex) xSemaphoreGive(_mutex);
}

void Settings::save()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);

  uint32_t handle;

  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "nvs_open failed in save(): %s - settings not persisted", esp_err_to_name(err));
    if (_mutex) xSemaphoreGive(_mutex);
    return;
  }

  SET_STR(handle, "adminPassword", _adminPassword);
  SET_STR(handle, "adminUsername", _adminUsername);
  SET_BOOL(handle, "passwordChanged", _passwordChanged);

  SET_STR(handle, "hostname", _hostname);
  SET_BOOL(handle, "useDHCP", _useDHCP);
  SET_IP_ADDR(handle, "localIP", _localIP);
  SET_IP_ADDR(handle, "netmask", _netmask);
  SET_IP_ADDR(handle, "gateway", _gateway);
  SET_IP_ADDR(handle, "dns1", _dns1);
  SET_IP_ADDR(handle, "dns2", _dns2);

  SET_INT(handle, "timesource", _timesource);

  SET_INT(handle, "dcfOffset", _dcfOffset);

  SET_INT(handle, "gpsBaudrate", _gpsBaudrate);

  SET_STR(handle, "ntpServer", _ntpServer);

  // Save LED programs
  char key[16];
  for (int i = 0; i < 7; i++) {
      snprintf(key, sizeof(key), "ledProg%d", i);
      SET_INT(handle, key, _ledPrograms[i]);
  }

  SET_INT(handle, "ledBrightness", _ledBrightness);

  // Save IPv6 settings
  SET_BOOL(handle, "enableIPv6", _enableIPv6);
  SET_STR(handle, "ipv6Mode", _ipv6Mode);
  SET_STR(handle, "ipv6Address", _ipv6Address);
  SET_INT(handle, "ipv6Prefix", _ipv6PrefixLength);
  SET_STR(handle, "ipv6Gateway", _ipv6Gateway);
  SET_STR(handle, "ipv6Dns1", _ipv6Dns1);
  SET_STR(handle, "ipv6Dns2", _ipv6Dns2);

  SET_STR(handle, "ccuIP", _ccuIP);

  SET_BOOL(handle, "betaChannel", _betaChannel);
  SET_BOOL(handle, "sysLogEnabled", _systemLogEnabled);
  SET_BOOL(handle, "flashPause", _flashPause);
  SET_BOOL(handle, "testDesign", _testDesignEnabled);

  SET_STR(handle, "supporterKey", _supporterKey);

  ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_commit(handle));
  nvs_close(handle);

  if (_mutex) xSemaphoreGive(_mutex);
}

void Settings::clear()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);

  // Factory reset must wipe all user-configurable namespaces, not only the
  // base settings namespace. Monitoring keeps MQTT/CheckMK/Prometheus/Syslog
  // and notification settings in its own namespace; leaving it intact made
  // those integrations come back after a "factory" reset.
  esp_err_t settings_err = erase_nvs_namespace(NVS_NAMESPACE);
  esp_err_t monitoring_err = erase_nvs_namespace(MONITORING_NVS_NAMESPACE);

  if (_mutex) xSemaphoreGive(_mutex);

  if (settings_err != ESP_OK || monitoring_err != ESP_OK) {
    ESP_LOGW(TAG, "Factory reset completed with NVS erase errors (settings=%s, monitoring=%s)",
             esp_err_to_name(settings_err), esp_err_to_name(monitoring_err));
  }

  // Reload defaults into RAM after releasing the mutex; load() takes it again.
  load();
}

char *Settings::getAdminPassword()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  char *result = _adminPassword;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

char *Settings::getAdminUsername()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  char *result = _adminUsername;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

bool Settings::setAdminPassword(const char *adminPassword)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  if (adminPassword == nullptr || adminPassword[0] == '\0' || strlen(adminPassword) >= sizeof(_adminPassword) - 1)
  {
    ESP_LOGE(TAG, "Invalid admin password length, keeping current password");
    if (_mutex) xSemaphoreGive(_mutex);
    return false;
  }

  snprintf(_adminPassword, sizeof(_adminPassword), "%s", adminPassword);
  // Mark password as changed when it's explicitly set
  _passwordChanged = true;
  if (_mutex) xSemaphoreGive(_mutex);
  return true;
}

bool Settings::setAdminUsername(const char *adminUsername)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  if (adminUsername == nullptr || adminUsername[0] == '\0' || strlen(adminUsername) >= sizeof(_adminUsername) - 1)
  {
    ESP_LOGE(TAG, "Invalid admin username length, keeping current username");
    if (_mutex) xSemaphoreGive(_mutex);
    return false;
  }

  const size_t len = strlen(adminUsername);
  for (size_t i = 0; i < len; i++)
  {
    const char c = adminUsername[i];
    const bool valid = (c >= 'A' && c <= 'Z') ||
                       (c >= 'a' && c <= 'z') ||
                       (c >= '0' && c <= '9') ||
                       c == '-' || c == '_' || c == '.';
    if (!valid)
    {
      ESP_LOGE(TAG, "Invalid character in admin username, keeping current username");
      if (_mutex) xSemaphoreGive(_mutex);
      return false;
    }
  }

  snprintf(_adminUsername, sizeof(_adminUsername), "%s", adminUsername);
  if (_mutex) xSemaphoreGive(_mutex);
  return true;
}

bool Settings::getPasswordChanged()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  bool result = _passwordChanged;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

char *Settings::getHostname()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  char *result = _hostname;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

bool Settings::getUseDHCP()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  bool result = _useDHCP;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

ip4_addr_t Settings::getLocalIP()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  ip4_addr_t result = _localIP;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

ip4_addr_t Settings::getNetmask()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  ip4_addr_t result = _netmask;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

ip4_addr_t Settings::getGateway()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  ip4_addr_t result = _gateway;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

ip4_addr_t Settings::getDns1()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  ip4_addr_t result = _dns1;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

ip4_addr_t Settings::getDns2()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  ip4_addr_t result = _dns2;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

bool Settings::setNetworkSettings(const char *hostname, bool useDHCP, ip4_addr_t localIP, ip4_addr_t netmask, ip4_addr_t gateway, ip4_addr_t dns1, ip4_addr_t dns2)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  // Validate hostname
  if (!validateHostname(hostname))
  {
    ESP_LOGE(TAG, "Invalid hostname provided, keeping current value");
    if (_mutex) xSemaphoreGive(_mutex);
    return false;
  }

  // Validate IP addresses if not using DHCP
  if (!useDHCP)
  {
    if (!validateIPAddress(localIP))
    {
      ESP_LOGE(TAG, "Invalid local IP address, keeping current settings");
      if (_mutex) xSemaphoreGive(_mutex);
      return false;
    }
    if (!validateNetmask(netmask))
    {
      ESP_LOGE(TAG, "Invalid netmask, keeping current settings");
      if (_mutex) xSemaphoreGive(_mutex);
      return false;
    }
    if (!validateIPAddress(gateway))
    {
      ESP_LOGE(TAG, "Invalid gateway, keeping current settings");
      if (_mutex) xSemaphoreGive(_mutex);
      return false;
    }
  }

  // Validate DNS servers (optional, can be 0.0.0.0)
  if (dns1.addr != IPADDR_ANY && !validateIPAddress(dns1))
  {
    ESP_LOGE(TAG, "Invalid DNS1 address, keeping current settings");
    if (_mutex) xSemaphoreGive(_mutex);
    return false;
  }
  if (dns2.addr != IPADDR_ANY && !validateIPAddress(dns2))
  {
    ESP_LOGE(TAG, "Invalid DNS2 address, keeping current settings");
    if (_mutex) xSemaphoreGive(_mutex);
    return false;
  }

  // All validations passed, update settings
  strncpy(_hostname, hostname, sizeof(_hostname) - 1);
  _useDHCP = useDHCP;
  _localIP = localIP;
  _netmask = netmask;
  _gateway = gateway;
  _dns1 = dns1;
  _dns2 = dns2;

  if (_mutex) xSemaphoreGive(_mutex);
  return true;
}

int Settings::getDcfOffset()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  int result = _dcfOffset;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

void Settings::setDcfOffset(int dcfOffset)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  if (!validateDcfOffset(dcfOffset))
  {
    ESP_LOGE(TAG, "Invalid DCF offset, keeping current value");
    if (_mutex) xSemaphoreGive(_mutex);
    return;
  }
  _dcfOffset = dcfOffset;
  if (_mutex) xSemaphoreGive(_mutex);
}

int Settings::getGpsBaudrate()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  int result = _gpsBaudrate;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

void Settings::setGpsBaudrate(int gpsBaudrate)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  if (!validateGpsBaudrate(gpsBaudrate))
  {
    ESP_LOGE(TAG, "Invalid GPS baudrate, keeping current value");
    if (_mutex) xSemaphoreGive(_mutex);
    return;
  }
  _gpsBaudrate = gpsBaudrate;
  if (_mutex) xSemaphoreGive(_mutex);
}

timesource_t Settings::getTimesource()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  timesource_t result = (timesource_t)_timesource;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

void Settings::setTimesource(timesource_t timesource)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  if (timesource < TIMESOURCE_NTP || timesource > TIMESOURCE_GPS)
  {
    ESP_LOGE(TAG, "Invalid time source, keeping current value");
    if (_mutex) xSemaphoreGive(_mutex);
    return;
  }
  _timesource = timesource;
  if (_mutex) xSemaphoreGive(_mutex);
}

char *Settings::getNtpServer()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  char *result = _ntpServer;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

void Settings::setNtpServer(const char *ntpServer)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  if (!validateNtpServer(ntpServer))
  {
    ESP_LOGE(TAG, "Invalid NTP server, keeping current value");
    if (_mutex) xSemaphoreGive(_mutex);
    return;
  }
  strncpy(_ntpServer, ntpServer, sizeof(_ntpServer) - 1);
  if (_mutex) xSemaphoreGive(_mutex);
}

int Settings::getLEDBrightness()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  int result = _ledBrightness;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

void Settings::setLEDBrightness(int ledBrightness)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  if (!validateLEDBrightness(ledBrightness))
  {
    ESP_LOGE(TAG, "Invalid LED brightness, keeping current value");
    if (_mutex) xSemaphoreGive(_mutex);
    return;
  }
  _ledBrightness = ledBrightness;
  if (_mutex) xSemaphoreGive(_mutex);
}

int Settings::getLedProgram(int program)
{
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    if (program < 0 || program >= 7) {
      if (_mutex) xSemaphoreGive(_mutex);
      return 0;
    }
    int result = _ledPrograms[program];
    if (_mutex) xSemaphoreGive(_mutex);
    return result;
}

void Settings::setLedProgram(int program, int state)
{
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    if (program >= 0 && program < 7) {
        // Validate state (0-10)
        if (state >= 0 && state <= 10) {
            _ledPrograms[program] = state;
        }
    }
    if (_mutex) xSemaphoreGive(_mutex);
}

// IPv6 Getters
bool Settings::getEnableIPv6() {
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  bool result = _enableIPv6;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}
char *Settings::getIPv6Mode() {
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  char *result = _ipv6Mode;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}
char *Settings::getIPv6Address() {
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  char *result = _ipv6Address;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}
int Settings::getIPv6PrefixLength() {
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  int result = _ipv6PrefixLength;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}
char *Settings::getIPv6Gateway() {
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  char *result = _ipv6Gateway;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}
char *Settings::getIPv6Dns1() {
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  char *result = _ipv6Dns1;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}
char *Settings::getIPv6Dns2() {
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  char *result = _ipv6Dns2;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

// IPv6 Setter
void Settings::setIPv6Settings(bool enableIPv6, const char *ipv6Mode, const char *ipv6Address, int ipv6PrefixLength, const char *ipv6Gateway, const char *ipv6Dns1, const char *ipv6Dns2)
{
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    _enableIPv6 = enableIPv6;

    // Validate IPv6 mode
    if (ipv6Mode != NULL && (strcmp(ipv6Mode, "auto") == 0 || strcmp(ipv6Mode, "static") == 0 || strcmp(ipv6Mode, "disabled") == 0))
    {
        strncpy(_ipv6Mode, ipv6Mode, sizeof(_ipv6Mode) - 1);
    }
    else
    {
        ESP_LOGE(TAG, "Invalid IPv6 mode, defaulting to 'auto'");
        strncpy(_ipv6Mode, "auto", sizeof(_ipv6Mode) - 1);
    }

    // Validate IPv6 address if provided
    if (ipv6Address != NULL && strlen(ipv6Address) > 0)
    {
        if (!validateIPv6Address(ipv6Address))
        {
            ESP_LOGE(TAG, "Invalid IPv6 address, keeping current value");
            // Don't update _ipv6Address if invalid
        }
        else
        {
            strncpy(_ipv6Address, ipv6Address, sizeof(_ipv6Address) - 1);
        }
    }

    // Validate and clamp prefix length
    if (ipv6PrefixLength < 1) ipv6PrefixLength = 1;
    if (ipv6PrefixLength > 128) ipv6PrefixLength = 128;
    _ipv6PrefixLength = ipv6PrefixLength;

    // Validate IPv6 gateway if provided
    if (ipv6Gateway != NULL && strlen(ipv6Gateway) > 0)
    {
        if (!validateIPv6Address(ipv6Gateway))
        {
            ESP_LOGE(TAG, "Invalid IPv6 gateway, keeping current value");
        }
        else
        {
            strncpy(_ipv6Gateway, ipv6Gateway, sizeof(_ipv6Gateway) - 1);
        }
    }

    // Validate IPv6 DNS servers if provided
    if (ipv6Dns1 != NULL && strlen(ipv6Dns1) > 0)
    {
        if (!validateIPv6Address(ipv6Dns1))
        {
            ESP_LOGE(TAG, "Invalid IPv6 DNS1, keeping current value");
        }
        else
        {
            strncpy(_ipv6Dns1, ipv6Dns1, sizeof(_ipv6Dns1) - 1);
        }
    }

    if (ipv6Dns2 != NULL && strlen(ipv6Dns2) > 0)
    {
        if (!validateIPv6Address(ipv6Dns2))
        {
            ESP_LOGE(TAG, "Invalid IPv6 DNS2, keeping current value");
        }
        else
        {
            strncpy(_ipv6Dns2, ipv6Dns2, sizeof(_ipv6Dns2) - 1);
        }
    }
    if (_mutex) xSemaphoreGive(_mutex);
}

char *Settings::getCCUIP()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  char *result = _ccuIP;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

void Settings::setCCUIP(const char *ip)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  if (ip == NULL || ip[0] == '\0') {
    _ccuIP[0] = 0;
    if (_mutex) xSemaphoreGive(_mutex);
    return;
  }

  if (!validateCcuAddress(ip)) {
    ESP_LOGE(TAG, "Invalid CCU address, keeping current value");
    if (_mutex) xSemaphoreGive(_mutex);
    return;
  }

  strncpy(_ccuIP, ip, sizeof(_ccuIP) - 1);
  if (_mutex) xSemaphoreGive(_mutex);
}

bool Settings::getBetaChannel()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  bool result = _betaChannel;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

void Settings::setBetaChannel(bool enabled)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  _betaChannel = enabled;
  if (_mutex) xSemaphoreGive(_mutex);
}

bool Settings::getSystemLogEnabled()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  bool result = _systemLogEnabled;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

void Settings::setSystemLogEnabled(bool enabled)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  _systemLogEnabled = enabled;
  if (_mutex) xSemaphoreGive(_mutex);
}

bool Settings::getFlashPause()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  bool result = _flashPause;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

void Settings::setFlashPause(bool enabled)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  _flashPause = enabled;
  if (_mutex) xSemaphoreGive(_mutex);
}

bool Settings::getTestDesignEnabled()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  bool result = _testDesignEnabled;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

void Settings::setTestDesignEnabled(bool enabled)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  _testDesignEnabled = enabled;
  if (_mutex) xSemaphoreGive(_mutex);
}

char *Settings::getSupporterKey()
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  char *result = _supporterKey;
  if (_mutex) xSemaphoreGive(_mutex);
  return result;
}

void Settings::setSupporterKey(const char *key)
{
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
  if (!key || key[0] == '\0')
  {
    _supporterKey[0] = 0;
  }
  else
  {
    // Copy raw input; validation/normalisation is the caller's responsibility
    // (webui.cpp validates via supporter_key_validate before storing).
    snprintf(_supporterKey, sizeof(_supporterKey), "%s", key);
  }
  if (_mutex) xSemaphoreGive(_mutex);
}

// ---- Admin token persistence (survives reboots) ---------------------------

bool Settings::loadAdminToken(char *out, size_t size)
{
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    if (!out || size == 0) {
      if (_mutex) xSemaphoreGive(_mutex);
      return false;
    }
    out[0] = '\0';
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
      if (_mutex) xSemaphoreGive(_mutex);
      return false;
    }
    size_t len = size;
    esp_err_t err = nvs_get_str(handle, "adminToken", out, &len);
    nvs_close(handle);
    if (_mutex) xSemaphoreGive(_mutex);
    return (err == ESP_OK && len > 1);
}

void Settings::saveAdminToken(const char *token)
{
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    if (!token || token[0] == '\0') {
      if (_mutex) xSemaphoreGive(_mutex);
      return;
    }
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
      if (_mutex) xSemaphoreGive(_mutex);
      return;
    }
    nvs_set_str(handle, "adminToken", token);
    nvs_commit(handle);
    nvs_close(handle);
    if (_mutex) xSemaphoreGive(_mutex);
}

void Settings::clearAdminToken()
{
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
      if (_mutex) xSemaphoreGive(_mutex);
      return;
    }
    nvs_erase_key(handle, "adminToken");
    nvs_commit(handle);
    nvs_close(handle);
    if (_mutex) xSemaphoreGive(_mutex);
}
