# 🚀 HB-RF-ETH-ng v2.2.5-Beta.5

[![License](https://img.shields.io/github/license/Xerolux/HB-RF-ETH-ng)](LICENSE.md)
[![Downloads](https://img.shields.io/github/downloads/Xerolux/HB-RF-ETH-ng/total)](https://github.com/Xerolux/HB-RF-ETH-ng/releases)

> ⚠️ **Pre-Release** - Testversion, Nutzung auf eigene Gefahr.

## 📋 Überblick

HB-RF-ETH-ng ist eine modernisierte Fork der originalen HB-RF-ETH Firmware von Alexander Reinert.
Diese Firmware ermöglicht es, ein Homematic Funkmodul (HM-MOD-RPI-PCB oder RPI-RF-MOD) per Netzwerk
an eine CCU-Installation (piVCCU3, debmatic, OpenCCU) anzubinden.

## 🆕 Was ist neu in v2.2.5-Beta.5?

### Changes
- docs(changelog): record recovery, webui and mqtt fixes under [Unreleased]
- fix(mqtt): drop duplicate version topics that produced two HA 'Firmware Version' sensors
- refactor(webui): migrate 154 font-size declarations to type scale tokens
- fix(webui): route header supporter chip to /settings?tab=license
- fix(webui): recenter brand mark in BrandLogo.vue
- refactor(webui): fix accent color picker and introduce type scale in main.css
- fix(diag): unblock /recovery login by removing duplicate CSP header
- chore: update manifests for v2.2.5-Beta.4

### Changes
- fix: recovery page login was silently non-functional due to duplicate Content-Security-Policy headers. `httpd_resp_set_hdr()` appends rather than overwrites, so the recovery route emitted two CSP headers (one strict `script-src 'self'`, one permissive `'unsafe-inline'`); browsers enforce the intersection and blocked the page's inline script. Added `add_security_headers_inline_script()` in `security_headers.h` and switched the `/recovery` handler to use it instead of stacking both CSPs.
- fix(webui): accent color picker in /theme now affects the whole New Design UI. Previously the three `body.newdesign-active` blocks in `main.css` hardcoded `--color-primary` to emerald green, which won the CSS cascade over the theme store's inline style on `<html>`. Removed the hardcoded overrides (light, dark, dark-shell); the subtree now inherits the value the store sets via `shiftColor()` / `rgbaColor()`. The hardcoded green login glow and hover-border literals were also replaced with `var(--color-primary-soft)`.
- fix(webui): the "Projekt unterstützen" / supporter chip in `NewDesignHeader.vue` now routes to `/settings?tab=license` (matching the hero chip on the dashboard) instead of the generic `/settings` landing on the "Allgemein" tab.
- fix(webui): centered the brand mark in `BrandLogo.vue`. The three leaves' bounding box (incl. Bezier control points) was centred near (241.5, 263.5) within the 512×512 viewBox; a `transform="translate(15, -7)"` on the group recentres it without altering leaf geometry.
- refactor(webui): introduced a unified type scale (`--fs-2xs` … `--fs-3xl`) and font-family tokens (`--font-sans`, `--font-mono`) in `main.css`. Migrated 154 ad-hoc `font-size: Xrem` declarations across 18 files to the scale, eliminating the dense 12.48/12.8/13.12/13.28/13.6/13.76/14.08px collision band. Consolidated four divergent monospace stacks (Consolas / Cascadia / SFMono / ui-monospace) plus two references to an undefined `--font-mono` onto the single token, and replaced one non-standard `font-weight: 650` with `600`.
- fix(mqtt): removed duplicate version topics that produced two Home Assistant sensors named "Firmware Version". The legacy short topics `status/version` / `status/latest_version` (plus their HA discovery announcements) duplicated the explicit `firmware_version` / `latest_firmware_version` 1:1 after the dual-version refactor. Empty retained discovery payloads are now published for `sensor.version` and `sensor.latest_version` so HA deletes the duplicate entities automatically on the next status publish. The explicit set (`firmware_version`, `webui_version`, `latest_firmware_version`, `latest_webui_version`) is unchanged.

## ✨ Hauptfunktionen

- **Moderne WebUI** mit Responsive Design, Dark/Light Theme und 10 Sprachen
- **Online-Updates** - Firmware direkt ueber den integrierten Update-Dienst herunterladen
- **MQTT-Support** mit Home Assistant Auto-Discovery
- **CheckMK Monitoring** für Integration in Monitoringsysteme
- **IPv6-Support** mit Auto-Konfiguration
- **Sichere Authentifizierung** mit automatischem Session-Timeout
- **Verbesserte OTA-Updates** mit besserer Fehlerbehandlung
- **LED-Helligkeitssteuerung** (0-100%)
- **Konfigurations-Backup/Restore** über WebUI

## 📥 Installation

### Update über WebUI

1. Die `firmware_*.bin` Datei aus diesem Release herunterladen
2. In der WebUI zu **Firmware Update** navigieren
3. Die .bin Datei hochladen
4. Auf Abschluss des Updates und automatischen Neustart warten

### Update per URL

Alternativ kann das Update direkt aus diesem Release per URL in der WebUI durchgeführt werden.

### Prüfsummen

SHA256-Prüfsummen befinden sich in `SHA256SUMS.txt`.

## ⚠️ Wichtige Hinweise

- **Backup der Einstellungen** vor dem Update erstellen (Einstellungen → Backup)
- **Nicht abschalten** während des Update-Vorgangs
- Bei sehr alten Versionen kann ein **Werksreset** erforderlich sein
- Nach erfolgreichem Update startet das Gerät **automatisch neu**

## 📦 Im Release enthalten

- **Firmware-Binary** (`firmware_2.2.5-Beta.5.bin`)
- **Bootloader** (`bootloader.bin`)
- **Partitionstabelle** (`partitions.bin`)
- **SHA256-Prüfsummen** (`SHA256SUMS.txt`)
- **Versionsinformationen** (`version.txt`)

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

- WebUI version: 
- WebUI API: 
- Minimum firmware: 
