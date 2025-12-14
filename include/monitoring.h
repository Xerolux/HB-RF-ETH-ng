/*
 *  monitoring.h is part of the HB-RF-ETH firmware v2.0
 *
 *  Copyright 2025 Xerolux
 *  SNMP and CheckMK monitoring support
 */

#ifndef MONITORING_H
#define MONITORING_H

#include <stdint.h>
#include "esp_err.h"

// SNMP Configuration
typedef struct {
    bool enabled;
    char community[64];
    char location[128];
    char contact[128];
    uint16_t port;  // default 161
} snmp_config_t;

// CheckMK Agent Configuration
typedef struct {
    bool enabled;
    uint16_t port;  // default 6556
    char allowed_hosts[256];  // comma-separated list of IPs
} checkmk_config_t;

// Monitoring configuration
typedef struct {
    snmp_config_t snmp;
    checkmk_config_t checkmk;
} monitoring_config_t;

// Initialize monitoring subsystem
esp_err_t monitoring_init(const monitoring_config_t *config);

// Update configuration
esp_err_t monitoring_update_config(const monitoring_config_t *config);

// Get current configuration
esp_err_t monitoring_get_config(monitoring_config_t *config);

// SNMP functions
esp_err_t snmp_start(const snmp_config_t *config);
esp_err_t snmp_stop(void);

// CheckMK Agent functions
esp_err_t checkmk_start(const checkmk_config_t *config);
esp_err_t checkmk_stop(void);

#endif // MONITORING_H
