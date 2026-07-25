# 🚀 HB-RF-ETH-ng v2.2.5-Beta.18

[![License](https://img.shields.io/github/license/Xerolux/HB-RF-ETH-ng)](LICENSE.md)
[![Downloads](https://img.shields.io/github/downloads/Xerolux/HB-RF-ETH-ng/total)](https://github.com/Xerolux/HB-RF-ETH-ng/releases)

> ⚠️ **Pre-Release** - Testversion, Nutzung auf eigene Gefahr.

## 📋 Überblick

HB-RF-ETH-ng ist eine modernisierte Fork der originalen HB-RF-ETH Firmware von Alexander Reinert.
Diese Firmware ermöglicht es, ein Homematic Funkmodul (HM-MOD-RPI-PCB oder RPI-RF-MOD) per Netzwerk
an eine CCU-Installation (piVCCU3, debmatic, OpenCCU) anzubinden.

## 🆕 Was ist neu in v2.2.5-Beta.18?

### Changes
- fix: stabilize updates, MQTT, Raw-UART and WebUI
- chore: update manifests for v2.2.5-Beta.17

- fix(mqtt): Alle Status-, Event-, OTA- und Home-Assistant-Publishes werden nun zusätzlich zum Client-Lebenszyklus auf den tatsächlichen Broker-Login (`MQTT_EVENT_CONNECTED`) geprüft. Der periodische Publisher wartet während Start und Reconnect, statt komplette QoS-0-Statusblöcke an einen noch nicht verbundenen Client zu übergeben und dadurch `Publish: Losing qos0 data when client not connected` zu fluten.
- fix(update): Die Heap-Freigabe der manuellen und täglichen Updatesuche berücksichtigt jetzt Gesamtgröße und Fragmentierung gemeinsam. Der auf einem Live-Gerät beobachtete gesunde Zustand mit 55 KB frei und 32 KB größtem Block darf die serialisierte Manifest-Abfrage starten; echte Risikozustände unter den absoluten Heap- beziehungsweise Blockgrenzen werden weiterhin abgewiesen.
- fix(recovery): `/recovery` bildet nun die aktuelle New-Design-Shell mit echter Seitenleiste, 88-px-Statusleiste, Markenlogo, flachen Karten, responsivem Mobile-Header sowie dem auf dem Gerät gespeicherten Farbschema nach. Die Seite bleibt vollständig in der Firmware eingebettet und damit auch bei einem beschädigten externen WebUI erreichbar.
- perf(raw-uart): Der UDP-Empfang legt Queue-Ereignisse nun direkt in der FreeRTOS-Queue ab und verarbeitet gewöhnliche Pakete in einem 256-Byte-Stackpuffer. Damit entfallen die beiden Heap-Allokationen pro Standardpaket; größere Frames bleiben über einen geprüften Fallback vollständig unterstützt. Datenformat, Timeouts und CCU-Kompatibilität ändern sich nicht.
- fix(webui): Die Einstellungen-Navigation nutzt auf Desktop die vollständige Inhaltsbreite und bleibt in einer Zeile, statt wegen einer künstlichen 700-px-Grenze vorzeitig umzubrechen. Auf kleineren Ansichten wechselt sie kontrolliert auf drei beziehungsweise zwei Spalten; das Ping-Feld erhält einen angemessen breiten Aktionsbereich und stapelt Eingabe und Vollbreiten-Button auf Mobilgeräten.
- release(webui): Die New-Design-Oberfläche wird wegen der sichtbaren Einstellungen-/Ping-Änderungen als eigenständig erkennbares Update `1.0.0-Beta.15` ausgeliefert.

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

- **Firmware-Binary** (`firmware_2.2.5-Beta.18.bin`)
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

- WebUI version: `1.0.0-Beta.15`
- WebUI API: `1`
- Minimum firmware: `2.2.5-Beta.1`
