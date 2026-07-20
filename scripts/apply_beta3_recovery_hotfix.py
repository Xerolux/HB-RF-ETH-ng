from pathlib import Path


def replace_once(path_name: str, old: str, new: str) -> None:
    path = Path(path_name)
    text = path.read_text(encoding="utf-8")
    if old in text:
        path.write_text(text.replace(old, new, 1), encoding="utf-8")
        return
    if new in text:
        return
    raise RuntimeError(f"Expected text not found in {path_name}: {old[:120]!r}")


replace_once(
    "main/settings.cpp",
    '  GET_IP_ADDR(handle, "gateway", _gateway, IPADDR_ANY);\n'
    '  GET_IP_ADDR(handle, "dns1", _dns1, IPADDR_ANY);\n'
    '  GET_IP_ADDR(handle, "dns2", _dns2, IPADDR_ANY);',
    '''  GET_IP_ADDR(handle, "gateway", _gateway, IPADDR_ANY);

  // Use Cloudflare DNS when no primary DNS was stored. Existing non-empty
  // user settings remain authoritative. Legacy 0.0.0.0 values are migrated
  // once so static IPv4 installations can resolve NTP and OTA hosts.
  ip4_addr_t defaultDns1;
  IP4_ADDR(&defaultDns1, 1, 1, 1, 1);
  GET_IP_ADDR(handle, "dns1", _dns1, defaultDns1.addr);
  GET_IP_ADDR(handle, "dns2", _dns2, IPADDR_ANY);
  if (_dns1.addr == IPADDR_ANY || _dns1.addr == IPADDR_NONE)
  {
    _dns1 = defaultDns1;
    ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_set_u32(handle, "dns1", _dns1.addr));
    ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_commit(handle));
  }''',
)

replace_once(
    "main/system_overview_api.cpp",
    '    add_security_headers(req);\n'
    '    httpd_resp_set_type(req, "text/html; charset=utf-8");',
    '''    add_security_headers(req);
    // Recovery is a static self-contained document. Permit only this route's
    // own inline script; the normal New Design retains the strict global CSP.
    httpd_resp_set_hdr(req, "Content-Security-Policy",
                       "default-src 'self'; style-src 'self' 'unsafe-inline'; "
                       "script-src 'self' 'unsafe-inline'; img-src 'self' data:; "
                       "connect-src 'self'; font-src 'self' data:; object-src 'none'; "
                       "base-uri 'self'; form-action 'self'; frame-ancestors 'self';");
    httpd_resp_set_type(req, "text/html; charset=utf-8");''',
)

storage = Path("main/webui_storage.cpp")
storage_text = storage.read_text(encoding="utf-8")
for old, new in [
    ('{"/main.js", BASE_PATH "/main.js.br", "application/javascript", "br"}',
     '{"/main.js", BASE_PATH "/main.js.gz", "application/javascript", "gzip"}'),
    ('{"/main.css", BASE_PATH "/main.css.br", "text/css", "br"}',
     '{"/main.css", BASE_PATH "/main.css.gz", "text/css", "gzip"}'),
    ('strcmp(js_encoding->valuestring, "br") == 0',
     'strcmp(js_encoding->valuestring, "gzip") == 0'),
    ('strcmp(css_encoding->valuestring, "br") == 0',
     'strcmp(css_encoding->valuestring, "gzip") == 0'),
    ('BASE_PATH "/main.js.br"', 'BASE_PATH "/main.js.gz"'),
    ('BASE_PATH "/main.css.br"', 'BASE_PATH "/main.css.gz"'),
    ('// Only the New Design is accepted. Brotli keeps the complete single-file\n'
     '    // Vue application inside the existing 320 KiB partition.',
     '// Only the New Design is accepted. Gzip works reliably on the device\n'
     '    // private-LAN HTTP origin.'),
]:
    if old in storage_text:
        storage_text = storage_text.replace(old, new)
    elif new not in storage_text:
        raise RuntimeError(f"Expected WebUI storage text not found: {old}")
storage.write_text(storage_text, encoding="utf-8")

packager = Path("rename_webui_files.py")
packager_text = packager.read_text(encoding="utf-8")
packager_text = packager_text.replace(
    '"main.js.br",\n        "main.css.br",',
    '"main.js.gz",\n        "main.css.gz",',
)
packager_text = packager_text.replace(
    '"main.js": "br",\n            "main.css": "br",',
    '"main.js": "gzip",\n            "main.css": "gzip",',
)
if '"main.js.gz"' not in packager_text or '"main.js": "gzip"' not in packager_text:
    raise RuntimeError("Could not switch standalone WebUI packaging to gzip")
packager.write_text(packager_text, encoding="utf-8")

replace_once(
    "webui/src/firmwareupdate.vue",
    '''const handleFileSelect = (event) => {
  const selectedFile = event.target.files[0]
  if (selectedFile && selectedFile.name.endsWith('.bin')) {
    file.value = selectedFile
  }
}

const handleDrop = (event) => {
  isDragging.value = false
  const selectedFile = event.dataTransfer.files[0]
  if (selectedFile && selectedFile.name.endsWith('.bin')) {
    file.value = selectedFile
  }
}''',
    '''const WEBUI_IMAGE_SIZE = 0x50000
const isWebUiImage = (selectedFile) => {
  if (!selectedFile) return false
  const name = String(selectedFile.name || '').toLowerCase()
  return name.startsWith('webui_')
    || name === 'spiffs.bin'
    || Number(selectedFile.size) === WEBUI_IMAGE_SIZE
}

const acceptFirmwareFile = (selectedFile) => {
  if (!selectedFile || !selectedFile.name.toLowerCase().endsWith('.bin')) return
  if (isWebUiImage(selectedFile)) {
    clearFile()
    uiStore.pushToast({
      type: 'warning',
      title: 'WebUI-Datei erkannt',
      message: 'Diese Datei unter System → WebUI installieren, nicht als Firmware.',
      duration: 7000
    })
    return
  }
  file.value = selectedFile
}

const handleFileSelect = (event) => acceptFirmwareFile(event.target.files[0])

const handleDrop = (event) => {
  isDragging.value = false
  acceptFirmwareFile(event.dataTransfer.files[0])
}''',
)

for filename in ("webui/package.json", "webui/package-lock.json"):
    path = Path(filename)
    text = path.read_text(encoding="utf-8")
    text = text.replace("1.0.0-Beta.1", "1.0.0-Beta.2")
    if "1.0.0-Beta.2" not in text:
        raise RuntimeError(f"Could not bump WebUI version in {filename}")
    path.write_text(text, encoding="utf-8")

for temporary in (
    Path(".github/workflows/bootstrap-recovery-hotfix.yml"),
    Path("docs/HOTFIX_BETA3_RECOVERY.md"),
    Path(__file__),
):
    temporary.unlink(missing_ok=True)
