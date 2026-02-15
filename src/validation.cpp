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

// Helper function to check if a string is a valid IPv4 address
static bool isValidIPv4(const char *str, size_t len)
{
    int octets[4];
    int octetIndex = 0;
    int currentNum = -1;

    for (size_t i = 0; i <= len; i++)
    {
        char c = (i < len) ? str[i] : '\0';

        if (isdigit(c))
        {
            if (currentNum == -1) currentNum = 0;
            currentNum = currentNum * 10 + (c - '0');

            if (currentNum > 255)
            {
                return false;
            }
        }
        else if (c == '.' || c == '\0')
        {
            if (currentNum == -1 || octetIndex >= 4)
            {
                return false;
            }
            octets[octetIndex++] = currentNum;
            currentNum = -1;

            if (c == '\0') break;
        }
        else
        {
            return false;
        }
    }

    return (octetIndex == 4);
}

// Helper function to validate a hostname label (part between dots)
static bool isValidHostnameLabel(const char *label, size_t len)
{
    if (len == 0 || len > 63)
    {
        return false;
    }

    // Must start and end with alphanumeric
    if (!isalnum(label[0]) || !isalnum(label[len - 1]))
    {
        return false;
    }

    // Middle can contain alphanumeric and hyphens
    for (size_t i = 1; i < len - 1; i++)
    {
        if (!isalnum(label[i]) && label[i] != '-')
        {
            return false;
        }
    }

    return true;
}

// Helper function to validate a complete hostname
static bool isValidHostname(const char *hostname, size_t len)
{
    if (len == 0 || len > MAX_HOSTNAME_LENGTH)
    {
        return false;
    }

    // Must start with alphanumeric
    if (!isalnum(hostname[0]))
    {
        return false;
    }

    size_t labelStart = 0;
    for (size_t i = 0; i <= len; i++)
    {
        if (i == len || hostname[i] == '.')
        {
            size_t labelLen = i - labelStart;

            if (labelLen == 0)
            {
                return false;  // Empty label (consecutive dots)
            }

            if (!isValidHostnameLabel(&hostname[labelStart], labelLen))
            {
                return false;
            }

            labelStart = i + 1;
        }
        else if (!isalnum(hostname[i]) && hostname[i] != '-' && hostname[i] != '.')
        {
            return false;  // Invalid character
        }
    }

    // Must not end with a dot
    if (len > 0 && hostname[len - 1] == '.')
    {
        return false;
    }

    return true;
}

bool validateServerAddress(const char *server, size_t maxLength)
{
    if (server == NULL || server[0] == '\0')
    {
        ESP_LOGW(TAG, "Server address is NULL or empty");
        return false;
    }

    size_t len = strlen(server);
    if (len > maxLength)
    {
        ESP_LOGW(TAG, "Server address string too long: %zu (max %zu)", len, maxLength);
        return false;
    }

    // Check for IPv6 in brackets: [2001:db8::1] or [2001:db8::1]:123
    if (server[0] == '[')
    {
        // Find closing bracket
        size_t closeBracket = 0;
        for (size_t i = 1; i < len; i++)
        {
            if (server[i] == ']')
            {
                closeBracket = i;
                break;
            }
        }

        if (closeBracket == 0)
        {
            ESP_LOGW(TAG, "Invalid server address: unclosed bracket for IPv6");
            return false;
        }

        // Extract IPv6 address (without brackets)
        char ipv6[46];
        size_t ipv6Len = closeBracket - 1;
        if (ipv6Len >= sizeof(ipv6))
        {
            ESP_LOGW(TAG, "Invalid server address: IPv6 address too long");
            return false;
        }
        strncpy(ipv6, &server[1], ipv6Len);
        ipv6[ipv6Len] = '\0';

        if (!validateIPv6Address(ipv6))
        {
            ESP_LOGW(TAG, "Invalid server address: invalid IPv6 address");
            return false;
        }

        // Check for optional port after bracket
        if (closeBracket + 1 < len)
        {
            if (server[closeBracket + 1] != ':')
            {
                ESP_LOGW(TAG, "Invalid server address: expected ':' after IPv6 bracket");
                return false;
            }

            // Validate port number
            int port = 0;
            for (size_t i = closeBracket + 2; i < len; i++)
            {
                if (!isdigit(server[i]))
                {
                    ESP_LOGW(TAG, "Invalid server address: invalid port character");
                    return false;
                }
                port = port * 10 + (server[i] - '0');
            }

            if (!validatePort(port))
            {
                ESP_LOGW(TAG, "Invalid server address: invalid port number");
                return false;
            }
        }

        return true;
    }

    // Find optional port (last occurrence of ':')
    size_t portSep = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (server[i] == ':')
        {
            portSep = i;
        }
    }

    // Extract hostname/IP part (before optional port)
    size_t hostLen = (portSep > 0) ? portSep : len;
    char host[MAX_SERVER_ADDRESS_LENGTH + 1];
    if (hostLen >= sizeof(host))
    {
        ESP_LOGW(TAG, "Invalid server address: hostname too long");
        return false;
    }
    strncpy(host, server, hostLen);
    host[hostLen] = '\0';

    // Validate port if present
    if (portSep > 0)
    {
        int port = 0;
        for (size_t i = portSep + 1; i < len; i++)
        {
            if (!isdigit(server[i]))
            {
                ESP_LOGW(TAG, "Invalid server address: invalid port character");
                return false;
            }
            port = port * 10 + (server[i] - '0');
        }

        if (!validatePort(port))
        {
            ESP_LOGW(TAG, "Invalid server address: invalid port number");
            return false;
        }
    }

    // Check if host is a valid IPv4 address
    if (isValidIPv4(host, hostLen))
    {
        return true;
    }

    // Check if host is a valid IPv6 address (without brackets)
    if (validateIPv6Address(host))
    {
        return true;
    }

    // Check if host is a valid hostname
    if (isValidHostname(host, hostLen))
    {
        return true;
    }

    ESP_LOGW(TAG, "Invalid server address: not a valid hostname, IPv4, or IPv6 address");
    return false;
}

bool validateNtpServer(const char *ntpServer)
{
    // NTP server validation uses the generic server address validation
    return validateServerAddress(ntpServer, MAX_NTP_SERVER_LENGTH);
}

bool validateSnmpCommunity(const char *community)
{
    if (community == NULL || community[0] == '\0')
    {
        ESP_LOGW(TAG, "SNMP community string is NULL or empty");
        return false;
    }

    size_t len = strlen(community);
    if (len > MAX_SNMP_COMMUNITY_LENGTH)
    {
        ESP_LOGW(TAG, "SNMP community string too long: %zu (max %d)", len, MAX_SNMP_COMMUNITY_LENGTH);
        return false;
    }

    // SNMP community strings should only contain:
    // - Alphanumeric characters (a-z, A-Z, 0-9)
    // - Hyphens (-)
    // - Underscores (_)
    // No spaces, special characters, or control characters for security
    for (size_t i = 0; i < len; i++)
    {
        char c = community[i];
        if (!isalnum(c) && c != '-' && c != '_')
        {
            ESP_LOGW(TAG, "Invalid character in SNMP community string: '%c' (only alphanumeric, hyphen, underscore allowed)", c);
            return false;
        }
    }

    // Community string should not start or end with hyphen or underscore
    // (best practice for readability and consistency)
    if (community[0] == '-' || community[0] == '_' ||
        community[len - 1] == '-' || community[len - 1] == '_')
    {
        ESP_LOGW(TAG, "SNMP community string should not start or end with hyphen or underscore");
        return false;
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

    size_t len = strlen(ipv6);
    if (len < 2 || len > 45)  // IPv6 max length is 45 chars (8 groups of 4 hex + 7 colons + potential IPv4 suffix)
    {
        ESP_LOGW(TAG, "Invalid IPv6: length %zu out of range", len);
        return false;
    }

    int segmentCount = 0;
    int segmentLen = 0;
    bool hasDoubleColon = false;
    bool hasIPv4Suffix = false;
    int dotCount = 0;

    for (size_t i = 0; i < len; i++)
    {
        char c = ipv6[i];

        if (c == ':')
        {
            // Check for double colon
            if (i + 1 < len && ipv6[i + 1] == ':')
            {
                if (hasDoubleColon)
                {
                    ESP_LOGW(TAG, "Invalid IPv6: multiple double colons");
                    return false;
                }
                hasDoubleColon = true;

                // Allow :: at start or in middle, but validate segment before it
                if (segmentLen > 4)
                {
                    ESP_LOGW(TAG, "Invalid IPv6: segment too long (%d hex digits)", segmentLen);
                    return false;
                }
                if (segmentLen > 0) segmentCount++;
                segmentLen = 0;
                i++;  // Skip second colon
                continue;
            }

            // Single colon - end of segment
            if (segmentLen > 4)
            {
                ESP_LOGW(TAG, "Invalid IPv6: segment too long (%d hex digits)", segmentLen);
                return false;
            }
            if (segmentLen > 0 || i == 0)  // Allow empty segment only at start after ::
            {
                segmentCount++;
            }
            segmentLen = 0;
        }
        else if (c == '.')
        {
            // Potential IPv4 suffix (e.g., ::ffff:192.168.1.1)
            hasIPv4Suffix = true;
            dotCount++;

            if (dotCount > 3)
            {
                ESP_LOGW(TAG, "Invalid IPv6: too many dots in IPv4 suffix");
                return false;
            }
        }
        else if (isxdigit(c))
        {
            if (hasIPv4Suffix)
            {
                // After seeing a dot, we should only see decimal digits
                if (!isdigit(c))
                {
                    ESP_LOGW(TAG, "Invalid IPv6: non-decimal digit in IPv4 suffix");
                    return false;
                }
            }
            segmentLen++;
        }
        else if (isdigit(c) && hasIPv4Suffix)
        {
            // Allow decimal digits in IPv4 suffix
            continue;
        }
        else
        {
            ESP_LOGW(TAG, "Invalid IPv6: invalid character '%c'", c);
            return false;
        }
    }

    // Count the last segment
    if (segmentLen > 0)
    {
        if (!hasIPv4Suffix && segmentLen > 4)
        {
            ESP_LOGW(TAG, "Invalid IPv6: final segment too long (%d hex digits)", segmentLen);
            return false;
        }
        segmentCount++;
    }

    // Validate IPv4 suffix if present
    if (hasIPv4Suffix && dotCount != 3)
    {
        ESP_LOGW(TAG, "Invalid IPv6: IPv4 suffix must have exactly 3 dots");
        return false;
    }

    // IPv6 addresses have 8 segments (or fewer with ::)
    // If :: is present, we can have 0-7 segments
    // If :: is not present, we must have exactly 8 segments (or 6 + IPv4)
    if (hasDoubleColon)
    {
        if (segmentCount > 7)
        {
            ESP_LOGW(TAG, "Invalid IPv6: too many segments (%d) with double colon", segmentCount);
            return false;
        }
    }
    else
    {
        int expectedSegments = hasIPv4Suffix ? 6 : 8;
        if (segmentCount != expectedSegments)
        {
            ESP_LOGW(TAG, "Invalid IPv6: expected %d segments, got %d", expectedSegments, segmentCount);
            return false;
        }
    }

    return true;
}
