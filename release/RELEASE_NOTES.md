# 🚀 HB-RF-ETH-ng v2.2.7-Beta.9

[![License](https://img.shields.io/github/license/Xerolux/HB-RF-ETH-ng)](LICENSE.md)
[![Downloads](https://img.shields.io/github/downloads/Xerolux/HB-RF-ETH-ng/total)](https://github.com/Xerolux/HB-RF-ETH-ng/releases)

> ⚠️ **Pre-Release** - Testversion, Nutzung auf eigene Gefahr.

## 📋 Überblick

HB-RF-ETH-ng ist eine modernisierte Fork der originalen HB-RF-ETH Firmware von Alexander Reinert.
Diese Firmware ermöglicht es, ein Homematic Funkmodul (HM-MOD-RPI-PCB oder RPI-RF-MOD) per Netzwerk
an eine CCU-Installation (piVCCU3, debmatic, OpenCCU) anzubinden.

## 🆕 Was ist neu in v2.2.7-Beta.9?

### Changes
- docs: align update-search docs with hardware field test (#443)
- feat(update): manual update search, reporting only (stage B1) (#442)
- feat(release): slim device update manifest + auto-update plan (#441)
- chore: update manifests for v2.2.7-Beta.8

### Added
- feat(update): Manuelle Update-Suche. Auf Knopfdruck holt das Gerät eine
  kleine Versionsdatei (max. 1024 Byte) von GitHub Pages und vergleicht sie mit
  der laufenden Firmware und WebUI. Es lädt und installiert dabei nichts —
  Installation bleibt der manuelle Upload. Kanal (stabil/beta) ist wählbar.
- feat(mqtt): Vier **lesende** Home-Assistant-Entitäten für das Ergebnis
  (`latest_firmware_version`, `latest_webui_version`,
  `firmware_update_available`, `webui_update_available`). Vor der ersten Suche
  melden sie „unbekannt“ statt „kein Update“. Bewusst kein Kommando-Topic und
  kein Install-Button.
- test: Host-Tests für die Zulassungsschranke der Suche und für den
  Versionsvergleich; beide laufen in CI ohne Hardware.

### Notes
- Es gibt **keinen** Zeitplan und keine Hintergrundsuche: ein unbeaufsichtigtes
  Gerät baut von sich aus keine ausgehende Verbindung auf. Der Worker läuft auf
  Priorität 3 (Relay-Kette: 15) und kann dem Funkpfad keine CPU nehmen.
- Eine übersprungene Suche wird als solche angezeigt und niemals als
  „kein Update gefunden“.
- Hardware-Feldtest (2026-09-09, productive Gerät mit aktiver CCU über die
  UART/UDP-Brücke): 30+ Suchen über API und WebUI, Heap-Drift nach 20 Suchen
  172 Bytes, kein Watchdog-Reset, keine CCU-Trennung, keine Crash-Einträge.
  Doku entsprechend korrigiert: Die Suche blockt nur bei laufender
  **Firmware**-Installation (nicht WebUI-Upload), und die
  Speicher-Akzeptanzschwelle des Testprotokolls unterscheidet jetzt Idle- und
  CCU-Lastfall.

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

- **Firmware-Binary** (`firmware_2.2.7-Beta.9.bin`)
- **Kompatibles WebUI-Binary** (`webui_1.0.0.bin`)
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

- WebUI version: `1.0.0`
- WebUI API: `1`
- Minimum firmware: `2.2.5-Beta.1`
