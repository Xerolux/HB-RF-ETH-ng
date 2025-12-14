/*
 *  monitoring_api.h is part of the HB-RF-ETH firmware v2.0
 *
 *  Copyright 2025 Xerolux
 *  API endpoints for monitoring configuration
 */

#ifndef MONITORING_API_H
#define MONITORING_API_H

#include "esp_http_server.h"

// API handlers
esp_err_t get_monitoring_handler_func(httpd_req_t *req);
esp_err_t post_monitoring_handler_func(httpd_req_t *req);

// Handler structures
extern httpd_uri_t get_monitoring_handler;
extern httpd_uri_t post_monitoring_handler;

#endif  // MONITORING_API_H
