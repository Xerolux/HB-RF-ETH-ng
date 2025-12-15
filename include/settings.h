/*
 *  settings.h is part of the HB-RF-ETH firmware v2.0
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

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <lwip/ip4_addr.h>

typedef enum
{
    TIMESOURCE_NTP = 0,
    TIMESOURCE_DCF = 1,
    TIMESOURCE_GPS = 2
} timesource_t;

typedef enum
{
    PROXY_MODE_STANDALONE = 0,
    PROXY_MODE_MASTER = 1,
    PROXY_MODE_SLAVE = 2
} proxy_mode_t;

class Settings
{
private:
  char _adminPassword[33] = {0};
  bool _passwordChanged;

  char _hostname[33] = {0};
  bool _useDHCP;
  ip4_addr_t _localIP;
  ip4_addr_t _netmask;
  ip4_addr_t _gateway;
  ip4_addr_t _dns1;
  ip4_addr_t _dns2;

  int32_t _timesource;

  int32_t _dcfOffset;

  int32_t _gpsBaudrate;

  char _ntpServer[65] = {0};

  int32_t _ledBrightness;
  bool _checkUpdates;
  bool _allowPrerelease;

  bool _experimentalFeaturesEnabled;
  int32_t _proxyMode;
  ip4_addr_t _masterIP;

  bool _enableIPv6;
  char _ipv6Mode[10] = {0};
  char _ipv6Address[40] = {0};
  int32_t _ipv6PrefixLength;
  char _ipv6Gateway[40] = {0};
  char _ipv6Dns1[40] = {0};
  char _ipv6Dns2[40] = {0};

public:
  Settings();
  void load();
  void save();
  void clear();

  char *getAdminPassword();
  void setAdminPassword(char* password);
  bool getPasswordChanged();

  char *getHostname();
  bool getUseDHCP();
  ip4_addr_t getLocalIP();
  ip4_addr_t getNetmask();
  ip4_addr_t getGateway();
  ip4_addr_t getDns1();
  ip4_addr_t getDns2();

  void setNetworkSettings(char *hostname, bool useDHCP, ip4_addr_t localIP, ip4_addr_t netmask, ip4_addr_t gateway, ip4_addr_t dns1, ip4_addr_t dns2);

  timesource_t getTimesource();
  void setTimesource(timesource_t timesource);

  int getDcfOffset();
  void setDcfOffset(int offset);

  int getGpsBaudrate();
  void setGpsBaudrate(int baudrate);

  char *getNtpServer();
  void setNtpServer(char *ntpServer);

  int getLEDBrightness();
  void setLEDBrightness(int brightness);

  bool getCheckUpdates();
  void setCheckUpdates(bool checkUpdates);

  bool getAllowPrerelease();
  void setAllowPrerelease(bool allowPrerelease);

  bool getExperimentalFeaturesEnabled();
  void setExperimentalFeaturesEnabled(bool enabled);

  proxy_mode_t getProxyMode();
  void setProxyMode(proxy_mode_t mode);

  ip4_addr_t getMasterIP();
  void setMasterIP(ip4_addr_t ip);

  // IPv6 getters
  bool getEnableIPv6();
  char *getIPv6Mode();
  char *getIPv6Address();
  int getIPv6PrefixLength();
  char *getIPv6Gateway();
  char *getIPv6Dns1();
  char *getIPv6Dns2();

  // IPv6 setter
  void setIPv6Settings(bool enableIPv6, char *ipv6Mode, char *ipv6Address, int ipv6PrefixLength, char *ipv6Gateway, char *ipv6Dns1, char *ipv6Dns2);
};