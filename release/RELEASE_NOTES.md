# 🚀 HB-RF-ETH-ng v2.2.7-Beta.11

[![License](https://img.shields.io/github/license/Xerolux/HB-RF-ETH-ng)](LICENSE.md)
[![Downloads](https://img.shields.io/github/downloads/Xerolux/HB-RF-ETH-ng/total)](https://github.com/Xerolux/HB-RF-ETH-ng/releases)

> ⚠️ **Pre-Release** - Testversion, Nutzung auf eigene Gefahr.

## 📋 Überblick

HB-RF-ETH-ng ist eine modernisierte Fork der originalen HB-RF-ETH Firmware von Alexander Reinert.
Diese Firmware ermöglicht es, ein Homematic Funkmodul (HM-MOD-RPI-PCB oder RPI-RF-MOD) per Netzwerk
an eine CCU-Installation (piVCCU3, debmatic, OpenCCU) anzubinden.

## 🆕 Was ist neu in v2.2.7-Beta.11?

### Changes
- chore: bump WebUI version to 1.0.3
- fix(webui): live log auth, PWA asset routing, font CDN, ANSI display (#449)
- feat(webui): add a Radio & Relay statistics page and restore the 2.1.10 queue depth (#448)
- chore(deps)(deps): bump marked from 18.0.10 to 18.0.11 in /webui (#435)
- chore(deps)(deps): bump axios from 1.19.0 to 1.20.0 in /webui (#434)
- chore(deps)(deps): bump vue-router from 5.2.0 to 5.3.1 in /webui (#433)
- chore(deps)(deps-dev): bump eslint from 10.8.1 to 10.10.0 in /webui (#432)
- chore(deps)(deps-dev): bump sass from 1.103.1 to 1.104.0 in /webui (#431)
- chore(ci)(deps): bump crate-ci/typos from 1.50.0 to 1.50.1 (#436)
- chore: update manifests for v2.2.7-Beta.10

### Added
- feat(webui): Neue Seite **Funk & Relay** (`/diagnostics`) zeigt die Kennzahlen
  der Raw-UART-Weiterleitung zwischen CCU und Funkmodul: empfangene und
  gesendete Frames, Keepalives, verworfene Datagramme samt Verwerf-Rate,
  längste Queue-Wartezeit, Spitzenbelegung gegen die Queue-Kapazität sowie
  Verzögerungen über 10 ms / 100 ms / 1 s. Bisher waren diese Werte nur über
  Prometheus oder MQTT sichtbar — also gerade nicht für die Anwender, die
  Kommunikationsprobleme melden (#447).
- feat(api): `ccuRelay` in `GET /api/system/overview` und
  `POST /api/system/relay-stats/reset` zum Zurücksetzen der Spitzenwerte
  (Frame-Zähler bleiben erhalten, damit die Verwerf-Rate ihren Nenner behält).

### Fixed
- fix(relay): UDP-Queue-Tiefe von 32 zurück auf 64 (Stand 2.1.10). Die
  Halbierung war auf einem einzelnen, kaum belasteten Prüfstand gemessen
  worden, nicht in einer Installation mit vielen Geräten; sie ist die einzige
  Regression im Relay-Pfad gegenüber 2.1.10, die Anwender tatsächlich spüren
  können. Kosten: ~1 KB Heap.
- fix(webui): Countdown und Text des Neustart-Sync-Overlays nannten
  unterschiedliche Dauern (120 s vs. 40 s). Der Neustart-Sync des Geräts
  hält Ethernet fest 35 s unten (`main/system_reset.cpp`); beide Zahlen
  kommen jetzt aus derselben Konstanten `FLASH_PAUSE_SECONDS` — auch in den
  Bestätigungsdialogen, die zuvor einen nicht interpolierten `{seconds}`-
  Platzhalter zeigten, und in den fr/it-Hinweisen (standen auf 40 s).
- fix(webui): Der WebSocket-Live-Stream des System-Logs war aus dem Browser
  dauerhaft unautorisiert (Endlos-401-Reconnect). `httpd_query_key_value()`
  liefert den Query-Wert undekodiert; der per `encodeURIComponent()` gesendete
  Base64-Admin-Token (`%2B`/`%2F`/`%3D`) stimmte nie mit dem gespeicherten
  Token überein. Der Wert wird jetzt vor dem Vergleich percent-dekodiert
  (`include/url_decode.h`, Host-Test `test_url_decode.cpp`).
- fix(webui): `/manifest.webmanifest` und `/icon-256.png` antworteten mit der
  gzipped `index.html`, weil die PWA-Routen nach dem `/*`-SPA-Catch-all
  registriert wurden und bei `httpd_uri_match_wildcard` die erste Registrierung
  gewinnt. Die Routen sind jetzt vor dem Catch-all registriert; PWA-Installation
  und Manifest laden wieder.
- fix(webui): Die Google-Fonts-CDN-Links in `index.html` waren durch die
  offline-first-CSP (`style-src/font-src 'self'`) blockiert und erzeugten bei
  jedem Laden einen Konsole-Fehler. Entfernt; das `--font-sans`-Fallback
  (Segoe UI/system-ui/Roboto) rendert unverändert.
- fix(webui): Das Live-Log zeigte rohe ANSI-Farbcodes (`[0;32m…`) je
  Zeile. Die Escape-Sequenzen werden jetzt beim Einlesen entfernt — Anzeige,
  Suche und Kopieren arbeiten mit der lesbaren Zeile; der Download liefert
  weiterhin die Rohdatei.

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

- **Firmware-Binary** (`firmware_2.2.7-Beta.11.bin`)
- **Kompatibles WebUI-Binary** (`webui_1.0.3.bin`)
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

- WebUI version: `1.0.3`
- WebUI API: `1`
- Minimum firmware: `2.2.5-Beta.1`
