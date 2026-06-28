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

// Publish status update (status sensors + LWT "online" marker)
void mqtt_handler_publish_status(void);

// Publish a one-shot event payload under <prefix>/<subtopic> (non-retained).
// Used for transient events like OTA started / finished / command rejected.
void mqtt_handler_publish_event(const char *subtopic, const char *payload);

// Force a single immediate status publish cycle (e.g. after a setting changed
// or after OTA state changed). Safe to call from any task; returns immediately
// and schedules the publish on the periodic task if it is running.
void mqtt_handler_trigger_status_publish(void);

#endif // MQTT_HANDLER_H
