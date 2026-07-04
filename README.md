<div align="center">

# HB-RF-ETH-ng

**Modernisierte HomeMatic Netzwerk-Firmware | ESP-IDF 6.0**

[![GitHub Release][releases-shield]][releases]
[![GitHub Activity][commits-shield]][commits]
[![License][license-shield]](LICENSE.md)

[![Buy Me A Coffee][buymeacoffee-badge]][buymeacoffee]
[![Tesla](https://img.shields.io/badge/Tesla-Referral-red?style=for-the-badge&logo=tesla)](https://ts.la/sebastian564489)

[![Release Management](https://github.com/Xerolux/HB-RF-ETH-ng/actions/workflows/release.yml/badge.svg)](https://github.com/Xerolux/HB-RF-ETH-ng/actions/workflows/release.yml)

</div>

## Modernisierte Fork von Xerolux (2025)

Diese Version ist eine modernisierte und aktualisierte Fork der originalen HB-RF-ETH Firmware von Alexander Reinert. Die Firmware basiert auf ESP-IDF 6.0 und ist für moderne Toolchains optimiert.

> Alle detaillierten Änderungen pro Version finden Sie im [CHANGELOG.md](CHANGELOG.md).

### Worum es geht
Dieses Repository enthält die Firmware für die HB-RF-ETH Platine, welches es ermöglicht, ein Homematic Funkmodul HM-MOD-RPI-PCB oder RPI-RF-MOD per Netzwerk an eine debmatic oder piVCCU3 Installation anzubinden.

Hierbei gilt, dass bei einer debmatic oder piVCCU3 Installation immer nur ein Funkmodul angebunden werden kann, egal ob die Anbindung direkt per GPIO Leiste, USB mittels HB-RF-USB(-2) Platine oder per HB-RF-ETH Platine erfolgt.

---

📖 **Umfassende Dokumentation zu Funktionen, Installation, Home Assistant Integration und vielem mehr finden Sie im [offiziellen Wiki](https://github.com/Xerolux/HB-RF-ETH-ng/wiki).**

---

### Kurzüberblick
- Firmware für HB-RF-ETH mit Unterstützung für `HM-MOD-RPI-PCB`, `RPI-RF-MOD` und `HmIP-RFUSB`
- Moderne WebUI auf Basis von Vue 3, Vite und Bootstrap 5 (Dark/Light, 10 Sprachen)
- Login mit Benutzername und Passwort: Standard-Benutzername `admin`, das bestehende Administrator-Passwort bleibt nach Updates erhalten und der Benutzername kann in den Einstellungen geändert werden.
- Dashboard, Kopfzeile und Browser-Tab zeigen den unter Einstellungen/Netzwerk gesetzten Hostnamen, damit mehrere HB-RF-ETH-ng Geräte sofort unterscheidbar sind.
- System-Log bleibt nach Aktivierung auch über einen Reboot aktiv; beim Deaktivieren bleibt es nach dem nächsten Start wieder aus.
- **Monitoring via MQTT** (mit Home Assistant Auto-Discovery, TLS/mTLS, Kommando-Token) und CheckMK
- OTA-Updates per Datei-Upload, URL-Download oder direkt aus Home Assistant heraus
- ESP-IDF 6.0 Toolchain (native `idf.py` Builds), GCC 14.2 (xtensa-esp-elf)

### Login nach Update
Nach dem Update auf eine Version mit Benutzername-Pflicht muss die Anmeldung einmalig mit dem Standard-Benutzernamen `admin` und dem bisherigen Administrator-Passwort erfolgen. Alte gespeicherte Browser-Sessions werden dabei aus Sicherheitsgründen ungültig. Der Benutzername kann anschließend unter **Einstellungen > Allgemein > Sicherheit** geändert werden, z.B. für Passwortmanager oder Installationen mit mehreren Geräten.

### Backup & Restore
Backups enthalten den konfigurierten Administrator-Benutzernamen und die System-Log-Aktivierung, damit Login- und Diagnose-Einstellungen bei mehreren Geräten oder nach einer Wiederherstellung konsistent bleiben. Das Administrator-Passwort wird aus Sicherheitsgründen weiterhin nicht exportiert; beim Restore bleibt das aktuelle Passwort des Zielgeräts erhalten.

> Die vollständige MQTT-API-Referenz (alle Status-, Event- und Command-Topics,
> HA-Entitäten, TLS-Konfiguration, Sicherheitsmodell) findet sich im
> [Wiki – MQTT-Sektion](docs/WIKI.md#home-assistant-mqtt-integration).

### Entwickler-Build (ESP-IDF 6.0)
```bash
./scripts/setup_esp_idf.sh
. ~/esp-idf/export.sh

cd webui
npm ci
npm run build
cd ..
python3 rename_webui_files.py

./idf.py build
```

### Update- und Release-Hinweise
- Die WebUI prüft neue Versionen standardmäßig über den gehosteten Update-Dienst unter `https://xerolux.de/firmware/HB-RF-ETH-ng/`.
- Releases auf GitHub bleiben die zentrale Quelle für Release Notes, Artefakte und Historie.
- Für produktive Systeme sollten bevorzugt Stable-Releases verwendet werden; Pre-Releases eignen sich zum Vorabtesten neuer Fixes.

### Screenshots
| Login | Dashboard |
| :---: | :---: |
| ![Login](screenshots/01_Login.png) | ![Dashboard](screenshots/02_Dashboard.png) |

| Settings | Monitoring |
| :---: | :---: |
| ![Settings](screenshots/04_Settings.png) | ![Monitoring](screenshots/05_Monitoring.png) |

| Firmware Update | System Log |
| :---: | :---: |
| ![Firmware Update](screenshots/06_FirmwareUpdate.png) | ![System Log](screenshots/07_SystemLog.png) |

| About | Changelog |
| :---: | :---: |
| ![About](screenshots/08_About.png) | ![Changelog](screenshots/09_Changelog.png) |

### Danksagung
Ein großer Dank geht an **Alexander Reinert** für die Entwicklung der originalen HB-RF-ETH Firmware und Hardware. Seine Arbeit bildet die Grundlage für diese modernisierte Version.

### Lizenz
Die Firmware steht unter Creative Commons Attribution-NonCommercial-ShareAlike 4.0 Lizenz.

<!-- Links -->
[releases-shield]: https://img.shields.io/github/release/Xerolux/HB-RF-ETH-ng.svg?style=for-the-badge
[releases]: https://github.com/Xerolux/HB-RF-ETH-ng/releases
[downloads-shield]: https://img.shields.io/github/downloads/Xerolux/HB-RF-ETH-ng/latest/total.svg?style=for-the-badge
[commits-shield]: https://img.shields.io/github/commit-activity/y/Xerolux/HB-RF-ETH-ng.svg?style=for-the-badge
[commits]: https://github.com/Xerolux/HB-RF-ETH-ng/commits/main
[license-shield]: https://img.shields.io/github/license/Xerolux/HB-RF-ETH-ng.svg?style=for-the-badge
[buymeacoffee]: https://www.buymeacoffee.com/xerolux
[buymeacoffee-badge]: https://img.shields.io/badge/-Buy%20Me%20A%20Coffee-FFDD00?style=for-the-badge&logo=buy-me-a-coffee&logoColor=black
