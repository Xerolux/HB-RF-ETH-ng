/*
 *  dtls_api.h is part of the HB-RF-ETH firmware v2.1
 *
 *  Copyright 2025 Xerolux
 *  DTLS API Handlers for WebUI
 *  Ported to ESP-IDF 5.x
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
 * Register DTLS API handlers with HTTP server
 *
 * Endpoints:
 * - GET  /api/dtls/status       - Get DTLS configuration status
 * - POST /api/dtls/generate-psk - Generate new PSK
 * - POST /api/dtls/config       - Update DTLS configuration
 * - GET  /api/dtls/stats        - Get encryption statistics
 */
void register_dtls_api_handlers(httpd_handle_t server, Settings *settings, DTLSEncryption *dtls);
