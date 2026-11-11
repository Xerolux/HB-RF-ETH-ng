# 🚀 HB-RF-ETH-ng v2.4.0-Beta.1

[![License](https://img.shields.io/github/license/Xerolux/HB-RF-ETH-ng)](LICENSE.md)
[![Downloads](https://img.shields.io/github/downloads/Xerolux/HB-RF-ETH-ng/total)](https://github.com/Xerolux/HB-RF-ETH-ng/releases)

> ⚠️ **Pre-Release** - Testversion, Nutzung auf eigene Gefahr.

## 📋 Überblick

HB-RF-ETH-ng ist eine modernisierte Fork der originalen HB-RF-ETH Firmware von Alexander Reinert.
Diese Firmware ermöglicht es, ein Homematic Funkmodul (HM-MOD-RPI-PCB oder RPI-RF-MOD) per Netzwerk
an eine CCU-Installation (piVCCU3, debmatic, OpenCCU) anzubinden.

## 🆕 Was ist neu in v2.4.0-Beta.1?

### Added
- **mDNS**: the device announces itself as `<hostname>.local` (plus `_http._tcp`), so the WebUI is reachable by name; the WebFlasher links straight to it after a successful flash.
- **Event journal**: reboot-surviving history of CCU/link/crash/watchdog events (NVS mirror), served by `GET /api/events` and shown on the diagnostics page ("Letzte Ereignisse").
- **`GET /api/health`**: minimal unauthenticated liveness document for uptime monitors.
- **Runtime log levels**: `GET/POST /api/loglevel` raises one module's verbosity until the next restart; surfaced as a card on the system-log page (field debugging without a reflash).
- **Scheduled maintenance restart**: opt-in weekly restart (day + time) with boot-loop protection; settings card included. New reset reason `Scheduled Restart`.
- **Network watchdog witness**: before a gateway-unreachable restart the connected CCU (same subnet) is probed; a responding witness defers the restart and is journaled + counted (`hbrfeth_net_witness_deferred_total`).
- Home Assistant discovery sensor `raw_uart_stale_reconnects` for the #480 recovery path.
- OpenAPI drift check in CI: a new route without documentation fails the build (legacy routes carry an explicit allowlist).
- Visual regression tests: 12 screenshot baselines (6 routes × light/dark) fail CI on layout drift.
- **Home Assistant integration** (separate repo [hb-rf-eth-hass](https://github.com/Xerolux/hb-rf-eth-hass), beta): device with sensors, connectivity entities and a one-click firmware update entity.

### Changed
- Include WebUI 1.2.0-Beta.1 (event journal card, log-level card, restart schedule, OTA header chip, crash-transcript notification toggle).
- `docs/openapi.yaml`: documented `/api/health`, `/api/events`, `/api/loglevel`; corrected the `login.json` schema (username + password).

### Fixed
- Network watchdog: probe the connected CCU (same subnet, no gateway involved) before restarting on gateway-unreachable. A responding witness proves MAC/PHY/lwIP are alive and the outage is router-side, so the restart - and with it the CCU session drop - is deferred.
- New `EVENT_CRASH_TRANSCRIPT` notification (bit 11): when the previous boot ended in a panic, configured channels receive the first transcript line instead of waiting for a manual `/api/crash_log` read.
- `hbrfeth_udp_stale_reconnect_total` Prometheus counter and `status/raw_uart_stale_reconnects` MQTT topic (#480 field telemetry).
- Update checks fetch the manifest with a cache-busting `?fetch=` counter: freshly published releases are visible immediately instead of after the ~10 minute GitHub Pages CDN window.
- `HTTP_CLIENT` EAGAIN retry warnings (dozens per TLS artifact download, all benign) are silenced; errors stay visible.
- WebFlasher: image downloads retry transient failures up to three times, the release catalog shows its newest entry date (with a stale-catalog warning after 21 days), and the manual mode documents the esptool flash layout including `ota_data_initial.bin` (now also a release asset).
- Release workflows rebase their metadata commit onto the live `main` head before pushing, so a concurrent push can no longer fail a release.
- Dependabot patch/minor bumps with green CI merge automatically (`auto-merge.yml`); majors stay manual.
- WebUI: OTA progress chip in the header visible from every page; monitoring password fields wrapped in forms (Chrome DOM hint); return-based router guard; zero-console E2E guard for every route.

### Validation
- Local: 74 browser tests (53 regressions + 9 zero-console + 12 visual baselines), 51 Python tests, six C++ host programs; WebUI build, translations, OpenAPI drift check and ESP-IDF build passed.

## ✨ Hauptfunktionen

- **Moderne WebUI** mit Responsive Design, Dark/Light Theme und 4 Sprachen
- **Manuelle Firmware-Updates** - lokale `firmware_*.bin` sicher über die WebUI hochladen
- **MQTT-Support** mit Home Assistant Auto-Discovery
- **CheckMK Monitoring** für Integration in Monitoringsysteme
- **IPv6-Support** mit Auto-Konfiguration
- **Sichere Authentifizierung** mit automatischem Session-Timeout
- **Robuster Raw-Binary-Upload** mit Image-Pruefung und sicherem Neustart
- **LED-Helligkeitssteuerung** (0-100%)
- **Konfigurations-Backup/Restore** über WebUI

## 📥 Installation

### Update über WebUI

1. Die `firmware_*.bin` Datei aus diesem Release herunterladen
2. In der WebUI zu **System → Firmware** navigieren
3. Die .bin Datei hochladen
4. Auf Abschluss des Updates und automatischen Neustart warten
5. Das enthaltene `webui_*.bin` bei Bedarf separat unter **System → WebUI** installieren; niemals am Firmware-Endpunkt hochladen

### Prüfsummen

SHA256-Prüfsummen befinden sich in `SHA256SUMS.txt`.

## ⚠️ Wichtige Hinweise

- **Backup der Einstellungen** vor dem Update erstellen (Einstellungen → Backup & Reset)
- **Nicht abschalten** während des Update-Vorgangs
- Ein **Werksreset ist für dieses Update normalerweise nicht erforderlich**
- Nach erfolgreichem Update startet das Gerät **automatisch neu**

## 📦 Im Release enthalten

- **Firmware-Binary** (`firmware_2.4.0-Beta.1.bin`)
- **Kompatibles WebUI-Binary** (`webui_1.2.0-Beta.1.bin`)
- **Bootloader** (`bootloader.bin`)
- **Partitionstabelle** (`partitions.bin`)
- **SHA256-Prüfsummen** (`SHA256SUMS.txt`)
- **Versionsinformationen** (`firmware-version.txt` und `webui-version.txt`)

## 🔗 Kompatible CCU-Systeme

- **[OpenCCU](https://openccu.de/)** - Open-Source CCU-Betriebssystem
- **[piVCCU3](https://github.com/leon-vi/piVccu)** - Homematic auf Raspberry Pi
- **[debmatic](https://github.com/leopes91/debmatic)** - Homematic auf Debian-basierten Systemen

## 💬 Support & Community

- **Issues**: Bitte [Fehler melden](https://github.com/Xerolux/HB-RF-ETH-ng/issues)
- **Discussions**: [Community-Diskussionen](https://github.com/Xerolux/HB-RF-ETH-ng/discussions)
- **Dokumentation**: Siehe [README.md](https://github.com/Xerolux/HB-RF-ETH-ng/blob/main/README.md)

## 🙏 Unterstützung

Dir gefällt dieses Projekt und du möchtest es unterstützen?

[![Buy Me A Coffee][buymeacoffee-badge]][buymeacoffee]
[![Tesla Referral](https://img.shields.io/badge/Tesla-Referral-red?style=for-the-badge&logo=tesla)](https://ts.la/sebastian564489)

[buymeacoffee]: https://www.buymeacoffee.com/xerolux
[buymeacoffee-badge]: https://img.shields.io/badge/buy%20me%20a%20coffee-donate-yellow.svg?style=for-the-badge

## 📄 Lizenz

Diese Firmware steht unter [Creative Commons Attribution-NonCommercial-ShareAlike 4.0](LICENSE.md).

---

**Vielen Dank an alle Beitragenden!** 🙏

*Diese Firmware basiert auf der originalen Arbeit von [Alexander Reinert](https://github.com/ja-ra). 
Die modernisierte Fork wird von [Xerolux](https://github.com/Xerolux) gewartet.*

## Included WebUI

- WebUI version: `1.2.0-Beta.1`
- WebUI API: `1`
- Minimum firmware: `2.3.0-Beta.1`
