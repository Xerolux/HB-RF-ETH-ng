#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

/** Register public theme read and authenticated theme write endpoints. */
esp_err_t theme_api_register(httpd_handle_t server);
