/*
 *  test_ccu_validation.cpp
 *
 *  Unit tests for CCU address validation
 *  Verifies validateCcuAddress correctly validates hostnames, IPv4, IPv6
 *  WITHOUT port specification (CCU does not use ports in address)
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
// Valid CCU Address Tests - Hostnames
// ==========================================

void test_ccu_valid_hostname(void)
{
    // Standard CCU hostnames
    TEST_ASSERT_TRUE(validateCcuAddress("ccu3.local"));
    TEST_ASSERT_TRUE(validateCcuAddress("homematic-ccu3"));
    TEST_ASSERT_TRUE(validateCcuAddress("ccu2.homematic.local"));
    TEST_ASSERT_TRUE(validateCcuAddress("raspberrymatic"));
}

void test_ccu_valid_hostname_single_label(void)
{
    // Single label hostnames
    TEST_ASSERT_TRUE(validateCcuAddress("ccu"));
    TEST_ASSERT_TRUE(validateCcuAddress("ccu3"));
    TEST_ASSERT_TRUE(validateCcuAddress("raspberrymatic"));
}

void test_ccu_valid_hostname_with_numbers(void)
{
    // Hostnames with numbers
    TEST_ASSERT_TRUE(validateCcuAddress("ccu3"));
    TEST_ASSERT_TRUE(validateCcuAddress("homematic2"));
    TEST_ASSERT_TRUE(validateCcuAddress("rpi123"));
}

void test_ccu_valid_hostname_with_hyphens(void)
{
    // Hostnames with hyphens
    TEST_ASSERT_TRUE(validateCcuAddress("homematic-ccu"));
    TEST_ASSERT_TRUE(validateCcuAddress("raspberry-matic"));
    TEST_ASSERT_TRUE(validateCcuAddress("my-ccu3"));
}

// ==========================================
// Valid CCU Address Tests - IPv4
// ==========================================

void test_ccu_valid_ipv4(void)
{
    // Standard IPv4 addresses
    TEST_ASSERT_TRUE(validateCcuAddress("192.168.1.10"));
    TEST_ASSERT_TRUE(validateCcuAddress("10.0.0.100"));
    TEST_ASSERT_TRUE(validateCcuAddress("172.16.0.50"));
}

void test_ccu_valid_ipv4_private_ranges(void)
{
    // Common private IP ranges
    TEST_ASSERT_TRUE(validateCcuAddress("192.168.0.1"));
    TEST_ASSERT_TRUE(validateCcuAddress("192.168.178.100"));
    TEST_ASSERT_TRUE(validateCcuAddress("10.1.1.1"));
    TEST_ASSERT_TRUE(validateCcuAddress("172.16.0.1"));
}

void test_ccu_valid_ipv4_localhost(void)
{
    // Localhost
    TEST_ASSERT_TRUE(validateCcuAddress("127.0.0.1"));
}

// ==========================================
// Valid CCU Address Tests - IPv6
// ==========================================

void test_ccu_valid_ipv6_in_brackets(void)
{
    // IPv6 in brackets (no port)
    TEST_ASSERT_TRUE(validateCcuAddress("[2001:db8::1]"));
    TEST_ASSERT_TRUE(validateCcuAddress("[fe80::1]"));
    TEST_ASSERT_TRUE(validateCcuAddress("[::1]"));
}

void test_ccu_valid_ipv6_without_brackets(void)
{
    // IPv6 without brackets
    TEST_ASSERT_TRUE(validateCcuAddress("2001:db8::1"));
    TEST_ASSERT_TRUE(validateCcuAddress("fe80::a1b2:c3d4:e5f6:7890"));
    TEST_ASSERT_TRUE(validateCcuAddress("::1"));
}

void test_ccu_valid_ipv6_link_local(void)
{
    // Link-local addresses (common in HomeMatic setups)
    TEST_ASSERT_TRUE(validateCcuAddress("fe80::1"));
    TEST_ASSERT_TRUE(validateCcuAddress("[fe80::a1b2:c3d4:e5f6:7890]"));
}

void test_ccu_valid_ipv6_full(void)
{
    // Full IPv6 addresses
    TEST_ASSERT_TRUE(validateCcuAddress("2001:0db8:85a3:0000:0000:8a2e:0370:7334"));
    TEST_ASSERT_TRUE(validateCcuAddress("[2001:db8:85a3:0:0:8a2e:370:7334]"));
}

// ==========================================
// Invalid CCU Address Tests - Port Not Allowed
// ==========================================

void test_ccu_invalid_ipv4_with_port(void)
{
    // IPv4 with port - NOT allowed for CCU
    TEST_ASSERT_FALSE(validateCcuAddress("192.168.1.10:80"));
    TEST_ASSERT_FALSE(validateCcuAddress("10.0.0.1:8080"));
    TEST_ASSERT_FALSE(validateCcuAddress("192.168.178.100:2001"));
}

void test_ccu_invalid_hostname_with_port(void)
{
    // Hostname with port - NOT allowed for CCU
    TEST_ASSERT_FALSE(validateCcuAddress("ccu3.local:80"));
    TEST_ASSERT_FALSE(validateCcuAddress("homematic-ccu:2001"));
    TEST_ASSERT_FALSE(validateCcuAddress("raspberrymatic:8080"));
}

void test_ccu_invalid_ipv6_with_port(void)
{
    // IPv6 with port - NOT allowed for CCU
    TEST_ASSERT_FALSE(validateCcuAddress("[2001:db8::1]:80"));
    TEST_ASSERT_FALSE(validateCcuAddress("[fe80::1]:2001"));
    TEST_ASSERT_FALSE(validateCcuAddress("[::1]:8080"));
}

// ==========================================
// Invalid CCU Address Tests - Malformed
// ==========================================

void test_ccu_invalid_empty(void)
{
    // Empty or NULL
    TEST_ASSERT_FALSE(validateCcuAddress(""));
    TEST_ASSERT_FALSE(validateCcuAddress(NULL));
}

void test_ccu_invalid_hostname_malformed(void)
{
    // Malformed hostnames
    TEST_ASSERT_FALSE(validateCcuAddress("-ccu3"));
    TEST_ASSERT_FALSE(validateCcuAddress("ccu3-"));
    TEST_ASSERT_FALSE(validateCcuAddress("ccu..local"));
    TEST_ASSERT_FALSE(validateCcuAddress(".ccu3"));
    TEST_ASSERT_FALSE(validateCcuAddress("ccu3."));
}

void test_ccu_invalid_hostname_special_chars(void)
{
    // Invalid special characters
    TEST_ASSERT_FALSE(validateCcuAddress("ccu@home"));
    TEST_ASSERT_FALSE(validateCcuAddress("ccu#3"));
    TEST_ASSERT_FALSE(validateCcuAddress("ccu_3"));
    TEST_ASSERT_FALSE(validateCcuAddress("ccu 3"));  // space
}

void test_ccu_invalid_ipv4_malformed(void)
{
    // Malformed IPv4
    TEST_ASSERT_FALSE(validateCcuAddress("256.1.1.1"));
    TEST_ASSERT_FALSE(validateCcuAddress("192.168.1"));
    TEST_ASSERT_FALSE(validateCcuAddress("192.168.1.1.1"));
    TEST_ASSERT_FALSE(validateCcuAddress("192..1.1"));
}

void test_ccu_invalid_ipv6_malformed(void)
{
    // Malformed IPv6
    TEST_ASSERT_FALSE(validateCcuAddress("[2001:db8::1"));  // unclosed bracket
    TEST_ASSERT_FALSE(validateCcuAddress("2001:db8::1]"));  // no opening bracket
    TEST_ASSERT_FALSE(validateCcuAddress("[2001:::1]"));    // triple colon
    TEST_ASSERT_FALSE(validateCcuAddress("[2001:db8::1]abc"));  // chars after bracket
}

void test_ccu_invalid_ipv6_bracket_with_port(void)
{
    // IPv6 in brackets with something after (looks like port attempt)
    TEST_ASSERT_FALSE(validateCcuAddress("[2001:db8::1]:"));
    TEST_ASSERT_FALSE(validateCcuAddress("[fe80::1]:80"));
    TEST_ASSERT_FALSE(validateCcuAddress("[::1]:2001"));
}

void test_ccu_invalid_too_long(void)
{
    // String too long (MAX_HOSTNAME_LENGTH = 63)
    char longAddr[80];
    memset(longAddr, 'a', sizeof(longAddr) - 1);
    longAddr[sizeof(longAddr) - 1] = '\0';
    TEST_ASSERT_FALSE(validateCcuAddress(longAddr));
}

void test_ccu_invalid_only_dots(void)
{
    // Only dots
    TEST_ASSERT_FALSE(validateCcuAddress("..."));
    TEST_ASSERT_FALSE(validateCcuAddress("."));
}

void test_ccu_invalid_only_hyphens(void)
{
    // Only hyphens (starts/ends with hyphen)
    TEST_ASSERT_FALSE(validateCcuAddress("---"));
    TEST_ASSERT_FALSE(validateCcuAddress("-"));
}

// ==========================================
// Edge Cases
// ==========================================

void test_ccu_valid_shortest_hostname(void)
{
    // Single character hostname
    TEST_ASSERT_TRUE(validateCcuAddress("a"));
    TEST_ASSERT_TRUE(validateCcuAddress("x"));
}

void test_ccu_valid_ipv6_loopback(void)
{
    // IPv6 loopback in various forms
    TEST_ASSERT_TRUE(validateCcuAddress("::1"));
    TEST_ASSERT_TRUE(validateCcuAddress("[::1]"));
}

void test_ccu_valid_ipv6_unspecified(void)
{
    // IPv6 unspecified address
    TEST_ASSERT_TRUE(validateCcuAddress("::"));
    TEST_ASSERT_TRUE(validateCcuAddress("[::]"));
}

// ==========================================
// Main test runner
// ==========================================

void setup()
{
    delay(2000);  // Wait for ESP32 to stabilize
    UNITY_BEGIN();

    // Valid hostname tests
    RUN_TEST(test_ccu_valid_hostname);
    RUN_TEST(test_ccu_valid_hostname_single_label);
    RUN_TEST(test_ccu_valid_hostname_with_numbers);
    RUN_TEST(test_ccu_valid_hostname_with_hyphens);

    // Valid IPv4 tests
    RUN_TEST(test_ccu_valid_ipv4);
    RUN_TEST(test_ccu_valid_ipv4_private_ranges);
    RUN_TEST(test_ccu_valid_ipv4_localhost);

    // Valid IPv6 tests
    RUN_TEST(test_ccu_valid_ipv6_in_brackets);
    RUN_TEST(test_ccu_valid_ipv6_without_brackets);
    RUN_TEST(test_ccu_valid_ipv6_link_local);
    RUN_TEST(test_ccu_valid_ipv6_full);

    // Invalid - port not allowed
    RUN_TEST(test_ccu_invalid_ipv4_with_port);
    RUN_TEST(test_ccu_invalid_hostname_with_port);
    RUN_TEST(test_ccu_invalid_ipv6_with_port);

    // Invalid - malformed
    RUN_TEST(test_ccu_invalid_empty);
    RUN_TEST(test_ccu_invalid_hostname_malformed);
    RUN_TEST(test_ccu_invalid_hostname_special_chars);
    RUN_TEST(test_ccu_invalid_ipv4_malformed);
    RUN_TEST(test_ccu_invalid_ipv6_malformed);
    RUN_TEST(test_ccu_invalid_ipv6_bracket_with_port);
    RUN_TEST(test_ccu_invalid_too_long);
    RUN_TEST(test_ccu_invalid_only_dots);
    RUN_TEST(test_ccu_invalid_only_hyphens);

    // Edge cases
    RUN_TEST(test_ccu_valid_shortest_hostname);
    RUN_TEST(test_ccu_valid_ipv6_loopback);
    RUN_TEST(test_ccu_valid_ipv6_unspecified);

    UNITY_END();
}

void loop()
{
    // Nothing to do in loop for unit tests
}
