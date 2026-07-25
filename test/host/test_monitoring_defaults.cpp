#include <cassert>
#include <cstring>

#include "monitoring.h"

static void test_empty_namespace_uses_complete_defaults()
{
    monitoring_config_t config = {};

    monitoring_config_set_defaults(&config);

    assert(!config.checkmk.enabled);
    assert(config.checkmk.port == 6556);
    assert(std::strcmp(config.checkmk.allowed_hosts, "*") == 0);

    assert(!config.mqtt.enabled);
    assert(config.mqtt.port == 1883);
    assert(std::strcmp(config.mqtt.topic_prefix, "hb-rf-eth-ng") == 0);
    assert(std::strcmp(config.mqtt.ha_discovery_prefix, "homeassistant") == 0);
    assert(config.mqtt.command_enabled);

    assert(config.prometheus.port == 9100);
    assert(std::strcmp(config.prometheus.allowed_hosts, "*") == 0);
    assert(config.syslog.port == 514);
    assert(config.syslog.min_severity == 6);
    assert(config.notify.smtp_port == 587);
    assert(config.notify.smtp_tls == 1);
    assert(config.notify.cooldown_seconds == 300);
}

static void test_partial_namespace_preserves_mqtt_and_repairs_missing_checkmk()
{
    monitoring_config_t config = {};
    monitoring_config_set_defaults(&config);

    // Model the NVS overlay performed by load_config_from_nvs(): only keys
    // present in an old/partial namespace replace their defaults.
    config.mqtt.enabled = true;
    std::strcpy(config.mqtt.server, "broker.example.test");
    config.mqtt.port = 8883;
    std::strcpy(config.mqtt.user, "mqtt-user");
    std::strcpy(config.mqtt.password, "mqtt-password");

    monitoring_config_normalize(&config);

    assert(config.checkmk.port == 6556);
    assert(std::strcmp(config.checkmk.allowed_hosts, "*") == 0);
    assert(config.mqtt.enabled);
    assert(std::strcmp(config.mqtt.server, "broker.example.test") == 0);
    assert(config.mqtt.port == 8883);
    assert(std::strcmp(config.mqtt.user, "mqtt-user") == 0);
    assert(std::strcmp(config.mqtt.password, "mqtt-password") == 0);
}

static void test_invalid_scalar_values_are_repaired_without_erasing_secrets()
{
    monitoring_config_t config = {};
    monitoring_config_set_defaults(&config);

    config.checkmk.port = 0;
    config.mqtt.port = 0;
    config.prometheus.port = 0;
    config.syslog.port = 0;
    config.syslog.transport = 9;
    config.syslog.min_severity = 9;
    config.notify.smtp_port = 0;
    config.notify.smtp_tls = 9;
    std::strcpy(config.mqtt.password, "keep-me");
    std::strcpy(config.mqtt.command_token, "KeepMe123");

    monitoring_config_normalize(&config);

    assert(config.checkmk.port == 6556);
    assert(config.mqtt.port == 1883);
    assert(config.prometheus.port == 9100);
    assert(config.syslog.port == 514);
    assert(config.syslog.transport == 0);
    assert(config.syslog.min_severity == 6);
    assert(config.notify.smtp_port == 587);
    assert(config.notify.smtp_tls == 1);
    assert(std::strcmp(config.mqtt.password, "keep-me") == 0);
    assert(std::strcmp(config.mqtt.command_token, "KeepMe123") == 0);
}

int main()
{
    test_empty_namespace_uses_complete_defaults();
    test_partial_namespace_preserves_mqtt_and_repairs_missing_checkmk();
    test_invalid_scalar_values_are_repaired_without_erasing_secrets();
    return 0;
}
