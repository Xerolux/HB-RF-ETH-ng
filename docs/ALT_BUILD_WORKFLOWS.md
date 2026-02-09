# HB-RF-ETH-ng Build & Release Workflows (Referenz aus alt/)

> Dokumentiert am 2026-02-09. Enthält alle Build-Skripte, Konfigurationen
> und Workflows aus dem alt/-Verzeichnis als Referenz für zukünftige Nutzung.

---

## 1. PlatformIO Build-Konfiguration

### platformio.ini (mit Build-Varianten)

```ini
[platformio]
default_envs = hb-rf-eth-ng-standard

[env]
platform = espressif32@6.12.0
framework = espidf

; Base configuration shared by all variants
[env:base]
board = hb-rf-eth-ng
monitor_speed = 115200
monitor_filters = esp32_exception_decoder
board_build.embed_files =
    webui/dist/index.html.gz
    webui/dist/main.css.gz
    webui/dist/main.js.gz
    webui/dist/favicon.ico.gz
board_build.partitions = partitions.csv
build_flags =
    -DCONFIG_COMPILER_CXX_RTTI=1
    -DIDF_MAINTAINER=1
    -DCONFIG_LWIP_SNMP=1
    -DCONFIG_LWIP_SNMP_MAX_OBJ_ID_LEN=50
    -DCONFIG_LWIP_MIB2_CALLBACKS=1
    -DCONFIG_HTTPD_WS_SUPPORT=1
    -DCONFIG_FREERTOS_USE_TRACE_FACILITY=1
    -DCONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS=1
    -DCONFIG_ESP32_APPTRACE_ENABLE=n
    -DCONFIG_ESP32_DEBUG_STUBS_ENABLE=n
    -DCONFIG_LOG_MAXIMUM_LEVEL=3
extra_scripts =
    pre:append_version_to_progname.py
    pre:build_webui.py
    pre:rename_webui_files.py

; --- Firmware Variants ---

; Standard (smallest footprint)
[env:hb-rf-eth-ng-standard]
extends = env:base
build_flags =
    ${env:base.build_flags}
    -DENABLE_HMLGW=0
    -DENABLE_ANALYZER=0
    -DFIRMWARE_VARIANT=\"standard\"

; HMLGW variant
[env:hb-rf-eth-ng-hmlgw]
extends = env:base
build_flags =
    ${env:base.build_flags}
    -DENABLE_HMLGW=1
    -DENABLE_ANALYZER=0
    -DFIRMWARE_VARIANT=\"hmlgw\"

; Analyzer variant
[env:hb-rf-eth-ng-analyzer]
extends = env:base
build_flags =
    ${env:base.build_flags}
    -DENABLE_HMLGW=0
    -DENABLE_ANALYZER=1
    -DFIRMWARE_VARIANT=\"analyzer\"

; Full variant (all features)
[env:hb-rf-eth-ng-full]
extends = env:base
build_flags =
    ${env:base.build_flags}
    -DENABLE_HMLGW=1
    -DENABLE_ANALYZER=1
    -DFIRMWARE_VARIANT=\"full\"

; Legacy alias
[env:hb-rf-eth-ng]
extends = env:hb-rf-eth-ng-full

; --- OTA Upload Environments ---
; Set upload_port to device IP or use UPLOAD_PORT env variable

[env:hb-rf-eth-ng-standard-ota]
extends = env:hb-rf-eth-ng-standard
upload_protocol = custom
upload_port = 192.168.1.100
extra_scripts =
    ${env:base.extra_scripts}
    ota_upload.py

[env:hb-rf-eth-ng-hmlgw-ota]
extends = env:hb-rf-eth-ng-hmlgw
upload_protocol = custom
upload_port = 192.168.1.100
extra_scripts =
    ${env:base.extra_scripts}
    ota_upload.py

[env:hb-rf-eth-ng-full-ota]
extends = env:hb-rf-eth-ng-full
upload_protocol = custom
upload_port = 192.168.1.100
extra_scripts =
    ${env:base.extra_scripts}
    ota_upload.py
```

### platformio_local.ini.example

```ini
; Copy to platformio_local.ini and customize (git-ignored)
[env:hb-rf-eth-ng-standard-ota]
upload_port = 192.168.178.56
```

---

## 2. Board Definition

### boards/hb-rf-eth-ng.json

```json
{
  "build": {
    "core": "esp32",
    "f_cpu": "240000000L",
    "f_flash": "40000000L",
    "flash_mode": "qio",
    "mcu": "esp32",
    "variant": "hb-rf-eth-ng",
    "hwids": [["0x1A86", "0x7523"]]
  },
  "connectivity": ["wifi", "bluetooth", "ethernet"],
  "debug": { "openocd_board": "esp-wroom-32.cfg" },
  "frameworks": ["espidf"],
  "name": "HB-RF-ETH-ng",
  "upload": {
    "flash_size": "4MB",
    "maximum_ram_size": 327680,
    "maximum_size": 4194304,
    "require_upload_port": true,
    "speed": 921600
  },
  "url": "https://github.com/Xerolux/HB-RF-ETH-ng",
  "vendor": "Xerolux"
}
```

---

## 3. Partition Table

### partitions.csv

```
# ESP-IDF Partition Table
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,    ,   0x4000,
otadata,  data, ota,    ,   0x2000,
phy_init, data, phy,    ,   0x1000,
ota_0,    app,  ota_0,  , 0x1d0000,
ota_1,    app,  ota_1,  , 0x1d0000,
spiffs,   data, spiffs, ,  0x50000
```

---

## 4. Build-Skripte

### append_version_to_progname.py
Liest `version.txt` und setzt den Firmware-Namen auf `firmware_<version>`.

```python
from pathlib import Path
Import("env")

version_file = Path("version.txt")
if version_file.exists():
    with open(version_file, "r", encoding="utf-8") as fp:
        version = fp.readline().strip()
        if version:
            env.Replace(PROGNAME=f"firmware_{version}")
```

### build_webui.py
PlatformIO Pre-Build-Skript: installiert npm Dependencies und baut die WebUI.

- Prueft ob `npm` vorhanden ist
- Fuehrt `npm install` aus (falls `node_modules/` fehlt)
- Fuehrt `npm run build` aus
- Synchronisiert Version aus `version.txt` in WebUI-Dateien (via `update_version.py`)

### rename_webui_files.py
Benennt Parcel-2-Output mit Content-Hashes in feste Namen um:

- `webui.*.js.gz` -> `main.js.gz`
- `webui.*.css.gz` -> `main.css.gz`
- Aktualisiert `index.html.gz` mit den neuen Dateinamen
- Kopiert und komprimiert `favicon.ico`

---

## 5. OTA Upload

### ota_upload.py
Authentifiziertes OTA-Firmware-Upload via Web-API:

1. Login mit Passwort (aus `HB_RF_ETH_PASSWORD` Env-Variable oder interaktiv)
2. Token empfangen von `/login.json`
3. Firmware-Binary an `/ota_update` senden mit `Authorization: Token <token>`
4. Timeout: 120 Sekunden

Nutzung: In `platformio.ini` als `extra_scripts` einbinden + `upload_protocol = custom`.

---

## 6. Release-Skripte

### update_version.py
Aktualisiert Versionsnummern im gesamten Projekt:

```bash
python update_version.py 2.2.0
```

Aktualisiert:
- `version.txt`
- `README.md` (Titel + Versions-Abschnitt)
- `webui/src/locales/de.js` und `en.js` (Version-String)
- `webui/src/about.vue` (GitHub-Link)
- `webui/package.json` (version-Feld)
- `docs/openapi.yaml` (API-Version)
- `docs/TROUBLESHOOTING.md` (Header)

### update_changelog.py
Generiert automatische Changelog-Eintraege aus Git-Log:

```bash
python update_changelog.py 2.2.0
```

- Findet den vorherigen Tag
- Extrahiert Git-Commits seit dem letzten Tag
- Fuegt neuen `## Version X.X.X` Abschnitt in CHANGELOG.md ein

### generate_release_notes.py
Generiert vollstaendige Release-Notes fuer GitHub Releases:

```bash
python generate_release_notes.py 2.2.0 release_notes.md
```

Enthalt:
- Changelog-Extraktion
- Download-Anweisungen (WebUI + Serial Flash)
- SHA256-Verifikation
- Feature-Liste, Kompatibilitaet, Credits, Links

### update_headers.py
Aktualisiert Copyright-Header in allen Source-Dateien (`src/`, `include/`):

- Erkennt alte "Copyright 2022 Alexander Reinert" Header
- Ersetzt durch neuen Header mit Original + Modified Copyright

---

## 7. Docker

### docker-compose.yml

```yaml
version: '3'
services:
  webui:
    image: node:20
    working_dir: /app
    volumes:
      - ./webui:/app
    ports:
      - "1234:1234"
    command: sh -c "npm install && npm run dev"
```

Fuer lokale WebUI-Entwicklung mit Hot-Reload.

---

## 8. Test-Skript

### verify_redirect.py
Playwright-basierter E2E-Test fuer Router-Redirects:

- Prueft ob `/#/config` korrekt auf `/#/settings` weiterleitet
- Mockt API-Endpunkte (sysinfo, settings, monitoring)
- Injiziert Session-Token fuer Auth
- Erstellt Screenshot zur Verifikation

---

## 9. .gitignore

```
.pio/
.vscode/
platformio_local.ini
build/
sdkconfig
sdkconfig.*
managed_components/
*.bin *.elf *.map *.o *.a
webui/node_modules/
webui/.parcel-cache/
webui/dist/
__pycache__/ *.pyc
*.log
.DS_Store
.idea/
*.zip *.tar.gz
.venv/
```

---

## 10. Dependencies

```yaml
dependencies:
  espressif/mdns: 1.9.1
  idf: 5.5.0
target: esp32
```

---

## 11. Dokumentation (Referenz)

Folgende Docs existierten im alt/-Verzeichnis:

| Datei | Beschreibung |
|-------|-------------|
| `docs/API.md` | REST API Dokumentation |
| `docs/openapi.yaml` | OpenAPI 3.0 Spezifikation |
| `docs/TROUBLESHOOTING.md` | Fehlerbehebung |
| `docs/DTLS_ENCRYPTION_GUIDE.md` | DTLS Setup-Anleitung |
| `docs/DTLS_QUICK_REFERENCE.md` | DTLS Kurzreferenz |
| `docs/DTLS_README.md` | DTLS Uebersicht |
| `OTA_UPLOAD.md` | OTA Upload Anleitung |
| `RADIO_SIGNAL_OPTIMIZATION.md` | Funksignal-Optimierung |
| `SECURITY.md` | Sicherheitsrichtlinien |
| `STABILITY_FIXES.md` | Stabilitaets-Fixes |
| `STABILITY_IMPROVEMENTS.md` | Stabilitaets-Verbesserungen |
| `TODO_v2.1.1_to_v2.1.4.md` | Feature-Migrationsplan |
| `CHANGELOG.md` | Aenderungsprotokoll |
| `README.md` | Projekt-README |
