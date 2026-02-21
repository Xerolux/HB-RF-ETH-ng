#include "ota_config.h"
#include "esp_crt_bundle.h"

void configure_ota_http_client(esp_http_client_config_t& config, const char* url) {
    config.url = url;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    // Fix for Bug #235: GitHub redirects fail with keep-alive
    config.keep_alive_enable = false;

    // Fix for Bug #235: Default TX buffer (512) is too small for large HTTPS headers
    config.buffer_size_tx = 4096;

    // Enable redirection following (defensive programming)
    config.max_redirection_count = 5;
}
