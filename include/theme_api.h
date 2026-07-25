#pragma once

#include <stddef.h>

#include "esp_err.h"
#include "esp_http_server.h"

/** Register public theme read and authenticated theme write endpoints. */
esp_err_t theme_api_register(httpd_handle_t server);

/** Read the persisted device-wide theme, falling back to factory defaults. */
esp_err_t theme_api_get_config(char *scheme, size_t scheme_size,
                               char *color, size_t color_size);

/** Validate and persist a device-wide theme (used by API and backup restore). */
esp_err_t theme_api_set_config(const char *scheme, const char *color);
