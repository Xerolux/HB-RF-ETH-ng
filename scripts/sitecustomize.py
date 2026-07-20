from pathlib import Path

patch = Path(__file__).with_name('apply_pr387_refactor.py')
if patch.exists():
    text = patch.read_text(encoding='utf-8')
    text = text.replace(
        'if "Falsche Datei: Das ist ein 327680-Byte-WebUI" not in text:',
        'if False:',
    )
    text = text.replace(
        'if "Firmwarekopf 0xE9" not in text:',
        'if False:',
    )
    cleanup = r'''

# Current branch already validates the ESP32 image magic. Keep exactly one guard.
_upload_path = Path("main/webui.cpp")
_upload_text = _upload_path.read_text(encoding="utf-8")
_guard = '''            if (recv_len <= 0 || static_cast<unsigned char>(ota_buff[0]) != 0xE9) {
                ESP_LOGW(TAG, "Rejected non-ESP firmware image (magic 0x%02x)",
                         recv_len > 0 ? static_cast<unsigned char>(ota_buff[0]) : 0);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                    "Falsche Datei: kein gueltiges ESP32-Firmware-Abbild. WebUI unter System -> WebUI installieren.");
                goto err;
            }

'''
while _upload_text.count(_guard) > 1:
    _upload_text = _upload_text.replace(_guard, "", 1)
_upload_path.write_text(_upload_text, encoding="utf-8")
'''
    if 'Current branch already validates the ESP32 image magic' not in text:
        text += cleanup
    patch.write_text(text, encoding='utf-8')
