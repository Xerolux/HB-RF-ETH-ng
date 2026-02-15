/*
 *  test_monitoring_validation.cpp
 *
 *  Unit tests for monitoring (MQTT/SNMP) validation
 *  Verifies validateServerAddress and validateSnmpCommunity
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
// Valid Server Address Tests (MQTT)
// ==========================================

void test_mqtt_valid_hostname(void)
{
    // Standard MQTT broker hostnames
    TEST_ASSERT_TRUE(validateServerAddress("mqtt.example.com", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_TRUE(validateServerAddress("broker.hivemq.com", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_TRUE(validateServerAddress("test.mosquitto.org", MAX_SERVER_ADDRESS_LENGTH));
}

void test_mqtt_valid_hostname_with_port(void)
{
    // MQTT with non-standard ports
    TEST_ASSERT_TRUE(validateServerAddress("mqtt.example.com:1883", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_TRUE(validateServerAddress("broker.example.com:8883", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_TRUE(validateServerAddress("localhost:1883", MAX_SERVER_ADDRESS_LENGTH));
}

void test_mqtt_valid_ipv4(void)
{
    // MQTT with IPv4 addresses
    TEST_ASSERT_TRUE(validateServerAddress("192.168.1.100", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_TRUE(validateServerAddress("10.0.0.50", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_TRUE(validateServerAddress("172.16.0.1:1883", MAX_SERVER_ADDRESS_LENGTH));
}

void test_mqtt_valid_ipv6(void)
{
    // MQTT with IPv6 addresses
    TEST_ASSERT_TRUE(validateServerAddress("[2001:db8::1]", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_TRUE(validateServerAddress("[2001:db8::1]:1883", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_TRUE(validateServerAddress("[fe80::1]", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_TRUE(validateServerAddress("2001:db8::1", MAX_SERVER_ADDRESS_LENGTH));
}

void test_mqtt_valid_local(void)
{
    // Local MQTT brokers
    TEST_ASSERT_TRUE(validateServerAddress("localhost", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_TRUE(validateServerAddress("localhost:1883", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_TRUE(validateServerAddress("127.0.0.1", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_TRUE(validateServerAddress("::1", MAX_SERVER_ADDRESS_LENGTH));
}

// ==========================================
// Invalid Server Address Tests (MQTT)
// ==========================================

void test_mqtt_invalid_empty(void)
{
    // Empty or NULL
    TEST_ASSERT_FALSE(validateServerAddress("", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_FALSE(validateServerAddress(NULL, MAX_SERVER_ADDRESS_LENGTH));
}

void test_mqtt_invalid_malformed_hostname(void)
{
    // Malformed hostnames
    TEST_ASSERT_FALSE(validateServerAddress("-mqtt.example.com", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_FALSE(validateServerAddress("mqtt-.example.com", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_FALSE(validateServerAddress("mqtt..example.com", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_FALSE(validateServerAddress(".mqtt.example.com", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_FALSE(validateServerAddress("mqtt.example.com.", MAX_SERVER_ADDRESS_LENGTH));
}

void test_mqtt_invalid_special_chars(void)
{
    // Invalid special characters
    TEST_ASSERT_FALSE(validateServerAddress("mqtt@example.com", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_FALSE(validateServerAddress("mqtt#broker", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_FALSE(validateServerAddress("mqtt broker", MAX_SERVER_ADDRESS_LENGTH)); // space
    TEST_ASSERT_FALSE(validateServerAddress("mqtt_broker.com", MAX_SERVER_ADDRESS_LENGTH));
}

void test_mqtt_invalid_port(void)
{
    // Invalid ports
    TEST_ASSERT_FALSE(validateServerAddress("mqtt.example.com:0", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_FALSE(validateServerAddress("mqtt.example.com:65536", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_FALSE(validateServerAddress("mqtt.example.com:abc", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_FALSE(validateServerAddress("mqtt.example.com:", MAX_SERVER_ADDRESS_LENGTH));
}

void test_mqtt_invalid_ipv4(void)
{
    // Invalid IPv4
    TEST_ASSERT_FALSE(validateServerAddress("256.1.1.1", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_FALSE(validateServerAddress("192.168.1", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_FALSE(validateServerAddress("192.168.1.1.1", MAX_SERVER_ADDRESS_LENGTH));
}

void test_mqtt_invalid_ipv6(void)
{
    // Invalid IPv6
    TEST_ASSERT_FALSE(validateServerAddress("[2001:db8::1", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_FALSE(validateServerAddress("[2001:::1]", MAX_SERVER_ADDRESS_LENGTH));
    TEST_ASSERT_FALSE(validateServerAddress("[2001:db8::1]abc", MAX_SERVER_ADDRESS_LENGTH));
}

void test_mqtt_invalid_too_long(void)
{
    // String too long
    char longServer[150];
    memset(longServer, 'a', sizeof(longServer) - 1);
    longServer[sizeof(longServer) - 1] = '\0';
    TEST_ASSERT_FALSE(validateServerAddress(longServer, MAX_SERVER_ADDRESS_LENGTH));
}

// ==========================================
// Valid SNMP Community Tests
// ==========================================

void test_snmp_valid_simple(void)
{
    // Simple community strings
    TEST_ASSERT_TRUE(validateSnmpCommunity("public"));
    TEST_ASSERT_TRUE(validateSnmpCommunity("private"));
    TEST_ASSERT_TRUE(validateSnmpCommunity("community"));
}

void test_snmp_valid_alphanumeric(void)
{
    // Alphanumeric community strings
    TEST_ASSERT_TRUE(validateSnmpCommunity("public123"));
    TEST_ASSERT_TRUE(validateSnmpCommunity("MyComm456"));
    TEST_ASSERT_TRUE(validateSnmpCommunity("ABC123xyz"));
}

void test_snmp_valid_with_hyphen(void)
{
    // With hyphens in middle
    TEST_ASSERT_TRUE(validateSnmpCommunity("my-community"));
    TEST_ASSERT_TRUE(validateSnmpCommunity("snmp-read-only"));
    TEST_ASSERT_TRUE(validateSnmpCommunity("test-123"));
}

void test_snmp_valid_with_underscore(void)
{
    // With underscores in middle
    TEST_ASSERT_TRUE(validateSnmpCommunity("my_community"));
    TEST_ASSERT_TRUE(validateSnmpCommunity("snmp_read_only"));
    TEST_ASSERT_TRUE(validateSnmpCommunity("test_123"));
}

void test_snmp_valid_mixed(void)
{
    // Mixed valid characters
    TEST_ASSERT_TRUE(validateSnmpCommunity("my-snmp_community123"));
    TEST_ASSERT_TRUE(validateSnmpCommunity("ABC_123-xyz"));
}

// ==========================================
// Invalid SNMP Community Tests
// ==========================================

void test_snmp_invalid_empty(void)
{
    // Empty or NULL
    TEST_ASSERT_FALSE(validateSnmpCommunity(""));
    TEST_ASSERT_FALSE(validateSnmpCommunity(NULL));
}

void test_snmp_invalid_start_with_hyphen(void)
{
    // Starts with hyphen
    TEST_ASSERT_FALSE(validateSnmpCommunity("-community"));
    TEST_ASSERT_FALSE(validateSnmpCommunity("-public"));
}

void test_snmp_invalid_end_with_hyphen(void)
{
    // Ends with hyphen
    TEST_ASSERT_FALSE(validateSnmpCommunity("community-"));
    TEST_ASSERT_FALSE(validateSnmpCommunity("public-"));
}

void test_snmp_invalid_start_with_underscore(void)
{
    // Starts with underscore
    TEST_ASSERT_FALSE(validateSnmpCommunity("_community"));
    TEST_ASSERT_FALSE(validateSnmpCommunity("_public"));
}

void test_snmp_invalid_end_with_underscore(void)
{
    // Ends with underscore
    TEST_ASSERT_FALSE(validateSnmpCommunity("community_"));
    TEST_ASSERT_FALSE(validateSnmpCommunity("public_"));
}

void test_snmp_invalid_special_chars(void)
{
    // Invalid special characters
    TEST_ASSERT_FALSE(validateSnmpCommunity("public@123"));
    TEST_ASSERT_FALSE(validateSnmpCommunity("public#snmp"));
    TEST_ASSERT_FALSE(validateSnmpCommunity("public.snmp"));
    TEST_ASSERT_FALSE(validateSnmpCommunity("public!"));
    TEST_ASSERT_FALSE(validateSnmpCommunity("public$"));
    TEST_ASSERT_FALSE(validateSnmpCommunity("public%"));
}

void test_snmp_invalid_spaces(void)
{
    // Spaces not allowed
    TEST_ASSERT_FALSE(validateSnmpCommunity("public snmp"));
    TEST_ASSERT_FALSE(validateSnmpCommunity("my community"));
    TEST_ASSERT_FALSE(validateSnmpCommunity(" public"));
    TEST_ASSERT_FALSE(validateSnmpCommunity("public "));
}

void test_snmp_invalid_too_long(void)
{
    // String too long (MAX_SNMP_COMMUNITY_LENGTH = 32)
    char longComm[50];
    memset(longComm, 'a', sizeof(longComm) - 1);
    longComm[sizeof(longComm) - 1] = '\0';
    TEST_ASSERT_FALSE(validateSnmpCommunity(longComm));
}

void test_snmp_invalid_only_hyphens(void)
{
    // Only hyphens (starts/ends with hyphen)
    TEST_ASSERT_FALSE(validateSnmpCommunity("-"));
    TEST_ASSERT_FALSE(validateSnmpCommunity("---"));
}

void test_snmp_invalid_only_underscores(void)
{
    // Only underscores (starts/ends with underscore)
    TEST_ASSERT_FALSE(validateSnmpCommunity("_"));
    TEST_ASSERT_FALSE(validateSnmpCommunity("___"));
}

// ==========================================
// Main test runner
// ==========================================

void setup()
{
    delay(2000);  // Wait for ESP32 to stabilize
    UNITY_BEGIN();

    // MQTT Server Address Tests
    RUN_TEST(test_mqtt_valid_hostname);
    RUN_TEST(test_mqtt_valid_hostname_with_port);
    RUN_TEST(test_mqtt_valid_ipv4);
    RUN_TEST(test_mqtt_valid_ipv6);
    RUN_TEST(test_mqtt_valid_local);

    RUN_TEST(test_mqtt_invalid_empty);
    RUN_TEST(test_mqtt_invalid_malformed_hostname);
    RUN_TEST(test_mqtt_invalid_special_chars);
    RUN_TEST(test_mqtt_invalid_port);
    RUN_TEST(test_mqtt_invalid_ipv4);
    RUN_TEST(test_mqtt_invalid_ipv6);
    RUN_TEST(test_mqtt_invalid_too_long);

    // SNMP Community Tests
    RUN_TEST(test_snmp_valid_simple);
    RUN_TEST(test_snmp_valid_alphanumeric);
    RUN_TEST(test_snmp_valid_with_hyphen);
    RUN_TEST(test_snmp_valid_with_underscore);
    RUN_TEST(test_snmp_valid_mixed);

    RUN_TEST(test_snmp_invalid_empty);
    RUN_TEST(test_snmp_invalid_start_with_hyphen);
    RUN_TEST(test_snmp_invalid_end_with_hyphen);
    RUN_TEST(test_snmp_invalid_start_with_underscore);
    RUN_TEST(test_snmp_invalid_end_with_underscore);
    RUN_TEST(test_snmp_invalid_special_chars);
    RUN_TEST(test_snmp_invalid_spaces);
    RUN_TEST(test_snmp_invalid_too_long);
    RUN_TEST(test_snmp_invalid_only_hyphens);
    RUN_TEST(test_snmp_invalid_only_underscores);

    UNITY_END();
}

void loop()
{
    // Nothing to do in loop for unit tests
}
