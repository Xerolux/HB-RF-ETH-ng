# 🚀 HB-RF-ETH-ng v2.2.7

[![License](https://img.shields.io/github/license/Xerolux/HB-RF-ETH-ng)](LICENSE.md)
[![Downloads](https://img.shields.io/github/downloads/Xerolux/HB-RF-ETH-ng/total)](https://github.com/Xerolux/HB-RF-ETH-ng/releases)

## 📋 Überblick

HB-RF-ETH-ng ist eine modernisierte Fork der originalen HB-RF-ETH Firmware von Alexander Reinert.
Diese Firmware ermöglicht es, ein Homematic Funkmodul (HM-MOD-RPI-PCB oder RPI-RF-MOD) per Netzwerk
an eine CCU-Installation (piVCCU3, debmatic, OpenCCU) anzubinden.

## 🆕 Was ist neu in v2.2.7?

### Changes
- chore: bump WebUI version to 1.0.6
- feat(diagnostics): count module resets by requester and widen the break window after a reset (#447)
- fix(release): deploy the manifest commit to Pages, not whatever "main" resolved to (#451)
- chore: update manifests for v2.2.7-Beta.13


### Changes
- chore: bump WebUI version to 1.0.5
- fix(diagnostics): keep module resets out of the UART line-error count and make the counters resettable (#447)
- chore: update manifests for v2.2.7-Beta.12


### Changes
- fix(diagnostics): keep UART capacity constants matchable by the policy test
- chore: bump WebUI version to 1.0.4
- feat(diagnostics): instrument radio-module UART and CCU send path (#447)
- docs(claude): radio module detection and bridge are UART-only
- chore: update manifests for v2.2.7-Beta.11


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


### Changes
- chore: update WebUI manifest for webui-v1.0.2

### Added
- feat(api): `cooldownRemainingSec` in `GET /api/update/status` und in der
  Cooldown-Antwort von `POST /api/update/check`. Das Fenster beginnt beim
  angenommenen Versuch (auch wenn er später übersprungen wird), deshalb kann
  die Oberfläche daraus exakt „in X Sekunden erneut versuchen“ anzeigen,
  statt aus `lastCheck` zu raten.
- feat(webui): Skip-Gründe der Update-Suche werden lokalisiert (zu wenig
  Speicher mit Kennzahlen, Installation läuft, Netzwerk beschäftigt,
  Versions-Puffer). Unbekannte Gründe neuerer Firmware werden unverändert
  angezeigt, statt versteckt zu werden. Die Cooldown-Meldung zeigt einen
  live tickenden Countdown „versuche es in X Sekunden erneut“.


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


### Changes
- fix(stability): serialize LEDC duty writes, restore high-speed mode (#362)
- chore: update manifests for v2.2.7-Beta.7


### Changes
- fix(stability): UART RX-FIFO overflow hardening + panic transcript capture (#362)
- docs: green NewDesign screenshots, real version, wiki set refreshed (#439)
- ci: skip the ESP-IDF ECO3 patch on the JavaScript CodeQL leg (#438)
- docs: refresh README screenshots from the current WebUI (#437)
- chore: update WebUI manifest for webui-v1.0.0


### Changes
- fix(stability): tcpip liveness sentinel, network-watchdog blindness fix, hardening (#362)
- docs: third-party audit response (Fable) for issue #362 - app delta exonerated, evidence points to IDF-internal/hardware
- docs: compact token-efficient audit brief for issue #362 (Fable mission: initiator wild-write audit + INTLEVEL holder)
- docs: handoff v2 for issue #362 - amplification topology mapped, initiator remains open; includes audit of first external AI response
- docs: AI investigation handoff for issue #362 - full state, eliminated paths, open leads, response contract
- feat(stability): network wedge self-healing + event-driven blackbox snapshots (#362)
- docs(planning): #362 status update - delta isolated to FreeRTOS SMP kernel, decision tree for field results
- chore: update manifests for v2.2.7-Beta.5


### Changes
- fix(stability): latch pre-reset tick sentinel; restore relay priority 15 (#362)
- chore: update manifests for v2.2.7-Beta.4


### Changes
- fix(stability): enable ESP32 ECO3 cache-livelock workaround for #362
- chore: update manifests for v2.2.7-Beta.3


### Changes
- fix(release): resolve F-01, F-02 and F-04 safety gaps (#430)
- chore(deps)(deps-dev): bump esbuild from 0.28.1 to 0.28.2 in /webui (#424)
- chore(deps)(deps): bump vue-i18n from 11.4.8 to 11.4.10 in /webui (#428)
- chore(deps)(deps): bump vue from 3.5.41 to 3.5.42 in /webui (#427)
- chore(deps)(deps-dev): bump sass from 1.102.0 to 1.103.1 in /webui (#425)
- chore(deps)(deps-dev): bump vite from 8.2.1 to 8.2.2 in /webui (#423)
- chore(ci)(deps): bump crate-ci/typos from 1.49.0 to 1.50.0 (#429)
- chore(ci)(deps): bump DavidAnson/markdownlint-cli2-action (#426)
- chore: ignore build-*/ output directories
- chore: update manifests for v2.2.7-Beta.2


### Changes
- fix(gps): validate NMEA RMC fix status, check digit ranges and normalize timeval
- fix(dcf): normalize timeval microsecond calculation and prevent out-of-range values
- fix(rtc): add null I2C bus handle guards in GetTime and SetTime
- fix(radiomodule): free detection semaphore after scan to prevent memory leak


### Changes
- chore(idf): upgrade ESP-IDF from v6.1-beta1 to v6.1-rc1 (#422)

### Changes
- feat: notification event selection, CCU latency diagnostics, CI gates and webui.cpp split (#420)
- fix(ota): drop the unused varargs from the OTA error setter
- test: follow the WebUI handlers into their new translation units
- style(webui): apply clang-format to the extracted WebUI units
- refactor(webui): split backup/restore and OTA out of webui.cpp
- fix(diag): repair the stack, CPU and NVS diagnostics used to size the device
- feat(diag): measure CCU relay latency so delayed switching can be diagnosed
- fix(ci): move ESLint to 10.x and drop a redundant assignment it caught
- feat(notify): let users choose which events trigger a notification
- ci: add ESLint, C++ format ratchet and Playwright gate; drop dead artifacts
- chore: update manifests for v2.2.6-Beta.11

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

- **Firmware-Binary** (`firmware_2.2.7.bin`)
- **Kompatibles WebUI-Binary** (`webui_1.0.6.bin`)
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

- WebUI version: `1.0.6`
- WebUI API: `1`
- Minimum firmware: `2.2.5-Beta.1`
