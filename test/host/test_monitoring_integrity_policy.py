#!/usr/bin/env python3
"""Source-level invariants for monitoring config integrity and async replies."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
MONITORING = (ROOT / "main" / "monitoring.cpp").read_text(encoding="utf-8")
API = (ROOT / "main" / "monitoring_api.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "include" / "monitoring.h").read_text(encoding="utf-8")


def section(source: str, start: str, end: str) -> str:
    begin = source.index(start)
    return source[begin : source.index(end, begin)]


class MonitoringIntegrityPolicyTest(unittest.TestCase):
    def test_only_missing_security_keys_keep_legacy_defaults(self) -> None:
        for signature, end in (
            ("static esp_err_t load_optional_integrity_bool", "static esp_err_t load_optional_integrity_u8"),
            ("static esp_err_t load_optional_integrity_u8", "static esp_err_t load_optional_integrity_u16"),
            ("static esp_err_t load_optional_integrity_u16", "static esp_err_t load_optional_integrity_text"),
        ):
            with self.subTest(helper=signature):
                helper = section(MONITORING, signature, end)
                missing = helper.index("err == ESP_ERR_NVS_NOT_FOUND")
                failure = helper.index("err != ESP_OK")
                self.assertLess(missing, failure)
                self.assertIn("return err;", helper)

        text_helper = section(
            MONITORING,
            "static esp_err_t load_optional_integrity_text",
            "static esp_err_t log_integrity_read_error",
        )
        self.assertIn(
            "err == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : err", text_helper
        )

    def test_all_security_domains_use_integrity_reads(self) -> None:
        strict = section(
            MONITORING,
            "static esp_err_t load_integrity_sensitive_config",
            "// Load configuration from NVS",
        )
        required_keys = (
            # Unauthenticated listener activation and allowlists.
            "NVS_CHECKMK_ENABLED",
            "NVS_CHECKMK_PORT",
            "NVS_CHECKMK_HOSTS",
            "NVS_PROM_ENABLED",
            "NVS_PROM_PORT",
            "NVS_PROM_HOSTS",
            # MQTT activation, credentials, TLS, commands and HA discovery.
            "NVS_MQTT_ENABLED",
            "NVS_MQTT_USER",
            "NVS_MQTT_PASS",
            "NVS_MQTT_HA_ENABLED",
            "NVS_MQTT_HA_PREFIX",
            "NVS_MQTT_TLS_EN",
            "NVS_MQTT_TLS_SKIP",
            "NVS_MQTT_TLS_CA",
            "NVS_MQTT_TLS_CRT",
            "NVS_MQTT_TLS_KEY",
            "NVS_MQTT_CMD_EN",
            "NVS_MQTT_CMD_TOK",
            # Syslog activation, endpoint and transport security.
            "NVS_SYSLOG_ENABLED",
            "NVS_SYSLOG_SERVER",
            "NVS_SYSLOG_PORT",
            "NVS_SYSLOG_XPORT",
            # Notification routing and channel secrets/TLS.
            "NVS_NOTIFY_ENABLED",
            "NVS_NOTIFY_CHANS",
            "NVS_NOTIFY_WSECRET",
            "NVS_NOTIFY_TGTOKEN",
            "NVS_NOTIFY_SMTPTLS",
            "NVS_NOTIFY_SMTPUSER",
            "NVS_NOTIFY_SMTPPW",
        )
        for key in required_keys:
            with self.subTest(key=key):
                self.assertIn(f"LOAD_INTEGRITY({key}", strict)

        self.assertIn(
            "if (err != ESP_OK) return log_integrity_read_error", strict
        )

        loader = section(
            MONITORING,
            "static esp_err_t load_config_from_nvs",
            "// Low-heap watchdog",
        )
        self.assertNotIn("(void)load_config_text", loader)
        self.assertNotIn("NVS_PROM_HOSTS,", loader[loader.index("monitoring_config_normalize") - 500 :])

    def test_one_atomic_state_serializes_updates_and_exclusive_operations(self) -> None:
        gate = section(
            MONITORING,
            "enum class OperationState",
            "bool monitoring_ota_pause_active",
        )
        self.assertIn("std::atomic<uint32_t> g_operation_state", gate)
        self.assertEqual(gate.count("compare_exchange_strong"), 2)
        self.assertIn("operation_try_begin(OperationState::EXCLUSIVE)", gate)
        self.assertIn("operation_active(OperationState::MONITORING_UPDATE)", gate)
        self.assertNotIn("std::atomic<bool>", gate)

        scheduler = section(
            MONITORING,
            "esp_err_t monitoring_schedule_update_config",
            "// Get current configuration",
        )
        self.assertIn(
            "operation_try_begin(OperationState::MONITORING_UPDATE)", scheduler
        )
        self.assertNotIn("ota_operation_active()", scheduler)

    def test_loader_propagates_open_and_security_read_failures(self) -> None:
        loader = section(
            MONITORING,
            "static esp_err_t load_config_from_nvs",
            "// Low-heap watchdog",
        )
        self.assertIn("if (err == ESP_ERR_NVS_NOT_FOUND)", loader)
        self.assertIn("if (err != ESP_OK)", loader)
        self.assertIn("Could not open monitoring NVS", loader)
        self.assertIn("return err;", loader)

        strict_call = loader.index("load_integrity_sensitive_config")
        close_after_strict = loader.index("nvs_close(nvs_handle)", strict_call)
        return_after_strict = loader.index("return err;", close_after_strict)
        self.assertLess(strict_call, close_after_strict)
        self.assertLess(close_after_strict, return_after_strict)

        init = section(
            MONITORING,
            "esp_err_t monitoring_init",
            "struct bounded_config_snapshot_t",
        )
        self.assertLess(
            init.index("load_config_from_nvs"), init.index("mqtt_handler_init")
        )
        self.assertLess(
            init.index("load_config_from_nvs"), init.index("checkmk_start")
        )
        self.assertLess(
            init.index("load_config_from_nvs"), init.index("syslog_start")
        )
        self.assertLess(
            init.index("load_config_from_nvs"), init.index("events_start")
        )
        self.assertLess(
            init.index("load_config_from_nvs"),
            init.index("start_mqtt_when_network_ready"),
        )

    def test_http_success_is_sent_only_from_update_completion(self) -> None:
        self.assertIn("monitoring_update_completion_t", HEADER)

        response = section(
            API,
            "static void send_monitoring_update_result",
            "static void monitoring_update_request_complete",
        )
        self.assertIn('"{\\"success\\":true}"', response)
        self.assertIn("update_result == ESP_OK", response)
        self.assertIn("ESP_ERR_NVS_NOT_ENOUGH_SPACE", response)
        self.assertIn('"507 Insufficient Storage"', response)
        self.assertIn("ESP_ERR_INVALID_STATE", response)
        self.assertIn('"503 Service Unavailable"', response)
        self.assertIn('"500 Internal Server Error"', response)
        self.assertEqual(API.count('"{\\"success\\":true}"'), 1)

        completion = section(
            API,
            "static void monitoring_update_request_complete",
            "// POST /api/monitoring",
        )
        self.assertLess(
            completion.index("send_monitoring_update_result"),
            completion.index("httpd_req_async_handler_complete"),
        )

        post = section(
            API,
            "esp_err_t post_monitoring_handler_func",
            "httpd_uri_t get_monitoring_handler",
        )
        self.assertLess(
            post.index("httpd_req_async_handler_begin"),
            post.index("monitoring_schedule_update_config"),
        )
        self.assertNotIn('"{\\"success\\":true}"', post)
        self.assertIn(
            "monitoring_update_request_complete(schedule_err, response)", post
        )

        worker = section(
            MONITORING,
            "static void apply_config_task",
            "// Schedule configuration update asynchronously",
        )
        self.assertLess(
            worker.index("monitoring_update_config(&job->config)"),
            worker.index("completion(update_result, completion_context)"),
        )
        self.assertLess(
            worker.index("completion(update_result, completion_context)"),
            worker.index("operation_finish(OperationState::MONITORING_UPDATE)"),
        )

    def test_notification_diagnostic_uses_locked_snapshot(self) -> None:
        diagnostic = section(
            MONITORING,
            "esp_err_t monitoring_run_diagnostic",
            "snprintf(code, code_len, \"monitoring.diag.unsupported\")",
        )
        unlock = diagnostic.index("xSemaphoreGive(config_mutex)")
        self.assertLess(
            diagnostic.index("notify_enabled = current_config.notify.enabled"),
            unlock,
        )
        self.assertLess(
            diagnostic.index("notify_channels = current_config.notify.channels"),
            unlock,
        )
        notify_branch = diagnostic[diagnostic.index('strcmp(target, "notify")') :]
        self.assertNotIn("current_config.notify", notify_branch)
        self.assertIn("if (!notify_enabled)", notify_branch)
        self.assertIn("notify_channels", notify_branch)


if __name__ == "__main__":
    unittest.main()
