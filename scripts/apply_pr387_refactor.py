from pathlib import Path
import re


def read(path: str) -> str:
    return Path(path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8")


def replace_once(text: str, old: str, new: str, path: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one occurrence, found {count}: {old[:80]!r}")
    return text.replace(old, new, 1)


def remove_function(text: str, signature: str, path: str) -> str:
    start = text.find(signature)
    if start < 0:
        return text
    brace = text.find("{", start)
    if brace < 0:
        raise RuntimeError(f"{path}: function body not found for {signature}")
    depth = 0
    in_string = False
    in_char = False
    escaped = False
    index = brace
    while index < len(text):
        char = text[index]
        if escaped:
            escaped = False
        elif char == "\\" and (in_string or in_char):
            escaped = True
        elif char == '"' and not in_char:
            in_string = not in_string
        elif char == "'" and not in_string:
            in_char = not in_char
        elif not in_string and not in_char:
            if char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
                if depth == 0:
                    end = index + 1
                    while end < len(text) and text[end] in " \t\r\n":
                        end += 1
                    return text[:start] + text[end:]
        index += 1
    raise RuntimeError(f"{path}: unbalanced function {signature}")


# ---------------------------------------------------------------------------
# Settings: New Design and restart sync are permanent defaults.
# ---------------------------------------------------------------------------
path = "webui/src/settings.vue"
text = read(path)
start = text.find('              <div class="experimental-warning">')
end_marker = '            </div>\n          </div>\n        </div>\n      </Transition>'
if start >= 0:
    end = text.find(end_marker, start)
    if end < 0:
        raise RuntimeError(f"{path}: experimental section end not found")
    replacement = '''              <div class="experimental-empty-state">
                <AppIcon name="check" />
                <div>
                  <strong>{{ t('settings.experimentalEmptyTitle') }}</strong>
                  <p>{{ t('settings.experimentalEmptyText') }}</p>
                </div>
              </div>
'''
    text = text[:start] + replacement + text[end:]
text = text.replace(
    "import { useExperimentalStore, useSettingsStore, useLoginStore, useUiStore, useSysInfoStore, useRestartUiStore } from './stores.js'",
    "import { useSettingsStore, useLoginStore, useUiStore, useSysInfoStore, useRestartUiStore } from './stores.js'",
)
text = text.replace("const experimentalStore = useExperimentalStore()\n", "")
text = text.replace("const flashPause = ref(false)\n", "")
text = re.sub(r"\n\s*flashPause:\s*flashPause\.value,?", "", text)
text = text.replace("  flashPause.value = settingsStore.flashPause\n", "")
text = text.replace('v-if="settingsStore.flashPause"', 'v-if="true"')
text = text.replace("includeFlashPause: settingsStore.flashPause", "includeFlashPause: true")
write(path, text)

path = "webui/src/stores.js"
text = read(path)
text = re.sub(
    r"export const useExperimentalStore = defineStore\('experimental', \{.*?\n\}\)\n\nexport const useLoginStore",
    """export const useExperimentalStore = defineStore('experimental', {
  state: () => ({ testDesignEnabled: true }),
  actions: {
    applyDesignClass() { document.body.classList.add('newdesign-active') },
    setTestDesignEnabled() { this.testDesignEnabled = true; this.applyDesignClass() },
    syncFromServer() { this.testDesignEnabled = true; this.applyDesignClass() },
    init() { this.applyDesignClass() }
  }
})

export const useLoginStore""",
    text,
    count=1,
    flags=re.S,
)
write(path, text)

path = "main/settings.cpp"
text = read(path)
text = text.replace(
    '  GET_BOOL(handle, "flashPause", _flashPause, false);\n  GET_BOOL(handle, "testDesign", _testDesignEnabled, false);',
    '  // Fixed defaults for all devices; legacy false values are ignored.\n  _flashPause = true;\n  _testDesignEnabled = true;',
)
text = text.replace(
    '  SET_BOOL(handle, "flashPause", _flashPause);\n  SET_BOOL(handle, "testDesign", _testDesignEnabled);',
    '  SET_BOOL(handle, "flashPause", true);\n  SET_BOOL(handle, "testDesign", true);',
)
text = re.sub(
    r"bool Settings::getFlashPause\(\)\s*\{.*?\n\}",
    "bool Settings::getFlashPause()\n{\n  return true;\n}",
    text,
    count=1,
    flags=re.S,
)
text = re.sub(
    r"void Settings::setFlashPause\(bool enabled\)\s*\{.*?\n\}",
    "void Settings::setFlashPause(bool enabled)\n{\n  (void)enabled;\n  _flashPause = true;\n}",
    text,
    count=1,
    flags=re.S,
)
text = re.sub(
    r"bool Settings::getTestDesignEnabled\(\)\s*\{.*?\n\}",
    "bool Settings::getTestDesignEnabled()\n{\n  return true;\n}",
    text,
    count=1,
    flags=re.S,
)
text = re.sub(
    r"void Settings::setTestDesignEnabled\(bool enabled\)\s*\{.*?\n\}",
    "void Settings::setTestDesignEnabled(bool enabled)\n{\n  (void)enabled;\n  _testDesignEnabled = true;\n}",
    text,
    count=1,
    flags=re.S,
)
write(path, text)

path = "webui/src/locales/de.js"
text = read(path)
anchor = "    experimentalWarningText: 'Diese Funktionen sind zum Testen gedacht. Es gibt keine Garantie auf Funktion oder Darstellung.',"
if "experimentalEmptyTitle" not in text:
    text = replace_once(
        text,
        anchor,
        anchor + "\n    experimentalEmptyTitle: 'Derzeit nichts vorhanden',\n    experimentalEmptyText: 'Aktuell sind keine experimentellen Funktionen verfügbar. Das New Design und der Neustart-Sync sind feste Standards für alle Geräte.',",
        path,
    )
write(path, text)


# ---------------------------------------------------------------------------
# Independent WebUI version in firmware and browser.
# ---------------------------------------------------------------------------
path = "main/CMakeLists.txt"
text = read(path)
archive_block = '''# Embedded firmware release archive. Regenerate with scripts/update_archive.py
# whenever archive.json changes (release process). Served from flash by
# /api/firmware_archive so the WebUI never has to fetch it live from GitHub.
target_add_binary_data(${COMPONENT_TARGET} "generated/archive.json.gz" BINARY)

'''
text = text.replace(archive_block, "")
text = text.replace("                            esp_https_ota\n", "")
metadata = '''
# Keep the WebUI version independent from the firmware version. Both the
# embedded fallback and a separately installed WWW image use package.json.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../webui/package.json" WEBUI_PACKAGE_JSON)
string(JSON HB_WEBUI_VERSION GET "${WEBUI_PACKAGE_JSON}" version)
target_compile_definitions(${COMPONENT_LIB} PRIVATE HB_WEBUI_VERSION=\\"${HB_WEBUI_VERSION}\\")

'''
if "string(JSON HB_WEBUI_VERSION" not in text:
    text = text.replace("\n# Transition firmware:", metadata + "# Transition firmware:")
text = text.replace("minimal Brotli-compressed New Design shell", "gzip-compressed New Design shell")
write(path, text)

path = "include/webui_storage.h"
text = read(path)
anchor = "/** True only when the partition is mounted and the New Design manifest is valid. */\nbool webui_storage_is_valid();\n"
if "webui_storage_get_effective_version" not in text:
    text = replace_once(
        text,
        anchor,
        anchor + "\n/** Copy the active WebUI version (external image or embedded fallback). */\nvoid webui_storage_get_effective_version(char *output, size_t outputSize);\n",
        path,
    )
write(path, text)

path = "main/webui_storage.cpp"
text = read(path)
if "#ifndef HB_WEBUI_VERSION" not in text:
    text = text.replace(
        'extern esp_err_t validate_auth(httpd_req_t *req);',
        'extern esp_err_t validate_auth(httpd_req_t *req);\n\n#ifndef HB_WEBUI_VERSION\n#define HB_WEBUI_VERSION "unknown"\n#endif',
    )
status_marker = '''WebUIStorageStatus webui_storage_get_status()
{
    StorageLock lock;
    if (!lock)
    {
        WebUIStorageStatus failed = {};
        copy_safe(failed.lastError, sizeof(failed.lastError),
                  "storage mutex unavailable");
        return failed;
    }
    return s_status;
}
'''
if "void webui_storage_get_effective_version" not in text:
    text = replace_once(
        text,
        status_marker,
        status_marker + '''
void webui_storage_get_effective_version(char *output, size_t outputSize)
{
    if (!output || outputSize == 0) return;
    StorageLock lock;
    if (!lock)
    {
        copy_safe(output, outputSize, HB_WEBUI_VERSION);
        return;
    }
    copy_safe(output, outputSize,
              s_status.valid && s_status.version[0]
                  ? s_status.version
                  : HB_WEBUI_VERSION);
}
''',
        path,
    )
text = text.replace(
    '    char safe_version[sizeof(status.version)] = {};\n    char safe_error[sizeof(status.lastError)] = {};\n    copy_json_safe(safe_version, sizeof(safe_version), status.version);',
    '    char safe_version[sizeof(status.version)] = {};\n    char effective_version[sizeof(status.version)] = {};\n    char safe_error[sizeof(status.lastError)] = {};\n    copy_json_safe(safe_version, sizeof(safe_version), status.version);\n    webui_storage_get_effective_version(effective_version, sizeof(effective_version));',
)
text = text.replace(
    '\"usedBytes\":%u,\"bytesWritten\":%u,\"version\":\"%s\",\n             \"design\":\"newdesign\",\"lastError\":\"%s\"}',
    '\"usedBytes\":%u,\"bytesWritten\":%u,\"version\":\"%s\",\n             \"effectiveVersion\":\"%s\",\"design\":\"newdesign\",\n             \"lastError\":\"%s\"}',
)
text = text.replace(
    '              safe_version,\n              safe_error);',
    '              safe_version,\n              effective_version,\n              safe_error);',
)
text = text.replace(
    'set_error_locked("WWW image size does not match SPIFFS partition");',
    'set_error_locked("Falsche WebUI-Datei: Erwartet wird ein 327680-Byte-WWW-Image (webui_*.bin oder spiffs.bin)");',
)
write(path, text)

path = "main/system_overview_api.cpp"
text = read(path)
if "webui_effective_version" not in text:
    text = text.replace(
        '    const WebUIStorageStatus webui = webui_storage_get_status();',
        '    const WebUIStorageStatus webui = webui_storage_get_status();\n    char webui_effective_version[32] = {};\n    webui_storage_get_effective_version(webui_effective_version, sizeof(webui_effective_version));',
    )
text = text.replace(
    '        cJSON_AddStringToObject(webui_object, "version",\n                                webui.version[0] ? webui.version : "embedded");',
    '        cJSON_AddStringToObject(webui_object, "version",\n                                webui_effective_version);',
)
write(path, text)

path = "webui/src/app.vue"
text = read(path)
text = replace_once(
    text,
    "          <small class=\"text-muted\">{{ t('app.footerCopyright', { version: sysInfoStore.currentVersion ? 'v' + sysInfoStore.currentVersion : '' }) }}</small>",
    "          <small class=\"text-muted\">Firmware v{{ sysInfoStore.currentVersion || '—' }} · WebUI v{{ webUiVersion }} © 2025-2026 Xerolux</small>",
    path,
)
if "import axios from 'axios'" not in text:
    text = text.replace("import { useI18n } from 'vue-i18n'", "import { useI18n } from 'vue-i18n'\nimport axios from 'axios'")
if "const webUiVersion = ref" not in text:
    text = text.replace(
        "const otaUpdateVersion = ref('')",
        "const otaUpdateVersion = ref('')\nconst webUiVersion = ref(typeof __WEBUI_VERSION__ !== 'undefined' ? __WEBUI_VERSION__ : 'unbekannt')",
    )
mount_anchor = '''  sysInfoStore.update().catch((error) => {
    console.warn('Failed to load system info on app mount:', error)
  })
'''
if "effectiveVersion" not in text:
    text = replace_once(
        text,
        mount_anchor,
        mount_anchor + '''
  axios.get('/api/webui/status', { timeout: 5000, silent: true })
    .then(response => {
      webUiVersion.value = response.data?.effectiveVersion || response.data?.version || webUiVersion.value
    })
    .catch(() => {})
''',
        path,
    )
write(path, text)


# ---------------------------------------------------------------------------
# Lightweight update snapshot includes the separately available WebUI version.
# ---------------------------------------------------------------------------
path = "include/updatecheck.h"
text = read(path)
if "webuiValid" not in text:
    text = text.replace(
        '    bool isPrerelease = false;\n    char error[128] = {0};',
        '    bool isPrerelease = false;\n    bool webuiValid = false;\n    char webuiVersion[32] = {0};\n    char error[128] = {0};',
    )
text = text.replace("    void performOnlineUpdate();\n", "")
write(path, text)

path = "main/updatecheck.cpp"
text = read(path)
text = text.replace(
    '        s.isPrerelease = _release.isPrerelease;\n        snprintf(s.error, sizeof(s.error), "%s", _release.error);',
    '        s.isPrerelease = _release.isPrerelease;\n        s.webuiValid = _release.webui.valid;\n        snprintf(s.webuiVersion, sizeof(s.webuiVersion), "%s", _release.webui.version);\n        snprintf(s.error, sizeof(s.error), "%s", _release.error);',
)
text = remove_function(text, "void UpdateCheck::performOnlineUpdate()", path)
text = text.replace('#include "esp_https_ota.h"\n', '')
write(path, text)


# ---------------------------------------------------------------------------
# Firmware HTTP API: raw manual upload only, with clear file discrimination.
# ---------------------------------------------------------------------------
path = "main/webui.cpp"
text = read(path)
text = remove_function(text, "static esp_err_t post_ota_url_handler_func", path)
text = re.sub(
    r"httpd_uri_t post_ota_url_handler = \{.*?\};\n",
    "",
    text,
    count=1,
    flags=re.S,
)
# External proxy and changelog are no longer used by the WebUI.
proxy_start = text.find("// ---- Async proxy for external HTTPS fetches")
proxy_end = text.find("// Build and send a JSON snapshot", proxy_start)
if proxy_start >= 0 and proxy_end > proxy_start:
    text = text[:proxy_start] + text[proxy_end:]
archive_start = text.find("esp_err_t get_changelog_handler_func")
archive_end = text.find("esp_err_t get_log_handler_func", archive_start)
if archive_start >= 0 and archive_end > archive_start:
    text = text[:archive_start] + text[archive_end:]
text = text.replace('        httpd_register_uri_handler(_httpd_handle, &post_ota_url_handler);\n', '')
text = text.replace('        httpd_register_uri_handler(_httpd_handle, &get_changelog_handler);\n', '')
text = text.replace('        httpd_register_uri_handler(_httpd_handle, &get_firmware_archive_handler);\n', '')
text = text.replace('#include "esp_https_ota.h"\n', '')
text = text.replace('#include "esp_crt_bundle.h"\n', '')
text = text.replace('#include "ota_config.h"\n', '')
length_anchor = '''    int content_length = req->content_len;
    int content_received = 0;
'''
if "Falsche Datei: Das ist ein WebUI-/WWW-Image" not in text:
    text = replace_once(
        text,
        length_anchor,
        '''    int content_length = req->content_len;
    if (content_length == 0x50000)
    {
        free(ota_buff);
        _ota_status = OTA_FAILED;
        _updateCheck->finishOtaOperation();
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
            "Falsche Datei: Das ist ein 327680-Byte-WebUI-/WWW-Image. Bitte unter System -> WebUI installieren.");
    }
    int content_received = 0;
''',
        path,
    )
magic_anchor = '''            OTA_CHECK(esp_ota_begin(update_partition, content_length, &ota_handle) == ESP_OK, "Could not start OTA");
'''
if "ESP32-Firmwarekopf 0xE9" not in text:
    text = replace_once(
        text,
        magic_anchor,
        '''            if (recv_len < 1 || static_cast<unsigned char>(ota_buff[0]) != 0xE9)
            {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                    "Ungueltige Firmware-Datei: Der ESP32-Firmwarekopf 0xE9 fehlt. WebUI-Dateien unter System -> WebUI installieren.");
                goto err;
            }

''' + magic_anchor,
        path,
    )
write(path, text)


# ---------------------------------------------------------------------------
# MQTT: report two versions, but never install firmware remotely.
# ---------------------------------------------------------------------------
path = "main/mqtt_handler.cpp"
text = read(path)
if '#include "webui_storage.h"' not in text:
    text = text.replace('#include "updatecheck.h"', '#include "updatecheck.h"\n#include "webui_storage.h"')
text = re.sub(r"static constexpr uint32_t MQTT_UPDATE_TASK_STACK_BYTES = \d+;\n\n", "", text)
text = re.sub(
    r"\s*\} else if \(strcmp\(command, \"update\"\) == 0\) \{.*?\n\s*\} else \{",
    "\n    } else {",
    text,
    count=1,
    flags=re.S,
)
text = re.sub(r"\s*const char\* update_payload\s*=.*?;\n", "", text, count=1)
identity = '''    PUBLISH_STR("status/serial", sysInfo->getSerialNumber());
    PUBLISH_STR("status/version", sysInfo->getCurrentVersion());
    PUBLISH_STR("status/board_revision", sysInfo->getBoardRevisionString());
'''
identity_new = '''    PUBLISH_STR("status/serial", sysInfo->getSerialNumber());
    PUBLISH_STR("status/version", sysInfo->getCurrentVersion());
    PUBLISH_STR("status/firmware_version", sysInfo->getCurrentVersion());
    char webuiVersion[32] = {};
    webui_storage_get_effective_version(webuiVersion, sizeof(webuiVersion));
    PUBLISH_STR("status/webui_version", webuiVersion);
    PUBLISH_STR("status/board_revision", sysInfo->getBoardRevisionString());
'''
text = replace_once(text, identity, identity_new, path)
old_update = '''    // ---- Update info ------------------------------------------------------
    if (updateCheck) {
        VersionSnapshot rel = updateCheck->getVersionSnapshot();
        const char* currentVersion = sysInfo->getCurrentVersion();
        // Always publish a valid version string on latest_version so the HA
        // MQTT update entity can compare it against installed_version. If no
        // newer release is known, fall back to the current version so the
        // entity state stays "up to date" instead of "unknown".
        const char* latest = rel.valid ? rel.version : currentVersion;
        if (rel.valid && compareVersions(latest, currentVersion) <= 0) {
            latest = currentVersion;
        }
        PUBLISH_STR("status/latest_version", latest);
        bool updateAvailable = rel.valid &&
            compareVersions(rel.version, currentVersion) > 0;
        PUBLISH_STR("status/update_available", updateAvailable ? "true" : "false");
    }
'''
new_update = '''    // ---- Informational update status -------------------------------------
    if (updateCheck) {
        VersionSnapshot rel = updateCheck->getVersionSnapshot();
        const char* currentVersion = sysInfo->getCurrentVersion();
        const char* latestFirmware = rel.valid ? rel.version : currentVersion;
        if (rel.valid && compareVersions(latestFirmware, currentVersion) <= 0) {
            latestFirmware = currentVersion;
        }
        const bool firmwareUpdateAvailable = rel.valid &&
            compareVersions(rel.version, currentVersion) > 0;
        const char* latestWebui = rel.webuiValid ? rel.webuiVersion : webuiVersion;
        const bool webuiUpdateAvailable = rel.webuiValid &&
            compareVersions(rel.webuiVersion, webuiVersion) > 0;

        // Compatibility aliases remain firmware-oriented.
        PUBLISH_STR("status/latest_version", latestFirmware);
        PUBLISH_STR("status/update_available", firmwareUpdateAvailable ? "true" : "false");
        PUBLISH_STR("status/latest_firmware_version", latestFirmware);
        PUBLISH_STR("status/latest_webui_version", latestWebui);
        PUBLISH_STR("status/firmware_update_available", firmwareUpdateAvailable ? "true" : "false");
        PUBLISH_STR("status/webui_update_available", webuiUpdateAvailable ? "true" : "false");
    }
'''
text = replace_once(text, old_update, new_update, path)
text = text.replace(
    '    publish_config("sensor", "version", "Current Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:package-variant");\n    publish_config("sensor", "latest_version", "Latest Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:package-up");',
    '    publish_config("sensor", "version", "Firmware Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:package-variant");\n    publish_config("sensor", "firmware_version", "Firmware Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:package-variant");\n    publish_config("sensor", "webui_version", "WebUI Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:web");\n    publish_config("sensor", "latest_version", "Latest Firmware Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:package-up");\n    publish_config("sensor", "latest_firmware_version", "Latest Firmware Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:package-up");\n    publish_config("sensor", "latest_webui_version", "Latest WebUI Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:web-sync");\n    publish_config("binary_sensor", "firmware_update_available", "Firmware Update Available", "update", NULL, NULL, NULL, "diagnostic", "mdi:package-up", "true", "false");\n    publish_config("binary_sensor", "webui_update_available", "WebUI Update Available", "update", NULL, NULL, NULL, "diagnostic", "mdi:web-sync", "true", "false");',
)
text = text.replace(
    '    publish_config("sensor", "ota_progress", "OTA Progress", NULL, "measurement", "%", NULL, "diagnostic", "mdi:progress-download");',
    '    remove_config("sensor", "ota_progress");',
)
ha_start = text.find("    // ---- HA Update entity")
ha_end = text.find("    cJSON_Delete(device);", ha_start)
if ha_start >= 0 and ha_end > ha_start:
    text = text[:ha_start] + '    // Remove the former install-capable HA update entity.\n    remove_config("update", "firmware_update");\n\n' + text[ha_end:]
write(path, text)


# ---------------------------------------------------------------------------
# Remove archive source and build steps. Update search remains informational.
# ---------------------------------------------------------------------------
for obsolete in [
    "archive.json",
    "main/generated/archive.json.gz",
    "scripts/update_archive.py",
    ".github/workflows/rebuild-archive.yml",
]:
    file = Path(obsolete)
    if file.exists():
        file.unlink()

for workflow in Path(".github/workflows").glob("*.yml"):
    data = workflow.read_text(encoding="utf-8")
    data = re.sub(
        r"\n\s*- name: Regenerate embedded firmware archive\n\s*run: python3 scripts/update_archive\.py\n",
        "\n",
        data,
    )
    workflow.write_text(data, encoding="utf-8")

print("Applied PR #387 manual-update, dual-version, MQTT and fixed-default refactor.")
