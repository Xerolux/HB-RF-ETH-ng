/*
 *  nextcloud_api.h is part of the HB-RF-ETH firmware v2.0
 *
 *  Copyright 2025 Xerolux
 *  API endpoints for Nextcloud backup configuration
 */

#ifndef NEXTCLOUD_API_H
#define NEXTCLOUD_API_H

#include "esp_http_server.h"

// API handlers
esp_err_t get_nextcloud_handler_func(httpd_req_t *req);
esp_err_t post_nextcloud_handler_func(httpd_req_t *req);
esp_err_t post_nextcloud_test_handler_func(httpd_req_t *req);
esp_err_t post_nextcloud_upload_handler_func(httpd_req_t *req);

// Handler structures
extern httpd_uri_t get_nextcloud_handler;
extern httpd_uri_t post_nextcloud_handler;
extern httpd_uri_t post_nextcloud_test_handler;
extern httpd_uri_t post_nextcloud_upload_handler;

#endif // NEXTCLOUD_API_H
