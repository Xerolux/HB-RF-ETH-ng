# HB-RF-ETH-ng Firmware v2.1.3

[![GitHub Release][releases-shield]][releases]
[![Downloads][downloads-shield]][releases]
[![GitHub Activity][commits-shield]][commits]
[![License][license-shield]](LICENSE.md)


[![Buy Me A Coffee][buymeacoffee-badge]][buymeacoffee]
[![Tesla](https://img.shields.io/badge/Tesla-Referral-red?style=for-the-badge&logo=tesla)](https://ts.la/sebastian564489)


[![Release Management](https://github.com/Xerolux/HB-RF-ETH-ng/actions/workflows/release.yml/badge.svg)](https://github.com/Xerolux/HB-RF-ETH-ng/actions/workflows/release.yml)

## Modernisierte Fork von Xerolux (2025)

Diese Version ist eine modernisierte und aktualisierte Fork der originalen HB-RF-ETH Firmware von Alexander Reinert. Die Firmware wurde auf ESP-IDF 5.x portiert und für moderne Toolchains optimiert.

**Version 2.1.2 Highlights:**
* **Framework**: ESP-IDF 5.5.1 (Platform espressif32@6.12.0) mit GCC 14.2.0 Toolchain
* **WebUI**: Vue 3.5.25, Parcel 2.16.3, Bootstrap 5.3.8
* **Sicherheit**: DTLS 1.2 Verschlüsselung, erzwungene Passwortänderung, Rate Limiting
* **Monitoring**: SNMP, Check_MK, MQTT Integration
* **Features**: HMLGW-Modus, Analyzer Light, IPv6 Support
* **Stabilität**: Optimierte Performance, Supply Voltage Monitoring, mbedTLS 3.6.4

### Worum es geht
Dieses Repository enhält die Firmware für die HB-RF-ETH Platine, welches es ermöglicht, ein Homematic Funkmodul HM-MOD-RPI-PCB oder RPI-RF-MOD per Netzwerk an eine debmatic oder piVCCU3 Installation anzubinden.

Hierbei gilt, dass bei einer debmatic oder piVCCU3 Installation immer nur ein Funkmodul angebunden werden kann, egal ob die Anbindung direkt per GPIO Leiste, USB mittels HB-RF-USB(-2) Platine oder per HB-RF-ETH Platine erfolgt.

### Was kann die Firmware

#### Kommunikation & Protokolle
* **Dual-Mode Funkmodul-Bereitstellung**:
  * **Raw UART über UDP**: Klassischer Modus für direkten UART-Zugriff
  * **HM-LGW Protokoll**: HomeMatic LAN Gateway Emulation über TCP (Port 2000 + Keep-Alive Port 2001)
* **Unterstützte Funkmodule**: RPI-RF-MOD, HM-MOD-RPI-PCB
  * Automatische Erkennung von Typ, Seriennummer, Firmwareversion
  * Ausgabe von BidCos/HmIP Radio MACs und SGTIN
  * Volle LED-Steuerung (RGB LEDs) des RPI-RF-MODs

#### Sicherheit & Verschlüsselung
* **DTLS 1.2 Verschlüsselung** für Raw UART UDP-Kommunikation
  * Cipher Suites: AES-128-GCM, AES-256-GCM, ChaCha20-Poly1305
  * PSK (Pre-Shared Key) Authentifizierung
  * Optional: Session Resumption für schnellere Verbindungsaufbauten
  * Sichere Schlüsselspeicherung in NVS
  * Detaillierte Verschlüsselungsstatistiken
* **Authentifizierung & Zugriffskontrolle**:
  * Token-basierte WebUI-Authentifizierung mit SHA-256 Hashing
  * Hardware-RNG für sichere Token-Generierung
  * Erzwungene Passwortänderung beim ersten Login
  * Rate Limiting: 5 Login-Versuche pro 60 Sekunden
  * IP-basierte Zugriffskontrolle für Check_MK

#### Netzwerk & Zeit-Synchronisation
* **Ethernet-Konnektivität**:
  * DHCP oder statische IPv4-Konfiguration
  * **IPv6-Unterstützung** (Auto/Static Mode)
  * Link-Speed-Erkennung (10/100 Mbps, Full/Half Duplex)
  * Mehrere DNS-Server konfigurierbar
* **mDNS Server**: Platine per Hostname.local im Netzwerk erreichbar (z.B. HB-RF-ETH-XXXXXX.local)
* **Zeit-Synchronisation** mit mehreren Quellen:
  * **(S)NTP Client**: Konfigurierbare NTP-Server (Standard: pool.ntp.org)
  * **(S)NTP Server**: Zeitverteilung an andere Geräte im Netzwerk (UDP Port 123)
  * **DCF77 Funkuhr**: Optional via [DCF-Modul](https://de.elv.com/elv-gehaeuse-fuer-externe-dcf-antenne-dcf-et1-komplettbausatz-ohne-dcf-modul-142883) an J5 (Pin 1: VCC, Pin 2: Signal, Pin 3: GND)
  * **GPS-Empfänger**: Optional via [NEO-6M GPS](https://www.amazon.de/AZDelivery-NEO-6M-GPS-baugleich-u-blox/dp/B01N38EMBF) an J5 (Pin 1: VCC, Pin 2: TX, Pin 3: GND)
  * **RTC-Module**: DS3231 oder RX8130 via I2C ([DS3231 Modul](https://www.amazon.de/ANGEEK-DS3231-Precision-Arduino-Raspberry/dp/B07WJSQ6M2))

#### Monitoring & Management
* **SNMP Support** (Simple Network Management Protocol):
  * MIB-2 Standard-Unterstützung
  * System-Metriken: CPU, Speicher, Uptime, Temperatur
  * Konfigurierbarer Community String, Location, Contact
  * Custom UDP Port (Standard: 161)
* **Check_MK Agent**:
  * Native Integration für professionelles Monitoring
  * Erweiterte Metriken und Statusinformationen
  * IP-basierte Zugriffskontrolle (Allowlist)
  * Standard Port 6556
* **MQTT Integration**:
  * Konfigurierbarer MQTT Broker (Server, Port, Credentials)
  * Topic Prefix anpassbar
  * Home Assistant Discovery Support
  * Status-Publishing

#### WebUI & Verwaltung
* **Moderne Web-Oberfläche** (Vue 3.5.25 + Bootstrap 5.3.8):
  * 10 Sprachen: EN, DE, ES, FR, IT, NL, PL, CS, NO, SV
  * Responsive Design für Desktop und Mobile
  * Initialpasswort: **admin** (muss nach dem ersten Login geändert werden)
* **Funktionen**:
  * Dashboard mit System-Übersicht
  * Umfassende Einstellungen (Netzwerk, Zeit, Sicherheit, Monitoring)
  * **Firmware-Update**: OTA-Updates per Webinterface mit Online-Update-Check
  * **Backup & Restore**: Komplette Konfiguration exportieren/importieren
  * **Analyzer Light**: Echtzeit-Funkrahmen-Analyse via WebSocket
  * **System-Neustart**: Direkter Restart aus dem WebUI
  * **System-Info**: CPU, RAM, Temperatur, Spannung, Ethernet-Status
  * **Supply Voltage Monitoring**: Farbcodierte Spannungsanzeige mit Warnungen

#### Diagnose & Wartung
* **Analyzer Light Feature**:
  * Echtzeit-Analyse von HomeMatic-Funkrahmen
  * WebSocket-basiertes Streaming
  * RSSI-Anzeige und Frame-Details
  * Mehrere gleichzeitige Clients unterstützt
* **Automatische Update-Prüfung**:
  * Regelmäßige Versionsüberprüfung
  * LED-Indikation bei verfügbaren Updates (langsames Blinken)
  * Optional: Prerelease-Versionen aktivierbar
* **Werksreset per Taster**: Physischer Button für Zurücksetzen auf Werkseinstellungen
* **LED-Status-Anzeige**: 5 LEDs (Power, Status, RGB für Funkmodul) mit konfigurierbarer Helligkeit (0-100%)
* **Detaillierte Systeminfo**:
  * Board-Typ-Erkennung (REV 1.8/1.10 Public/SK)
  * Reset-Grund-Reporting (Power-On, Software, Watchdog, etc.)
  * Uptime, Speichernutzung, CPU-Auslastung

### Technische Spezifikationen

#### Hardware
* **MCU**: ESP32 (Dual-Core Xtensa LX6)
* **Ethernet**: 10/100 Mbps mit Auto-MDI/MDIX
* **Funkmodule**: HM-MOD-RPI-PCB, RPI-RF-MOD
* **Schnittstellen**:
  * UART (Radio Modul, GPS/DCF77)
  * I2C (RTC Module)
  * GPIO (LEDs, Button, DCF77)
* **Board Revisionen**: REV 1.8, REV 1.10 (Public/SK Varianten)

#### Software
* **Framework**: ESP-IDF 5.5.1 (framework-espidf ~3.50501.0)
* **Platform**: espressif32 6.12.0
* **Toolchain**: xtensa-esp-elf 14.2.0
* **Build System**: PlatformIO + CMake
* **WebUI**: Vue 3.5.25, Parcel 2.16.3, Bootstrap 5.3.8
* **Security**: mbedTLS 3.6.4, OpenSSL 3.x compatible

#### Speicher
* **RAM-Nutzung**: ~18.9 KB von 327.7 KB (5.8%)
* **Flash-Nutzung**: ~918 KB von 1.9 MB (48.3%)
* **Partitionierung**: Custom (Bootloader, Partitions, OTA, Firmware)

#### Netzwerk
* **Protokolle**: TCP, UDP, HTTP, HTTPS, mDNS, NTP, SNMP, MQTT
* **Verschlüsselung**: DTLS 1.2 (TLS_PSK_WITH_AES_128_GCM_SHA256, TLS_PSK_WITH_AES_256_GCM_SHA384, TLS_PSK_WITH_CHACHA20_POLY1305_SHA256)
* **IPv4**: DHCP, Static IP
* **IPv6**: Auto/Static (experimentell)

#### API & Integration
* **REST API**: JSON-basierte HTTP-Endpunkte
* **WebSocket**: Echtzeit-Datenübertragung für Analyzer
* **SNMP**: MIB-2 kompatibel
* **Check_MK**: Native Agent-Integration
* **MQTT**: Broker-Client mit HA Discovery

### Bekannte Einschränkungen
* Nach einem Neustart der Platine (z.B. bei Stromausfall) findet kein automatischer Reconnect statt, in diesem Fall muss die CCU Software daher neu gestartet werden.
* Die Stromversorgung mittels des Funkmoduls RPI-RF-MOD darf nur erfolgen, wenn keine andere Stromversorgung (USB oder PoE) angeschlossen ist.
* **DTLS Verschlüsselung** funktioniert nur im Raw UART Modus, nicht kompatibel mit HM-LGW oder Analyzer Modus.
* **Analyzer Light** benötigt unverschlüsselte Daten und ist daher nicht kompatibel mit aktiviertem DTLS.

### Werksreset
Die Firmware kann per Taster auf Werkseinstellungen zurückgesetzt werden:
1. Platine vom Strom trennen
2. Taster drücken und gedrückt halten
3. Stromversorgung wiederherstellen
4. Nach ca. 4 Sekunden fängt die rote Status LED schnell zu blinken an und die grüne Power LED hört auf zu leuchten
5. Taster kurz loslassen und wieder drücken und gedrückt halten
6. Nach ca. 4 Sekunden leuchten die grüne Power LED und die rote Status LED für eine Sekunde
7. Danach ist der Werkreset abgeschlossen und es folgt der normale Bootvorgang

### Blinkcodes der LEDs
#### RPI-RF-MOD
Siehe Hilfe zum RPI-RF-MOD

#### Grüne Power LED und rote Status LED
* Blinken abwechselnd mit grüner Power LED: System bootet
* Schnelles Blinken der roten Status LED, grüne Power LED leuchtet nicht: Siehe Werksreset
* Schnelles Blinken der roten Status LED, grüne Power LED leuchtet dauerhaft: Firmware Update wird eingespielt
* Langsames Blinken der roten Status LED, grüne Power LED leuchtet dauerhaft: Es ist ein Firmware Update verfügbar
* Dauerhaftes Leuchten der grünen Power LED: Sytem ist gestartet

### Firmware Updates
Firmware Updates sind fertig kompiliert in den [Releases](https://github.com/Xerolux/HB-RF-ETH-ng/releases) zu finden und können per Webinterface eingespielt werden. Zum Übernehmen der Firmware muss die Platine neu gestartet werden (mittels Power-On Reset oder über den Neustart-Button im WebUI).

#### Update-Methoden
1. **Online-Update** (empfohlen):
   * Im WebUI unter "Firmware-Update" auf "Nach Updates suchen" klicken
   * Bei verfügbarem Update auf "Update installieren" klicken
   * Platine startet automatisch neu und installiert die neue Firmware

2. **Manuelles Update**:
   * Firmware-Datei (`firmware_X_X_X.bin`) von [Releases](https://github.com/Xerolux/HB-RF-ETH-ng/releases) herunterladen
   * Im WebUI unter "Firmware-Update" hochladen
   * Platine neu starten

3. **Serielle Programmierung** (für Entwickler):
   * Über USB-Serial-Adapter mit esptool.py oder PlatformIO

### Kompatible CCU-Systeme

Die HB-RF-ETH-ng Platine ist kompatibel mit verschiedenen CCU-Systemen:

#### OpenCCU
[OpenCCU](https://openccu.de/) ist ein freies, Open-Source-basiertes Betriebssystem für eine HomematicIP CCU-Zentrale. Es ist zu 100% funktionskompatibel mit der CCU3 und wird von vielen Anwendern bevorzugt.

**Hauptmerkmale:**
* Vollständige Kompatibilität mit HomematicIP und HomeMatic Geräten (Funk und Draht)
* Cloud-unabhängiger Betrieb - keine Internetverbindung erforderlich
* Läuft auf verschiedenen Plattformen:
  * Hardware: Raspberry Pi, Tinkerboard, ODROID
  * Virtualisierung: Docker, Proxmox, Home Assistant, etc.
* Verbesserte WebUI mit exklusiven Benutzerfreundlichkeitsverbesserungen
* Community-gesteuerte Entwicklung (entwickelt von Nutzern für Nutzer)
* Basiert auf der offiziellen OCCU-Umgebung von eQ3

Die HB-RF-ETH-ng Platine funktioniert nahtlos mit OpenCCU und ermöglicht die Netzwerkanbindung Ihres Funkmoduls.

#### piVCCU3 und debmatic
Die Unterstützung für die Platine HB-RF-ETH ist in piVCCU3 ab Version 3.51.6-41 und in debmatic ab Version 3.51.6-46 eingebaut. Die Installation der Platine erfolgt über das Paket "hb-rf-eth". Weitere Details finden Sie in der Installationsanleitung von piVCCU3 bzw. debmatic.

### Quick Start

1. **Erste Inbetriebnahme**:
   * Platine mit Ethernet und Stromversorgung verbinden
   * Funkmodul (RPI-RF-MOD oder HM-MOD-RPI-PCB) aufstecken
   * Nach ca. 10-15 Sekunden ist die Platine im Netzwerk erreichbar

2. **WebUI-Zugriff**:
   * Per mDNS: `http://HB-RF-ETH-XXXXXX.local` (XXXXXX = letzten 6 Zeichen der MAC-Adresse)
   * Per DHCP-IP: IP-Adresse aus dem Router auslesen
   * Anmeldung mit Benutzername: `admin` und Passwort: `admin`
   * **Wichtig**: Passwort beim ersten Login ändern!

3. **Grundkonfiguration**:
   * Hostname anpassen (optional)
   * Netzwerkeinstellungen konfigurieren (DHCP oder statisch)
   * LED-Helligkeit einstellen (optional)
   * Zeitquelle auswählen (NTP empfohlen)

4. **CCU-Integration**:
   * Bei **OpenCCU/piVCCU3/debmatic**: Paket `hb-rf-eth` installieren
   * Platine wird automatisch erkannt und als Funkmodul eingebunden
   * Details siehe [OpenCCU Dokumentation](https://openccu.de/) bzw. piVCCU3/debmatic Installationsanleitung

### Dokumentation

Detaillierte Dokumentation und Anleitungen finden Sie in den folgenden Dateien:

* **[API.md](docs/API.md)**: REST API Dokumentation mit allen Endpunkten
* **[DTLS_ENCRYPTION_GUIDE.md](docs/DTLS_ENCRYPTION_GUIDE.md)**: Umfassende Anleitung zur DTLS-Verschlüsselung
* **[DTLS_README.md](docs/DTLS_README.md)**: DTLS Übersicht und Konfiguration
* **[DTLS_QUICK_REFERENCE.md](docs/DTLS_QUICK_REFERENCE.md)**: DTLS Quick Reference
* **[TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)**: Fehlerbehebung und häufige Probleme
* **[CHANGELOG.md](CHANGELOG.md)**: Vollständige Versionshistorie mit allen Änderungen
* **[SECURITY.md](SECURITY.md)**: Sicherheitsrichtlinien und Meldung von Schwachstellen

### Support & Community

* **GitHub Issues**: [Bug-Reports und Feature-Requests](https://github.com/Xerolux/HB-RF-ETH-ng/issues)
* **GitHub Discussions**: Fragen und Austausch mit der Community
* **Original Repository**: [HB-RF-ETH by Alexander Reinert](https://github.com/alexreinert/HB-RF-ETH)

### Danksagung
Ein großer Dank geht an **Alexander Reinert** für die Entwicklung der originalen HB-RF-ETH Firmware und Hardware. Seine Arbeit bildet die Grundlage für diese modernisierte Version.

### Lizenz
Die Firmware steht unter Creative Commons Attribution-NonCommercial-ShareAlike 4.0 Lizenz.

<!-- Links -->
[releases-shield]: https://img.shields.io/github/release/Xerolux/HB-RF-ETH-ng.svg?style=for-the-badge
[releases]: https://github.com/Xerolux/HB-RF-ETH-ng/releases
[downloads-shield]: https://img.shields.io/github/downloads/Xerolux/HB-RF-ETH-ng/latest/total.svg?style=for-the-badge
[commits-shield]: https://img.shields.io/github/commit-activity/y/Xerolux/HB-RF-ETH-ng.svg?style=for-the-badge
[commits]: https://github.com/Xerolux/HB-RF-ETH-ng/commits/main
[license-shield]: https://img.shields.io/github/license/Xerolux/HB-RF-ETH-ng.svg?style=for-the-badge
[buymeacoffee]: https://www.buymeacoffee.com/xerolux
[buymeacoffee-badge]: https://img.shields.io/badge/buy%20me%20a%20coffee-donate-yellow.svg?style=for-the-badge
