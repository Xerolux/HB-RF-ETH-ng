/*
 *  monitoring.h is part of the HB-RF-ETH firmware v2.0
 *
 *  Copyright 2025 Xerolux
 *  CheckMK and MQTT monitoring support
 */

#ifndef MONITORING_H
#define MONITORING_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

// Forward declarations
class SysInfo;
class UpdateCheck;

// CheckMK Agent Configuration
typedef struct {
    bool enabled;
    uint16_t port;  // default 6556
    char allowed_hosts[256];  // comma-separated list of IPs
} checkmk_config_t;

// MQTT Configuration
typedef struct {
    bool enabled;
    char server[65];
    uint16_t port;
    char user[33];
    char password[33];
    char topic_prefix[65];
    bool ha_discovery_enabled;
    char ha_discovery_prefix[65];
} mqtt_config_t;

// Monitoring configuration
typedef struct {
    checkmk_config_t checkmk;
    mqtt_config_t mqtt;
} monitoring_config_t;

// Initialize monitoring subsystem
esp_err_t monitoring_init(const monitoring_config_t *config, SysInfo* sysInfo, UpdateCheck* updateCheck);

// Update configuration (synchronous - blocks caller)
esp_err_t monitoring_update_config(const monitoring_config_t *config);

// Schedule configuration update asynchronously (returns immediately, applies in background task)
esp_err_t monitoring_schedule_update_config(const monitoring_config_t *config);

// Get current configuration
esp_err_t monitoring_get_config(monitoring_config_t *config);

// Run a lightweight connectivity/self-test for a configured monitoring target.
// Supported targets: "checkmk", "mqtt"
esp_err_t monitoring_run_diagnostic(const char *target, bool *ok, char *message, size_t message_len);
// CheckMK Agent functions
esp_err_t checkmk_start(const checkmk_config_t *config);
esp_err_t checkmk_stop(void);

#endif // MONITORING_H
