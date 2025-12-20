#pragma once

#include <esp_http_server.h>

/**
 * Adds standard security headers to the HTTP response.
 *
 * Headers added:
 * - X-Content-Type-Options: nosniff (Prevents MIME sniffing)
 * - X-Frame-Options: SAMEORIGIN (Prevents Clickjacking)
 * - X-XSS-Protection: 1; mode=block (Legacy XSS protection)
 * - Referrer-Policy: strict-origin-when-cross-origin (Privacy protection)
 */
static inline void add_security_headers(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(req, "X-Frame-Options", "SAMEORIGIN");
    httpd_resp_set_hdr(req, "X-XSS-Protection", "1; mode=block");
    httpd_resp_set_hdr(req, "Referrer-Policy", "strict-origin-when-cross-origin");
}
