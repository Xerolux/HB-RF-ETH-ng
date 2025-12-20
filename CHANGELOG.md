# Changelog - HB-RF-ETH Firmware

## Version 2.1.3 (2025)

### Änderungen

- 53-relevant-fixes (eb41749)
- feat: Apply PR #53 improvements with security fixes (b6fcc45)
- settings-loading-state (ee1f795)
- feat(webui): Add loading state to Settings save button (16236e1)
- analyzer-optimization (ba8952e)
- password-toggle (3033157)
- buffer-overflow-fix (1cec388)
- udp-alloc-optimization (3d70597)
- ⚡ Bolt: Pre-calculate display values in Analyzer (03d306f)
- 🎨 UX: Add password visibility toggle (c1b7f9d)
- 🛡️ Sentinel: [CRITICAL] Fix stack buffer overflow in monitoring agent (8502a3b)
- feat: optimize UDP packet handling by removing heap allocations (d4851da)
- bugfix-optimization (87f114f)
- fix: add missing header includes for build (20c85ab)
- bugfix-optimization (5d348fa)
- feat: additional code quality improvements - 6 more fixes (edcfb2e)
- fix: comprehensive bugfixes and optimizations - 16 critical issues resolved (cd9d6be)
- sentinel-cache-control-fix (b71a2db)
- update-readme-project (ff89747)
- json-optimization (7fe95f2)
- upgrade: update to ESP-IDF 5.5.1 from 5.1.0 (10ec878)
- feat: Add Cache-Control headers to sensitive API endpoints (12c9c31)
- feat(analyzer): optimize JSON frame construction (f418e95)
- docs: comprehensive README update with complete v2.1.2 feature set (be5ceff)
- upgrade-components (ad0fb9a)
- chore: upgrade platform and dependencies to latest versions (8b316a2)
- complete-ui-backup-check (cf9c104)
- feat: ensure complete settings coverage in UI and backup (491d27b)
- feat: Implement DTLS encryption for ESP-IDF 5.x with mbedTLS 3.x API (d5a1c72)
- fix: Temporarily disable DTLS encryption due to mbedTLS API incompatibility (67019d9)
- fix: Correct settings.h duplicate declarations and DTLS API errors (1916364)
- fix: DTLS encryption now only active in Raw-UART mode, incompatible with HM-LGW and Analyzer (6259958)
- Merge remote-tracking branch 'origin/main' into main (14f1fbc)
- feat: Add optional DTLS/TLS transport encryption for Raw-UART UDP communication (47391a2)
- sentinel-fix-checkmk-ip-whitelist (9ff524d)
- webui-build-error (1c5e697)
- Fix undeclared identifier error in webui.cpp (29ca205)
- Sentinel: Fix IP whitelist bypass in CheckMK monitoring (6cfbd98)
- webui-etag-caching (f5ad7f0)
- ⚡ Bolt: Implement ETag caching for embedded static files (2c42d5c)
- maintenance-updates (8f67a3b)
- feat: add maintenance controls, update check, and conditional analyzer (d474137)
- fix-udp-reconnect (cd0ac42)
- Fix: Ensure reliable CCU reconnection after restart (a15df8a)


## Version 2.1.2 (2025)

### Änderungen

#### Framework Update auf ESP-IDF 5.5.1
- **ESP-IDF 5.5.1**: Aktualisierung von ESP-IDF 5.1.0 auf 5.5.1
  - Platform espressif32@6.12.0 mit ESP-IDF 5.5.1 Support
  - mbedTLS 3.6.4 für verbesserte Sicherheit
  - Log v2 mit zentralisiertem Logging-System
  - std::filesystem Support (C++17 Dateisystem-API)
  - Verbesserte NVS-Unterstützung
  - OpenSSL 3.x Kompatibilität
- **Python 3.9+**: Mindestanforderung erhöht (Python 3.8 nicht mehr unterstützt)
- **Dokumentation**: README und technische Spezifikationen aktualisiert

#### Features & Verbesserungen
- fix-udp-reconnect (6bc961b)
- Fix: Correctly extract UDP source address in RawUartUdpListener (d86d357)
- remove-password-backup (9e2c8f3)
- security: remove plain text password from backup JSON (9b717e5)
- uart-config-init-and-ui-check (1600617)
- fix(firmware): remove missing flags from uart_config_t for ESP-IDF 5.x compatibility (4d3e70d)
- hmlgw-mode (2a6f96c)
- Add HomeMatic LAN Gateway (HMLGW) Emulation Mode (5092396)
- Add HomeMatic LAN Gateway (HMLGW) Emulation Mode (e256a80)
- feature/analyzer-light (318b06f)
- feat: update Analyzer Light translations for all languages (cbbab81)
- fix: cleanup warnings and verify translations for Analyzer (0c686fb)
- feat: enhance Analyzer Light with RSSI, Naming and fix build (983e86b)
- feat: enhance Analyzer Light with RSSI and Naming (9e437bb)
- feat: add Analyzer Light feature to WebUI (927e77f)
- login-ux-improvement (38e1845)
- password-validation (7d5e29a)
- feat(webui): Improve login form UX with loading state and keyboard support (8461e7d)
- feat(security): enforce password length validation to prevent silent truncation (3e4fbbd)
- fix-pylance-errors (e0cca76)
- fix: suppress Pylance errors for SCons globals (3f0315b)
- sysinfo-voltage-opt (75f7dde)
- feat: Optimize supply voltage reading with background caching (d12c0f7)
- Merge pull request #32 from Xerolux/update-changelog-automation-6799761342686045634 (c2e1467)
- feat: Automate changelog updates in release workflow (aaf0adc)


## Version 2.1.1 Beta (2025)

### Änderungen
- Automatische Changelog-Generierung vorbereitet
- GitHub Actions Release Workflow Optimierungen
- Beta Release

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

**Framework-Versionen (Version 2.0.0):**
- ESP-IDF: 5.1.0 (framework-espidf ~3.50100.0)
- Platform: espressif32 ^6.9.0
- Toolchain: xtensa-esp-elf 14.2.0

**Aktuelle Framework-Versionen (Version 2.1.2):**
- ESP-IDF: 5.5.1 (framework-espidf ~3.50501.0)
- Platform: espressif32 6.12.0
- Toolchain: xtensa-esp-elf 14.2.0
- mbedTLS: 3.6.4

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
