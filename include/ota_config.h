#pragma once

#include "esp_http_client.h"

/**
 * @brief Configure HTTP client for OTA updates with safe defaults.
 *
 * Applies fixes for GitHub redirects (disables keep-alive), sets proper
 * TX buffer size, and enables redirection following.
 *
 * @param config Reference to the configuration struct to modify.
 * @param url The URL for the OTA update.
 */
void configure_ota_http_client(esp_http_client_config_t& config, const char* url);
