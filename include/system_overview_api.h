#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

/** Register the authenticated system-diagnostics endpoint. */
esp_err_t system_overview_api_register(httpd_handle_t server);
