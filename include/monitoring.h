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
#include "nextcloud_client.h"

// Forward declarations
class SysInfo;
class UpdateCheck;

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
    snmp_config_t snmp;
    checkmk_config_t checkmk;
    mqtt_config_t mqtt;
    nextcloud_config_t nextcloud;
} monitoring_config_t;

// Initialize monitoring subsystem
esp_err_t monitoring_init(const monitoring_config_t *config, SysInfo* sysInfo, UpdateCheck* updateCheck);

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

// MQTT functions
esp_err_t mqtt_start(const mqtt_config_t *config);
esp_err_t mqtt_stop(void);

#endif // MONITORING_H
