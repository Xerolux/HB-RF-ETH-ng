/*
 *  validation.cpp is part of the HB-RF-ETH firmware v2.0
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

#include "validation.h"
#include <string.h>
#include <ctype.h>
#include "esp_log.h"

static const char *TAG = "Validation";

bool validateHostname(const char *hostname)
{
    if (hostname == NULL || hostname[0] == '\0')
    {
        ESP_LOGW(TAG, "Hostname is NULL or empty");
        return false;
    }

    size_t len = strlen(hostname);
    if (len > MAX_HOSTNAME_LENGTH)
    {
        ESP_LOGW(TAG, "Hostname too long: %zu (max %d)", len, MAX_HOSTNAME_LENGTH);
        return false;
    }

    // Check for valid hostname characters (alphanumeric, hyphen, dot)
    // Must start with alphanumeric
    if (!isalnum(hostname[0]))
    {
        ESP_LOGW(TAG, "Hostname must start with alphanumeric character");
        return false;
    }

    for (size_t i = 0; i < len; i++)
    {
        char c = hostname[i];
        if (!isalnum(c) && c != '-' && c != '.')
        {
            ESP_LOGW(TAG, "Invalid character in hostname: '%c'", c);
            return false;
        }

        // Hyphen cannot be first or last character of a label
        if (c == '-' && (i == 0 || i == len - 1 || hostname[i-1] == '.' || hostname[i+1] == '.'))
        {
            ESP_LOGW(TAG, "Invalid hyphen placement in hostname");
            return false;
        }
    }

    return true;
}

bool validateIPAddress(ip4_addr_t addr)
{
    // IP address 0.0.0.0 is valid (IPADDR_ANY) for DHCP mode
    // Broadcast address 255.255.255.255 is also technically valid
    // Just check that it's not in the reserved ranges for invalid use

    uint32_t ip = addr.addr;

    // We allow IPADDR_ANY (0.0.0.0) as it's used for DHCP
    if (ip == IPADDR_ANY)
    {
        return true;
    }

    // Extract first octet (most significant byte in network byte order)
    uint8_t firstOctet = (ip >> 24) & 0xFF;

    // Reject class E addresses (240-255) except for limited broadcast (255.255.255.255)
    if (firstOctet >= 240 && ip != IPADDR_BROADCAST)
    {
        ESP_LOGW(TAG, "Invalid IP address: Class E address");
        return false;
    }

    return true;
}

bool validateLEDBrightness(int brightness)
{
    if (brightness < MIN_LED_BRIGHTNESS || brightness > MAX_LED_BRIGHTNESS)
    {
        ESP_LOGW(TAG, "Invalid LED brightness: %d (must be %d-%d)",
                 brightness, MIN_LED_BRIGHTNESS, MAX_LED_BRIGHTNESS);
        return false;
    }
    return true;
}

bool validateGpsBaudrate(int baudrate)
{
    // Common GPS baudrates
    const int validBaudrates[] = {4800, 9600, 19200, 38400, 57600, 115200};
    const int numValidBaudrates = sizeof(validBaudrates) / sizeof(validBaudrates[0]);

    for (int i = 0; i < numValidBaudrates; i++)
    {
        if (baudrate == validBaudrates[i])
        {
            return true;
        }
    }

    ESP_LOGW(TAG, "Invalid GPS baudrate: %d", baudrate);
    return false;
}

bool validateDcfOffset(int offset)
{
    if (offset < MIN_DCF_OFFSET || offset > MAX_DCF_OFFSET)
    {
        ESP_LOGW(TAG, "Invalid DCF offset: %d (must be %d-%d)",
                 offset, MIN_DCF_OFFSET, MAX_DCF_OFFSET);
        return false;
    }
    return true;
}

bool validateNtpServer(const char *ntpServer)
{
    if (ntpServer == NULL || ntpServer[0] == '\0')
    {
        ESP_LOGW(TAG, "NTP server is NULL or empty");
        return false;
    }

    size_t len = strlen(ntpServer);
    if (len > MAX_NTP_SERVER_LENGTH)
    {
        ESP_LOGW(TAG, "NTP server string too long: %zu (max %d)", len, MAX_NTP_SERVER_LENGTH);
        return false;
    }

    // Basic validation: check for valid hostname/IP characters
    for (size_t i = 0; i < len; i++)
    {
        char c = ntpServer[i];
        if (!isalnum(c) && c != '.' && c != '-' && c != ':')
        {
            ESP_LOGW(TAG, "Invalid character in NTP server: '%c'", c);
            return false;
        }
    }

    return true;
}

bool validatePort(int port)
{
    // Port must be in valid range: 1-65535
    // Port 0 is reserved and should not be used
    if (port < 1 || port > 65535)
    {
        ESP_LOGW(TAG, "Invalid port number: %d (must be 1-65535)", port);
        return false;
    }
    return true;
}

bool validateStringLength(const char *str, size_t maxLength)
{
    if (str == NULL)
    {
        ESP_LOGW(TAG, "String is NULL");
        return false;
    }

    size_t len = strlen(str);
    if (len > maxLength)
    {
        ESP_LOGW(TAG, "String too long: %zu (max %zu)", len, maxLength);
        return false;
    }

    return true;
}

bool validateIPv6Address(const char *ipv6)
{
    if (ipv6 == NULL || ipv6[0] == '\0')
    {
        // Empty IPv6 address is valid (disabled/auto mode)
        return true;
    }

    // Basic IPv6 format validation
    // IPv6 addresses can contain hex digits, colons, and dots (for IPv4-mapped addresses)
    int colons = 0;
    int dots = 0;
    int hexDigits = 0;
    int doubleColons = 0;

    for (size_t i = 0; ipv6[i] != '\0'; i++)
    {
        char c = ipv6[i];

        if (c == ':')
        {
            colons++;
            if (i > 0 && ipv6[i-1] == ':')
            {
                doubleColons++;
            }
            if (doubleColons > 1)
            {
                ESP_LOGW(TAG, "Invalid IPv6: multiple double colons");
                return false;
            }
        }
        else if (c == '.')
        {
            dots++;
        }
        else if (isxdigit(c))
        {
            hexDigits++;
        }
        else
        {
            ESP_LOGW(TAG, "Invalid IPv6: invalid character '%c'", c);
            return false;
        }
    }

    // Must have at least some colons for a valid IPv6 address
    if (colons < 2 && colons > 0)
    {
        ESP_LOGW(TAG, "Invalid IPv6: not enough colons");
        return false;
    }

    // If we have dots (IPv4-mapped), we should have a reasonable number
    if (dots > 0 && dots != 3 && dots != 4)
    {
        ESP_LOGW(TAG, "Invalid IPv6: invalid number of dots for IPv4-mapped address");
        return false;
    }

    return true;
}
