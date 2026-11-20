# 🚀 HB-RF-ETH-ng v2.4.1

[![License](https://img.shields.io/github/license/Xerolux/HB-RF-ETH-ng)](LICENSE.md)
[![Downloads](https://img.shields.io/github/downloads/Xerolux/HB-RF-ETH-ng/total)](https://github.com/Xerolux/HB-RF-ETH-ng/releases)

## 📋 Überblick

HB-RF-ETH-ng ist eine modernisierte Fork der originalen HB-RF-ETH Firmware von Alexander Reinert.
Diese Firmware ermöglicht es, ein Homematic Funkmodul (HM-MOD-RPI-PCB oder RPI-RF-MOD) per Netzwerk
an eine CCU-Installation (piVCCU3, debmatic, OpenCCU) anzubinden.

## 🆕 Was ist neu in v2.4.1?

### Changes
- chore: update WebUI manifest for webui-v1.2.0


### Changes
- Merge pull request #501 from Xerolux/Xe/pre-release-audit-2026-11-20
- style: format Beta.3 firmware changes
- fix(monitoring): avoid copying staged config for Beta.3
- Merge remote-tracking branch 'origin/main' into Xe/pre-release-audit-2026-11-20
- Merge pull request #492 from Xerolux/dependabot/github_actions/dependabot/fetch-metadata-3
- Merge remote-tracking branch 'origin/main' into Xe/pre-release-audit-2026-11-20
- Merge branch 'main' into dependabot/github_actions/dependabot/fetch-metadata-3
- Merge pull request #499 from ischraub/feat/checkmk-status-checks
- Merge pull request #498 from ischraub/fix/checkmk-cpu-mem-sections
- fix(mqtt): allocate queued command payloads on demand
- Merge branch 'main' into dependabot/github_actions/dependabot/fetch-metadata-3
- Merge pull request #500 from Xerolux/Xe/fix-low-heap-ci-2026-11-20
- style(checkmk): format monitoring status checks
- Merge commit '655f3eccc1fb6f9477fe691b57e582458ce80274' into HEAD
- style(checkmk): format monitoring local checks
- Merge commit 'd763474' into HEAD
- docs(api): document the legacy WebUI redirect
- fix(ci): scope watchdog regression to low-heap restart
- Prepare next firmware release with ESP-IDF v6.1.1 and MQTT heap fixes
- Merge branch 'fix/checkmk-cpu-mem-sections' into feat/checkmk-status-checks
- fix(monitoring): leave more headroom below the normal heap minimum
- feat(monitoring): report Ethernet, CCU, radio module and last restart to Checkmk
- fix(monitoring): show the CPU value in the Checkmk service summary
- fix(monitoring): fit Checkmk heap levels to the 2.4.x baseline
- fix(monitoring): report CPU and heap to Checkmk as local checks
- chore: update manifests for v2.4.1-Beta.2
- chore(ci)(deps): bump dependabot/fetch-metadata from 2 to 3

### Changed

- Build and package the firmware with ESP-IDF v6.1.1.

### Fixed

- Allocate the MQTT command worker and queue only when MQTT commands are enabled at client startup (#496).
- Reject incomplete or oversized MQTT command messages before queueing them.
- Allocate MQTT command payloads when received, reducing idle heap use.
- Transfer the monitoring configuration to the background worker without allocating a second copy.
- Keep the board revision string alive while registering the Home Assistant mDNS service.
- Clarify serial installation for releases without an OTA initialization image (#497).


### Changes
- fix(webui,log): legacy /esp redirect and line-aligned log exports
- chore: update manifests for v2.4.1-Beta.1

### Fixed
- fix(webui): legacy `/esp/*` paths (old firmware pages still polled once a minute by stale browser tabs and CCU `/esp` reverse-proxy embeddings via `POST /esp/system.htm?action=UpdateUI`) now answer with a 301 redirect to the WebUI root instead of an unmatched route whose 404 the CCU proxy reported as recurring HTTP 500 log spam; `max_uri_handlers` raised accordingly
- fix(log): system-log snapshot and download now start at a line boundary when older ring-buffer content was overwritten — no more garbled first line (e.g. `mI (678) ...` missing its ANSI colour escape) in exported logs


### Changes
- merge: feature/ha-mqtt-stage1-control — HA control surface (stage 1+2), zeroconf service, stack fixes
- docs: changelog entries and wiki mDNS section for the stage-1/2 release
- chore(deps)(deps): bump vue-i18n from 11.4.18 to 11.4.19 in /webui (#495)
- chore(deps)(deps-dev): bump @vitejs/plugin-vue in /webui (#494)
- chore(deps)(deps): bump bootstrap from 5.3.8 to 5.3.9 in /webui (#493)
- chore: bump firmware version to 2.4.1-Alpha.1
- feat(mdns): announce dedicated _hb-rf-eth._tcp service for HA discovery
- fix(mqtt): boot crash - stack overflow on the 2.3 KiB event-loop task
- feat(mqtt): stage-2 diagnostics surface - events, journal, log and crash tail in HA
- feat(mqtt): stage-1 Home Assistant full control surface
- feat(firmware): heap corruption tripwire in the heap watchdog
- chore: update manifests for v2.4.0-Beta.6

### Changes
- feat(mqtt): stage-1 Home Assistant full control surface — token-gated MQTT command layer (`main/mqtt_commands.cpp`) so every routine operation can be driven from Home Assistant after initial setup: LED brightness and per-state programs, log capture, maintenance restart schedule, hostname, CCU address, NTP server and timezone, the full auto-update configuration (mode/channel/interval/window/idleOnly), monitoring toggles incl. syslog/CheckMK/Prometheus/notify settings, native HA update entities for firmware and WebUI (check + install), `reset_relay_stats`, `test_notify` and a RAM `log_level` override. Retained settings states are published on connect, after accepted commands and on a slow cycle so WebUI edits converge. Factory reset, network/IPv4/IPv6 config, MQTT broker settings and credentials remain MQTT-unreachable by design.
- feat(mqtt): stage-2 diagnostics surface — incident diagnosis without the WebUI: live journalled device events as HA event trigger, the reboot-surviving event journal (10 newest entries), the system-log tail and the crash tail (non-destructive) become HA entities over MQTT, plus a token-gated `clear_crash_log` command sharing the WebUI endpoint's NVS slot and lock discipline. Discovery now also announces `min_free_heap` and the six `radio_uart_*` metrics.
- feat(mdns): dedicated `_hb-rf-eth._tcp` zeroconf service (port 80, serial/version/board TXT records). Home Assistant's zeroconf router ignores the generic `_http._tcp` announcement; the dedicated service lets the hb-rf-eth-hass integration discover gateways automatically and prefill its config flow.
- feat(firmware): heap corruption tripwire in the heap watchdog (issue #491). A failing `heap_caps_check_integrity_all()` check now saves the crash tail, records a diagnostic reset reason and restarts instead of letting corruption detonate later as an unrelated panic.

### Fixed
- fix(mqtt): boot crash at the initial MQTT connect — stack overflow on the 2304-byte ESP event-loop task (`xQueueGiveMutexRecursive` assert after stack corruption; found during the first live OTA install of the stage-1/2 build, rolled back by the bootloader inside the 15 s self-test window). Connect-time discovery and state batches now run on the 5 KiB `mqtt_publish` task; commands execute serialized on a dedicated 6 KiB worker task fed by a queue (concurrent commands ordered, queue depth bounded, overflows rejected with `reason=worker_busy`); `monitoring_config_t` (~6.7 KiB TLS PEM buffers) and the 1 KiB log-tail buffer are heap-allocated in both paths; the journal listener defers the snapshot to the publish task via a dirty flag; update entities use the correct state key (no more `..._update_update` double suffix).

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

- **Firmware-Binary** (`firmware_2.4.1.bin`)
- **Kompatibles WebUI-Binary** (`webui_1.2.0.bin`)
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

- WebUI version: `1.2.0`
- WebUI API: `1`
- Minimum firmware: `2.3.1`
