/*
 *  validation.h is part of the HB-RF-ETH firmware v2.0
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

#include <stdbool.h>
#include <stdint.h>
#include <lwip/ip4_addr.h>

// Validation limits
#define MAX_HOSTNAME_LENGTH 63
#define MIN_LED_BRIGHTNESS 0
#define MAX_LED_BRIGHTNESS 100
#define MIN_DCF_OFFSET -60000
#define MAX_DCF_OFFSET 60000
#define MAX_NTP_SERVER_LENGTH 64
#define MAX_SERVER_ADDRESS_LENGTH 128
#define MAX_SNMP_COMMUNITY_LENGTH 32

// Validation functions
bool validateHostname(const char *hostname);
bool validateIPAddress(ip4_addr_t addr);
bool validateLEDBrightness(int brightness);
bool validateGpsBaudrate(int baudrate);
bool validateDcfOffset(int offset);
// NTP server validation (hostname, IPv4, IPv6, with optional port)
bool validateNtpServer(const char *ntpServer);

// Port validation
bool validatePort(int port);

// String length validation
bool validateStringLength(const char *str, size_t maxLength);

// IPv6 validation (comprehensive format check with segment validation)
bool validateIPv6Address(const char *ipv6);

// Server address validation (hostname, IPv4, IPv6, with optional port)
// Used for MQTT, NTP, and other network services
bool validateServerAddress(const char *server, size_t maxLength);

// SNMP community string validation (alphanumeric, hyphen, underscore only)
bool validateSnmpCommunity(const char *community);

// CCU address validation (hostname, IPv4, IPv6, WITHOUT port)
// Used for HomeMatic CCU connection - does not allow port specification
bool validateCcuAddress(const char *address);
