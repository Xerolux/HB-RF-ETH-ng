# Changelog - HB-RF-ETH Firmware

## Version 2.1.5 (2025)

### Änderungen

- sysinfo-reset-reason-performance (ab88999)
- Fix reset reason localization, online update comparison, and optimize SysInfo performance (32f2cc5)
- webui-version-sync (6c91298)
- feat: Auto-sync WebUI version during build (88f8a1d)
- chore: Update version to 2.1.4 (5af170e)
- fix-radio-signal-delivery (9ae2fdd)
- CRITICAL: Optimize radio signal delivery and CCU connection stability (938f802)
- code-review-refactor-hmFAd (33eef11)
- Phase 2: Complete translations for 8 languages & unify DTLS structure (aeff93b)
- claude/code-review-refactor (fcffea6)
- Phase 1: Critical Security Fixes & Improvements (d0b0ec1)
- fix-build-error-gerw7 (5bbf646)
- Update package-lock.json to version 2.1.3 (e961da8)
- Fix analyzer build error with ESP-IDF 5.5.0 (e653995)
- fix-data-display-issue-in-analyzer (b6b6ceb)
- Send analyzer WS frames synchronously on httpd thread (cbc8d5a)
- Fix analyzer WebSocket delivery via queued sends (5a6b662)
- fix-update-check-and-add-online-update (b3303cf)
- Improve firmware update checks (8702c29)
- chore: Update version to 2.1.3 (c87156c)
- Merge pull request #87 from Xerolux/sentinel-csp-enhancement-9716366961139859601 (9632209)
- feat: Add Content Security Policy header (b0494e5)
- palette-settings-loading-states (1149992)
- feat(ui): add loading states to backup and restore actions (86cd112)
- bolt-analyzer-memory-opt (665e3d9)
- feat: Optimize AnalyzerFrame memory usage - Implement small buffer optimization (SBO) for `AnalyzerFrame` - Reduce queue heap usage from ~20KB to ~1.5KB - Reduce memcpy overhead for typical frames (13x improvement) - Handle dynamic allocation for large frames - Ensure correct memory cleanup on queue full, drop, and destruction (655c086)
- refactor-hmlgw-stop (2c38053)
- Refactor Hmlgw::stop() to use lambda for task deletion (b6eae7f)
- check-repo-files-SCV28 (9e99ccd)
- Fix critical race condition in HMLGW task termination (3ba9ad4)
- optimize-voma-analyzer-and-ensure-hw-gw-lan-gateway (a0d4c95)
- Polish analyzer UI and lifecycle (e599c49)
- review-the-code (f4f4ed1)
- Harden authentication handling (7a772c9)
- Merge pull request #80 from Xerolux/codex/fix-analyzer-and-verify-lan-functionality (f424641)
- Disable analyzer in HMLGW mode (246f034)
- Merge pull request #75 from Xerolux/bolt/sysinfo-optimization-217233130365365499 (6f1f78c)
- Merge pull request #77 from Xerolux/fix-analyzer-and-updatecheck-15594490126689462312 (5d70fde)
- Merge pull request #78 from Xerolux/palette-aria-label-14641652339104296509 (6b967d9)
- Merge pull request #79 from Xerolux/sentinel-security-headers-956351346989704317 (86146d3)
- feat(security): add HTTP security headers (f0bbe18)
- chore(webui): localize aria-label in PasswordInput (4dd1767)
- Fix analyzer stack overflow and update check buffer size (1ea19ff)
- fix-analyzer-crashes-8RXrE (30d199a)
- fix: Critical fixes for Analyzer crashes and signal blocking (77ceeeb)
- ⚡ Bolt: optimize sysinfo metrics caching (cfa028c)
- chore: Update version to 2.1.2 (deb3e8f)
- Merge pull request #74 from Xerolux/claude/fix-analyzer-signal-HmsBf (b0bcc02)
- fix: Make Analyzer asynchronous to prevent signal blocking (3b5940f)
- chore: Update version to 2.1.4 (31ddfa0)
- fix-analyzer-empty-list-oU70a (e4c5e28)
- fix: Register analyzer WebSocket clients during handshake (87c1b59)
- fix-firmware-search-XYeke (55bba08)
- feat: Enhance firmware update UI and improve version detection (6f5669d)
- chore: Update version to 2.1.3 (5398ab0)
- fix-firmware-search-XYeke (7051ce8)
- fix: Improve firmware search to properly fetch and filter GitHub releases (9f40e48)
- fix-empty-list-panic-nDaI9 (ea3de82)
- fix: Prevent panic when client list is empty in analyzer (f260b59)
- chore: Update version to 2.1.2 (2194f1c)
- fix-stability-issues-Jqunb (0157f80)
- fix: Comprehensive stability improvements for signal transmission and connection reliability (05d86b3)
- refactor-dtls-password-modal-Orqjq (a838761)
- Address code review feedback for DTLS password modal (19d9670)


## Version 2.1.4 (2025)

### Änderungen

- fix-analyzer-light-login-PqJad (e6102ff)
- Fix Analyzer Light and HM GW LAN UI improvements (ce45494)
- Merge pull request #66 from Xerolux/claude/fix-analyzer-light-empty-gePBP (b34bf1e)
- Fix Analyzer Light: Empty device list and no received packets (1784801)
- fix-uart-config-warnings (f35cd8b)
- Fix missing field initializer warnings for uart_config_t (18af250)
- Update webui.cpp (d23aff2)
- Merge branch 'main' of https://github.com/Xerolux/HB-RF-ETH-ng (f456a05)
- Update settings.local.json (8a78a28)
- Merge pull request #62 from Xerolux/bugfix/increase-uri-handler-limit-4361093178095781993 (b10a7b4)
- Fix 'Nothing matches the given URI' error by increasing max_uri_handlers to 32 (64b2f18)


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
