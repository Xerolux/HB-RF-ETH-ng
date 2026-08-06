/*
 *  settings.h is part of the HB-RF-ETH firmware v2.0
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

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <lwip/ip4_addr.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_err.h"

typedef enum
{
    TIMESOURCE_NTP = 0,
    TIMESOURCE_DCF = 1,
    TIMESOURCE_GPS = 2
} timesource_t;

// Complete in-RAM settings image used to roll back a multi-namespace restore
// if a later NVS commit fails. Authentication tokens are intentionally not
// included because restore does not modify them.
typedef struct {
  char adminPassword[33];
  char adminUsername[33];
  bool passwordChanged;
  char hostname[64];
  bool useDHCP;
  ip4_addr_t localIP;
  ip4_addr_t netmask;
  ip4_addr_t gateway;
  ip4_addr_t dns1;
  ip4_addr_t dns2;
  int32_t timesource;
  int32_t dcfOffset;
  int32_t gpsBaudrate;
  char ntpServer[65];
  int32_t ledBrightness;
  int32_t ledPrograms[7];
  bool enableIPv6;
  char ipv6Mode[10];
  char ipv6Address[40];
  int32_t ipv6PrefixLength;
  char ipv6Gateway[40];
  char ipv6Dns1[40];
  char ipv6Dns2[40];
  char ccuIP[64];
  bool systemLogEnabled;
  bool flashPause;
  bool testDesignEnabled;
} settings_snapshot_t;

class Settings
{
private:
  void resetToSafeDefaultsLocked();
  void lockAuthenticationAfterStorageFailureLocked();
  esp_err_t validateStorageCapacityLocked(uint32_t handle,
                                          size_t *requiredEntries = nullptr);

  char _adminPassword[33] = {0};
  char _adminUsername[33] = {0};
  bool _passwordChanged;
  // A failed load must not make a previously persisted bearer token active.
  // Published only after every authentication field was read successfully.
  bool _storageHealthy = false;

  char _hostname[64] = {0};
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
  int32_t _ledPrograms[7];

  bool _enableIPv6;
  char _ipv6Mode[10] = {0};
  char _ipv6Address[40] = {0};
  int32_t _ipv6PrefixLength;
  char _ipv6Gateway[40] = {0};
  char _ipv6Dns1[40] = {0};
  char _ipv6Dns2[40] = {0};

  char _ccuIP[64] = {0};

  // System log capture persists across reboots when enabled from the WebUI.
  bool _systemLogEnabled;

  // Extend Ethernet drop during restart to >30s so the CCU watchdog triggers.
  bool _flashPause;

  // Experimental WebUI layout. Persisted on the device so the selection
  // survives device restarts and browser reloads after login.
  bool _testDesignEnabled;

  // Serializes concurrent reads/writes across FreeRTOS tasks.
  SemaphoreHandle_t _mutex = NULL;

public:
  Settings();
  // Load one complete settings generation. If an interrupted settings or
  // backup-restore transaction is found, affected namespaces are erased
  // before any value is made visible and safe defaults are loaded instead.
  esp_err_t load();
  // Persist the complete settings snapshot. Callers that report success to a
  // user must check the return value. A persistent transaction marker makes
  // interrupted multi-key writes fail-safe even though ESP-IDF 6 writes each
  // nvs_set_* call immediately and nvs_commit() is not a batch transaction.
  esp_err_t save();
  esp_err_t clear();

  // Preflight the complete in-RAM Settings generation without changing NVS.
  // The shared NvsStorageLock must remain held by callers which combine this
  // check with writes to other namespaces.
  esp_err_t validateStorageCapacity();

  // Bracket a backup restore spanning Settings, theme, and monitoring. The
  // caller must hold NvsStorageLock from before begin through finish. On any
  // failed write, deliberately do not call finish: the next boot will erase
  // all affected namespaces rather than activating a mixed generation.
  static esp_err_t beginRestoreTransaction();
  static esp_err_t finishRestoreTransaction();
  void snapshot(settings_snapshot_t *out);
  void restoreSnapshot(const settings_snapshot_t *snapshot);

  char *getAdminPassword();
  char *getAdminUsername();
  bool setAdminPassword(const char* password);
  bool restoreAdminPassword(const char* password, bool passwordChanged);
  bool setAdminUsername(const char* username);
  bool getPasswordChanged();

  char *getHostname();
  bool getUseDHCP();
  ip4_addr_t getLocalIP();
  ip4_addr_t getNetmask();
  ip4_addr_t getGateway();
  ip4_addr_t getDns1();
  ip4_addr_t getDns2();

  bool setNetworkSettings(const char *hostname, bool useDHCP, ip4_addr_t localIP, ip4_addr_t netmask, ip4_addr_t gateway, ip4_addr_t dns1, ip4_addr_t dns2);

  timesource_t getTimesource();
  void setTimesource(timesource_t timesource);

  int getDcfOffset();
  void setDcfOffset(int offset);

  int getGpsBaudrate();
  void setGpsBaudrate(int baudrate);

  char *getNtpServer();
  void setNtpServer(const char *ntpServer);

  int getLEDBrightness();
  void setLEDBrightness(int brightness);

  int getLedProgram(int program);
  void setLedProgram(int program, int state);

  // IPv6 getters
  bool getEnableIPv6();
  char *getIPv6Mode();
  char *getIPv6Address();
  int getIPv6PrefixLength();
  char *getIPv6Gateway();
  char *getIPv6Dns1();
  char *getIPv6Dns2();

  // IPv6 setter
  void setIPv6Settings(bool enableIPv6, const char *ipv6Mode, const char *ipv6Address, int ipv6PrefixLength, const char *ipv6Gateway, const char *ipv6Dns1, const char *ipv6Dns2);

  char *getCCUIP();
  void setCCUIP(const char *ip);

  bool getSystemLogEnabled();
  void setSystemLogEnabled(bool enabled);

  bool getFlashPause();
  void setFlashPause(bool enabled);

  bool getTestDesignEnabled();
  void setTestDesignEnabled(bool enabled);

  // Authentication token persistence (NVS).  The token survives reboots so
  // the browser "remember me" stays valid after a firmware update or restart.
  // Empty on first boot – generateToken() fills and saves it automatically.
  bool loadAdminToken(char *out, size_t size);
  esp_err_t saveAdminToken(const char *token);
  esp_err_t clearAdminToken();
};
