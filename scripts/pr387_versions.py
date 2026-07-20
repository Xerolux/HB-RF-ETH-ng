from pathlib import Path


def _read(path):
    return Path(path).read_text(encoding='utf-8')


def _write(path, text):
    Path(path).write_text(text, encoding='utf-8')


def apply():
    path = 'main/CMakeLists.txt'
    text = _read(path)
    text = text.replace(
        '# Embedded firmware release archive. Regenerate with scripts/update_archive.py\n# whenever archive.json changes (release process). Served from flash by\n# /api/firmware_archive so the WebUI never has to fetch it live from GitHub.\ntarget_add_binary_data(${COMPONENT_TARGET} "generated/archive.json.gz" BINARY)\n\n',
        '',
    )
    metadata = '''
# Keep the WebUI version independent from the firmware version.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../webui/package.json" WEBUI_PACKAGE_JSON)
string(JSON HB_WEBUI_VERSION GET "${WEBUI_PACKAGE_JSON}" version)
target_compile_definitions(${COMPONENT_LIB} PRIVATE HB_WEBUI_VERSION=\\"${HB_WEBUI_VERSION}\\")

'''
    if 'string(JSON HB_WEBUI_VERSION' not in text:
        text = text.replace('\n# Transition firmware:', metadata + '# Transition firmware:')
    text = text.replace('minimal Brotli-compressed New Design shell', 'gzip-compressed New Design shell')
    _write(path, text)

    path = 'include/webui_storage.h'
    text = _read(path)
    if 'webui_storage_get_effective_version' not in text:
        anchor = '/** True only when the partition is mounted and the New Design manifest is valid. */\nbool webui_storage_is_valid();\n'
        if anchor not in text:
            raise RuntimeError('webui_storage.h: validity anchor missing')
        text = text.replace(anchor, anchor + '\n/** Copy the active WebUI version (external image or embedded fallback). */\nvoid webui_storage_get_effective_version(char *output, size_t outputSize);\n', 1)
    _write(path, text)

    path = 'main/webui_storage.cpp'
    text = _read(path)
    if '#ifndef HB_WEBUI_VERSION' not in text:
        text = text.replace(
            'extern esp_err_t validate_auth(httpd_req_t *req);',
            'extern esp_err_t validate_auth(httpd_req_t *req);\n\n#ifndef HB_WEBUI_VERSION\n#define HB_WEBUI_VERSION "unknown"\n#endif',
            1,
        )
    marker = '''WebUIStorageStatus webui_storage_get_status()
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
    if 'void webui_storage_get_effective_version' not in text:
        if marker not in text:
            raise RuntimeError('webui_storage.cpp: status function anchor missing')
        text = text.replace(marker, marker + '''
void webui_storage_get_effective_version(char *output, size_t outputSize)
{
    if (!output || outputSize == 0) return;
    StorageLock lock;
    if (!lock) {
        copy_safe(output, outputSize, HB_WEBUI_VERSION);
        return;
    }
    copy_safe(output, outputSize,
              s_status.valid && s_status.version[0]
                  ? s_status.version
                  : HB_WEBUI_VERSION);
}
''', 1)
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
    _write(path, text)

    path = 'main/system_overview_api.cpp'
    text = _read(path)
    if 'webui_effective_version' not in text:
        text = text.replace(
            '    const WebUIStorageStatus webui = webui_storage_get_status();',
            '    const WebUIStorageStatus webui = webui_storage_get_status();\n    char webui_effective_version[32] = {};\n    webui_storage_get_effective_version(webui_effective_version, sizeof(webui_effective_version));',
            1,
        )
    text = text.replace(
        '        cJSON_AddStringToObject(webui_object, "version",\n                                webui.version[0] ? webui.version : "embedded");',
        '        cJSON_AddStringToObject(webui_object, "version",\n                                webui_effective_version);',
    )
    _write(path, text)

    path = 'webui/src/app.vue'
    text = _read(path)
    old = "          <small class=\"text-muted\">{{ t('app.footerCopyright', { version: sysInfoStore.currentVersion ? 'v' + sysInfoStore.currentVersion : '' }) }}</small>"
    new = "          <small class=\"text-muted\">Firmware v{{ sysInfoStore.currentVersion || '—' }} · WebUI v{{ webUiVersion }} © 2025-2026 Xerolux</small>"
    if old in text:
        text = text.replace(old, new, 1)
    if "import axios from 'axios'" not in text:
        text = text.replace("import { useI18n } from 'vue-i18n'", "import { useI18n } from 'vue-i18n'\nimport axios from 'axios'", 1)
    if 'const webUiVersion = ref' not in text:
        text = text.replace("const otaUpdateVersion = ref('')", "const otaUpdateVersion = ref('')\nconst webUiVersion = ref(typeof __WEBUI_VERSION__ !== 'undefined' ? __WEBUI_VERSION__ : 'unbekannt')", 1)
    mount = "  sysInfoStore.update().catch((error) => {\n    console.warn('Failed to load system info on app mount:', error)\n  })\n"
    if 'response.data?.effectiveVersion' not in text:
        if mount not in text:
            raise RuntimeError('app.vue: mount anchor missing')
        text = text.replace(mount, mount + "\n  axios.get('/api/webui/status', { timeout: 5000, silent: true })\n    .then(response => { webUiVersion.value = response.data?.effectiveVersion || response.data?.version || webUiVersion.value })\n    .catch(() => {})\n", 1)
    _write(path, text)
