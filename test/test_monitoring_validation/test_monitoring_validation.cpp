/*
 *  test_monitoring_validation.cpp
 *
 *  Unit tests for monitoring (MQTT) validation
 *  - validateServerAddress (broker host / IP / port)
 *  - validateMqttCommandToken (Phase A: shared-secret for MQTT commands)
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
// Valid Command Token Tests (Phase A)
// ==========================================

void test_token_valid_alphanumeric(void)
{
    TEST_ASSERT_TRUE(validateMqttCommandToken("MyToken123"));
    TEST_ASSERT_TRUE(validateMqttCommandToken("abcdefgh"));
    TEST_ASSERT_TRUE(validateMqttCommandToken("ABCDEFGH"));
}

void test_token_valid_with_separators(void)
{
    // - _ . allowed in any position
    TEST_ASSERT_TRUE(validateMqttCommandToken("my-token-123"));
    TEST_ASSERT_TRUE(validateMqttCommandToken("my_token_456"));
    TEST_ASSERT_TRUE(validateMqttCommandToken("my.token.789"));
    TEST_ASSERT_TRUE(validateMqttCommandToken("a.b-c_d"));
}

void test_token_valid_boundary_lengths(void)
{
    // Min length is 8, max is 63
    TEST_ASSERT_TRUE(validateMqttCommandToken("12345678"));
    TEST_ASSERT_TRUE(validateMqttCommandToken("1234567-"));  // separator at end is OK
    char t[64];
    memset(t, 'a', 63);
    t[63] = '\0';
    TEST_ASSERT_TRUE(validateMqttCommandToken(t));
}

// ==========================================
// Invalid Command Token Tests (Phase A)
// ==========================================

void test_token_invalid_empty(void)
{
    TEST_ASSERT_FALSE(validateMqttCommandToken(""));
    TEST_ASSERT_FALSE(validateMqttCommandToken(NULL));
}

void test_token_invalid_too_short(void)
{
    // < 8 chars rejected (must resist trivial brute force and accidental
    // single-char tokens like "-")
    TEST_ASSERT_FALSE(validateMqttCommandToken("short"));
    TEST_ASSERT_FALSE(validateMqttCommandToken("1234567"));
    TEST_ASSERT_FALSE(validateMqttCommandToken("-"));
}

void test_token_invalid_too_long(void)
{
    // > 63 chars rejected (NVS key/value size + sane upper bound)
    char t[70];
    memset(t, 'a', 69);
    t[69] = '\0';
    TEST_ASSERT_FALSE(validateMqttCommandToken(t));
}

void test_token_invalid_special_chars(void)
{
    // Anything outside [A-Za-z0-9-_.] must be rejected so the token is
    // unambiguous inside JSON payloads, MQTT topics and plain text.
    TEST_ASSERT_FALSE(validateMqttCommandToken("token!123"));
    TEST_ASSERT_FALSE(validateMqttCommandToken("token with space"));
    TEST_ASSERT_FALSE(validateMqttCommandToken("token\"quote"));
    TEST_ASSERT_FALSE(validateMqttCommandToken("token/slash"));
    TEST_ASSERT_FALSE(validateMqttCommandToken("token#hash"));
    TEST_ASSERT_FALSE(validateMqttCommandToken("token@at"));
    TEST_ASSERT_FALSE(validateMqttCommandToken("token$colon:"));
    TEST_ASSERT_FALSE(validateMqttCommandToken("token+plus"));
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

    // MQTT Command Token Tests (Phase A)
    RUN_TEST(test_token_valid_alphanumeric);
    RUN_TEST(test_token_valid_with_separators);
    RUN_TEST(test_token_valid_boundary_lengths);

    RUN_TEST(test_token_invalid_empty);
    RUN_TEST(test_token_invalid_too_short);
    RUN_TEST(test_token_invalid_too_long);
    RUN_TEST(test_token_invalid_special_chars);

    UNITY_END();
}

void loop()
{
    // Nothing to do in loop for unit tests
}
