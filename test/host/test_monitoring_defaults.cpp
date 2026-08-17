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

static void test_event_mask_defaults_to_every_supported_event()
{
    monitoring_config_t config = {};
    monitoring_config_set_defaults(&config);

    // A fresh device must notify about everything it can. Anything less would
    // make a newly configured installation look broken: the user enables
    // notifications, gets the test mail, and then never hears from it again.
    assert(config.notify.event_mask == NOTIFY_EVENT_ALL);
    assert((config.notify.event_mask & NOTIFY_EVENT_ETH_LINK_DOWN) != 0);
    assert((config.notify.event_mask & NOTIFY_EVENT_CCU_DISCONNECTED) != 0);
    assert((config.notify.event_mask & NOTIFY_EVENT_LOW_HEAP) != 0);
}

static void test_event_mask_normalization_drops_unknown_bits_but_keeps_none()
{
    monitoring_config_t config = {};
    monitoring_config_set_defaults(&config);

    // Bits above the defined events can only come from a newer build's
    // backup or a hand-written API call. Drop them rather than persisting
    // state this firmware cannot render.
    config.notify.event_mask = 0xFFFFu;
    monitoring_config_normalize(&config);
    assert(config.notify.event_mask == NOTIFY_EVENT_ALL);

    // A single selected event must survive untouched.
    config.notify.event_mask = NOTIFY_EVENT_CCU_DISCONNECTED;
    monitoring_config_normalize(&config);
    assert(config.notify.event_mask == NOTIFY_EVENT_CCU_DISCONNECTED);

    // "Notify nothing" is a legitimate choice and must not be silently
    // rewritten into "notify everything" — the migration case for an older
    // installation is handled by the NVS loader leaving the default in place,
    // not by normalization second-guessing a stored zero.
    config.notify.event_mask = 0;
    monitoring_config_normalize(&config);
    assert(config.notify.event_mask == 0);
}

int main()
{
    test_empty_namespace_uses_complete_defaults();
    test_partial_namespace_preserves_mqtt_and_repairs_missing_checkmk();
    test_invalid_scalar_values_are_repaired_without_erasing_secrets();
    test_event_mask_defaults_to_every_supported_event();
    test_event_mask_normalization_drops_unknown_bits_but_keeps_none();
    return 0;
}
