from pathlib import Path
import re


def _read(path):
    return Path(path).read_text(encoding='utf-8')


def _write(path, text):
    Path(path).write_text(text, encoding='utf-8')


def apply():
    path = 'include/updatecheck.h'
    text = _read(path)
    if 'webuiValid' not in text:
        text = text.replace(
            '    bool isPrerelease = false;\n    char error[128] = {0};',
            '    bool isPrerelease = false;\n    bool webuiValid = false;\n    char webuiVersion[32] = {0};\n    char error[128] = {0};',
            1,
        )
    _write(path, text)

    path = 'main/updatecheck.cpp'
    text = _read(path)
    old = '        s.isPrerelease = _release.isPrerelease;\n        snprintf(s.error, sizeof(s.error), "%s", _release.error);'
    new = '        s.isPrerelease = _release.isPrerelease;\n        s.webuiValid = _release.webui.valid;\n        snprintf(s.webuiVersion, sizeof(s.webuiVersion), "%s", _release.webui.version);\n        snprintf(s.error, sizeof(s.error), "%s", _release.error);'
    if old in text:
        text = text.replace(old, new, 1)
    _write(path, text)

    path = 'main/webui.cpp'
    text = _read(path)

    # The online installers remain unreachable legacy sections for this hotfix;
    # no HTTP route or MQTT command can invoke them. Linker GC discards them.
    for registration in [
        '        httpd_register_uri_handler(_httpd_handle, &post_ota_url_handler);\n',
        '        httpd_register_uri_handler(_httpd_handle, &get_changelog_handler);\n',
        '        httpd_register_uri_handler(_httpd_handle, &get_firmware_archive_handler);\n',
    ]:
        text = text.replace(registration, '')

    # Remove the no-longer-used browser proxy and embedded archive handlers.
    proxy_start = text.find('// ---- Async proxy for external HTTPS fetches')
    proxy_end = text.find('// Build and send a JSON snapshot', proxy_start)
    if proxy_start >= 0 and proxy_end > proxy_start:
        text = text[:proxy_start] + text[proxy_end:]

    old_start = text.find('esp_err_t get_changelog_handler_func')
    old_end = text.find('esp_err_t get_log_handler_func', old_start)
    if old_start >= 0 and old_end > old_start:
        text = text[:old_start] + text[old_end:]

    # Keep exactly one backend firmware magic check.
    guard = '''            if (recv_len <= 0 || static_cast<unsigned char>(ota_buff[0]) != 0xE9) {
                ESP_LOGW(TAG, "Rejected non-ESP firmware image (magic 0x%02x)",
                         recv_len > 0 ? static_cast<unsigned char>(ota_buff[0]) : 0);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                    "Falsche Datei: kein gueltiges ESP32-Firmware-Abbild. WebUI unter System -> WebUI installieren.");
                goto err;
            }

'''
    while text.count(guard) > 1:
        text = text.replace(guard, '', 1)

    # Make all remaining user-visible file errors explicit.
    text = text.replace(
        '"Falsche Datei: WebUI-Abbild erkannt. Unter System -> WebUI installieren."',
        '"Falsche Datei: Das 327680-Byte-WebUI-/WWW-Image muss unter System -> WebUI installiert werden."',
    )
    _write(path, text)
