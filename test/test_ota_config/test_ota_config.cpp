#include <unity.h>
#include "ota_config.h"
#include "updatecheck.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

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

    // Cert verification is intentionally disabled on the OTA/fetch path to work
    // around PSA insufficient-memory (-141) handshake failures on the ESP32, so
    // no CA bundle must be attached and the CN check is skipped.
    TEST_ASSERT_NULL(config.crt_bundle_attach);
    TEST_ASSERT_TRUE(config.skip_cert_common_name_check);
}

// GitHub Releases API URL: stable channel hits /releases/latest, beta
// channel hits /releases?per_page=3 — fetches enough releases to handle
// API ordering quirks while staying within the 32 KB response cap.
void test_build_releases_api_url_stable(void) {
    char buf[128];
    buildReleasesApiUrl(false, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(
        "https://api.github.com/repos/Xerolux/HB-RF-ETH-ng/releases/latest",
        buf);
}

void test_build_releases_api_url_beta(void) {
    char buf[128];
    buildReleasesApiUrl(true, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(
        "https://api.github.com/repos/Xerolux/HB-RF-ETH-ng/releases?per_page=3",
        buf);
}

// Output buffer too small must yield a safe truncated null-terminated string.
void test_build_releases_api_url_truncation(void) {
    char buf[16];
    buildReleasesApiUrl(false, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(16, (int)strlen(buf) + 1);  // filled to the brim
    TEST_ASSERT_EQUAL('\0', buf[15]);
}

// tag_name normalization: leading v/V is stripped only when followed by a
// digit, so e.g. "vendor" stays intact (defensive).
void test_normalize_tag_strips_v_prefix(void) {
    char out[32];

    normalizeTag("v2.1.11", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("2.1.11", out);

    normalizeTag("V2.2.0-beta.1", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("2.2.0-beta.1", out);

    // No v-prefix - already normalized.
    normalizeTag("3.0.0", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("3.0.0", out);

    // "vendor" starts with 'v' but is not followed by a digit; we leave
    // such strings untouched rather than producing "endor".
    normalizeTag("vendor", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("vendor", out);
}

void test_normalize_tag_handles_null_and_empty(void) {
    char out[8];

    normalizeTag(NULL, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("", out);

    normalizeTag("", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("", out);
}

void setup() {
    // Wait for board to stabilize
    vTaskDelay(pdMS_TO_TICKS(2000));
    UNITY_BEGIN();
    RUN_TEST(test_ota_config_defaults);
    RUN_TEST(test_build_releases_api_url_stable);
    RUN_TEST(test_build_releases_api_url_beta);
    RUN_TEST(test_build_releases_api_url_truncation);
    RUN_TEST(test_normalize_tag_strips_v_prefix);
    RUN_TEST(test_normalize_tag_handles_null_and_empty);
    UNITY_END();
}

void loop() {}
