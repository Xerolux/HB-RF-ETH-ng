/*
 *  rate_limiter.h is part of the HB-RF-ETH firmware v2.0
 *
 *  Copyright 2025 Xerolux
 *  Rate limiting for HTTP endpoints
 *
 *  The HB-RF-ETH firmware is licensed under a
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 */

#pragma once

#include <esp_http_server.h>
#include <stdbool.h>

// Rate limiter configuration
#define MAX_LOGIN_ATTEMPTS 5
#define RATE_LIMIT_WINDOW_SECONDS 60
#define MAX_TRACKED_IPS 20

// Rate limiter functions
void rate_limiter_init();
bool rate_limiter_check_login(httpd_req_t *req);
void rate_limiter_reset_ip(httpd_req_t *req);
void rate_limiter_cleanup();
