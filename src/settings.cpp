/*
 *  settings.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "settings.h"
#include "validation.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "esp_log.h"
#include <string.h>

Settings::Settings()
{
  load();
}

static const char *TAG = "Settings";
static const char *NVS_NAMESPACE = "HB-RF-ETH";

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

#define GET_UINT16(handle, name, var, defaultValue) \
  if (nvs_get_u16(handle, name, &var) != ESP_OK) \
  {                                              \
    var = defaultValue;                          \
  }

#define SET_IP_ADDR(handle, name, var) ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_set_u32(handle, name, var.addr));
#define SET_INT(handle, name, var) ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_set_i32(handle, name, var));
#define SET_STR(handle, name, var) ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_set_str(handle, name, var));
#define SET_BOOL(handle, name, var) ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_set_i8(handle, name, var ? 1 : 0));
#define SET_UINT16(handle, name, var) ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_set_u16(handle, name, var));

void Settings::load()
{
  uint32_t handle;

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle));

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

  GET_BOOL(handle, "useDHCP", _useDHCP, true);
  GET_IP_ADDR(handle, "localIP", _localIP, IPADDR_ANY);
  GET_IP_ADDR(handle, "netmask", _netmask, IPADDR_ANY);
  GET_IP_ADDR(handle, "gateway", _gateway, IPADDR_ANY);
  GET_IP_ADDR(handle, "dns1", _dns1, IPADDR_ANY);
  GET_IP_ADDR(handle, "dns2", _dns2, IPADDR_ANY);

  GET_INT(handle, "timesource", _timesource, TIMESOURCE_NTP);
  
  GET_INT(handle, "dcfOffset", _dcfOffset, 40000);

  GET_INT(handle, "gpsBaudrate", _gpsBaudrate, 9600);

  size_t ntpServerLength = sizeof(_ntpServer);
  if (nvs_get_str(handle, "ntpServer", _ntpServer, &ntpServerLength) != ESP_OK)
  {
    strncpy(_ntpServer, "pool.ntp.org", sizeof(_ntpServer) - 1);
  }

  GET_BOOL(handle, "checkUpdates", _checkUpdates, true);
  GET_BOOL(handle, "allowPrerelease", _allowPrerelease, false);

  GET_INT(handle, "ledBrightness", _ledBrightness, 100);

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

  GET_BOOL(handle, "hmlgwEnabled", _hmlgwEnabled, false);
  GET_UINT16(handle, "hmlgwPort", _hmlgwPort, 2000);
  GET_UINT16(handle, "hmlgwKeepAlivePort", _hmlgwKeepAlivePort, 2001);

  GET_BOOL(handle, "analyzerEnabled", _analyzerEnabled, false);

  nvs_close(handle);
}

void Settings::save()
{
  uint32_t handle;

  ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle));

  SET_STR(handle, "adminPassword", _adminPassword);
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

  SET_BOOL(handle, "checkUpdates", _checkUpdates);
  SET_BOOL(handle, "allowPrerelease", _allowPrerelease);

  SET_INT(handle, "ledBrightness", _ledBrightness);

  // Save IPv6 settings
  SET_BOOL(handle, "enableIPv6", _enableIPv6);
  SET_STR(handle, "ipv6Mode", _ipv6Mode);
  SET_STR(handle, "ipv6Address", _ipv6Address);
  SET_INT(handle, "ipv6Prefix", _ipv6PrefixLength);
  SET_STR(handle, "ipv6Gateway", _ipv6Gateway);
  SET_STR(handle, "ipv6Dns1", _ipv6Dns1);
  SET_STR(handle, "ipv6Dns2", _ipv6Dns2);

  SET_BOOL(handle, "hmlgwEnabled", _hmlgwEnabled);
  SET_UINT16(handle, "hmlgwPort", _hmlgwPort);
  SET_UINT16(handle, "hmlgwKeepAlivePort", _hmlgwKeepAlivePort);

  SET_BOOL(handle, "analyzerEnabled", _analyzerEnabled);

  nvs_close(handle);
}

void Settings::clear()
{
  uint32_t handle;

  ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle));
  ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_erase_all(handle));
  ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_commit(handle));
  nvs_close(handle);

  load();
}

char *Settings::getAdminPassword()
{
  return _adminPassword;
}

void Settings::setAdminPassword(char *adminPassword)
{
  strncpy(_adminPassword, adminPassword, sizeof(_adminPassword) - 1);
  // Mark password as changed when it's explicitly set
  _passwordChanged = true;
}

bool Settings::getPasswordChanged()
{
  return _passwordChanged;
}

char *Settings::getHostname()
{
  return _hostname;
}

bool Settings::getUseDHCP()
{
  return _useDHCP;
}

ip4_addr_t Settings::getLocalIP()
{
  return _localIP;
}

ip4_addr_t Settings::getNetmask()
{
  return _netmask;
}

ip4_addr_t Settings::getGateway()
{
  return _gateway;
}

ip4_addr_t Settings::getDns1()
{
  return _dns1;
}

ip4_addr_t Settings::getDns2()
{
  return _dns2;
}

void Settings::setNetworkSettings(char *hostname, bool useDHCP, ip4_addr_t localIP, ip4_addr_t netmask, ip4_addr_t gateway, ip4_addr_t dns1, ip4_addr_t dns2)
{
  // Validate hostname
  if (!validateHostname(hostname))
  {
    ESP_LOGE(TAG, "Invalid hostname provided, keeping current value");
    return;
  }

  // Validate IP addresses if not using DHCP
  if (!useDHCP)
  {
    if (!validateIPAddress(localIP))
    {
      ESP_LOGE(TAG, "Invalid local IP address, keeping current settings");
      return;
    }
    if (!validateIPAddress(netmask))
    {
      ESP_LOGE(TAG, "Invalid netmask, keeping current settings");
      return;
    }
    if (!validateIPAddress(gateway))
    {
      ESP_LOGE(TAG, "Invalid gateway, keeping current settings");
      return;
    }
  }

  // Validate DNS servers (optional, can be 0.0.0.0)
  if (dns1.addr != IPADDR_ANY && !validateIPAddress(dns1))
  {
    ESP_LOGE(TAG, "Invalid DNS1 address, keeping current settings");
    return;
  }
  if (dns2.addr != IPADDR_ANY && !validateIPAddress(dns2))
  {
    ESP_LOGE(TAG, "Invalid DNS2 address, keeping current settings");
    return;
  }

  // All validations passed, update settings
  strncpy(_hostname, hostname, sizeof(_hostname) - 1);
  _useDHCP = useDHCP;
  _localIP = localIP;
  _netmask = netmask;
  _gateway = gateway;
  _dns1 = dns1;
  _dns2 = dns2;
}

int Settings::getDcfOffset()
{
  return _dcfOffset;
}

void Settings::setDcfOffset(int dcfOffset)
{
  if (!validateDcfOffset(dcfOffset))
  {
    ESP_LOGE(TAG, "Invalid DCF offset, keeping current value");
    return;
  }
  _dcfOffset = dcfOffset;
}

int Settings::getGpsBaudrate()
{
  return _gpsBaudrate;
}

void Settings::setGpsBaudrate(int gpsBaudrate)
{
  if (!validateGpsBaudrate(gpsBaudrate))
  {
    ESP_LOGE(TAG, "Invalid GPS baudrate, keeping current value");
    return;
  }
  _gpsBaudrate = gpsBaudrate;
}

timesource_t Settings::getTimesource()
{
  return (timesource_t)_timesource;
}

void Settings::setTimesource(timesource_t timesource)
{
  _timesource = timesource;
}

char *Settings::getNtpServer()
{
  return _ntpServer;
}

void Settings::setNtpServer(char *ntpServer)
{
  if (!validateNtpServer(ntpServer))
  {
    ESP_LOGE(TAG, "Invalid NTP server, keeping current value");
    return;
  }
  strncpy(_ntpServer, ntpServer, sizeof(_ntpServer) - 1);
}

int Settings::getLEDBrightness()
{
  return _ledBrightness;
}

void Settings::setLEDBrightness(int ledBrightness)
{
  if (!validateLEDBrightness(ledBrightness))
  {
    ESP_LOGE(TAG, "Invalid LED brightness, keeping current value");
    return;
  }
  _ledBrightness = ledBrightness;
}

bool Settings::getCheckUpdates()
{
  return _checkUpdates;
}

void Settings::setCheckUpdates(bool checkUpdates)
{
  _checkUpdates = checkUpdates;
}

bool Settings::getAllowPrerelease()
{
  return _allowPrerelease;
}

void Settings::setAllowPrerelease(bool allowPrerelease)
{
  _allowPrerelease = allowPrerelease;
}

// IPv6 Getters
bool Settings::getEnableIPv6() { return _enableIPv6; }
char *Settings::getIPv6Mode() { return _ipv6Mode; }
char *Settings::getIPv6Address() { return _ipv6Address; }
int Settings::getIPv6PrefixLength() { return _ipv6PrefixLength; }
char *Settings::getIPv6Gateway() { return _ipv6Gateway; }
char *Settings::getIPv6Dns1() { return _ipv6Dns1; }
char *Settings::getIPv6Dns2() { return _ipv6Dns2; }

// IPv6 Setter
void Settings::setIPv6Settings(bool enableIPv6, char *ipv6Mode, char *ipv6Address, int ipv6PrefixLength, char *ipv6Gateway, char *ipv6Dns1, char *ipv6Dns2)
{
    _enableIPv6 = enableIPv6;
    strncpy(_ipv6Mode, ipv6Mode, sizeof(_ipv6Mode) - 1);

    // In a real scenario, we should validate IPv6 addresses here.
    // Since we don't have a robust IPv6 validator in validation.cpp yet, we trust the WebUI regex or just store it.
    // Ideally use inet_pton(AF_INET6, ...) to validate.

    strncpy(_ipv6Address, ipv6Address, sizeof(_ipv6Address) - 1);

    if (ipv6PrefixLength < 1) ipv6PrefixLength = 1;
    if (ipv6PrefixLength > 128) ipv6PrefixLength = 128;
    _ipv6PrefixLength = ipv6PrefixLength;

    strncpy(_ipv6Gateway, ipv6Gateway, sizeof(_ipv6Gateway) - 1);
    strncpy(_ipv6Dns1, ipv6Dns1, sizeof(_ipv6Dns1) - 1);
    strncpy(_ipv6Dns2, ipv6Dns2, sizeof(_ipv6Dns2) - 1);
}

bool Settings::getHmlgwEnabled() { return _hmlgwEnabled; }
void Settings::setHmlgwEnabled(bool enabled) { _hmlgwEnabled = enabled; }

uint16_t Settings::getHmlgwPort() { return _hmlgwPort; }
void Settings::setHmlgwPort(uint16_t port) { _hmlgwPort = port; }

uint16_t Settings::getHmlgwKeepAlivePort() { return _hmlgwKeepAlivePort; }
void Settings::setHmlgwKeepAlivePort(uint16_t port) { _hmlgwKeepAlivePort = port; }

bool Settings::getAnalyzerEnabled() { return _analyzerEnabled; }
void Settings::setAnalyzerEnabled(bool enabled) { _analyzerEnabled = enabled; }
