# Changelog - HB-RF-ETH Firmware

## Version 2.0.0 (2025) - Modernisierte Fork von Xerolux

Diese Version stellt eine umfassende Modernisierung der HB-RF-ETH Firmware dar.

### Hauptänderungen

#### Framework & Toolchain
- **ESP-IDF 5.1**: Aktualisierung von älterem ESP-IDF auf Version 5.1.0
- **Moderne Toolchain**: Kompatibilität mit GCC 14.2.0 Toolchain
- **Newlib 4.x**: Vollständige Unterstützung für moderne newlib Version
- **Automatisches Patching**: Intelligentes Build-System für Framework-Kompatibilität

#### WebUI Modernisierung
- **Vue 3.5.13**: Aktualisierung auf aktuelle Vue.js Version mit Composition API
- **Parcel 2.13.2**: Moderner Build-Prozess mit optimiertem Bundling
- **Pinia 2.3.1**: Moderne State-Management-Lösung
- **Bootstrap Vue Next 0.28.5**: Aktuelle UI-Komponenten-Bibliothek
- **Neustart-Button**: Direkter System-Neustart aus dem WebUI (unter Firmware-Update)
- **Fork-Information**: Klare Kennzeichnung als v2.0 Fork mit Links zu GitHub-Repositories
- **Verbesserte UX**: Optimierte Benutzerführung beim Firmware-Update

#### API-Modernisierungen
- **ADC API**: Migration von deprecated `esp_adc_cal` zu `adc_oneshot` API
- **SNTP API**: Aktualisierung auf `esp_sntp_*` Funktionen
- **mDNS**: Integration als managed component (espressif/mdns ^1.4.3)
- **I2C Konfiguration**: Anpassung an ESP-IDF 5.1 Syntax

#### Code-Verbesserungen
- **RTC Driver**: Behebung zirkulärer Include-Abhängigkeiten
- **WPA Supplicant**: Fehlende Header-Includes hinzugefügt
- **Build-System**: Optimierte CMake und PlatformIO Konfiguration
- **Asset-Handling**: Vereinfachtes WebUI Asset-Management

#### Stabilität & Performance
- **Bessere Fehlerbehandlung**: Robustere Fehlerbehandlung in allen Komponenten
- **Speicheroptimierung**: Effizientere Speichernutzung (RAM: 5.8%, Flash: 48.3%)
- **Kompilierung**: Alle 21 Quellcode-Dateien kompilieren fehlerfrei

### Technische Details

**Kompilierte Firmware-Größen:**
- Firmware: 918 KB (mit Neustart-Funktion)
- Bootloader: 24 KB

**Speicherverbrauch:**
- RAM: 18.924 von 327.680 Bytes (5.8%)
- Flash: 918.177 von 1.900.544 Bytes (48.3%)

**Framework-Versionen:**
- ESP-IDF: 5.1.0 (framework-espidf ~3.50100.0)
- Platform: espressif32 ^6.9.0
- Toolchain: xtensa-esp-elf 14.2.0

### Bekannte Einschränkungen

Die aus der Original-Version bekannten Einschränkungen bleiben bestehen:
- Kein automatischer Reconnect nach Neustart der Platine
- Stromversorgung mittels RPI-RF-MOD nur ohne andere Stromquellen

### Danksagung

Ein großer Dank geht an **Alexander Reinert** für die Entwicklung der originalen HB-RF-ETH Firmware und Hardware. Diese Version baut auf seiner exzellenten Arbeit auf und modernisiert sie für aktuelle Entwicklungsumgebungen.

### Lizenz

Die Firmware bleibt unter der Creative Commons Attribution-NonCommercial-ShareAlike 4.0 Lizenz.

---

## Frühere Versionen

Für Änderungen in Versionen vor 2.0 siehe das [Original-Repository](https://github.com/alexreinert/HB-RF-ETH).
