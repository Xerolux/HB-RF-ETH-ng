# 🚀 HB-RF-ETH-ng v2.2.5-Beta.16

[![License](https://img.shields.io/github/license/Xerolux/HB-RF-ETH-ng)](LICENSE.md)
[![Downloads](https://img.shields.io/github/downloads/Xerolux/HB-RF-ETH-ng/total)](https://github.com/Xerolux/HB-RF-ETH-ng/releases)

> ⚠️ **Pre-Release** - Testversion, Nutzung auf eigene Gefahr.

## 📋 Überblick

HB-RF-ETH-ng ist eine modernisierte Fork der originalen HB-RF-ETH Firmware von Alexander Reinert.
Diese Firmware ermöglicht es, ein Homematic Funkmodul (HM-MOD-RPI-PCB oder RPI-RF-MOD) per Netzwerk
an eine CCU-Installation (piVCCU3, debmatic, OpenCCU) anzubinden.

## 🆕 Was ist neu in v2.2.5-Beta.16?

### Changes
- feat: secure portable backups and improve recovery UX
- fix: preserve WebUI release metadata
- chore: update manifests for v2.2.5-Beta.15

- feat(backup): Vollständige JSON-Sicherungen kennzeichnen Klartext-Zugangsdaten nun sowohl in der WebUI als auch direkt in der Datei. Die Datei bleibt bewusst editierbar und kann nach Anpassung von Hostname, IP, Administrator-Passwort und gerätespezifischen MQTT-Werten als Vorlage für weitere Geräte verwendet werden.
- feat(recovery): Die stets sichtbare Schaltfläche „Zur normalen WebUI“ führt sowohl von der Recovery-Ansicht als auch von deren Notfall-Anmeldung zurück zur regulären Oberfläche.
- fix(webui): Der Einstellungspunkt „Backup“ heißt nun in allen vier unterstützten Sprachen „Backup & Reset“ beziehungsweise die jeweilige Übersetzung.
- docs(i18n): README, Changelog, Wiki und Release-Notes nennen konsistent die vier unterstützten Sprachen Deutsch, Englisch, Französisch und Italienisch.

## ✨ Hauptfunktionen

- **Moderne WebUI** mit Responsive Design, Dark/Light Theme und 4 Sprachen
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

- **Backup der Einstellungen** vor dem Update erstellen (Einstellungen → Backup & Reset)
- **Nicht abschalten** während des Update-Vorgangs
- Bei sehr alten Versionen kann ein **Werksreset** erforderlich sein
- Nach erfolgreichem Update startet das Gerät **automatisch neu**

## 📦 Im Release enthalten

- **Firmware-Binary** (`firmware_2.2.5-Beta.16.bin`)
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

- WebUI version: `1.0.0-Beta.14`
- WebUI API: `1`
- Minimum firmware: `2.2.5-Beta.1`
