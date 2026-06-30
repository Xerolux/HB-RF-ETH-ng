#include "ota_config.h"

void configure_ota_http_client(esp_http_client_config_t& config, const char* url) {
    config.url = url;

    // NOTE: server certificate verification is intentionally DISABLED on the
    // GitHub fetch / OTA download path.
    //
    // On the ESP32 (WROOM-32, internal RAM only, no PSRAM) the PSA-crypto
    // certificate-chain verification under ESP-IDF 6.x runs out of heap during
    // the TLS handshake and aborts with:
    //     esp-x509-crt-bundle: PSA signature verification failed with error
    //         0xffffff73 (decimal: -141)   // PSA_ERROR_INSUFFICIENT_MEMORY
    //     esp-x509-crt-bundle: Certificate matched but signature verification failed
    // i.e. GitHub's certificate is trusted ("matched") but there is not enough
    // memory to run the signature math, so BOTH the update check and the OTA
    // download fail with ESP_ERR_HTTP_CONNECT.
    //
    // Because no CA bundle is attached here, esp-tls falls back to
    // MBEDTLS_SSL_VERIFY_NONE (enabled via CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY
    // in sdkconfig), so the handshake completes without the memory-hungry
    // verification step.
    //
    // TRADE-OFF: the OTA path no longer authenticates the server, so a
    // man-in-the-middle on the network path could serve arbitrary firmware.
    // This was a deliberate choice to restore update functionality on
    // memory-constrained hardware. MQTT TLS and the log-share upload keep full
    // certificate verification (they do not use this helper).
    //
    // NOTE: skipping verification alone did NOT fix the handshake - it still
    // aborted with PSA_ERROR_INSUFFICIENT_MEMORY (mbedtls_ssl_handshake
    // returned -0x008D / -141) because the handshake's own ECDHE signature
    // verification runs through PSA crypto and needs heap regardless of chain
    // verification. The actual fix is freeing heap during the handshake:
    // CONFIG_MBEDTLS_DYNAMIC_BUFFER (dynamic RX/TX buffers) plus deferring the
    // ~48 KB GitHub-response allocation in updatecheck.cpp until after the
    // handshake completes.
    // Leave skip_cert_common_name_check at its default (false): with no CA
    // attached the connection already skips all verification, and keeping the
    // hostname check enabled means that if a CA bundle is ever re-attached here
    // later, hostname verification is not silently left off.
    config.crt_bundle_attach = nullptr;

    // Fix for Bug #235: GitHub redirects fail with keep-alive
    config.keep_alive_enable = false;

    // Fix for Bug #235: Default TX buffer (512) is too small for large HTTPS headers
    config.buffer_size_tx = 2048;

    // Enable redirection following (defensive programming)
    config.max_redirection_count = 5;
}
