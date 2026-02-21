#include <unity.h>
#include "ota_config.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void setUp(void) {}
void tearDown(void) {}

void test_ota_config_defaults(void) {
    esp_http_client_config_t config = {};
    const char* url = "https://example.com/fw.bin";

    // Call the helper function
    configure_ota_http_client(config, url);

    // Verify the URL is set
    TEST_ASSERT_EQUAL_STRING(url, config.url);

    // Verify Fix 1: Keep-alive disabled for GitHub redirects
    TEST_ASSERT_FALSE(config.keep_alive_enable);

    // Verify Fix 2: TX buffer increased to 4096
    TEST_ASSERT_EQUAL(4096, config.buffer_size_tx);

    // Verify Fix 3: Max redirection count set
    TEST_ASSERT_EQUAL(5, config.max_redirection_count);

    // Verify CRT bundle is attached
    TEST_ASSERT_NOT_NULL(config.crt_bundle_attach);
}

void setup() {
    // Wait for board to stabilize
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    UNITY_BEGIN();
    RUN_TEST(test_ota_config_defaults);
    UNITY_END();
}

void loop() {}
