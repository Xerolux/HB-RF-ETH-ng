/*
 *  dtls_api.h is part of the HB-RF-ETH firmware v2.1
 *
 *  Copyright 2025 Xerolux
 *  DTLS API Endpoints for WebUI
 *
 *  The HB-RF-ETH firmware is licensed under a
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 *
 *  You should have received a copy of the license along with this
 *  work.  If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
 *
 */

#pragma once

#include "esp_http_server.h"
#include "settings.h"
#include "dtls_encryption.h"

/**
 * DTLS API Endpoints for WebUI
 *
 * Provides REST API for DTLS configuration and PSK management
 */

// GET /api/dtls/status - Get DTLS status and configuration
esp_err_t api_dtls_status_handler(httpd_req_t *req);

// POST /api/dtls/generate-psk - Generate new PSK
esp_err_t api_dtls_generate_psk_handler(httpd_req_t *req);

// POST /api/dtls/config - Update DTLS configuration
esp_err_t api_dtls_config_handler(httpd_req_t *req);

// GET /api/dtls/stats - Get DTLS statistics
esp_err_t api_dtls_stats_handler(httpd_req_t *req);

// Helper function to register all DTLS API endpoints
void register_dtls_api_handlers(httpd_handle_t server, Settings *settings, DTLSEncryption *dtls);
