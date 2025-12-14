/*
 *  mqtt_handler.h is part of the HB-RF-ETH firmware v2.0
 *
 *  Copyright 2025 Xerolux
 *  MQTT support
 */

#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include "monitoring.h"
#include "esp_err.h"

// Initialize MQTT subsystem
esp_err_t mqtt_handler_init(void);

// Start MQTT client
esp_err_t mqtt_handler_start(const mqtt_config_t *config);

// Stop MQTT client
esp_err_t mqtt_handler_stop(void);

// Publish status update
void mqtt_handler_publish_status(void);

#endif // MQTT_HANDLER_H
