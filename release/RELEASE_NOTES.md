# 🚀 HB-RF-ETH-ng v2.2.5-Beta.14

[![License](https://img.shields.io/github/license/Xerolux/HB-RF-ETH-ng)](LICENSE.md)
[![Downloads](https://img.shields.io/github/downloads/Xerolux/HB-RF-ETH-ng/total)](https://github.com/Xerolux/HB-RF-ETH-ng/releases)

> ⚠️ **Pre-Release** - Testversion, Nutzung auf eigene Gefahr.

## 📋 Überblick

HB-RF-ETH-ng ist eine modernisierte Fork der originalen HB-RF-ETH Firmware von Alexander Reinert.
Diese Firmware ermöglicht es, ein Homematic Funkmodul (HM-MOD-RPI-PCB oder RPI-RF-MOD) per Netzwerk
an eine CCU-Installation (piVCCU3, debmatic, OpenCCU) anzubinden.

## 🆕 Was ist neu in v2.2.5-Beta.14?

### Changes
- fix(factory-reset): Der Werksreset entfernt jetzt sämtliche Benutzerkonfigurationen aus den Einstellungen-, Monitoring-, Theme-, Reset-/Crash- und Update-Cache-Namespaces. Auch lokale Browser- und Sitzungsdaten einschließlich Theme und Akzentfarbe werden gelöscht.
- feat(backup): Backup und Wiederherstellung sichern jetzt alle wiederherstellbaren Benutzereinstellungen einschließlich Administrator-Zugangsdaten, Netzwerk, Zeit, LEDs, Theme, Akzentfarbe, Supporter-Key, Browser-Präferenzen sowie vollständiger Monitoring-Konfiguration mit MQTT-/Benachrichtigungs-Passwörtern, Tokens, Zertifikaten und privaten Schlüsseln.
- fix(settings): Feldgenaue Validierungsfehler für Administratorname, CCU-Adresse, Hostname, IPv4, Netzmaske, IPv6 und NTP ergänzt; ungültige Werte werden nun auch im Backend vor Änderungen vollständig abgewiesen.
- fix(network): Die Ping-Diagnose verwendet authentifizierte Requests, meldet Latenz sowie verständliche DNS-/Timeout-Fehler und leitet nach einer statischen IP-Änderung zur neuen Geräteadresse weiter.
- fix(restart): Wiederherstellung, manueller Neustart und MQTT-Neustart verwenden einheitlich die Neustart-Synchronisierung; doppelte Aktionen werden verhindert.
- fix(mqtt): Unsicheren MQTT-Werksreset sowie die nicht mehr installierbare Home-Assistant-Firmware-Update-Entität entfernt.
- fix(webui): Passwortfehler werden übersetzt, ANSI-farbige Systemlog-Zeilen korrekt gefiltert, die Recovery-Seite ist direkt verlinkt und Systemaktionen befinden sich unter „Sichern & Wiederherstellen“.
- test: Regressionstests für vollständigen Werksreset, Backup/Restore, Neustart-Synchronisierung, Validierung, Mobile-Layout und die korrigierten Bedienabläufe ergänzt.

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

- **Firmware-Binary** (`firmware_2.2.5-Beta.14.bin`)
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
