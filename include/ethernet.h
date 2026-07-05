/*
 *  ethernet.h is part of the HB-RF-ETH firmware v2.0
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
#include <string.h>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "settings.h"

class Ethernet
{
private:
  esp_netif_t *_eth_netif;
  esp_eth_handle_t _eth_handle;
  eth_mac_config_t _mac_config;
  eth_phy_config_t _phy_config;
  esp_eth_mac_t *_mac;
  esp_eth_phy_t *_phy;
  esp_eth_config_t _eth_config;
  esp_event_handler_instance_t _eth_event_instance;
  esp_event_handler_instance_t _ip_event_instance;

  Settings *_settings;
  std::atomic<bool> _isConnected;
  std::atomic<eth_speed_t> _linkSpeed;
  std::atomic<eth_duplex_t> _duplexMode;

  // DNS Cache
  static const int DNS_CACHE_SIZE = 8;
  struct dns_cache_entry {
    char hostname[64];
    ip4_addr_t ip_addr;
    uint64_t expiry_time;
    bool valid;
  };
  static dns_cache_entry _dns_cache[DNS_CACHE_SIZE];
  static uint64_t _current_time;
  static SemaphoreHandle_t _dns_cache_mutex;

public:
  Ethernet(Settings *settings);

  void start();
  void stop();

  void getNetworkSettings(ip4_addr_t *ip, ip4_addr_t *netmask, ip4_addr_t *gateway, ip4_addr_t *dns1, ip4_addr_t *dns2);

  bool isConnected() { return _isConnected.load(); }
  int getLinkSpeedMbps();
  const char* getDuplexMode();
  const char* getHostname() { return _settings ? _settings->getHostname() : ""; }

  // Fills out[0..max_count-1] with the textual IPv6 addresses currently
  // assigned to the Ethernet interface (link-local + global / SLAAC / static).
  // Returns the number of addresses written.
  int getIPv6AddressStrings(char out[][48], int max_count);

  // DNS Cache Funktionen
  static void dnsCacheInit();
  static bool dnsCacheLookup(const char* hostname, ip4_addr_t* ip_addr);
  static void dnsCacheAdd(const char* hostname, const ip4_addr_t* ip_addr, uint32_t ttl);
  static void dnsCacheClear();
  static void dnsCleanupTask(void* pvParameters);

  void _handleETHEvent(esp_event_base_t event_base, int32_t event_id, void *event_data);
  void _handleIPEvent(esp_event_base_t event_base, int32_t event_id, void *event_data);
};
