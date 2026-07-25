/*
 *  monitoring_config.cpp is part of the HB-RF-ETH firmware v2.0
 *
 *  Original work Copyright 2022 Alexander Reinert
 *  https://github.com/alexreinert/HB-RF-ETH
 *
 *  Modified work Copyright 2025 Xerolux
 *  Modernized fork - Updated to ESP-IDF 6.0 and modern toolchains
 *
 *  The HB-RF-ETH firmware is licensed under a
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 */

#include "monitoring.h"

#include <string.h>

void monitoring_config_set_defaults(monitoring_config_t *config)
{
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));

    config->checkmk.port = 6556;
    strncpy(config->checkmk.allowed_hosts, "*",
            sizeof(config->checkmk.allowed_hosts) - 1);

    config->mqtt.port = 1883;
    strncpy(config->mqtt.topic_prefix, "hb-rf-eth-ng",
            sizeof(config->mqtt.topic_prefix) - 1);
    strncpy(config->mqtt.ha_discovery_prefix, "homeassistant",
            sizeof(config->mqtt.ha_discovery_prefix) - 1);
    config->mqtt.command_enabled = true;

    config->prometheus.port = 9100;
    strncpy(config->prometheus.allowed_hosts, "*",
            sizeof(config->prometheus.allowed_hosts) - 1);

    config->syslog.port = 514;
    config->syslog.transport = 0;
    config->syslog.min_severity = 6;

    config->notify.smtp_port = 587;
    config->notify.smtp_tls = 1;
    config->notify.cooldown_seconds = 300;
}

void monitoring_config_normalize(monitoring_config_t *config)
{
    if (config == NULL) {
        return;
    }

    // uint16_t already limits the upper bound; zero is the only invalid port
    // value that can survive a missing/corrupt NVS key.
    if (config->checkmk.port == 0) {
        config->checkmk.port = 6556;
    }
    if (config->mqtt.port == 0) {
        config->mqtt.port = 1883;
    }
    if (config->prometheus.port == 0) {
        config->prometheus.port = 9100;
    }
    if (config->syslog.port == 0) {
        config->syslog.port = 514;
    }
    if (config->notify.smtp_port == 0) {
        config->notify.smtp_port = 587;
    }

    if (config->syslog.transport > 2) {
        config->syslog.transport = 0;
    }
    if (config->syslog.min_severity > 7) {
        config->syslog.min_severity = 6;
    }
    if (config->notify.smtp_tls > 2) {
        config->notify.smtp_tls = 1;
    }
}
