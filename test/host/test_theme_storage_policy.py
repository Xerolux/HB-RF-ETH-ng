from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]


class ThemeStoragePolicyTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = (ROOT / "main/theme_api.cpp").read_text(encoding="utf-8")

    def function(self, start: str, end: str) -> str:
        return self.source[
            self.source.index(start) : self.source.index(end)
        ]

    def test_theme_is_stored_as_one_versioned_validated_blob(self) -> None:
        self.assertIn("struct ThemeConfigBlob", self.source)
        self.assertIn("THEME_CONFIG_VERSION", self.source)
        self.assertIn("uint32_t checksum", self.source)
        self.assertIn("config.size == sizeof(config)", self.source)
        self.assertIn("config.checksum == config_checksum(config)", self.source)
        self.assertIn("valid_scheme(config.scheme)", self.source)
        self.assertIn("valid_color(config.color)", self.source)
        self.assertIn("config.scheme[sizeof(config.scheme) - 1] == '\\0'", self.source)
        self.assertIn("config.color[sizeof(config.color) - 1] == '\\0'", self.source)

    def test_loader_prefers_blob_and_only_falls_back_when_it_is_absent(self) -> None:
        loader = self.function("void load_theme(", "esp_err_t normalize_erase_result")
        blob_lookup = loader.index("nvs_get_blob(")
        legacy_lookup = loader.index("load_legacy_theme(")
        self.assertLess(blob_lookup, legacy_lookup)
        self.assertIn("if (blob_size_result == ESP_ERR_NVS_NOT_FOUND)", loader)
        self.assertIn("NvsStorageLock storage_lock", loader)

        legacy = self.function("void load_legacy_theme(", "void load_theme(")
        self.assertIn("scheme_result == ESP_OK && color_result == ESP_OK", legacy)

    def test_blob_commit_precedes_best_effort_legacy_cleanup(self) -> None:
        setter = self.function(
            "esp_err_t theme_api_set_config(", "esp_err_t theme_api_register("
        )
        blob_write = setter.index("nvs_set_blob(")
        durable_commit = setter.index("nvs_commit(handle)", blob_write)
        scheme_erase = setter.index("nvs_erase_key(handle", durable_commit)
        color_erase = setter.index("nvs_erase_key(handle", scheme_erase + 1)
        cleanup_commit = setter.index("nvs_commit(handle)", color_erase)

        self.assertLess(blob_write, durable_commit)
        self.assertLess(durable_commit, scheme_erase)
        self.assertLess(scheme_erase, color_erase)
        self.assertLess(color_erase, cleanup_commit)
        self.assertNotIn("nvs_set_str", setter)
        self.assertIn("return result", setter)


if __name__ == "__main__":
    unittest.main()
