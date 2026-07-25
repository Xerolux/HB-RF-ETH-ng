#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Ping the target host once.
// target: IP address or hostname
// timeout_ms: ping timeout in milliseconds
// returns: latency in ms, or one of the negative PING_SERVICE_* error codes.
#define PING_SERVICE_TIMEOUT   -1
#define PING_SERVICE_DNS_ERROR -2
#define PING_SERVICE_INTERNAL  -3
int ping_service_ping(const char* target, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
