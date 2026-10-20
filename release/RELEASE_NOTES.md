# 🚀 HB-RF-ETH-ng v2.2.9-Beta.1

[![License](https://img.shields.io/github/license/Xerolux/HB-RF-ETH-ng)](LICENSE.md)
[![Downloads](https://img.shields.io/github/downloads/Xerolux/HB-RF-ETH-ng/total)](https://github.com/Xerolux/HB-RF-ETH-ng/releases)

> ⚠️ **Pre-Release** - Testversion, Nutzung auf eigene Gefahr.

## 📋 Überblick

HB-RF-ETH-ng ist eine modernisierte Fork der originalen HB-RF-ETH Firmware von Alexander Reinert.
Diese Firmware ermöglicht es, ein Homematic Funkmodul (HM-MOD-RPI-PCB oder RPI-RF-MOD) per Netzwerk
an eine CCU-Installation (piVCCU3, debmatic, OpenCCU) anzubinden.

## 🆕 Was ist neu in v2.2.9-Beta.1?

### Changes
- chore: bump WebUI version to 1.0.7
- chore(webui): drop unused marked dependency; changelog for stability batch
- fix(webui): lifecycle and session robustness batch
- fix(firmware): stability batch - detector UAF, syslog backoff, httpd unblocking
- docs: update landing page to ESP-IDF 6.1
- fix(flasher): serve release assets same-origin via Pages mirror
- feat(docs): add browser-based web flasher on GitHub Pages
- docs: resolve migration root cause - rollback reverts updates inside the 15s self-test window
- chore(deps)(deps): bump pinia from 4.0.3 to 4.1.0 in /webui (#474)
- chore(deps)(deps): bump vue-i18n from 11.4.13 to 11.4.14 in /webui (#473)
- chore(deps)(deps-dev): bump sass from 1.105.1 to 1.105.2 in /webui (#471)
- chore(deps)(deps-dev): bump @playwright/test in /webui (#470)
- docs: document the on-board CH340 USB serial console
- chore(deps)(deps-dev): bump @playwright/test in /webui (#467)
- chore(deps)(deps-dev): bump vite from 8.3.2 to 8.3.4 in /webui (#466)
- chore(deps)(deps): bump vue-router from 5.3.1 to 5.4.0 in /webui (#465)
- chore(deps)(deps): bump marked from 18.0.14 to 18.1.0 in /webui (#464)
- chore(ci)(deps): bump crate-ci/typos from 1.50.3 to 1.51.1 (#463)
- fix(migration): verify running firmware after legacy upload (#469)
- chore(deps): bump source-map-js to 1.2.2 and postcss-selector-parser to 7.1.6
- docs: describe the manual update search that stage B1 re-added
- chore(deps)(deps-dev): bump brace-expansion (#468)
- chore: update manifests for v2.2.8

### Fixed
- Migration helper (#461): verify the image's running version after a manual
  reboot instead of reporting upload acknowledgement as migration success.
  Add read-only `--verify-only`, a bounded observation window and application
  descriptor/chip/partition-size preflight checks. Correct the legacy HTTP
  simulation and retract the unsupported socket-timeout diagnosis.
- Stability batch — firmware:
  - Boot: the radio-module detector no longer deletes its frame-data
    semaphore while the UART task may still be inside the frame handler —
    a rare use-after-free that could surface as unexplained crashes after
    module detection (the semaphore now lives for the object's lifetime,
    as upstream intended).
  - Syslog forwarder: reconnects to an unreachable server now back off
    (5 s → 30 s cap, reset on success) instead of re-resolving and
    re-connecting on every dequeued log line, which could overflow the
    queue while a flapping link generated log spam.
  - `POST /api/monitoring` sizes its body buffer to the actual request
    instead of reserving a fixed contiguous 16 KiB block, so monitoring
    saves no longer fail with "Out of memory" on a fragmented heap (same
    fix the settings handler already has).
  - WebUI image install: the 320 KiB partition erase runs in 32 KiB chunks
    with yields between them, keeping the HTTP server — including the
    upload status poll — responsive during the erase.
  - Restart, factory reset and restore hand the post-response restart to a
    background task (like the OTA finalize), so the HTTP server is no
    longer blocked for the potentially 90 s worker-stop phase.
  - RTC: out-of-range register values (dead backup battery, halted
    oscillator) are rejected instead of being converted into an absurd
    system time.
- Stability batch — WebUI:
  - Leaving the syslog page mid-poll no longer leaves a zombie 5 s poll
    loop and a reconnecting WebSocket running for the rest of the browser
    session.
  - A 401 response now always invalidates the session; `silent` requests
    only suppress the toast. After a firmware restart, stale tabs redirect
    to the login page instead of showing outdated data forever.
  - Navigating away during an update search no longer permanently disables
    the "check now" buttons on both update pages.
  - The dashboard poller can no longer orphan an interval on rapid
    visibility changes (iOS lock/unlock).
  - The diagnostics page pauses its 5 s poll while the tab is hidden, and
    the syslog WebSocket reconnect backs off up to 30 s while the device
    is unreachable.
  - A failed initial settings load shows an error banner with a retry
    button instead of silently presenting defaults, and the
    unsaved-changes guard is always armed.

### Added
- Web flasher (`/flash/` on the GitHub Pages site): install or repair the
  firmware over USB straight from Chrome/Edge using esptool-js and Web
  Serial — release picker (stable/beta/all/manual), full-install and
  firmware-only profiles, SHA256 verification and a clean OTA-selection
  reset. The Pages deploy mirrors the official release assets to the same
  origin because GitHub release downloads cannot be fetched cross-origin
  from a browser. Documented in README, wiki and troubleshooting guide.

### Changed
- WebUI: removed the unused `marked` dependency (declared but never
  imported; it never entered the bundle).

### Documentation
- Migration root cause (#461): original-firmware boards run the compatible
  v0.9.0 release bootloader with application rollback; any reset inside the
  15-second self-test window reverts the update although the upload was
  activated. Documented the keep-powered rule for every firmware update and
  removed the unresolved-cause notices from README and troubleshooting guide.

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

- **Firmware-Binary** (`firmware_2.2.9-Beta.1.bin`)
- **Kompatibles WebUI-Binary** (`webui_1.0.7.bin`)
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

- WebUI version: `1.0.7`
- WebUI API: `1`
- Minimum firmware: `2.2.5-Beta.1`
