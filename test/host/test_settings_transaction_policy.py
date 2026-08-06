#!/usr/bin/env python3
"""Source-level invariants for fail-safe Settings NVS generations."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
SETTINGS = (ROOT / "main" / "settings.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "include" / "settings.h").read_text(encoding="utf-8")
BUTTON = (ROOT / "main" / "pushbuttonhandler.cpp").read_text(encoding="utf-8")
WEBUI = (ROOT / "main" / "webui.cpp").read_text(encoding="utf-8")
MQTT = (ROOT / "main" / "mqtt_handler.cpp").read_text(encoding="utf-8")


def function_body(source: str, signature: str, next_signature: str) -> str:
    start = source.index(signature)
    end = source.index(next_signature, start)
    return source[start:end]


class SettingsTransactionPolicyTest(unittest.TestCase):
    def test_public_errors_are_propagated(self) -> None:
        self.assertIn("esp_err_t load();", HEADER)
        self.assertIn("esp_err_t save();", HEADER)
        self.assertIn("esp_err_t clear();", HEADER)
        self.assertIn("esp_err_t validateStorageCapacity();", HEADER)
        self.assertIn("esp_err_t saveAdminToken(const char *token);", HEADER)
        self.assertIn("esp_err_t clearAdminToken();", HEADER)

    def test_auth_load_uses_defaults_only_for_missing_keys(self) -> None:
        load = function_body(
            SETTINGS, "esp_err_t Settings::load()", "static esp_err_t add_nvs_string_entries"
        )
        self.assertIn("const esp_err_t initialization_error = err", load)
        init_repair = load[
            load.index("const esp_err_t initialization_error = err") :
            load.index("if (err != ESP_OK)", load.index("const esp_err_t initialization_error = err"))
        ]
        self.assertIn("lockAuthenticationAfterStorageFailureLocked()", init_repair)
        self.assertIn("return repair_error == ESP_OK ? initialization_error", init_repair)

        username = load[
            load.index("size_t adminUsernameLength") :
            load.index("size_t adminPasswordLength")
        ]
        password = load[
            load.index("size_t adminPasswordLength") :
            load.index("int8_t stored_password_changed")
        ]
        changed = load[
            load.index("int8_t stored_password_changed") :
            load.index("size_t hostnameLength")
        ]
        for key_body, key_name in (
            (username, "adminUsername"),
            (password, "adminPassword"),
            (changed, "passwordChanged"),
        ):
            with self.subTest(key=key_name):
                self.assertIn("ESP_ERR_NVS_NOT_FOUND", key_body)
                self.assertTrue(
                    "err != ESP_OK" in key_body
                    or "err != ESP_ERR_NVS_NOT_FOUND" in key_body
                )
                self.assertIn(f'fail_auth_load("{key_name}"', key_body)

        self.assertIn("resetToSafeDefaultsLocked()", load)
        self.assertIn("lockAuthenticationAfterStorageFailureLocked()", load)
        self.assertIn("_storageHealthy = false", load)

    def test_token_io_is_checked_and_failed_load_cannot_reactivate_token(self) -> None:
        load_token = function_body(
            SETTINGS,
            "bool Settings::loadAdminToken",
            "esp_err_t Settings::saveAdminToken",
        )
        self.assertIn("if (!_storageHealthy)", load_token)

        save_token = function_body(
            SETTINGS,
            "esp_err_t Settings::saveAdminToken",
            "esp_err_t Settings::clearAdminToken",
        )
        self.assertLess(save_token.index("nvs_set_str"), save_token.index("nvs_commit"))
        self.assertIn("if (err == ESP_OK) err = nvs_commit(handle)", save_token)
        self.assertIn("return err", save_token)

        clear_token = SETTINGS[SETTINGS.index("esp_err_t Settings::clearAdminToken") :]
        self.assertLess(clear_token.index("nvs_erase_key"), clear_token.index("nvs_commit"))
        self.assertIn("if (err == ESP_OK) err = nvs_commit(handle)", clear_token)
        self.assertIn("return err", clear_token)

    def test_auth_backup_is_durable_before_pending_marker(self) -> None:
        arm = function_body(
            SETTINGS,
            "static esp_err_t write_storage_transaction",
            "static esp_err_t clear_storage_transaction",
        )
        self.assertLess(
            arm.index("nvs_set_blob(handle, SETTINGS_TXN_AUTH_BACKUP_KEY"),
            arm.index("nvs_set_u8(handle, SETTINGS_TXN_PENDING_KEY"),
        )
        backup_source = function_body(
            SETTINGS,
            "static esp_err_t read_auth_backup_source",
            "static bool valid_auth_backup",
        )
        self.assertIn("adminUsername", backup_source)
        self.assertIn("adminPassword", backup_source)
        self.assertIn("adminToken", backup_source)

    def test_capacity_and_marker_precede_destructive_write(self) -> None:
        save = function_body(
            SETTINGS, "esp_err_t Settings::save()", "void Settings::snapshot"
        )
        self.assertLess(
            save.index("validateStorageCapacityLocked(handle)"),
            save.index("write_storage_transaction(STORAGE_SCOPE_SETTINGS"),
        )
        self.assertLess(
            save.index("write_storage_transaction(STORAGE_SCOPE_SETTINGS"),
            save.index("nvs_erase_all(handle)"),
        )
        self.assertIn("stats.available_entries < SETTINGS_TXN_REQUIRED_ENTRIES", SETTINGS)

    def test_auth_backup_cleanup_cannot_reject_committed_generation(self) -> None:
        finish = function_body(
            SETTINGS,
            "static esp_err_t clear_storage_transaction",
            "// Must run before opening/loading",
        )
        self.assertIn("esp_err_t cleanup_err", finish)
        self.assertIn("return err;", finish)
        self.assertNotIn("err = nvs_erase_key(handle, SETTINGS_TXN_AUTH_BACKUP_KEY)", finish)

    def test_same_boot_rollback_only_recovers_settings_scope(self) -> None:
        save = function_body(
            SETTINGS, "esp_err_t Settings::save()", "void Settings::snapshot"
        )
        self.assertIn("pending_scope == STORAGE_SCOPE_SETTINGS", save)
        self.assertIn("pending_scope != 0", save)
        self.assertIn("ESP_ERR_INVALID_STATE", save)

        recovery = function_body(
            SETTINGS,
            "static esp_err_t recover_storage_transaction",
            "#define GET_IP_ADDR",
        )
        self.assertIn("s_restore_transaction_active = false", recovery)

    def test_factory_reset_has_no_auth_restore_and_erases_all_scopes(self) -> None:
        arm = function_body(
            SETTINGS,
            "static esp_err_t arm_factory_reset_transaction",
            "static esp_err_t clear_storage_transaction",
        )
        self.assertIn("STORAGE_SCOPE_FACTORY_RESET", arm)
        marker_write = arm.index("nvs_set_u8")
        marker_commit = arm.index("nvs_commit", marker_write)
        stale_backup_cleanup = arm.index(
            "nvs_erase_key(handle, SETTINGS_TXN_AUTH_BACKUP_KEY)",
            marker_commit,
        )
        self.assertLess(marker_write, marker_commit)
        self.assertLess(marker_commit, stale_backup_cleanup)
        self.assertIn("esp_err_t cleanup_err", arm)
        self.assertIn("return err;", arm)

        recovery = function_body(
            SETTINGS,
            "static esp_err_t recover_factory_reset_transaction",
            "// Must run before opening/loading",
        )
        for namespace in (
            "NVS_NAMESPACE",
            "MONITORING_NVS_NAMESPACE",
            "MONITORING_TXN_NVS_NAMESPACE",
            "THEME_NVS_NAMESPACE",
            "RESET_INFO_NVS_NAMESPACE",
            "UPDATE_CACHE_NVS_NAMESPACE",
            "MQTT_CLEANUP_NVS_NAMESPACE",
        ):
            self.assertIn(f"erase_nvs_namespace({namespace})", recovery)
        # Devices that once stored a supporter-CRL cache must purge the
        # residue on factory reset; the legacy namespace lives on for that.
        self.assertIn(
            "erase_nvs_namespace(LEGACY_SUPPORTER_CRL_NVS_NAMESPACE)", recovery
        )
        self.assertNotIn("restore_auth_backup", recovery)
        self.assertGreater(
            recovery.index("erase_nvs_namespace(SETTINGS_TXN_NVS_NAMESPACE)"),
            recovery.index("erase_nvs_namespace(MQTT_CLEANUP_NVS_NAMESPACE)"),
        )

        ordinary_recovery = function_body(
            SETTINGS,
            "static esp_err_t recover_storage_transaction",
            "#define GET_IP_ADDR",
        )
        self.assertLess(
            ordinary_recovery.index("scope == STORAGE_SCOPE_FACTORY_RESET"),
            ordinary_recovery.index("load_transaction_auth_backup"),
        )
        self.assertIn("restore_auth_backup", ordinary_recovery)

        corrupt_marker = ordinary_recovery[
            ordinary_recovery.index("ESP_ERR_NVS_TYPE_MISMATCH") :
            ordinary_recovery.index("if (err != ESP_OK || scope == 0)")
        ]
        self.assertIn("return recover_factory_reset_transaction()",
                      corrupt_marker)
        self.assertNotIn("scope = STORAGE_SCOPE_RESTORE", corrupt_marker)

        exact_scope_check = (
            "if (scope != STORAGE_SCOPE_SETTINGS && "
            "scope != STORAGE_SCOPE_RESTORE)"
        )
        unknown_marker = ordinary_recovery[
            ordinary_recovery.index(exact_scope_check) :
            ordinary_recovery.index(
                "ESP_LOGW(TAG", ordinary_recovery.index(exact_scope_check)
            )
        ]
        self.assertIn("return recover_factory_reset_transaction()",
                      unknown_marker)
        self.assertNotIn("scope = STORAGE_SCOPE_RESTORE", unknown_marker)
        self.assertNotIn("scope & ~STORAGE_SCOPE_RESTORE", ordinary_recovery)

        clear = function_body(
            SETTINGS, "esp_err_t Settings::clear()", "char *Settings::getAdminPassword"
        )
        self.assertIn("arm_factory_reset_transaction()", clear)
        self.assertIn("recover_factory_reset_transaction()", clear)
        self.assertNotIn("write_storage_transaction", clear)
        self.assertNotIn("recover_storage_transaction", clear)

    def test_every_password_replacement_rotates_token_before_password(self) -> None:
        rotate = function_body(
            WEBUI, "static esp_err_t rotate_admin_token()", "void generateToken()"
        )
        self.assertLess(
            rotate.index("generate_fresh_admin_token"),
            rotate.index("saveAdminToken"),
        )
        self.assertLess(rotate.index("saveAdminToken"), rotate.index("memcpy(_token"))
        self.assertIn("if (result == ESP_OK)", rotate)

        handlers = {
            "physical": function_body(
                WEBUI,
                "esp_err_t post_password_reset_complete_handler_func",
                "httpd_uri_t post_password_reset_complete_handler",
            ),
            "settings": function_body(
                WEBUI,
                "esp_err_t post_settings_json_handler_func",
                "httpd_uri_t post_settings_json_handler",
            ),
            "restore": function_body(
                WEBUI,
                "esp_err_t post_restore_handler_func",
                "httpd_uri_t post_restore_handler",
            ),
            "change": function_body(
                WEBUI,
                "esp_err_t post_change_password_handler_func",
                "httpd_uri_t post_change_password_handler",
            ),
        }
        for name, handler in handlers.items():
            with self.subTest(handler=name):
                rotation = handler.index("rotate_admin_token()")
                password_set = min(
                    index for index in (
                        handler.find("setAdminPassword", rotation),
                        handler.find("restoreAdminPassword", rotation),
                    ) if index >= 0
                )
                self.assertLess(rotation, password_set)
                self.assertIn("if (token_result != ESP_OK)", handler)

        self.assertNotIn("clearAdminToken", WEBUI)

    def test_bulk_password_changes_require_constant_time_reauthentication(self) -> None:
        handlers = {
            "settings": function_body(
                WEBUI,
                "esp_err_t post_settings_json_handler_func",
                "httpd_uri_t post_settings_json_handler",
            ),
            "restore": function_body(
                WEBUI,
                "esp_err_t post_restore_handler_func",
                "httpd_uri_t post_restore_handler",
            ),
        }

        for name, handler in handlers.items():
            with self.subTest(handler=name):
                change_compare = handler.index("secure_strcmp(adminPassword")
                required_error = handler.index('"current_password_required"')
                reauth_compare = handler.index("secure_strcmp(currentPassword")
                incorrect_error = handler.index('"current_password_incorrect"')
                rotation = handler.index("rotate_admin_token()")

                self.assertIn("bool admin_password_change_requested = false",
                              handler)
                self.assertLess(change_compare, required_error)
                self.assertLess(required_error, reauth_compare)
                self.assertLess(reauth_compare, incorrect_error)
                self.assertLess(incorrect_error, rotation)
                self.assertNotIn(
                    "strcmp(currentPassword",
                    handler.replace("secure_strcmp", ""),
                )

                # Both token rotation and the password setter are reachable
                # only through the genuine-change predicate. A backup that
                # echoes the active password therefore remains configuration-
                # only and does not invalidate the current session.
                rotation_guard = handler.rfind(
                    "if (admin_password_change_requested)", 0, rotation
                )
                self.assertGreater(rotation_guard, reauth_compare)
                password_set = min(
                    index for index in (
                        handler.find("setAdminPassword", rotation),
                        handler.find("restoreAdminPassword", rotation),
                    ) if index >= 0
                )
                setter_guard = handler.rfind(
                    "if (admin_password_change_requested)", 0, password_set
                )
                self.assertGreaterEqual(setter_guard, rotation_guard)

    def test_mqtt_cleanup_marker_uses_dedicated_namespace(self) -> None:
        mqtt_cleanup = function_body(
            MQTT,
            "static void publish_legacy_topic_cleanup(void)",
            "void mqtt_handler_publish_ha_discovery(void)",
        )
        self.assertIn('CLEANUP_NS = "mqtt_cleanup"', mqtt_cleanup)
        self.assertIn("marker_result == ESP_ERR_NVS_NOT_FOUND", mqtt_cleanup)
        self.assertIn("Invalid legacy-topic cleanup marker", mqtt_cleanup)
        persist = mqtt_cleanup[mqtt_cleanup.index("// Mark this cleanup version complete") :]
        self.assertLess(persist.index("nvs_set_u8"), persist.index("nvs_commit"))
        self.assertLess(
            persist.index("nvs_commit"),
            persist.index("Legacy retained-topic cleanup performed"),
        )

    def test_boolean_members_are_not_forced_true_on_save(self) -> None:
        self.assertIn(
            'nvs_set_i8(handle, "flashPause", _flashPause ? 1 : 0)', SETTINGS
        )
        self.assertIn(
            'nvs_set_i8(handle, "testDesign", _testDesignEnabled ? 1 : 0)',
            SETTINGS,
        )

    def test_physical_reset_never_reports_false_success(self) -> None:
        self.assertIn("const esp_err_t clear_result = settings->clear();", BUTTON)
        self.assertLess(
            BUTTON.index("if (clear_result != ESP_OK)"),
            BUTTON.index("ResetInfo::storeResetReason(RESET_REASON_FACTORY_RESET)"),
        )
        self.assertIn("LED_STATE_STROBE", BUTTON)


if __name__ == "__main__":
    unittest.main()
