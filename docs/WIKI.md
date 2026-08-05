# HB-RF-ETH-ng Wiki

Willkommen im Wiki der modernisierten HB-RF-ETH-ng Firmware. Hier finden Sie alle detaillierten Informationen zu Funktionen, Konfiguration und Betrieb.

## Aktuelle Verbesserungen (v2.2.0-Beta 7-10)

Die Firmware wurde kürzlich mit wichtigen neuen Funktionen und Verbesserungen aktualisiert:

### Neue Features
- **Benutzername + Passwort Login**: Standard-Benutzername `admin`, spaeter in den Einstellungen aenderbar
- **Hostname im Dashboard**: Der konfigurierte Hostname wird im Systemstatus prominent angezeigt
- **Session-Persistierung**: Login bleibt nach Neustarts erhalten
- **Firmware-Update-Feedback**: Visueller Bestätigungs-Dialog nach einem erfolgreichen manuellen Upload
- **Verbesserte TLS-Sicherheit**: Vollständiges Mozilla-CA-Bundle für optionale ausgehende TLS-Verbindungen

### Behobene Probleme
- Watchdog-Resets bei großen Release-Notes (Stack-Overflow)
- Sofortiges Logout bei Seitenladung (bei Boot-Zeit-Synchronisation)
- MQTT-Verbindungsstabilität und Reconnect-Logik

---

## Was kann die Firmware

### Funkmodul & Netzwerk
* Bereitstellung des Funkmoduls RPI-RF-MOD oder HM-MOD-RPI-PCB per UDP als raw-uart Gerät inkl. Ansteuerung der LEDs des RPI-RF-MODs
* Erkennung des Funkmoduls und Ausgabe von Typ, Seriennummer, Funkadresse und SGTIN in der WebUI
* **Stabile CCU-Verbindung** mit optimiertem UDP-Handling
  * Non-blocking UDP-Queue (32 Einträge) für zuverlässige Paketverarbeitung
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
* **Multi-Language Support** (4 Sprachen: Deutsch, Englisch, Französisch und Italienisch)
* **System Log Viewer** - Live-Ansicht über WebSocket mit 5-Sekunden-Polling als Fallback, Download-Funktion, Ein/Aus-Schalter und manueller Aktualisierung (High-Contrast)
  * Aktivierung bleibt ueber Reboots erhalten; beim Deaktivieren bleibt das Log nach dem naechsten Start wieder aus
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
* Manueller Upload einer lokalen `firmware_*.bin` mit Fortschrittsanzeige
* Keine automatische Update-Suche und kein Download einer Firmware-URL durch
  das Gerät
* Keine Firmware-Installation über MQTT oder Home Assistant
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
  * Administrator-Passwort, MQTT-/Benachrichtigungs-Zugangsdaten, Tokens, Zertifikate und private Schlüssel werden für eine vollständige Wiederherstellung im Klartext exportiert. Die JSON-Datei muss wie ein Passwort geschützt und darf nicht veröffentlicht werden
  * Die JSON-Datei kann als Vorlage mit einem Texteditor angepasst und auf weitere Geräte eingespielt werden. Vorher insbesondere Hostname, statische IP, Administrator-Passwort und gerätespezifische MQTT-Werte ändern, um Konflikte zu vermeiden

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
  * Status-Publishing für Systemmetriken im 60-Sekunden-Takt; explizite Trigger werden binnen etwa 5 Sekunden verarbeitet
  * **HA-Sensoren** für alle Systemmetriken (CPU, RAM, Temperatur, Spannung, Uptime)
  * **HA-Button** für den geschützten Neustart
  * **Versionsanzeige** für die installierte Firmware und WebUI
  * Firmware-Installation und Werksreset bleiben aus Sicherheitsgründen WebUI-Aktionen
  * **TLS/mTLS Support** - Verschlüsselte Verbindungen und Client-Zertifikate
* **Check_MK Agent**
  * Native Unterstützung für Check_MK/CheckMK Monitoring
  * Erweiterte Systemmetriken und Statusinformationen
  * IP-basierte Zugriffskontrolle (IPv4/IPv6)
* **Hardware-Überwachung** - Echtzeit-Temperatur, Spannung, CPU- und Speicheranzeige
* **Event-basierte Benachrichtigungen** - Nicht-retained Events für Automatisierungen

### Technische Basis
* **ESP-IDF 6.1-beta1** mit nativer `idf.py` Toolchain
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
2. Unter "Einstellungen" den Bereich "Systemaktionen" öffnen
3. Button "Factory Reset" klicken
4. Bestätigen und Neustart abwarten

## Blinkcodes der LEDs
### RPI-RF-MOD
Siehe Hilfe zum RPI-RF-MOD

### Grüne Power LED und rote Status LED
* Blinken abwechselnd mit grüner Power LED: System bootet
* Schnelles Blinken der roten Status LED, grüne Power LED leuchtet nicht: Siehe Werksreset
* Schnelles Blinken der roten Status LED, grüne Power LED leuchtet dauerhaft: Firmware Update wird eingespielt
* Das frühere langsame Blinken für ein verfügbares Update wird nicht mehr
  automatisch ausgelöst, weil das Gerät nicht nach Releases sucht
* Dauerhaftes Leuchten der grünen Power LED: Sytem ist gestartet

## Firmware Updates
Fertig kompilierte Firmware finden Sie unter
[GitHub Releases](https://github.com/Xerolux/HB-RF-ETH-ng/releases). Das Gerät
sucht nicht selbst nach Updates. Eine Firmware wird ausschließlich als lokale
Raw-`.bin`-Datei installiert:

**Per Webinterface (File Upload):**
1. Herunterladen der passenden `firmware_*.bin` Datei aus dem Release
2. In der WebUI zur Seite "Firmware Update" navigieren
3. Die .bin Datei hochladen
4. Update wird automatisch eingespielt und die Platine neu gestartet
5. Warten, bis das Gerät neu gestartet und die WebUI wieder erreichbar ist

**Sicherheitshinweise:**
- Die Standard-Authentifizierung schützt Firmware-Updates ausreichend
- Die Firmware validiert alle Updates vor der Installation
- Bei fehlerhaften Updates wird die OTA-Operation korrekt abgebrochen
- URL-Download, automatische Suche und MQTT/Home-Assistant-OTA sind entfernt

## Notfall-Wiederherstellung (Rescue Script)
Sollte die WebUI nicht mehr erreichbar sein, aber die Platine noch im Netzwerk antworten (Ping), kann die Firmware über das mitgelieferte Python-Script `test_ota_function.py` neu installiert werden.

**Voraussetzung:**
- Python 3 installiert
- Platine ist im Netzwerk erreichbar
- Admin-Passwort ist bekannt

**Anwendung:**
Das Script befindet sich im Root-Verzeichnis des Repositories.

```bash
# Syntax (Passwort wird sicher abgefragt)
python3 test_ota_function.py <IP-ADRESSE> <LOKALE-FIRMWARE.bin>

# Beispiel
python3 test_ota_function.py 192.168.1.100 build/firmware.bin
```

Das Script authentifiziert sich und sendet die lokale Datei als
`application/octet-stream` direkt an `POST /ota_update`.

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
| Status-Publish-Intervall | 60 s | Explizite Trigger werden binnen etwa 5 s verarbeitet |
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
└── {sensor|binary_sensor|button}/hb-rf-eth-<serial>/<obj_id>/config
                                  – HA Auto-Discovery Configs (retained, QoS 1)
```

### 1. Status Topics (`<prefix>/status/*`)

Alle Status-Werte sind **retained** und werden mit **QoS 0** veröffentlicht.
Das reguläre Publish-Intervall beträgt 60 Sekunden; explizite Status-Trigger
werden spätestens beim nächsten 5-Sekunden-Teilschritt verarbeitet. Nach jedem
(Re)Connect erfolgt sofort ein vollständiger Publish-Cycle plus optional eine
HA-Discovery-Runde.

#### Identity & Version

| Topic | Typ | Beispiel | Beschreibung |
|-------|-----|----------|--------------|
| `status/online` | string | `online` / `offline` | LWT-Birth-Marker; "offline" wird vom Broker bei unclean disconnect gesetzt |
| `status/serial` | string | `A1B2C3D4E5F6` | ESP32-MAC-basierte Geräteseriennr. |
| `status/firmware_version` | string | `2.2.6-Beta.4` | Aktuell laufende Firmware-Version |
| `status/webui_version` | string | `1.0.0-Beta.16` | Effektiv ausgelieferte WebUI-Version |
| `status/board_revision` | string | `REV 1.10 (PUB)` | Hardware-Revision der Platine |

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
| `status/netmask` | IPv4 | `255.255.255.0` | Aktuelle IPv4-Netzmaske |
| `status/gateway` | IPv4 | `192.168.1.1` | Aktuelles IPv4-Gateway |
| `status/dns1`, `status/dns2` | IPv4 | `192.168.1.1` | DNS-Server |
| `status/ipv6_addresses` | string | `fe80::1234` | Kommagetrennte IPv6-Adressen oder leer |

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

### 2. Event Topics (`<prefix>/event/*`)

Events sind **nicht retained** und werden mit **QoS 0** veröffentlicht – sie
sind punktuelle Benachrichtigungen für Automatisierungen (HA, Node-RED, …).

| Topic | Payload | Auslöser |
|-------|---------|----------|
| `event/restart` | `requested` | Gerät restartet via MQTT-Kommando (vor dem tatsächlichen Reboot) |
| `event/command_rejected` | `rejected cmd=<name> reason=invalid_token` | Kommando mit falschem/fehendem Token empfangen |

---

### 3. Command Topics (`<prefix>/command/*`)

Das Gerät abonniert `<prefix>/command/#` (QoS 1) **nur**, wenn `commandEnabled`
ODER `haDiscoveryEnabled` aktiv ist. Ohne diese Flags wird kein Kommando
angenommen – auch nicht mit korrektem Token.

| Command-Topic | Aktion | Vorbedingung |
|---------------|--------|--------------|
| `command/restart` | Gerät neustarten (Reset-Grund `USER_RESTART`) | `commandEnabled` |

Werkreset, Update-Suche und Firmware-Installation werden nicht als
MQTT-Kommandos angeboten. Firmware wird nur als lokale Datei in der WebUI
installiert.

#### Payload-Regeln

- **Ohne Token (`command_token == ""`):** Payload wird ignoriert, jedes Publish
  auf das Topic löst die Aktion aus. **Broker-seitige ACL zwingend erforderlich.**
- **Mit Token:** Payload muss Byte-genau dem konfigurierten `command_token`
  entsprechen (8–63 Zeichen, Zeichensatz `A–Z a–z 0–9 - _ .`). Andernfalls wird
  das Kommando verworfen und `event/command_rejected` gepublished.
- Bei gesetztem Token wird der Token als `payload_press` in die
  HA-Discovery-Config geschrieben, so dass der Neustart-Button funktioniert.

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
| `firmware_version` | Firmware Version | – | – | `mdi:package-variant` |
| `webui_version` | WebUI Version | – | – | `mdi:web` |
| `board_revision` | Board Revision | – | – | `mdi:expansion-card` |
| `eth_link_speed` | Ethernet Speed | data_rate, measurement | Mbit/s | `mdi:speedometer` |
| `ip_address` | IP Address | – | – | `mdi:ip` |
| `radio_module_type` | Radio Module | – | – | `mdi:radio-tower` |
| `radio_module_serial` | Radio Serial | – | – | `mdi:barcode` |
| `radio_module_firmware` | Radio Firmware | – | – | `mdi:chip` |

Alle Sensoren haben `entity_category: "diagnostic"`.

#### Binary Sensoren (`binary_sensor/`)

| Object ID | Name | Class | Payload on / off |
|-----------|------|-------|------------------|
| `online` | Online | connectivity | `online` / `offline` (LWT) |
| `eth_connected` | Ethernet Link | connectivity | `true` / `false` |
| `ntp_synced` | NTP Synced | – | `true` / `false` |

Alle haben `entity_category: "diagnostic"`.

#### Buttons (`button/`)

| Object ID | Name | Class | Command | Icon |
|-----------|------|-------|---------|------|
| `restart` | Restart | restart | `command/restart` | `mdi:restart` |

Buttons werden **nur gepublished, wenn `commandEnabled = true`**. Ansonsten
sieht HA keinen klickbaren Button.

`entity_category: "config"`. `payload_press` entspricht dem Token (oder dem
Kommando-String, wenn kein Token gesetzt ist).

Frühere Update-/OTA-Sensoren, der Check-Button und die installierbare
MQTT-Update-Entität werden über leere retained Discovery-Nachrichten entfernt.


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

  - type: entities
    title: Versionen
    entities:
      - entity: sensor.hb_rf_eth_ng_firmware_version
      - entity: sensor.hb_rf_eth_ng_webui_version
```

### Sicherheitshinweise

- Die HA-Integration ist standardmäßig **DEAKTIVIERT** und muss explizit aktiviert werden
- Der einzige MQTT-Befehl `Restart` ist nur dann möglich, wenn
  `commandEnabled` aktiv ist (Standard: ja)
- Optional kann ein **Kommando-Token** gesetzt werden: jeder Kommando-Payload
  muss dann exakt diesem Token entsprechen. Ohne Token gilt: jeder MQTT-Client
  mit Publish-Rechten auf `<prefix>/command/#` kann das Gerät steuern
- Bei gesetztem Token veröffentlicht das HA Discovery JSON den Token als
  `payload_press` – die Broker-ACL muss deshalb
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
