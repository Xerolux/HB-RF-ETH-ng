# HB-RF-ETH-ng Wiki

Willkommen im Wiki der modernisierten HB-RF-ETH-ng Firmware. Hier finden Sie alle detaillierten Informationen zu Funktionen, Konfiguration und Betrieb.

## Aktuelle Verbesserungen (v2.2.0-Beta 7-10)

Die Firmware wurde kürzlich mit wichtigen neuen Funktionen und Verbesserungen aktualisiert:

### Neue Features
- **Benutzername + Passwort Login**: Standard-Benutzername `admin`, spaeter in den Einstellungen aenderbar
- **Hostname im Dashboard**: Der konfigurierte Hostname wird im Systemstatus prominent angezeigt
- **Session-Persistierung**: Login bleibt nach Neustarts erhalten
- **OTA Success Feedback**: Visueller Bestätigungs-Dialog nach erfolgreichen Updates
- **System Log Sharing**: Debug-Reports mit automatischer Maskierung sensibler Daten
- **Verbesserte TLS-Sicherheit**: Vollständiges Mozilla CA-Bundle für GitHub/Let's Encrypt

### Behobene Probleme
- Watchdog-Resets bei großen Release-Notes (Stack-Overflow)
- Sofortiges Logout bei Seitenladung (bei Boot-Zeit-Synchronisation)
- MQTT-Verbindungsstabilität und Reconnect-Logik
- Multipart-Upload für Debug-Reports

---

## Was kann die Firmware

### Funkmodul & Netzwerk
* Bereitstellung des Funkmoduls RPI-RF-MOD oder HM-MOD-RPI-PCB per UDP als raw-uart Gerät inkl. Ansteuerung der LEDs des RPI-RF-MODs
* Erkennung des Funkmoduls und Ausgabe von Typ, Seriennummer, Funkadresse und SGTIN in der WebUI
* **Stabile CCU-Verbindung** mit optimiertem UDP-Handling
  * Non-blocking UDP-Queue (64 Einträge) für zuverlässige Paketverarbeitung
  * Intelligentes Keep-Alive mit 10s Timeout und sofortiger Antwort
  * Sichere Disconnect-Erkennung ohne Race Conditions
* MDNS Server um Platine im Netzwerk bekannt zu machen
* Netzwerkeinstellungen per DHCP oder statisch konfigurierbar
* **IPv6 Support** (Auto-Konfiguration oder statisch)

### Zeitquellen & RTC
* (S)NTP Server für die Verteilung der Zeit im lokalen Netzwerk
* Unterstützung der RTC des RPI-RF-MODs oder eines [DS3231 Aufsteckmoduls](https://www.amazon.de/ANGEEK-DS3231-Precision-Arduino-Raspberry/dp/B07WJSQ6M2)
* Verschiedene mögliche Zeitquellen
  * (S)NTP Client
  * DCF77 Empfänger (aka Funkuhr) mittels [optionalem Moduls](https://de.elv.com/elv-gehaeuse-fuer-externe-dcf-antenne-dcf-et1-komplettbausatz-ohne-dcf-modul-142883):
    * Konnektor J5: Pin 1 VCC, Pin 2 DCF Signal, Pin 3 Gnd
  * GPS Empfänger mittels [optionalem Moduls](https://www.amazon.de/AZDelivery-NEO-6M-GPS-baugleich-u-blox/dp/B01N38EMBF):
    * Konnektor J5: Pin 1 VCC, Pin 2 TX, Pin 3 Gnd

### Moderne WebUI
* **Responsive Design** für Desktop und Mobile mit glassmorphen Effekten und modernem Styling
* Login mit Benutzername und Passwort
  * Standard-Benutzername: `admin`
  * Initiales Passwort: `admin` (muss nach dem ersten Login geändert werden)
  * Nach Updates mit bestehendem Passwort einmalig als `admin` anmelden; der Benutzername kann danach unter Einstellungen geändert werden
* **Dark/Light Theme Toggle** für helles und dunkles Design
* **Multi-Language Support** (10 Sprachen: Deutsch, Englisch, Spanisch, Französisch, Italienisch, Niederländisch, Norwegisch, Polnisch, Tschechisch, Schwedisch)
* **System Log Viewer** - Live-Ansicht der Systemlogs mit 3-Sekunden-Polling, Download-Funktion, Ein/Aus-Schalter und manueller Aktualisierung (High-Contrast)
  * Aktivierung bleibt ueber Reboots erhalten; beim Deaktivieren bleibt das Log nach dem naechsten Start wieder aus
* **Changelog Modal** - Vollständiger Changelog direkt in der WebUI mit Markdown-Rendering
* **Dashboard** mit Gradient-Icons, Hover-Effekten und kompaktem 3-Spalten-Grid auf Mobile
* **Hostname-Anzeige im Systemstatus und Header** - Der unter Einstellungen/Netzwerk konfigurierte Hostname wird auf der Startseite, in der Kopfzeile und im Browser-Tab angezeigt, hilfreich bei mehreren HB-RF-ETH-ng Boards
* **Sponsor-Button** im Footer mit verschiedenen Optionen (PayPal, Buy Me a Coffee, Tesla referral)
* **LED-Helligkeitssteuerung** (0-100%) für alle Status-LEDs
* **Konfigurierbare LED-Programme** für verschiedene Systemzustände
  * 11 verschiedene LED-Muster (Aus, An, Blinken, Breathing, Herzschlag, Strobe, etc.)
  * Separate Programme für Idle, CCU-Status, Update, Error, Booting
  * Alle LEDs können komplett ausgeschaltet werden (Helligkeit 0% oder Pattern "Aus")
* Detaillierte Systeminformationen und Neustart-Gründe
* **Neustart-Bestätigungsdialog** beim Speichern von Einstellungen (z.B. CCU IP)
* Barrierefreiheits-Optimierungen (Accessibility, `aria-hidden` für dekorative Icons)

### Firmware Updates
* Upload als .bin Datei mit Fortschrittsanzeige in Prozent
* **Online-Update über URL** über den integrierten Update-Dienst
* Automatische Prüfung auf neue Versionen mit Update-Banner in der WebUI
* Automatischer Neustart nach erfolgreichem Update
* Robuste Fehlerbehandlung verhindert Panic bei fehlerhaften Updates
* Werksreset per Taster oder über die WebUI

### Sicherheit
* **Sichere JSON-Verarbeitung** mit cJSON-Bibliothek (keine Buffer Overflows)
* **Memory Safety** - `secure_zero` zum sicheren Löschen sensibler Daten, XOR-basierter Constant-Time `secure_strcmp`
* **Benutzername + Passwort Authentifizierung** - Standard-Benutzername `admin`, spaeter in den Einstellungen aenderbar
* Stärkere Passwort-Anforderungen (8 Zeichen, Groß-/Kleinschreibung, Zahlen) mit Stärke-Anzeige
* **Persistente Sessions** via `localStorage` mit automatischem Logout nach 10 Minuten Inaktivität (Cross-Tab-synchronisiert)
* **Rate Limiting** bei Login-Versuchen mit vollständigem IPv4/IPv6-Support
* Moderne Security Headers (CSP, X-Frame-Options, etc.)
* HTTP gzip Kompression für schnellere Übertragung
* **Backup & Restore** der Einstellungen über die WebUI
  * Der Administrator-Benutzername wird im Backup gespeichert und beim Restore wiederhergestellt
  * Die System-Log-Aktivierung wird im Backup gespeichert und beim Restore wiederhergestellt
  * Das Administrator-Passwort wird aus Sicherheitsgründen nicht exportiert; beim Restore bleibt das aktuelle Passwort des Zielgeräts erhalten

### Netzwerk-Optimierung
* **DNS-Caching** für schnellere Verbindungen und reduzierten Netzwerk-Traffic
  * Cache für 8 DNS-Einträge mit konfigurierbarer TTL
  * Automatische Aufräumung abgelaufener Einträge
  * Deutliche Performance-Verbesserung bei häufigen DNS-Abfragen

### Monitoring und Überwachung
* **MQTT-Support mit Home Assistant Integration**
  * Vollständige MQTT-Client-Integration mit Thread-Safety
  * **Home Assistant Auto-Discovery** für automatische Einrichtung (Standard: deaktiviert)
  * Konfigurierbarer Server, Port und Authentifizierung
  * **Command Token Security** - Optionaler Token für sichere Befehlsausführung
  * Status-Publishing für Systemmetriken (adaptives Intervall: 5s idle / 1s bei OTA)
  * **HA-Sensoren** für alle Systemmetriken (CPU, RAM, Temperatur, Spannung, Uptime)
  * **HA-Buttons** für Restart, Factory Reset, Firmware-Updates und Update-Checks
  * **HA-Update-Integration** - OTA-Updates direkt aus Home Assistant
  * **Echtzeit OTA-Status** - Progress und Error-Meldungen in Echtzeit
  * **TLS/mTLS Support** - Verschlüsselte Verbindungen und Client-Zertifikate
* **Check_MK Agent**
  * Native Unterstützung für Check_MK/CheckMK Monitoring
  * Erweiterte Systemmetriken und Statusinformationen
  * IP-basierte Zugriffskontrolle (IPv4/IPv6)
* **Hardware-Überwachung** - Echtzeit-Temperatur, Spannung, CPU- und Speicheranzeige
* **Event-basierte Benachrichtigungen** - Nicht-retained Events für Automatisierungen

### Technische Basis
* **ESP-IDF 6.0.1** mit nativer `idf.py` Toolchain
* **Xtensa GCC 14.2.0+20251107** Toolchain (xtensa-esp-elf)
* **CMake** Build-System für Firmware (nicht PlatformIO)
* **Vue.js 3** mit Composition API, Vue Router, Pinia, Vue i18n
* **Bootstrap 5** + **Bootstrap Vue Next** UI-Komponentenbibliothek
* **Vite** Build-System (schnelle Builds, optimierte Bundles)

### Build & Abhängigkeiten
* Managed Components: `mdns ^1.11.2`, `mqtt ^1.0.0`
* External: `esp-eth-drivers` für LAN87xx PHY
* Modern C++17/20 für Firmware-Code
* Komplett native ESP-IDF Toolchain (keine PlatformIO-Abhängigkeit)

## Mobile Ansicht
Die Benutzeroberfläche wurde speziell für mobile Endgeräte optimiert und bietet eine intuitive Bedienung auf Smartphones und Tablets.

## Sicherheit & Sessions

### Token Persistierung (ab v2.2.0-Beta.7)
* **Sessions überleben Neustarts**: Der Authentifizierungs-Token wird in NVS gespeichert
* Login bleibt nach Firmware-Updates und Geräte-Reboots erhalten
* Beim Update auf die Benutzername-Pflicht werden alte Browser-Tokens einmalig ungueltig; Anmeldung danach mit Benutzername `admin` und dem bestehenden Administrator-Passwort
* **Automatisches Logout** nach 10 Minuten Inaktivität (Cross-Tab-synchronisiert)
* Token wird bei Passwortänderung neu generiert und beim Werkreset gelöscht

### Sicherheits-Header
* Content Security Policy (CSP) aktiviert
* X-Frame-Options für Clickjacking-Schutz
* gzip-Kompression für schnellere Übertragung
* Rate Limiting bei Login-Versuchen (5 Versuche/Minute pro IP)

## Bekannte Einschränkungen
* Nach einem Firmware-Wechsel oder Netzwerkproblem kann es je nach CCU-Setup kurz dauern, bis die Verbindung wieder sichtbar ist. Mit aktuellen Versionen wurden Reconnect und mDNS-Ankündigung bereits verbessert.
* Die Stromversorgung mittels des Funkmoduls RPI-RF-MOD darf nur erfolgen, wenn keine andere Stromversorgung (USB oder PoE) angeschlossen ist.

## Werksreset
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

## Blinkcodes der LEDs
### RPI-RF-MOD
Siehe Hilfe zum RPI-RF-MOD

### Grüne Power LED und rote Status LED
* Blinken abwechselnd mit grüner Power LED: System bootet
* Schnelles Blinken der roten Status LED, grüne Power LED leuchtet nicht: Siehe Werksreset
* Schnelles Blinken der roten Status LED, grüne Power LED leuchtet dauerhaft: Firmware Update wird eingespielt
* Langsames Blinken der roten Status LED, grüne Power LED leuchtet dauerhaft: Es ist ein Firmware Update verfügbar
* Dauerhaftes Leuchten der grünen Power LED: Sytem ist gestartet

## Firmware Updates
Firmware Updates sind fertig kompiliert in den [Releases](https://github.com/Xerolux/HB-RF-ETH-ng/releases) zu finden und können auf zwei Arten eingespielt werden:

**Per Webinterface (File Upload):**
1. Herunterladen der `firmware.bin` Datei aus dem Release
2. In der WebUI zur Seite "Firmware Update" navigieren
3. Die .bin Datei hochladen
4. Update wird automatisch eingespielt und die Platine neu gestartet
5. **Nach erfolgreichem Update**: Automatische Weiterleitung zur Startseite mit Bestätigungs-Dialog

**Per Webinterface (URL Download):**
1. In der WebUI zur Seite "Firmware Update" navigieren
2. Direkte URL zur .bin Datei eingeben (z.B. von GitHub)
3. Online-Update starten; die WebUI nutzt dafür den konfigurierten Update-Dienst
4. Firmware wird heruntergeladen, installiert und die Platine neu gestartet
5. **Nach erfolgreichem Update** (ab v2.2.0-Beta.9): 
   - Dialog "Update zu Version X erfolgreich" wird angezeigt
   - Automatisches Schließen nach 10 Sekunden oder bei Benutzerbestätigung

**Sicherheitshinweise:**
- Die Standard-Authentifizierung schützt Firmware-Updates ausreichend
- Die Firmware validiert alle Updates vor der Installation
- Bei fehlerhaften Updates wird die OTA-Operation korrekt abgebrochen
- TLS/Zertifikat-Verifikation (ab v2.2.0-Beta.10): Vollständiges Mozilla CA-Bundle für Kompatibilität mit Let's Encrypt / GitHub CDN

## Systemlog & Debug-Informationen (ab v2.2.0-Beta.8)

### System Log Sharing
Die WebUI bietet eine "Share"-Funktion um einen umfassenden Debug-Report zu erstellen:

**Inhalte des Debug-Reports:**
- Systeminfo (Version, Board-Revision, Seriennummer)
- Netzwerk-Konfiguration (IP, DNS, Ethernet-Status)
- Funkmodul-Status (Typ, Seriennummer, Firmware)
- Monitoring-Konfiguration (MQTT/CheckMK Settings)
- LED-Programme und Status
- Vollständiges Systemlog

**Sicherheit beim Sharing:**
- **Automatische Maskierung** sensibler Daten:
  - Passwörter werden als `****` maskiert
  - MQTT Command Tokens als `****` maskiert
  - TLS-Keys/Zertifikate als `<set>` gekennzeichnet
  - IP-Adressen und Hostnames bleiben für Debugging erhalten

**Upload-Dienst:**
- Reports werden zu MicroBin Paste-Dienst hochgeladen
- Teilbarer Link wird generiert und in die Zwischenablage kopiert
- Ideal für Support-Anfragen und Fehlerberichte

## Notfall-Wiederherstellung (Rescue Script)
Sollte die WebUI nicht mehr erreichbar sein, aber die Platine noch im Netzwerk antworten (Ping), kann die Firmware über das mitgelieferte Python-Script `test_ota_function.py` neu installiert werden.

**Voraussetzung:**
- Python 3 installiert
- Platine ist im Netzwerk erreichbar
- Admin-Passwort ist bekannt

**Anwendung:**
Das Script befindet sich im Root-Verzeichnis des Repositories.

```bash
# Syntax
python3 test_ota_function.py <IP-ADRESSE> <PASSWORD> [--url <FIRMWARE_URL>]

# Beispiel (mit Standard-URL zur neuesten Firmware)
python3 test_ota_function.py 192.168.1.100 meinPasswort

# Beispiel (mit eigener URL)
python3 test_ota_function.py 192.168.1.100 meinPasswort --url http://192.168.1.50/firmware.bin
```

Das Script authentifiziert sich, triggert das OTA-Update und überwacht den Fortschritt bis zum erfolgreichen Neustart.

## Kompatible CCU-Systeme

Die HB-RF-ETH-ng Platine ist kompatibel mit verschiedenen CCU-Systemen:

### OpenCCU
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

### piVCCU3 und debmatic
Die Unterstützung für die Platine HB-RF-ETH ist in piVCCU3 ab Version 3.51.6-41 und in debmatic ab Version 3.51.6-46 eingebaut. Die Installation der Platine erfolgt über das Paket "hb-rf-eth". Weitere Details finden Sie in der Installationsanleitung von piVCCU3 bzw. debmatic.

## Home Assistant MQTT Integration

Die HB-RF-ETH-ng Firmware bietet eine nahtlose Integration in Home Assistant via MQTT mit Auto-Discovery. Sobald MQTT in der WebUI konfiguriert und die HA-Integration aktiviert ist, werden alle Geräte und Sensoren automatisch in Home Assistant eingerichtet.

### Einrichtung

**Voraussetzungen:**
- MQTT-Broker muss in Home Assistant laufen (z.B. Mosquitto Add-on)
- HB-RF-ETH-ng muss mit dem MQTT-Broker verbunden sein

**Schritt-für-Schritt:**

1. **MQTT in der WebUI konfigurieren:**
   - Navigieren Sie zu "Monitoring" → "MQTT"
   - Aktivieren Sie MQTT und geben Sie die Broker-Verbindungsdaten ein
   - **WICHTIG:** Aktivieren Sie "Home Assistant Discovery" (Standard: deaktiviert)
   - Passen Sie bei Bedarf die Topic-Präfixe an (Standardwerte sind meistens optimal)

2. **Verbindung testen:**
   - Nach dem Speichern verbindet sich die Firmware mit dem MQTT-Broker
   - Die Status-LED zeigt eine erfolgreiche Verbindung an

3. **Home Assistant Konfiguration:**
   - In Home Assistant werden automatisch alle Geräte gefunden
   - Keine manuelle Konfiguration erforderlich!

### Verbindungseinstellungen (Gerät → Broker)

| Parameter | Standard | Beschreibung |
|-----------|----------|--------------|
| Transport | TCP (`mqtt://`) | Wechselt automatisch auf SSL (`mqtts://`) wenn `tlsEnable` an ist |
| Netzwerk-Timeout | 2000 ms | TCP-Connect / Socket-Timeout |
| Reconnect-Intervall | 30 000 ms | Pause zwischen Verbindungsversuchen |
| Keep-Alive | ESP-IDF-Default (120 s) | MQTT PINGREQ-Intervall |
| LWT Topic | `<prefix>/status/online` | Last Will & Testament, retained, QoS 1 |
| LWT Payload offline | `offline` (7 Bytes) | Wird vom Broker bei unclean disconnect gesetzt |
| Birth Payload | `online` | Beim (Re)Connect sofort gesendet, retained QoS 0 |
| Status-Publish-Intervall | 5 s (idle) / 1 s (OTA laufend) | Adaptives Intervall |
| Command-Subscribe | `<prefix>/command/#` | Nur wenn `commandEnabled` ODER `haDiscoveryEnabled` |

---

## MQTT-API-Referenz

Diese Sektion listet **alle** vom Gerät gepublishden bzw. abonnierten MQTT-Topics
auf. Generiert aus `main/mqtt_handler.cpp` (Stand v2.2.0-Beta.7).

### Topic-Struktur Überblick

```
<prefix>/                         Standard: "hb-rf-eth"
├── status/    (retained, QoS 0)  – Periodische Status-/Metrik-Werte
├── event/     (NICHT retained)   – Einmalige Ereignisse
└── command/   (Subscriber-Seite) – Steuerkommandos an das Gerät

<ha_prefix>/                      Standard: "homeassistant"
└── {sensor|binary_sensor|button|update}/hb-rf-eth-<serial>/<obj_id>/config
                                  – HA Auto-Discovery Configs (retained, QoS 1)
```

### 1. Status Topics (`<prefix>/status/*`)

Alle Status-Werte sind **retained** und werden mit **QoS 0** veröffentlicht.
Das Publish-Intervall beträgt 5 Sekunden im Idle-Zustand bzw. 1 Sekunde während
eines OTA-Updates. Nach jedem (Re)Connect erfolgt sofort ein vollständiger
Publish-Cycle plus optional eine HA-Discovery-Runde.

#### Identity & Version

| Topic | Typ | Beispiel | Beschreibung |
|-------|-----|----------|--------------|
| `status/online` | string | `online` / `offline` | LWT-Birth-Marker; "offline" wird vom Broker bei unclean disconnect gesetzt |
| `status/serial` | string | `A1B2C3D4E5F6` | ESP32-MAC-basierte Geräteseriennr. |
| `status/version` | string | `2.2.0-Beta.7` | Aktuell laufende Firmware-Version |
| `status/board_revision` | string | `REV 1.10 (PUB)` | Hardware-Revision der Platine |
| `status/latest_version` | string | `2.2.0` oder `n/a` | Neueste Version laut GitHub Releases (Beta/Stable je nach Kanal) |
| `status/update_available` | bool-string | `true` / `false` | `true`, wenn `latest > current` per Semver |

#### System-Metriken

| Topic | Typ | Beispiel | Beschreibung |
|-------|-----|----------|--------------|
| `status/cpu_usage` | float % | `12.5` | CPU-Auslastung (1 Dezimalstelle) |
| `status/memory_usage` | float % | `45.3` | RAM-Auslastung (1 Dezimalstelle) |
| `status/free_heap` | uint64 B | `184320` | Aktuell freier Heap (internes RAM) |
| `status/min_free_heap` | uint64 B | `145000` | Kleinster je beobachteter freier Heap seit Boot (Leak-Detektion) |
| `status/uptime` | uint64 s | `345678` | Laufzeit in Sekunden seit Boot (monoton steigend) |
| `status/uptime_text` | string | `4 d, 0 h, 1 m` | Menschenlesbare Laufzeit |
| `status/last_reset_reason` | string | `Power-On Reset` | Letzter Reset-Grund (Hardware + App-Ebene) |

#### Ethernet / Netzwerk

| Topic | Typ | Beispiel | Beschreibung |
|-------|-----|----------|--------------|
| `status/eth_connected` | bool-string | `true` | Ethernet-Link aktiv |
| `status/eth_link_speed` | int Mbit/s | `100` | Verhandelte Link-Geschwindigkeit |
| `status/eth_duplex` | string | `Full` / `Half` | Duplex-Modus (nur wenn Link aktiv) |
| `status/ip_address` | IPv4 | `192.168.1.100` | Aktuelle IPv4-Adresse |
| `status/gateway` | IPv4 | `192.168.1.1` | Aktuelles IPv4-Gateway |

> IPv6-Topics werden derzeit nicht gepublished; IPv6-Adresse(n) sind über
> `GET /sysinfo.json` abrufbar.

#### Funkmodul

| Topic | Typ | Beispiel | Beschreibung |
|-------|-----|----------|--------------|
| `status/radio_module_type` | enum-string | `RPI-RF-MOD` | `HM-MOD-RPI-PCB`, `RPI-RF-MOD`, `HmIP-RFUSB`, `none`, `unknown` |
| `status/radio_module_serial` | string | `KEQ0123456` | Seriennummer des Funkmoduls |
| `status/radio_module_firmware` | string | `2.8.6` | Firmware-Version des Funkmoduls (Format `X.Y.Z`) |

#### Zeitquelle / NTP

| Topic | Typ | Beispiel | Beschreibung |
|-------|-----|----------|--------------|
| `status/ntp_synced` | bool-string | `true` | Systemzeit ist synchronisiert |
| `status/last_ntp_sync` | uint64 | `1735300000` | Unix-Sekunden des letzten erfolgreichen Sync; `0` wenn nie synchron |

#### OTA-Zustand

Die OTA-Topics werden **zusätzlich** in Echtzeit aktualisiert (innerhalb ~1 s
nach State-Change), nicht nur im 5-Sekunden-Turnus. Progress-Events werden
höchstens alle ~5 % gepublished, um MQTT-Flooding zu vermeiden.

| Topic | Typ | Beispiel | Beschreibung |
|-------|-----|----------|--------------|
| `status/ota_state` | enum-string | `downloading` | `idle`, `checking`, `starting`, `downloading`, `flashing`, `success`, `failed` |
| `status/ota_progress` | int % | `42` | 0..100; `-1` wenn unbekannt/nicht im Download |
| `status/ota_error` | string | `Bundle verification failed` | Fehlertext; **nur** wenn `ota_state = failed` |

---

### 2. Event Topics (`<prefix>/event/*`)

Events sind **nicht retained** und werden mit **QoS 0** veröffentlicht – sie
sind punktuelle Benachrichtigungen für Automatisierungen (HA, Node-RED, …).

| Topic | Payload | Auslöser |
|-------|---------|----------|
| `event/restart` | `requested` | Gerät restartet via MQTT-Kommando (vor dem tatsächlichen Reboot) |
| `event/factory_reset` | `requested` | Werkreset via MQTT-Kommando (vor dem Löschen der NVS) |
| `event/update_started` | `requested` | OTA-Update via MQTT `update`-Kommando gestartet |
| `event/update_downloading` | `started` | OTA-State-Wechsel `starting` → `downloading` |
| `event/update_finished` | `success` oder `failed: <text> (code=0x<hex>)` | OTA-State-Wechsel nach `success` oder `failed` |
| `event/update_failed` | `task_create_failed` / `updatecheck_unavailable` | OTA konnte nicht gestartet werden (interner Fehler) |
| `event/check_update` | `requested` / `updatecheck_unavailable` | `check_update`-Kommando erhalten (Refresh läuft asynchron) |
| `event/command_rejected` | `rejected cmd=<name> reason=invalid_token` | Kommando mit falschem/fehendem Token empfangen |

---

### 3. Command Topics (`<prefix>/command/*`)

Das Gerät abonniert `<prefix>/command/#` (QoS 1) **nur**, wenn `commandEnabled`
ODER `haDiscoveryEnabled` aktiv ist. Ohne diese Flags wird kein Kommando
angenommen – auch nicht mit korrektem Token.

| Command-Topic | Aktion | Vorbedingung |
|---------------|--------|--------------|
| `command/restart` | Gerät neustarten (Reset-Grund `USER_RESTART`) | `commandEnabled` |
| `command/factory_reset` | NVS löschen + Werkreset + Reboot | `commandEnabled` |
| `command/update` | OTA-Update der neuesten Version triggern | `commandEnabled` |
| `command/check_update` | GitHub Releases neu abfragen (Beta/Stable je nach Setting) | `commandEnabled` |

#### Payload-Regeln

- **Ohne Token (`command_token == ""`):** Payload wird ignoriert, jedes Publish
  auf das Topic löst die Aktion aus. **Broker-seitige ACL zwingend erforderlich.**
- **Mit Token:** Payload muss Byte-genau dem konfigurierten `command_token`
  entsprechen (8–63 Zeichen, Zeichensatz `A–Z a–z 0–9 - _ .`). Andernfalls wird
  das Kommando verworfen und `event/command_rejected` gepublished.
- Bei gesetztem Token wird der Token als `payload_press` / `payload_install` in
  die HA-Discovery-Config geschrieben, so dass HA-Buttons "einfach funktionieren".

---

### 4. Home Assistant Auto-Discovery

Die Firmware veröffentlicht unter
`<ha_prefix>/<component>/hb-rf-eth-<serial>/<obj_id>/config`
eine vollständige HA-MQTT-Discovery-Config (retained, QoS 1). Discovery-Configs
werden beim (Re)Connect automatisch neu gepublished, falls `haDiscoveryEnabled`
aktiv ist.

#### Sensoren (`sensor/`)

| Object ID | Name | Class | Unit | Icon |
|-----------|------|-------|------|------|
| `cpu_usage` | CPU Usage | measurement | % | `mdi:cpu-64-bit` |
| `memory_usage` | Memory Usage | measurement | % | `mdi:memory` |
| `free_heap` | Free Heap | data_size, measurement | B | `mdi:memory` |
| `uptime` | Uptime | duration, total_increasing | s | `mdi:clock-outline` |
| `uptime_text` | Uptime (Text) | – | – | `mdi:clock-outline` |
| `version` | Current Version | – | – | `mdi:package-variant` |
| `latest_version` | Latest Version | – | – | `mdi:package-up` |
| `board_revision` | Board Revision | – | – | `mdi:expansion-card` |
| `eth_link_speed` | Ethernet Speed | data_rate, measurement | Mbit/s | `mdi:speedometer` |
| `ip_address` | IP Address | – | – | `mdi:ip` |
| `radio_module_type` | Radio Module | – | – | `mdi:radio-tower` |
| `radio_module_serial` | Radio Serial | – | – | `mdi:barcode` |
| `radio_module_firmware` | Radio Firmware | – | – | `mdi:chip` |
| `ota_progress` | OTA Progress | measurement | % | `mdi:progress-download` |

Alle Sensoren haben `entity_category: "diagnostic"`.

#### Binary Sensoren (`binary_sensor/`)

| Object ID | Name | Class | Payload on / off |
|-----------|------|-------|------------------|
| `online` | Online | connectivity | `online` / `offline` (LWT) |
| `eth_connected` | Ethernet Link | connectivity | `true` / `false` |
| `ntp_synced` | NTP Synced | – | `true` / `false` |
| `update_available` | Update Available | update | `true` / `false` |

Alle haben `entity_category: "diagnostic"`.

#### Buttons (`button/`)

| Object ID | Name | Class | Command | Icon |
|-----------|------|-------|---------|------|
| `restart` | Restart | restart | `command/restart` | `mdi:restart` |
| `factory_reset` | Factory Reset | restart | `command/factory_reset` | `mdi:lock-reset` |
| `check_update` | Check for Update | update | `command/check_update` | `mdi:refresh` |

Buttons werden **nur gepublished, wenn `commandEnabled = true`**. Ansonsten
sieht HA keinen klickbaren Button.

`entity_category: "config"`. `payload_press` entspricht dem Token (oder dem
Kommando-String, wenn kein Token gesetzt ist).

#### Update-Entität (`update/`)

| Object ID | Name | Class | State Topic | Command Topic |
|-----------|------|-------|-------------|---------------|
| `firmware_update` | Firmware Update | firmware | `status/latest_version` | `command/update` |

Vergleicht die laufende Version mit `latest_version_template` und bietet eine
"Install"-Aktion, die das OTA-Kommando triggert. Hat
`enabled_by_default: false`, wenn `commandEnabled = false`.


---

### 5. TLS / mTLS-Konfiguration

MQTT unterstützt Transport-Verschlüsselung und Client-Zertifikate. Konfiguriert
via `POST /api/monitoring` (siehe [REST API](API.md)):

| Feld | Typ | Beschreibung |
|------|-----|--------------|
| `tlsEnable` | bool | false → TCP (1883); true → TLS (8883 empfohlen) |
| `tlsSkipVerify` | bool | true überspringt CN-Check + Cert-Verifikation (nur Lab!) |
| `tlsCaCerts` | PEM string | Eigenes CA-Bundle; leer → eingebautes ESP-IDF CA-Bundle wird verwendet |
| `tlsCertfile` | PEM string | Client-Zertifikat für mTLS; leer → kein Client-Cert |
| `tlsKeyfile` | PEM string | Privater Schlüssel zum Client-Zertifikat; leer → kein Client-Cert |

Verbindungslogik in der Firmware (`main/mqtt_handler.cpp:747`):

1. `tlsEnable = false` → unverschlüsseltes TCP.
2. `tlsEnable = true` + `tlsSkipVerify = true` → TLS ohne jede Verifikation
   (selbst-signierte Certs, privates Lab).
3. `tlsEnable = true` + `tlsSkipVerify = false` + `tlsCaCerts` gesetzt →
   Verifikation gegen das hinterlegte PEM.
4. `tlsEnable = true` + `tlsSkipVerify = false` + `tlsCaCerts` leer →
   Verifikation gegen das eingebaute Mozilla/ESP-IDF CA-Bundle.
5. `tlsCertfile` + `tlsKeyfile` gesetzt → mTLS (mutual TLS), Broker
   authentifiziert den Client.

CA/Cert/Key werden als `blob` in NVS gespeichert (Schlüssel `mqtt_tls_ca`,
`mqtt_tls_crt`, `mqtt_tls_key`); Passwörter stehen als Klartext in NVS.

---

### 6. Beispiele für manuelle MQTT-Nutzung (ohne HA)

#### Lesen aller Statuswerte (mosquitto_sub)

```bash
mosquitto_sub -h <broker-ip> -t "hb-rf-eth/status/#" -v
```

#### Restart ohne Token triggern

```bash
mosquitto_pub -h <broker-ip> -t "hb-rf-eth/command/restart" -m ""
```

#### Restart mit Token triggern

```bash
mosquitto_pub -h <broker-ip> -t "hb-rf-eth/command/restart" -m "my-shared-secret-123"
```

#### OTA-Update per Skript überwachen

```bash
# In einem Terminal die Events abonnieren:
mosquitto_sub -h <broker-ip> -t "hb-rf-eth/event/#" -v

# In einem zweiten Terminal den Status verfolgen:
mosquitto_sub -h <broker-ip> -t "hb-rf-eth/status/ota_*" -v
```

---

### Beispiel-HA-Dashboard

Ein typisches Home Assistant Dashboard könnte so aussehen:

```yaml
type: vertical-stack
cards:
  - type: entities
    title: HB-RF-ETH-ng Status
    entities:
      - entity: sensor.hb_rf_eth_ng_cpu_usage
        name: CPU
      - entity: sensor.hb_rf_eth_ng_memory_usage
        name: RAM
      - entity: sensor.hb_rf_eth_ng_uptime_text
        name: Laufzeit

  - type: horizontal-stack
    cards:
      - type: entity-button
        entity: button.hb_rf_eth_ng_restart
        name: Neustart
      - type: entity-button
        entity: button.hb_rf_eth_ng_factory_reset
        name: Reset
      - type: entity-button
        entity: update.hb_rf_eth_ng_firmware_update
        name: Update

  - type: entities
    title: Firmware
    entities:
      - entity: sensor.hb_rf_eth_ng_current_version
      - entity: sensor.hb_rf_eth_ng_latest_version
      - entity: binary_sensor.hb_rf_eth_ng_update_available
      - entity: update.hb_rf_eth_ng_firmware_update
```

### Sicherheitshinweise

- Die HA-Integration ist standardmäßig **DEAKTIVIERT** und muss explizit aktiviert werden
- Commands (Restart, Factory Reset, Update, Check Update) sind nur dann per
  MQTT möglich, wenn `commandEnabled` aktiv ist (Standard: ja)
- Optional kann ein **Kommando-Token** gesetzt werden: jeder Kommando-Payload
  muss dann exakt diesem Token entsprechen. Ohne Token gilt: jeder MQTT-Client
  mit Publish-Rechten auf `<prefix>/command/#` kann das Gerät steuern
- Bei gesetztem Token veröffentlicht das HA Discovery JSON den Token als
  `payload_press` / `payload_install` – die Broker-ACL muss deshalb
  sicherstellen, dass nur das Gerät auf `homeassistant/#` publishen darf
- TLS/mTLS ist optional und unabhängig vom Token (z. B. für self-hosted
  Mosquitto mit self-signed Cert)
- Alle Commands werden im Systemlog protokolliert; bei ungültigem Token wird
  zusätzlich ein Event auf `<prefix>/event/command_rejected` gepublished
- Die WebUI-Authentifizierung bleibt unabhängig von MQTT jederzeit aktiv

#### Empfohlene Broker-ACL (Mosquitto-Beispiel)

```
# /etc/mosquitto/acls/hb-rf-eth.acl
user hb-rf-eth
topic hb-rf-eth/# rw
topic homeassistant/# rw

pattern hb-rf-eth/%u/#

# Alle anderen User dürfen nur lesen / nicht publishen
user readonly
topic read hb-rf-eth/#
topic read homeassistant/#
```

### Fehlersuche

**Falls Geräte nicht in HA erscheinen:**
1. Überprüfen Sie die MQTT-Verbindung in der WebUI
2. Kontrollieren Sie die MQTT-Logs im Home Assistant Dashboard
3. Stellen Sie sicher, dass "Home Assistant Discovery" aktiviert ist
4. Prüfen Sie, ob der Discovery-Topic (`homeassistant/`) vom MQTT-Broker erreicht wird

**MQTT-Topic-Manuell überprüfen:**
```bash
# Im MQTT-Broker prüfen
mosquitto_sub -h <broker-ip> -t "homeassistant/#" -v
```
