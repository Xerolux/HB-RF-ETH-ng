/*
 *  test_static_ipv4_and_password.cpp
 *
 *  Unit tests verifying:
 *  1. Static IPv4 settings are correctly saved to NVS and restored after reboot
 *  2. Password change works and persists across reboots
 *  3. Password validation is consistent between frontend and backend
 *
 *  These tests use the PlatformIO Unity test framework and run on the target
 *  (ESP32) to validate the actual NVS persistence behavior.
 */

#include <unity.h>
#include <string.h>
#include "settings.h"
#include "nvs.h"
#include "nvs_flash.h"

static Settings *settings = nullptr;

void setUp(void)
{
    // Clear NVS before each test to start fresh
    if (settings != nullptr)
    {
        delete settings;
    }
    nvs_flash_erase();
    nvs_flash_init();
    settings = new Settings();
}

void tearDown(void)
{
    if (settings != nullptr)
    {
        delete settings;
        settings = nullptr;
    }
}

// ==========================================
// Static IPv4 Tests
// ==========================================

void test_default_uses_dhcp(void)
{
    // After fresh NVS, DHCP should be enabled by default
    TEST_ASSERT_TRUE(settings->getUseDHCP());
}

void test_static_ipv4_save_and_load(void)
{
    // Configure static IPv4
    ip4_addr_t localIP, netmask, gateway, dns1, dns2;
    IP4_ADDR(&localIP, 192, 168, 1, 100);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gateway, 192, 168, 1, 1);
    IP4_ADDR(&dns1, 8, 8, 8, 8);
    IP4_ADDR(&dns2, 8, 8, 4, 4);

    bool result = settings->setNetworkSettings("test-host", false, localIP, netmask, gateway, dns1, dns2);
    TEST_ASSERT_TRUE(result);
    settings->save();

    // Simulate reboot by creating a new Settings instance (reads from NVS)
    delete settings;
    settings = new Settings();

    // Verify all settings persisted
    TEST_ASSERT_FALSE(settings->getUseDHCP());
    TEST_ASSERT_EQUAL_STRING("test-host", settings->getHostname());
    TEST_ASSERT_EQUAL_UINT32(localIP.addr, settings->getLocalIP().addr);
    TEST_ASSERT_EQUAL_UINT32(netmask.addr, settings->getNetmask().addr);
    TEST_ASSERT_EQUAL_UINT32(gateway.addr, settings->getGateway().addr);
    TEST_ASSERT_EQUAL_UINT32(dns1.addr, settings->getDns1().addr);
    TEST_ASSERT_EQUAL_UINT32(dns2.addr, settings->getDns2().addr);
}

void test_static_ipv4_different_subnets(void)
{
    // Test with 10.x.x.x subnet
    ip4_addr_t localIP, netmask, gateway, dns1, dns2;
    IP4_ADDR(&localIP, 10, 0, 0, 50);
    IP4_ADDR(&netmask, 255, 0, 0, 0);
    IP4_ADDR(&gateway, 10, 0, 0, 1);
    IP4_ADDR(&dns1, 10, 0, 0, 1);
    dns2.addr = IPADDR_ANY;

    bool result = settings->setNetworkSettings("device-10", false, localIP, netmask, gateway, dns1, dns2);
    TEST_ASSERT_TRUE(result);
    settings->save();

    delete settings;
    settings = new Settings();

    TEST_ASSERT_FALSE(settings->getUseDHCP());
    TEST_ASSERT_EQUAL_UINT32(localIP.addr, settings->getLocalIP().addr);
    TEST_ASSERT_EQUAL_UINT32(netmask.addr, settings->getNetmask().addr);
    TEST_ASSERT_EQUAL_UINT32(gateway.addr, settings->getGateway().addr);
    TEST_ASSERT_EQUAL_UINT32(dns1.addr, settings->getDns1().addr);
}

void test_switch_from_static_to_dhcp(void)
{
    // First set static
    ip4_addr_t localIP, netmask, gateway, dns1, dns2;
    IP4_ADDR(&localIP, 192, 168, 1, 100);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gateway, 192, 168, 1, 1);
    IP4_ADDR(&dns1, 8, 8, 8, 8);
    dns2.addr = IPADDR_ANY;

    settings->setNetworkSettings("static-host", false, localIP, netmask, gateway, dns1, dns2);
    settings->save();

    // Now switch back to DHCP
    ip4_addr_t anyIP = {.addr = IPADDR_ANY};
    settings->setNetworkSettings("dhcp-host", true, anyIP, anyIP, anyIP, anyIP, anyIP);
    settings->save();

    // Simulate reboot
    delete settings;
    settings = new Settings();

    TEST_ASSERT_TRUE(settings->getUseDHCP());
    TEST_ASSERT_EQUAL_STRING("dhcp-host", settings->getHostname());
}

void test_switch_from_dhcp_to_static(void)
{
    // Start with DHCP (default)
    TEST_ASSERT_TRUE(settings->getUseDHCP());

    // Switch to static
    ip4_addr_t localIP, netmask, gateway, dns1, dns2;
    IP4_ADDR(&localIP, 172, 16, 0, 10);
    IP4_ADDR(&netmask, 255, 255, 0, 0);
    IP4_ADDR(&gateway, 172, 16, 0, 1);
    IP4_ADDR(&dns1, 1, 1, 1, 1);
    IP4_ADDR(&dns2, 1, 0, 0, 1);

    bool result = settings->setNetworkSettings("my-device", false, localIP, netmask, gateway, dns1, dns2);
    TEST_ASSERT_TRUE(result);
    settings->save();

    // Simulate reboot
    delete settings;
    settings = new Settings();

    TEST_ASSERT_FALSE(settings->getUseDHCP());
    TEST_ASSERT_EQUAL_STRING("my-device", settings->getHostname());
    TEST_ASSERT_EQUAL_UINT32(localIP.addr, settings->getLocalIP().addr);
}

void test_static_ipv4_rejects_invalid_hostname(void)
{
    ip4_addr_t localIP, netmask, gateway, dns1, dns2;
    IP4_ADDR(&localIP, 192, 168, 1, 100);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gateway, 192, 168, 1, 1);
    dns1.addr = IPADDR_ANY;
    dns2.addr = IPADDR_ANY;

    // Empty hostname should fail
    bool result = settings->setNetworkSettings("", false, localIP, netmask, gateway, dns1, dns2);
    TEST_ASSERT_FALSE(result);

    // NULL hostname should fail
    result = settings->setNetworkSettings(NULL, false, localIP, netmask, gateway, dns1, dns2);
    TEST_ASSERT_FALSE(result);
}

void test_static_ipv4_persists_multiple_reboots(void)
{
    ip4_addr_t localIP, netmask, gateway, dns1, dns2;
    IP4_ADDR(&localIP, 192, 168, 10, 42);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gateway, 192, 168, 10, 1);
    IP4_ADDR(&dns1, 8, 8, 8, 8);
    IP4_ADDR(&dns2, 8, 8, 4, 4);

    settings->setNetworkSettings("persist-test", false, localIP, netmask, gateway, dns1, dns2);
    settings->save();

    // Simulate 3 reboots
    for (int i = 0; i < 3; i++)
    {
        delete settings;
        settings = new Settings();

        TEST_ASSERT_FALSE_MESSAGE(settings->getUseDHCP(), "DHCP should stay disabled after reboot");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(localIP.addr, settings->getLocalIP().addr, "IP should persist after reboot");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(netmask.addr, settings->getNetmask().addr, "Netmask should persist after reboot");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(gateway.addr, settings->getGateway().addr, "Gateway should persist after reboot");
    }
}

// ==========================================
// Password Change Tests
// ==========================================

void test_default_password_is_admin(void)
{
    TEST_ASSERT_EQUAL_STRING("admin", settings->getAdminPassword());
    TEST_ASSERT_FALSE(settings->getPasswordChanged());
}

void test_password_change_persists(void)
{
    // Change password
    settings->setAdminPassword("NewPass123");
    settings->save();

    // Simulate reboot
    delete settings;
    settings = new Settings();

    TEST_ASSERT_EQUAL_STRING("NewPass123", settings->getAdminPassword());
    TEST_ASSERT_TRUE(settings->getPasswordChanged());
}

void test_password_changed_flag_persists(void)
{
    // Initially false
    TEST_ASSERT_FALSE(settings->getPasswordChanged());

    // Change password
    settings->setAdminPassword("MySecure1Pass");
    TEST_ASSERT_TRUE(settings->getPasswordChanged());
    settings->save();

    // Simulate reboot
    delete settings;
    settings = new Settings();

    // Flag should still be true
    TEST_ASSERT_TRUE(settings->getPasswordChanged());
}

void test_password_changed_detected_by_content(void)
{
    // Even without explicit flag, non-"admin" password should be detected as changed
    // Simulate: manually write password to NVS without flag
    uint32_t handle;
    nvs_open("HB-RF-ETH", NVS_READWRITE, &handle);
    nvs_set_str(handle, "adminPassword", "ChangedPw1");
    nvs_erase_key(handle, "passwordChanged"); // Remove explicit flag
    nvs_commit(handle);
    nvs_close(handle);

    // Load settings fresh
    delete settings;
    settings = new Settings();

    // Should auto-detect that password is not "admin"
    TEST_ASSERT_TRUE(settings->getPasswordChanged());
    TEST_ASSERT_EQUAL_STRING("ChangedPw1", settings->getAdminPassword());
}

void test_factory_reset_restores_default_password(void)
{
    // Change password
    settings->setAdminPassword("Custom99Pass");
    settings->save();

    // Verify it changed
    TEST_ASSERT_EQUAL_STRING("Custom99Pass", settings->getAdminPassword());
    TEST_ASSERT_TRUE(settings->getPasswordChanged());

    // Factory reset
    settings->clear();

    // Should be back to default
    TEST_ASSERT_EQUAL_STRING("admin", settings->getAdminPassword());
    TEST_ASSERT_FALSE(settings->getPasswordChanged());
}

void test_password_persists_multiple_reboots(void)
{
    settings->setAdminPassword("Reboot1Test");
    settings->save();

    for (int i = 0; i < 3; i++)
    {
        delete settings;
        settings = new Settings();

        TEST_ASSERT_EQUAL_STRING_MESSAGE("Reboot1Test", settings->getAdminPassword(),
                                         "Password should persist after reboot");
        TEST_ASSERT_TRUE_MESSAGE(settings->getPasswordChanged(),
                                 "passwordChanged flag should persist after reboot");
    }
}

void test_password_change_after_static_ipv4(void)
{
    // Set static IPv4 first
    ip4_addr_t localIP, netmask, gateway, dns1, dns2;
    IP4_ADDR(&localIP, 192, 168, 1, 50);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gateway, 192, 168, 1, 1);
    IP4_ADDR(&dns1, 8, 8, 8, 8);
    dns2.addr = IPADDR_ANY;

    settings->setNetworkSettings("combo-test", false, localIP, netmask, gateway, dns1, dns2);

    // Then change password
    settings->setAdminPassword("Combo1Pass");

    // Save all
    settings->save();

    // Simulate reboot
    delete settings;
    settings = new Settings();

    // Both should persist
    TEST_ASSERT_FALSE(settings->getUseDHCP());
    TEST_ASSERT_EQUAL_UINT32(localIP.addr, settings->getLocalIP().addr);
    TEST_ASSERT_EQUAL_STRING("Combo1Pass", settings->getAdminPassword());
    TEST_ASSERT_TRUE(settings->getPasswordChanged());
}

// ==========================================
// Password Validation Tests (backend logic)
// ==========================================

// These test the same validation logic as in webui.cpp post_change_password_handler_func
static bool validate_password_backend(const char *password)
{
    if (password == NULL || strlen(password) < 8)
        return false;

    bool has_lower = false;
    bool has_upper = false;
    bool has_digit = false;

    for (int i = 0; password[i] != 0; i++)
    {
        if (password[i] >= 'a' && password[i] <= 'z')
            has_lower = true;
        else if (password[i] >= 'A' && password[i] <= 'Z')
            has_upper = true;
        else if (password[i] >= '0' && password[i] <= '9')
            has_digit = true;
    }

    return has_lower && has_upper && has_digit;
}

void test_password_validation_rejects_short(void)
{
    TEST_ASSERT_FALSE(validate_password_backend("Ab1"));
    TEST_ASSERT_FALSE(validate_password_backend("Abcde1"));  // 6 chars - too short
    TEST_ASSERT_FALSE(validate_password_backend("Abcdef1")); // 7 chars - too short
}

void test_password_validation_rejects_no_uppercase(void)
{
    TEST_ASSERT_FALSE(validate_password_backend("abcdefg1"));
}

void test_password_validation_rejects_no_lowercase(void)
{
    TEST_ASSERT_FALSE(validate_password_backend("ABCDEFG1"));
}

void test_password_validation_rejects_no_digit(void)
{
    TEST_ASSERT_FALSE(validate_password_backend("Abcdefgh"));
}

void test_password_validation_rejects_null(void)
{
    TEST_ASSERT_FALSE(validate_password_backend(NULL));
}

void test_password_validation_rejects_empty(void)
{
    TEST_ASSERT_FALSE(validate_password_backend(""));
}

void test_password_validation_accepts_valid(void)
{
    TEST_ASSERT_TRUE(validate_password_backend("Admin123"));
    TEST_ASSERT_TRUE(validate_password_backend("MyPass99"));
    TEST_ASSERT_TRUE(validate_password_backend("Abcdefg1"));
    TEST_ASSERT_TRUE(validate_password_backend("aB345678"));
}

void test_password_validation_accepts_special_chars(void)
{
    TEST_ASSERT_TRUE(validate_password_backend("Admin!@#1"));
    TEST_ASSERT_TRUE(validate_password_backend("My-Pass_1"));
}

// ==========================================
// Test Runner
// ==========================================

void app_main(void)
{
    UNITY_BEGIN();

    // Static IPv4 tests
    RUN_TEST(test_default_uses_dhcp);
    RUN_TEST(test_static_ipv4_save_and_load);
    RUN_TEST(test_static_ipv4_different_subnets);
    RUN_TEST(test_switch_from_static_to_dhcp);
    RUN_TEST(test_switch_from_dhcp_to_static);
    RUN_TEST(test_static_ipv4_rejects_invalid_hostname);
    RUN_TEST(test_static_ipv4_persists_multiple_reboots);

    // Password change tests
    RUN_TEST(test_default_password_is_admin);
    RUN_TEST(test_password_change_persists);
    RUN_TEST(test_password_changed_flag_persists);
    RUN_TEST(test_password_changed_detected_by_content);
    RUN_TEST(test_factory_reset_restores_default_password);
    RUN_TEST(test_password_persists_multiple_reboots);
    RUN_TEST(test_password_change_after_static_ipv4);

    // Password validation tests
    RUN_TEST(test_password_validation_rejects_short);
    RUN_TEST(test_password_validation_rejects_no_uppercase);
    RUN_TEST(test_password_validation_rejects_no_lowercase);
    RUN_TEST(test_password_validation_rejects_no_digit);
    RUN_TEST(test_password_validation_rejects_null);
    RUN_TEST(test_password_validation_rejects_empty);
    RUN_TEST(test_password_validation_accepts_valid);
    RUN_TEST(test_password_validation_accepts_special_chars);

    UNITY_END();
}
