/*
 *  test_ipv6_validation.cpp
 *
 *  Unit tests for IPv6 address validation
 *  Verifies that validateIPv6Address correctly validates various IPv6 formats
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
// Valid IPv6 Address Tests
// ==========================================

void test_ipv6_empty_is_valid(void)
{
    // Empty address is valid (auto/disabled mode)
    TEST_ASSERT_TRUE(validateIPv6Address(""));
    TEST_ASSERT_TRUE(validateIPv6Address(NULL));
}

void test_ipv6_full_address(void)
{
    // Full IPv6 address (no compression)
    TEST_ASSERT_TRUE(validateIPv6Address("2001:0db8:85a3:0000:0000:8a2e:0370:7334"));
    TEST_ASSERT_TRUE(validateIPv6Address("2001:db8:85a3:0:0:8a2e:370:7334"));
}

void test_ipv6_loopback(void)
{
    // Loopback address
    TEST_ASSERT_TRUE(validateIPv6Address("::1"));
}

void test_ipv6_unspecified(void)
{
    // Unspecified address
    TEST_ASSERT_TRUE(validateIPv6Address("::"));
}

void test_ipv6_with_double_colon_start(void)
{
    // Double colon at start
    TEST_ASSERT_TRUE(validateIPv6Address("::8a2e:370:7334"));
    TEST_ASSERT_TRUE(validateIPv6Address("::ffff:192.168.1.1"));
}

void test_ipv6_with_double_colon_middle(void)
{
    // Double colon in middle
    TEST_ASSERT_TRUE(validateIPv6Address("2001:db8::8a2e:370:7334"));
    TEST_ASSERT_TRUE(validateIPv6Address("2001:db8::1"));
}

void test_ipv6_with_double_colon_end(void)
{
    // Double colon at end
    TEST_ASSERT_TRUE(validateIPv6Address("2001:db8:85a3::"));
}

void test_ipv6_link_local(void)
{
    // Link-local address
    TEST_ASSERT_TRUE(validateIPv6Address("fe80::1"));
    TEST_ASSERT_TRUE(validateIPv6Address("fe80::a1b2:c3d4:e5f6:7890"));
}

void test_ipv6_unique_local(void)
{
    // Unique local address
    TEST_ASSERT_TRUE(validateIPv6Address("fd00::1"));
    TEST_ASSERT_TRUE(validateIPv6Address("fc00:1234:5678:9abc:def0:1234:5678:9abc"));
}

void test_ipv6_multicast(void)
{
    // Multicast address
    TEST_ASSERT_TRUE(validateIPv6Address("ff02::1"));
    TEST_ASSERT_TRUE(validateIPv6Address("ff00::"));
}

void test_ipv6_mapped_ipv4(void)
{
    // IPv4-mapped IPv6 addresses
    TEST_ASSERT_TRUE(validateIPv6Address("::ffff:192.168.1.1"));
    TEST_ASSERT_TRUE(validateIPv6Address("::ffff:10.0.0.1"));
    TEST_ASSERT_TRUE(validateIPv6Address("64:ff9b::192.0.2.33"));
}

void test_ipv6_common_dns_servers(void)
{
    // Common public DNS servers
    TEST_ASSERT_TRUE(validateIPv6Address("2001:4860:4860::8888"));  // Google DNS
    TEST_ASSERT_TRUE(validateIPv6Address("2001:4860:4860::8844"));  // Google DNS
    TEST_ASSERT_TRUE(validateIPv6Address("2606:4700:4700::1111"));  // Cloudflare DNS
}

// ==========================================
// Invalid IPv6 Address Tests
// ==========================================

void test_ipv6_too_many_segments(void)
{
    // Too many segments (more than 8)
    TEST_ASSERT_FALSE(validateIPv6Address("1:2:3:4:5:6:7:8:9"));
    TEST_ASSERT_FALSE(validateIPv6Address("2001:db8:85a3:0:0:8a2e:370:7334:extra"));
}

void test_ipv6_segment_too_long(void)
{
    // Segment with more than 4 hex digits
    TEST_ASSERT_FALSE(validateIPv6Address("2001:0db8:85a3:00000:0:8a2e:370:7334"));
    TEST_ASSERT_FALSE(validateIPv6Address("12345::1"));
}

void test_ipv6_multiple_double_colons(void)
{
    // Multiple double colons
    TEST_ASSERT_FALSE(validateIPv6Address("2001::85a3::7334"));
    TEST_ASSERT_FALSE(validateIPv6Address("::1::"));
}

void test_ipv6_invalid_characters(void)
{
    // Invalid characters
    TEST_ASSERT_FALSE(validateIPv6Address("2001:db8:85g3::1"));  // 'g' is not hex
    TEST_ASSERT_FALSE(validateIPv6Address("2001:db8:85a3::z"));  // 'z' is not hex
    TEST_ASSERT_FALSE(validateIPv6Address("2001:db8::!@#$"));
}

void test_ipv6_too_few_segments_without_compression(void)
{
    // Too few segments without ::
    TEST_ASSERT_FALSE(validateIPv6Address("2001:db8:85a3"));
    TEST_ASSERT_FALSE(validateIPv6Address("1:2:3:4:5:6:7"));
}

void test_ipv6_invalid_ipv4_suffix(void)
{
    // Invalid IPv4 suffix
    TEST_ASSERT_FALSE(validateIPv6Address("::ffff:192.168.1"));        // Only 2 dots
    TEST_ASSERT_FALSE(validateIPv6Address("::ffff:192.168.1.1.1"));    // Too many dots
    TEST_ASSERT_FALSE(validateIPv6Address("::ffff:192.168.1.abc"));    // Non-decimal in IPv4
}

void test_ipv6_triple_colon(void)
{
    // Triple colon
    TEST_ASSERT_FALSE(validateIPv6Address("2001:::1"));
}

void test_ipv6_ends_with_single_colon(void)
{
    // Ends with single colon (should be :: or a segment)
    TEST_ASSERT_FALSE(validateIPv6Address("2001:db8:"));
}

void test_ipv6_starts_with_single_colon(void)
{
    // Starts with single colon (should be :: or a segment)
    TEST_ASSERT_FALSE(validateIPv6Address(":1234"));
}

void test_ipv6_too_long(void)
{
    // String too long (max 45 chars for valid IPv6)
    TEST_ASSERT_FALSE(validateIPv6Address("2001:0db8:85a3:0000:0000:8a2e:0370:7334:extra:segments"));
}

void test_ipv6_hex_in_ipv4_suffix(void)
{
    // Hex characters in what should be IPv4 suffix
    TEST_ASSERT_FALSE(validateIPv6Address("::ffff:192.168.1.ff"));
}

// ==========================================
// Main test runner
// ==========================================

void setup()
{
    delay(2000);  // Wait for ESP32 to stabilize
    UNITY_BEGIN();

    // Valid address tests
    RUN_TEST(test_ipv6_empty_is_valid);
    RUN_TEST(test_ipv6_full_address);
    RUN_TEST(test_ipv6_loopback);
    RUN_TEST(test_ipv6_unspecified);
    RUN_TEST(test_ipv6_with_double_colon_start);
    RUN_TEST(test_ipv6_with_double_colon_middle);
    RUN_TEST(test_ipv6_with_double_colon_end);
    RUN_TEST(test_ipv6_link_local);
    RUN_TEST(test_ipv6_unique_local);
    RUN_TEST(test_ipv6_multicast);
    RUN_TEST(test_ipv6_mapped_ipv4);
    RUN_TEST(test_ipv6_common_dns_servers);

    // Invalid address tests
    RUN_TEST(test_ipv6_too_many_segments);
    RUN_TEST(test_ipv6_segment_too_long);
    RUN_TEST(test_ipv6_multiple_double_colons);
    RUN_TEST(test_ipv6_invalid_characters);
    RUN_TEST(test_ipv6_too_few_segments_without_compression);
    RUN_TEST(test_ipv6_invalid_ipv4_suffix);
    RUN_TEST(test_ipv6_triple_colon);
    RUN_TEST(test_ipv6_ends_with_single_colon);
    RUN_TEST(test_ipv6_starts_with_single_colon);
    RUN_TEST(test_ipv6_too_long);
    RUN_TEST(test_ipv6_hex_in_ipv4_suffix);

    UNITY_END();
}

void loop()
{
    // Nothing to do in loop for unit tests
}
