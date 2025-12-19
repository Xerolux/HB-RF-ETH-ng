#pragma once

#include "esp_http_server.h"

// Adds standard security headers to the response
// Call this at the start of every handler
static inline void add_security_headers(httpd_req_t *req) {
    // Prevent MIME-sniffing
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");

    // Prevent clickjacking
    httpd_resp_set_hdr(req, "X-Frame-Options", "DENY");

    // Enable XSS protection in older browsers
    httpd_resp_set_hdr(req, "X-XSS-Protection", "1; mode=block");

    // Content Security Policy
    // default-src 'self': Only allow resources from same origin
    // script-src 'self' 'unsafe-eval': Vue requires unsafe-eval in some modes, 'self' for bundled js
    // style-src 'self' 'unsafe-inline': Vue uses inline styles
    // img-src 'self' data:: Allow images from same origin and data URIs (svgs)
    // connect-src 'self' https://api.github.com ws: wss:: Allow API calls to self, GitHub (updates), and WebSockets
    httpd_resp_set_hdr(req, "Content-Security-Policy",
        "default-src 'self'; "
        "script-src 'self' 'unsafe-eval'; "
        "style-src 'self' 'unsafe-inline'; "
        "img-src 'self' data:; "
        "connect-src 'self' https://api.github.com ws: wss:;"
    );

    // Referrer Policy
    httpd_resp_set_hdr(req, "Referrer-Policy", "strict-origin-when-cross-origin");
}
