#include <unity.h>
#include "ota_config.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_ota_config_defaults(void) {
    esp_http_client_config_t config = {};
    const char* url = "https://example.com/resource.json";

    // Call the helper function
    configure_ota_http_client(config, url);

    // Verify the URL is set
    TEST_ASSERT_EQUAL_STRING(url, config.url);

    // Keep-alive is disabled so redirected HTTPS services close cleanly.
    TEST_ASSERT_FALSE(config.keep_alive_enable);

    // TX buffer must be large enough for request/redirect headers.
    TEST_ASSERT_EQUAL(2048, config.buffer_size_tx);

    // Verify Fix 3: Max redirection count set
    TEST_ASSERT_EQUAL(5, config.max_redirection_count);

    // Outbound HTTPS resources must use authenticated TLS.
    TEST_ASSERT_NOT_NULL(config.crt_bundle_attach);
    TEST_ASSERT_FALSE(config.skip_cert_common_name_check);
}

void setup() {
    // Wait for board to stabilize
    vTaskDelay(pdMS_TO_TICKS(2000));
    UNITY_BEGIN();
    RUN_TEST(test_ota_config_defaults);
    UNITY_END();
}

void loop() {}
