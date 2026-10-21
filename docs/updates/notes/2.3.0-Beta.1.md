# 🚀 HB-RF-ETH-ng v2.3.0-Beta.1

[![License](https://img.shields.io/github/license/Xerolux/HB-RF-ETH-ng)](LICENSE.md)
[![Downloads](https://img.shields.io/github/downloads/Xerolux/HB-RF-ETH-ng/total)](https://github.com/Xerolux/HB-RF-ETH-ng/releases)

> ⚠️ **Pre-Release** - Testversion, Nutzung auf eigene Gefahr.

## 📋 Überblick

HB-RF-ETH-ng ist eine modernisierte Fork der originalen HB-RF-ETH Firmware von Alexander Reinert.
Diese Firmware ermöglicht es, ein Homematic Funkmodul (HM-MOD-RPI-PCB oder RPI-RF-MOD) per Netzwerk
an eine CCU-Installation (piVCCU3, debmatic, OpenCCU) anzubinden.

## 🆕 Was ist neu in v2.3.0-Beta.1?

### Changes
- fix(update): size the schedule status block for -Wformat-truncation
- test(update): correct the defensive-clamp backoff expectation
- fix(webui): minFirmwareVersion 2.3.0-Beta.1 (semver: prerelease < release)
- chore: bump firmware version to 2.3.0-Beta.1 (dev)
- fix(update): review findings — parser strictness, lock scope, 2038, TZ race
- docs: automatic updates B2/C/D + scheduled install; scheduler cycle fix
- feat(webui): automatic-update card, window-install scheduling, pending banner
- feat(update): config and schedule endpoints plus scheduler status (B2/C/D)
- feat(update): scheduler for automatic checks and installs (stages B2/C/D)
- feat(update): persisted automatic-update configuration (plan §6.1)
- feat(update): host-tested scheduling policy for stages B2/C/D
- feat(settings): configurable POSIX time zone
- chore(deps)(deps-dev): bump @vitejs/plugin-vue in /webui (#472)
- docs(troubleshooting): power-cycle requires unplugging both PoE and USB (issue #461)
- chore: update manifests for v2.2.9-Beta.2

### Added
- Automatic updates (plan stages B2/C/D in docs/AUTO_UPDATE_PLAN.md §7.1),
  strictly opt-in: `upd_mode` ships as `off` for every device updated to
  this firmware, so an update never switches on uninvited outbound
  connections. Factory-fresh devices start on `notify` (search & display
  only). Modes `auto_webui`/`auto_all` install inside a configurable
  maintenance window in device-local time, with a synchronized clock and
  (by default) a quiet CCU session as hard gates; §4.3 postpones with
  `installBlocked: "no_valid_time"` instead of failing. The scheduler is an
  hourly `esp_timer` plus one first-check shot (10 min + MAC-derived
  jitter); all rules are host-tested in
  `test/host/test_update_schedule_policy.cpp`. A failed search backs off
  1 h → 2 h → 4 h, then waits the full interval; skipped searches never
  count as failures, and a manual search satisfies the current cycle.
- Scheduled one-shot install ("Im Wartungsfenster installieren"): from the
  install confirmation dialog, exactly the update the last search found can
  be staged for the next maintenance window instead of installing now —
  mode-independent, persisted across reboots (7-day expiry), cancellable,
  visible as `pending` in the status. `POST /api/update/schedule` never
  accepts a version from the request; the installer re-verifies the
  manifest at window time.
- Two-stage updates: firmware first, reboot, then the WebUI — for
  `auto_all` and for scheduled firmware installs with a pending WebUI
  update. The NVS marker survives two boots and is discarded on rollback.
- `GET/POST /api/update/config` (mode, channel, interval 6–168 h, window
  start/length, idle-only) and a `schedule` block in
  `GET /api/update/status` (mode, next check estimate, `installBlocked`
  reason, persisted seen record, pending/stage entries). The persisted
  channel now also applies to `POST /api/update/check` without a body.
- Configurable time zone (POSIX TZ string, default `UTC0` = unchanged
  behaviour) in the settings time tab with presets (UTC, Berlin, London),
  charset validation and a live wall-clock preview. Applied at boot and
  immediately on change; the maintenance window runs in this local time.
  Syslog timestamps deliberately stay UTC.
- WebUI: third card "Automatisches Update" on the firmware page, pending
  banner with cancel on both update pages, `installBlocked` hints (clock,
  window, CCU session, busy) as durable content. Translations de/en/fr/it.

### Changed
- `webui/compatibility.json` `minFirmwareVersion` raised to `2.3.0`
  (additive API, `apiVersion` unchanged per RELEASE_VERSIONING).

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

- **Firmware-Binary** (`firmware_2.3.0-Beta.1.bin`)
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
- Minimum firmware: `2.3.0-Beta.1`
