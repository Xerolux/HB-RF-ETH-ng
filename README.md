# HB-RF-ETH-ng Firmware v2.1.4

[![GitHub Release][releases-shield]][releases]
[![Downloads][downloads-shield]][releases]
[![GitHub Activity][commits-shield]][commits]
[![License][license-shield]](LICENSE.md)


[![Buy Me A Coffee][buymeacoffee-badge]][buymeacoffee]
[![Tesla](https://img.shields.io/badge/Tesla-Referral-red?style=for-the-badge&logo=tesla)](https://ts.la/sebastian564489)


[![Release Management](https://github.com/Xerolux/HB-RF-ETH-ng/actions/workflows/release.yml/badge.svg)](https://github.com/Xerolux/HB-RF-ETH-ng/actions/workflows/release.yml)

## Modernisierte Fork von Xerolux (2025)

Diese Version ist eine modernisierte und aktualisierte Fork der originalen HB-RF-ETH Firmware von Alexander Reinert. Die Firmware wurde auf ESP-IDF 5.x portiert und für moderne Toolchains optimiert.

**Version 2.1.4 Änderungen:**
* **Fehlerbehebungen (Notfall-Fixes)**
  * Speicherleck bei OTA-Update per URL behoben (Memory Leak Fix)
  * Variablen-Shadowing in Update-Check behoben
  * Verbesserte Code-Qualität (Const Correctness)
* **WebUI**
  * Aktualisierung auf v2.1.4
  * Korrektur von Versionsanzeigen in verschiedenen Sprachen

**Version 2.1.3 Änderungen:**
* **Sicherheitsfix**
  * Migration von Parcel zu Vite Build-System behoben Origin Validation Error Schwachstelle
  * Entfernung aller anfälligen Parcel-Dependencies
* **Verbesserter Release-Workflow**
  * Firmware-Benennung korrigiert (Unterstriche statt Punkte)
  * Umfassende, automatisch generierte Release Notes mit allen wichtigen Informationen
  * Alle erforderlichen Dateien im Release enthalten (firmware, bootloader, partitions, checksums)
* **Build-System**
  * Wechsel zu Vite 6.3.5 (schneller, sicherer, moderner)
  - Build-Zeit reduziert von ~10s auf ~2s
  - Kleinere Bundle-Größen durch bessere Optimierung
* **Fehlerbehebungen**
  * Korrektur des Import-Syntax für vite-plugin-compression
  * Automatische npm-Dependency-Installation vor Build
  - Verbesserte Fehlerbehandlung bei OTA-Updates (verhindert Panic bei fehlerhaften Updates)

**Version 2.1.2 Änderungen:**
* **Erhöhte Sicherheit**
  * Stärkere Passwort-Anforderungen (8 Zeichen, Groß-/Kleinschreibung, Zahlen)
  * Automatischer Logout nach 5 Minuten Inaktivität
  * Timing-attacken-geschützte Passwortvergleiche
* **Verbesserte Firmware-Update-UX**
  * Passwort-Stärke-Anzeige beim Setzen
  * Automatischer Neustart nach erfolgreichem Update
  * Verbesserte Fehlerbehandlung verhindert Panic bei fehlerhaften Updates
  * OTA-Passwort-Anforderung entfernt (Authentifizierung ist ausreichend)
* **Detaillierte Neustart-Gründe** in der WebUI
  * Zeigt Ursache des letzten Neustarts (Update, Werksreset, Fehler, etc.)
* **OTA Update per URL** - Firmware direkt aus dem Netzwerk herunterladen
* **Werksreset** auch über WebUI möglich
* **Moderne, responsive WebUI** mit Mobile-Support
  * Dark/Light Theme Toggle
  * Multi-Language Support (10 Sprachen: DE, EN, ES, FR, IT, NL, NO, PL, CS, SV)
  * LED-Helligkeitssteuerung (0-100%)
* **Neue Monitoring-Features**
  * MQTT-Support mit Home Assistant Auto-Discovery
  * Check_MK Agent für erweitertes Monitoring
  * Detailliertes SNMP-Monitoring mit MIB-2 Unterstützung
* **Backup & Restore** der Einstellungen über die WebUI
* **Technische Updates**
  * Espressif32 Platform 6.12.0 (neuestes ESP-IDF)
  * Vue.js 3.5.27, Vue Router 5, Pinia 3, Vue i18n 11
  * Bootstrap Vue Next 0.43.0
  * HTTP gzip Kompression für schnellere Ladezeiten
  * Alle npm-Pakete auf dem neuesten Stand

**Version 2.1.1 Änderungen:**
* Aktualisierung auf ESP-IDF 5.x
* Kompatibilität mit modernen Toolchains (GCC 14.2.0)
* Aktualisierte WebUI mit Vue 3.5 und modernem Design
* Verbesserte Stabilität und Performance
* Modernisierte API-Verwendung (ADC, SNTP, mDNS)
* **Neues Sicherheitskonzept**
  * Erzwungene Passwortänderung beim ersten Login
  * **Separates OTA-Passwort** für Firmware-Updates
  * Timing-attacken-geschützte Passwortvergleiche

### Worum es geht
Dieses Repository enhält die Firmware für die HB-RF-ETH Platine, welches es ermöglicht, ein Homematic Funkmodul HM-MOD-RPI-PCB oder RPI-RF-MOD per Netzwerk an eine debmatic oder piVCCU3 Installation anzubinden.

Hierbei gilt, dass bei einer debmatic oder piVCCU3 Installation immer nur ein Funkmodul angebunden werden kann, egal ob die Anbindung direkt per GPIO Leiste, USB mittels HB-RF-USB(-2) Platine oder per HB-RF-ETH Platine erfolgt.

### Was kann die Firmware
* Bereitstellung des Funkmoduls RPI-RF-MOD oder HM-MOD-RPI-PCB per UDP als raw-uart Gerät inkl. Ansteuerung der LEDs des RPI-RF-MODs
* (S)NTP Server für die Verteilung der Zeit im lokalen Netzwerk
* Unterstützung der RTC des RPI-RF-MODs oder eines [DS3231 Aufsteckmoduls](https://www.amazon.de/ANGEEK-DS3231-Precision-Arduino-Raspberry/dp/B07WJSQ6M2)
* Verschiedene mögliche Zeitquellen
  * (S)NTP Client
  * DCF77 Empfänger (aka Funkuhr) mittels [optionalem Moduls](https://de.elv.com/elv-gehaeuse-fuer-externe-dcf-antenne-dcf-et1-komplettbausatz-ohne-dcf-modul-142883):
    * Konnektor J5
    * Pin 1: VCC
    * Pin 2: DCF Signal
    * Pin 3: Gnd
  * GPS Empfänger mittels [optionalem Moduls](https://www.amazon.de/AZDelivery-NEO-6M-GPS-baugleich-u-blox/dp/B01N38EMBF):
    * Konnektor J5
    * Pin 1: VCC
    * Pin 2: TX
    * Pin 3: Gnd
* MDNS Server um Platine im Netzwerk bekannt zu machen
* Netzwerkeinsettings per DHCP oder statisch konfigurierbar
* **IPv6 Support** (Auto-Konfiguration oder statisch)
* **Moderne WebUI zur Konfiguration**
  * Initiales Passwort: `admin` (muss nach dem ersten Login geändert werden)
  * Separates **OTA-Passwort** für Firmware-Updates (optional)
  * Responsive Design für Desktop und Mobile
  * **Dark/Light Theme Toggle** für helles und dunkles Design
  * **Multi-Language Support** (10 Sprachen: Deutsch, Englisch, Spanisch, Französisch, Italienisch, Niederländisch, Norwegisch, Polnisch, Tschechisch, Schwedisch)
  * **LED-Helligkeitssteuerung** (0-100%) für alle Status-LEDs
  * Detaillierte Systeminformationen und Neustart-Gründe
  * Automatischer Logout nach 5 Minuten Inaktivität
* **Firmware Updates**
  * Upload als .bin Datei
  * **Download per URL** (z.B. direkt von GitHub Releases)
  * Verbesserte Fehlerbehandlung verhindert Panic bei fehlerhaften Updates
  * Automatischer Neustart nach erfolgreichem Update
  * Anzeige verfügbarer Updates in der WebUI
  * Upload-Fortschrittsanzeige in Prozent
* Erkennung des Funkmoduls und Ausgabe von Typ, Seriennummer, Funkadresse und SGTIN in der WebUI
* Regelmäßige Prüfung auf Firmwareupdates
* Werksreset per Taster oder über die WebUI
* **Backup & Restore** der Einstellungen über die WebUI
* **Sicherheits-Features**
  * Timing-attacken-geschützte Passwortvergleiche
  * Rate Limiting bei Login-Versuchen
  * Moderne Security Headers (CSP, X-Frame-Options, etc.)
  * HTTP gzip Kompression für schnellere Übertragung
* **Monitoring und Überwachung**
  * **MQTT-Support**
    * Vollständige MQTT-Client-Integration
    * Home Assistant Auto-Discovery für automatische Einrichtung
    * Konfigurierbarer Server, Port und Authentifizierung
    * Status-Publishing für Systemmetriken
  * **SNMP Support** (Simple Network Management Protocol)
    * Überwachung von Systemmetriken (CPU, Speicher, Uptime, Temperatur, Spannung)
    * Standard MIB-2 Unterstützung
    * Konfigurierbarer SNMP Community String
    * Konfigurierbare Location und Contact Informationen
  * **Check_MK Agent**
    * Native Unterstützung für Check_MK/CheckMK Monitoring
    * Erweiterte Systemmetriken und Statusinformationen
    * IP-basierte Zugriffskontrolle
    * Einfache Integration in bestehende Monitoring-Infrastruktur
* **Hardware-Überwachung**
  * Echtzeit-Temperaturüberwachung
  * Spannungsüberwachung
  * CPU- und Speicheranzeige

### Bekannte Einschränkungen
* Nach einem Neustart der Platine (z.B. bei Stromausfall) findet kein automatischer Reconnect statt, in diesem Fall muss die CCU Software daher neu gestartet werden.
* Die Stromversorgung mittels des Funkmoduls RPI-RF-MOD darf nur erfolgen, wenn keine andere Stromversorgung (USB oder PoE) angeschlossen ist.

### Werksreset
Die Firmware kann auf zwei Arten auf Werkseinstellungen zurückgesetzt werden:

**Per Taster:**
1. Platine vom Strom trennen
2. Taster drücken und gedrückt halten
3. Stromversorgung wiederherstellen
4. Nach ca. 4 Sekunden fängt die rote Status LED schnell zu blinken an und die grüne Power LED hört auf zu leuchten
5. Taster kurz loslassen und wieder drücken und gedrückt halten
6. Nach ca. 4 Sekunden leuchten die grüne Power LED und die rote Status LED für eine Sekunde
7. Danach ist der Werkreset abgeschlossen und es folgt der normale Bootvorgang

**Über die WebUI:**
1. Anmelden in der WebUI
2. Zur Seite "Firmware Update" navigieren
3. Button "Factory Reset" klicken
4. Bestätigen und Neustart abwarten

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
Firmware Updates sind fertig kompiliert in den [Releases](https://github.com/Xerolux/HB-RF-ETH-ng/releases) zu finden und können auf zwei Arten eingespielt werden:

**Per Webinterface (File Upload):**
1. Herunterladen der `firmware.bin` Datei aus dem Release
2. In der WebUI zur Seite "Firmware Update" navigieren
3. Die .bin Datei hochladen
4. Update wird automatisch eingespielt und die Platine neu gestartet

**Per Webinterface (URL Download):**
1. In der WebUI zur Seite "Firmware Update" navigieren
2. Direkte URL zur .bin Datei eingeben (z.B. von GitHub)
3. Quick-Button für die neueste GitHub-Version nutzen
4. Firmware wird heruntergeladen, installiert und die Platine neu gestartet

**Sicherheitshinweise:**
- Die Standard-Authentifizierung schützt Firmware-Updates ausreichend
- Ein separates OTA-Passwort ist optional verfügbar
- Die Firmware validiert alle Updates vor der Installation
- Bei fehlerhaften Updates wird die OTA-Operation korrekt abgebrochen

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
