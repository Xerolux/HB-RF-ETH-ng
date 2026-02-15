/*
 *  test_ntp_validation.cpp
 *
 *  Unit tests for NTP server validation
 *  Verifies that validateNtpServer correctly validates hostnames, IPv4, IPv6,
 *  and optional port specifications
 *
 *  These tests use the PlatformIO Unity test framework.
 */

#include <unity.h>
#include <string.h>
#include "validation.h"

void setUp(void)
{
    // Nothing to set up for validation tests
}

void tearDown(void)
{
    // Nothing to tear down
}

// ==========================================
// Valid NTP Server Tests - Hostnames
// ==========================================

void test_ntp_valid_hostname(void)
{
    // Standard NTP hostnames
    TEST_ASSERT_TRUE(validateNtpServer("pool.ntp.org"));
    TEST_ASSERT_TRUE(validateNtpServer("time.google.com"));
    TEST_ASSERT_TRUE(validateNtpServer("time.cloudflare.com"));
    TEST_ASSERT_TRUE(validateNtpServer("time.windows.com"));
}

void test_ntp_valid_hostname_with_subdomain(void)
{
    // Multiple subdomains
    TEST_ASSERT_TRUE(validateNtpServer("ntp.example.com"));
    TEST_ASSERT_TRUE(validateNtpServer("time.nist.gov"));
    TEST_ASSERT_TRUE(validateNtpServer("ntp1.example.co.uk"));
    TEST_ASSERT_TRUE(validateNtpServer("0.de.pool.ntp.org"));
}

void test_ntp_valid_hostname_with_hyphens(void)
{
    // Hostnames with hyphens
    TEST_ASSERT_TRUE(validateNtpServer("ntp-server.example.com"));
    TEST_ASSERT_TRUE(validateNtpServer("time-a-g.nist.gov"));
    TEST_ASSERT_TRUE(validateNtpServer("my-ntp-server.local"));
}

void test_ntp_valid_hostname_with_numbers(void)
{
    // Hostnames with numbers
    TEST_ASSERT_TRUE(validateNtpServer("ntp1.example.com"));
    TEST_ASSERT_TRUE(validateNtpServer("time2.google.com"));
    TEST_ASSERT_TRUE(validateNtpServer("server123.ntp.org"));
}

void test_ntp_valid_single_label_hostname(void)
{
    // Single label hostnames (valid in some networks)
    TEST_ASSERT_TRUE(validateNtpServer("localhost"));
    TEST_ASSERT_TRUE(validateNtpServer("ntpserver"));
    TEST_ASSERT_TRUE(validateNtpServer("time"));
}

// ==========================================
// Valid NTP Server Tests - IPv4
// ==========================================

void test_ntp_valid_ipv4(void)
{
    // Standard IPv4 addresses
    TEST_ASSERT_TRUE(validateNtpServer("192.168.1.1"));
    TEST_ASSERT_TRUE(validateNtpServer("10.0.0.1"));
    TEST_ASSERT_TRUE(validateNtpServer("172.16.0.1"));
}

void test_ntp_valid_ipv4_public(void)
{
    // Public NTP server IPs
    TEST_ASSERT_TRUE(validateNtpServer("216.239.35.0"));   // Google
    TEST_ASSERT_TRUE(validateNtpServer("162.159.200.1"));  // Cloudflare
    TEST_ASSERT_TRUE(validateNtpServer("129.6.15.28"));    // NIST
}

void test_ntp_valid_ipv4_with_port(void)
{
    // IPv4 with port
    TEST_ASSERT_TRUE(validateNtpServer("192.168.1.1:123"));
    TEST_ASSERT_TRUE(validateNtpServer("10.0.0.1:8123"));
    TEST_ASSERT_TRUE(validateNtpServer("172.16.0.1:1123"));
}

// ==========================================
// Valid NTP Server Tests - IPv6
// ==========================================

void test_ntp_valid_ipv6_in_brackets(void)
{
    // IPv6 in brackets (required for ambiguity with port)
    TEST_ASSERT_TRUE(validateNtpServer("[2001:4860:4860::8888]"));
    TEST_ASSERT_TRUE(validateNtpServer("[2001:4860:4860::8844]"));
    TEST_ASSERT_TRUE(validateNtpServer("[2606:4700:4700::1111]"));
}

void test_ntp_valid_ipv6_in_brackets_with_port(void)
{
    // IPv6 in brackets with port
    TEST_ASSERT_TRUE(validateNtpServer("[2001:4860:4860::8888]:123"));
    TEST_ASSERT_TRUE(validateNtpServer("[2606:4700:4700::1111]:8123"));
    TEST_ASSERT_TRUE(validateNtpServer("[fe80::1]:123"));
}

void test_ntp_valid_ipv6_without_brackets(void)
{
    // IPv6 without brackets (no port ambiguity)
    TEST_ASSERT_TRUE(validateNtpServer("2001:4860:4860::8888"));
    TEST_ASSERT_TRUE(validateNtpServer("2606:4700:4700::1111"));
    TEST_ASSERT_TRUE(validateNtpServer("fe80::1"));
}

void test_ntp_valid_ipv6_loopback(void)
{
    // IPv6 loopback
    TEST_ASSERT_TRUE(validateNtpServer("[::1]"));
    TEST_ASSERT_TRUE(validateNtpServer("::1"));
}

// ==========================================
// Valid NTP Server Tests - With Port
// ==========================================

void test_ntp_valid_hostname_with_port(void)
{
    // Hostname with port
    TEST_ASSERT_TRUE(validateNtpServer("pool.ntp.org:123"));
    TEST_ASSERT_TRUE(validateNtpServer("time.google.com:8123"));
    TEST_ASSERT_TRUE(validateNtpServer("ntp.example.com:1123"));
}

void test_ntp_valid_hostname_with_standard_port(void)
{
    // Standard NTP port 123
    TEST_ASSERT_TRUE(validateNtpServer("pool.ntp.org:123"));
    TEST_ASSERT_TRUE(validateNtpServer("192.168.1.1:123"));
    TEST_ASSERT_TRUE(validateNtpServer("[2001:db8::1]:123"));
}

// ==========================================
// Invalid NTP Server Tests - Hostnames
// ==========================================

void test_ntp_invalid_empty(void)
{
    // Empty or NULL
    TEST_ASSERT_FALSE(validateNtpServer(""));
    TEST_ASSERT_FALSE(validateNtpServer(NULL));
}

void test_ntp_invalid_hostname_start_with_hyphen(void)
{
    // Starts with hyphen
    TEST_ASSERT_FALSE(validateNtpServer("-ntp.example.com"));
    TEST_ASSERT_FALSE(validateNtpServer("example.-com"));
}

void test_ntp_invalid_hostname_end_with_hyphen(void)
{
    // Ends with hyphen
    TEST_ASSERT_FALSE(validateNtpServer("ntp-.example.com"));
    TEST_ASSERT_FALSE(validateNtpServer("example.com-"));
}

void test_ntp_invalid_hostname_start_with_dot(void)
{
    // Starts with dot
    TEST_ASSERT_FALSE(validateNtpServer(".example.com"));
    TEST_ASSERT_FALSE(validateNtpServer(".ntp.org"));
}

void test_ntp_invalid_hostname_end_with_dot(void)
{
    // Ends with dot
    TEST_ASSERT_FALSE(validateNtpServer("example.com."));
    TEST_ASSERT_FALSE(validateNtpServer("ntp.org."));
}

void test_ntp_invalid_hostname_consecutive_dots(void)
{
    // Consecutive dots
    TEST_ASSERT_FALSE(validateNtpServer("example..com"));
    TEST_ASSERT_FALSE(validateNtpServer("ntp...org"));
}

void test_ntp_invalid_hostname_special_chars(void)
{
    // Invalid special characters
    TEST_ASSERT_FALSE(validateNtpServer("ntp@example.com"));
    TEST_ASSERT_FALSE(validateNtpServer("ntp#server.com"));
    TEST_ASSERT_FALSE(validateNtpServer("ntp_server.com"));
    TEST_ASSERT_FALSE(validateNtpServer("ntp server.com"));  // space
}

void test_ntp_invalid_hostname_only_dots(void)
{
    // Only dots
    TEST_ASSERT_FALSE(validateNtpServer("..."));
    TEST_ASSERT_FALSE(validateNtpServer("."));
}

void test_ntp_invalid_hostname_only_hyphens(void)
{
    // Only hyphens
    TEST_ASSERT_FALSE(validateNtpServer("---"));
    TEST_ASSERT_FALSE(validateNtpServer("-"));
}

// ==========================================
// Invalid NTP Server Tests - IPv4
// ==========================================

void test_ntp_invalid_ipv4_out_of_range(void)
{
    // Octets out of range
    TEST_ASSERT_FALSE(validateNtpServer("256.1.1.1"));
    TEST_ASSERT_FALSE(validateNtpServer("192.256.1.1"));
    TEST_ASSERT_FALSE(validateNtpServer("192.168.256.1"));
    TEST_ASSERT_FALSE(validateNtpServer("192.168.1.256"));
}

void test_ntp_invalid_ipv4_too_few_octets(void)
{
    // Too few octets
    TEST_ASSERT_FALSE(validateNtpServer("192.168.1"));
    TEST_ASSERT_FALSE(validateNtpServer("192.168"));
}

void test_ntp_invalid_ipv4_too_many_octets(void)
{
    // Too many octets
    TEST_ASSERT_FALSE(validateNtpServer("192.168.1.1.1"));
}

void test_ntp_invalid_ipv4_empty_octet(void)
{
    // Empty octet
    TEST_ASSERT_FALSE(validateNtpServer("192..1.1"));
    TEST_ASSERT_FALSE(validateNtpServer("192.168..1"));
}

// ==========================================
// Invalid NTP Server Tests - IPv6
// ==========================================

void test_ntp_invalid_ipv6_unclosed_bracket(void)
{
    // Unclosed bracket
    TEST_ASSERT_FALSE(validateNtpServer("[2001:db8::1"));
    TEST_ASSERT_FALSE(validateNtpServer("[::1"));
}

void test_ntp_invalid_ipv6_unopened_bracket(void)
{
    // Bracket without opening
    TEST_ASSERT_FALSE(validateNtpServer("2001:db8::1]"));
}

void test_ntp_invalid_ipv6_invalid_after_bracket(void)
{
    // Invalid character after closing bracket
    TEST_ASSERT_FALSE(validateNtpServer("[2001:db8::1]abc"));
    TEST_ASSERT_FALSE(validateNtpServer("[2001:db8::1] :123"));
}

void test_ntp_invalid_ipv6_malformed(void)
{
    // Malformed IPv6 (caught by validateIPv6Address)
    TEST_ASSERT_FALSE(validateNtpServer("[2001:db8:::1]"));
    TEST_ASSERT_FALSE(validateNtpServer("[::::::1]"));
}

// ==========================================
// Invalid NTP Server Tests - Port
// ==========================================

void test_ntp_invalid_port_out_of_range(void)
{
    // Port out of range
    TEST_ASSERT_FALSE(validateNtpServer("pool.ntp.org:0"));
    TEST_ASSERT_FALSE(validateNtpServer("pool.ntp.org:65536"));
    TEST_ASSERT_FALSE(validateNtpServer("192.168.1.1:99999"));
}

void test_ntp_invalid_port_non_numeric(void)
{
    // Non-numeric port
    TEST_ASSERT_FALSE(validateNtpServer("pool.ntp.org:abc"));
    TEST_ASSERT_FALSE(validateNtpServer("192.168.1.1:12a3"));
    TEST_ASSERT_FALSE(validateNtpServer("[2001:db8::1]:abc"));
}

void test_ntp_invalid_port_empty(void)
{
    // Empty port after colon
    TEST_ASSERT_FALSE(validateNtpServer("pool.ntp.org:"));
    TEST_ASSERT_FALSE(validateNtpServer("192.168.1.1:"));
    TEST_ASSERT_FALSE(validateNtpServer("[2001:db8::1]:"));
}

void test_ntp_invalid_port_negative(void)
{
    // Negative port (hyphen after colon)
    TEST_ASSERT_FALSE(validateNtpServer("pool.ntp.org:-123"));
}

// ==========================================
// Invalid NTP Server Tests - Too Long
// ==========================================

void test_ntp_invalid_too_long(void)
{
    // String too long (MAX_NTP_SERVER_LENGTH = 64)
    char longServer[100];
    memset(longServer, 'a', sizeof(longServer) - 1);
    longServer[sizeof(longServer) - 1] = '\0';
    TEST_ASSERT_FALSE(validateNtpServer(longServer));
}

// ==========================================
// Main test runner
// ==========================================

void setup()
{
    delay(2000);  // Wait for ESP32 to stabilize
    UNITY_BEGIN();

    // Valid hostname tests
    RUN_TEST(test_ntp_valid_hostname);
    RUN_TEST(test_ntp_valid_hostname_with_subdomain);
    RUN_TEST(test_ntp_valid_hostname_with_hyphens);
    RUN_TEST(test_ntp_valid_hostname_with_numbers);
    RUN_TEST(test_ntp_valid_single_label_hostname);

    // Valid IPv4 tests
    RUN_TEST(test_ntp_valid_ipv4);
    RUN_TEST(test_ntp_valid_ipv4_public);
    RUN_TEST(test_ntp_valid_ipv4_with_port);

    // Valid IPv6 tests
    RUN_TEST(test_ntp_valid_ipv6_in_brackets);
    RUN_TEST(test_ntp_valid_ipv6_in_brackets_with_port);
    RUN_TEST(test_ntp_valid_ipv6_without_brackets);
    RUN_TEST(test_ntp_valid_ipv6_loopback);

    // Valid port tests
    RUN_TEST(test_ntp_valid_hostname_with_port);
    RUN_TEST(test_ntp_valid_hostname_with_standard_port);

    // Invalid hostname tests
    RUN_TEST(test_ntp_invalid_empty);
    RUN_TEST(test_ntp_invalid_hostname_start_with_hyphen);
    RUN_TEST(test_ntp_invalid_hostname_end_with_hyphen);
    RUN_TEST(test_ntp_invalid_hostname_start_with_dot);
    RUN_TEST(test_ntp_invalid_hostname_end_with_dot);
    RUN_TEST(test_ntp_invalid_hostname_consecutive_dots);
    RUN_TEST(test_ntp_invalid_hostname_special_chars);
    RUN_TEST(test_ntp_invalid_hostname_only_dots);
    RUN_TEST(test_ntp_invalid_hostname_only_hyphens);

    // Invalid IPv4 tests
    RUN_TEST(test_ntp_invalid_ipv4_out_of_range);
    RUN_TEST(test_ntp_invalid_ipv4_too_few_octets);
    RUN_TEST(test_ntp_invalid_ipv4_too_many_octets);
    RUN_TEST(test_ntp_invalid_ipv4_empty_octet);

    // Invalid IPv6 tests
    RUN_TEST(test_ntp_invalid_ipv6_unclosed_bracket);
    RUN_TEST(test_ntp_invalid_ipv6_unopened_bracket);
    RUN_TEST(test_ntp_invalid_ipv6_invalid_after_bracket);
    RUN_TEST(test_ntp_invalid_ipv6_malformed);

    // Invalid port tests
    RUN_TEST(test_ntp_invalid_port_out_of_range);
    RUN_TEST(test_ntp_invalid_port_non_numeric);
    RUN_TEST(test_ntp_invalid_port_empty);
    RUN_TEST(test_ntp_invalid_port_negative);

    // Invalid length tests
    RUN_TEST(test_ntp_invalid_too_long);

    UNITY_END();
}

void loop()
{
    // Nothing to do in loop for unit tests
}
