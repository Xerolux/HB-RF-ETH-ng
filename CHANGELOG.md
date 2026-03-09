# Changelog - HB-RF-ETH-ng Firmware

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [2.1.9] - 2026-03-09

### Changed
- Bumped version to 2.1.9

## [2.1.8] - 2026-02-22

### Added
- 🔄 **Backend Update Check Proxy** - Neuer `/api/check_update` Endpoint für Update-Prüfung über Backend (verhindert CORS-Fehler)
- 🧪 **OTA Test Script** - Python CLI Tool (`test_ota_function.py`) für automatisierte OTA-Update-Tests
- 🎯 **Manueller Update-Check Button** - Neuer Button für manuelle Update-Prüfung mit Status-Anzeige
- 🌍 **Update-Status Lokalisierung** - Neue Übersetzungsschlüssel für Update-Check-Status (DE/EN)

### Changed
- 🌐 **OTA Server Migration** - Update-Server auf xerolux.de umgestellt für zuverlässigere Updates
- ⏰ **Update-Check Intervall** - Automatische Prüfung von 8 Stunden auf 24 Stunden erhöht
- 🔧 **OTA Konfiguration** - HTTP-Client Konfiguration in separates Modul (`ota_config.cpp`) ausgelagert
- 🖥️ **Firmware-Update UI** - Verbesserte Statusanzeige mit Popup für Update-Check-Ergebnis
- 📖 **Changelog aus Update-Banner** - Klick auf "Anzeigen" im Update-Banner öffnet Changelog-Modal

### Fixed
- 🐛 **OTA GitHub Redirects (Bug #235)** - OTA-Updates mit GitHub-Redirects funktionieren jetzt zuverlässig
  - HTTP Keep-Alive für OTA-Verbindungen deaktiviert für Cross-Domain-Redirects
  - TX-Buffer auf 4096 Bytes erhöht für große HTTPS-Header
  - Redirect-Handling aktiviert (max. 5 Redirects)
  - Stack-Größe für OTA-Task auf 16384 Bytes erhöht (Stack-Overflow Prevention)
- 🐛 **CORS-Fehler bei manuellem Update-Check** - Update-Prüfung erfolgt jetzt über Backend-Proxy
- 🐛 **Store Reference Error** - `useFirmwareUpdateStore` Referenzfehler bei Datei-Upload behoben
- 🐛 **Caching bei Update-Check** - Timestamp-Parameter verhindert Caching der Version-Datei

### Security
- 🔒 **Input-Validierung** - OTA URL wird vollständig über Backend verarbeitet

## [2.1.7] - 2026-02-21

### Added
- 🏠 **Home Assistant MQTT Auto-Discovery** - Vollständige HA-Integration via MQTT Discovery Protokoll; Gerät erscheint automatisch in Home Assistant
- 💡 **Vollständig konfigurierbare LED-Programme** - LED-Muster für alle Betriebsmodi (Verbunden, Getrennt, Update verfügbar, etc.) sind jetzt einzeln konfigurierbar; doppeltes Update-Blink-Toggle entfernt
- 🔒 **Umfangreiche Backend-Validierung** - Vollständige Validierung aller kritischen Einstellungen:
  - CCU-Adress-Validierung mit IPv4- und IPv6-Unterstützung (130+ Unit-Tests)
  - MQTT-Server-Adress-Validierung (IPv4, IPv6, Hostname, Port)
  - SNMP Community-String-Validierung mit Längenbegrenzung
  - NTP-Server-Validierung mit DNS-Compliance-Prüfung
  - IPv6-Adress-Validierung mit korrekter Format-Überprüfung
- 🌐 **Frontend CCU IPv6-Unterstützung** - WebUI akzeptiert jetzt sowohl IPv4- als auch IPv6-Adressen für CCU-Einstellungen
  - Aktualisierte Validierung für beide Adressformate
  - Verbesserter Platzhaltertext mit Beispielen für beide Formate
- 🔔 **Spezifische Fehlermeldungen** - WebUI zeigt jetzt spezifische Fehlermeldungen vom Backend (statt generische Fehler)
- 🌍 **DNS-Caching** - DNS-Cache-Verbesserungen für stabilere Namensauflösung
- 🔧 **Automatischer CI-Versions-Bump** - Versionsnummer wird automatisch im Release-Workflow aktualisiert

### Security
- 🛡️ **Kritischer Security-Fix** - Schwachstelle bei der String-Längen-Validierung behoben
  - String-Länge wird jetzt VOR strncpy validiert, um Buffer-Overflow zu verhindern
  - Verhindert das Umgehen der Längenprüfungen durch Angreifer
- 🔐 **Input-Validierung gehärtet** - Alle Benutzereingaben werden jetzt vollständig validiert, bevor sie verarbeitet werden
- 🔐 **CCU-Adress-Validierung** - Kritischer Security-Fix: Vollständige Validierung der CCU-Adresse im Backend
- 🔐 **MQTT/SNMP-Validierung** - MQTT-Server und SNMP-Community werden jetzt serverseitig validiert

### Changed
- ⚡ **Settings-Caching** - Optimistisches Update im Settings-Store beim Speichern; LED-Zustand wird sofort aktualisiert
- 🔧 **Settings-Refactoring** - Redundante `checkUpdates`- und `allowPrerelease`-Einstellungen entfernt
- ✅ **Test-Coverage** - 130+ umfassende Unit-Tests für Validierungsfunktionen hinzugefügt
  - IPv6-Validierungstests mit Edge Cases
  - CCU-Validierungstests für IPv4 und IPv6
  - NTP-Server-Validierungstests mit DNS-Compliance
  - MQTT- und SNMP-Validierungstests
- 🌍 **i18n** - Fehlende `updateLedBlink`-Übersetzungsschlüssel in 8 Locale-Dateien ergänzt
- 📰 **README** - Banner mit zentriertem Header, Hero-Image und verbesserten Badges erneuert

### Fixed
- 🐛 **LED-Programme** - LED-Programme werden jetzt korrekt gespeichert und geladen
- 🐛 **IPv6-Warnungen** - Störende IPv6-Warnungen für Hostname-Server-Adressen werden unterdrückt
- 🐛 **GPS** - Bug in der GPS-Verarbeitung behoben
- 🐛 **OTA** - Fehlerbehandlung beim OTA-Update verbessert
- 🐛 **CPU-Task** - Bug im CPU-Task behoben
- 🐛 **UDP-Listener** - Mehrere Bugs im UDP-Listener behoben
- 🐛 **MQTT-Handler** - Mehrere Bugs im MQTT-Handler behoben
- 🐛 **DNS-Cache** - Mehrere Bugs im DNS-Cache behoben
- 🐛 **Settings-Persistenz** - Mehrere Bugs bei der Einstellungs-Speicherung und Sicherheit behoben
- 🐛 **MQTT Factory-Reset** - Linker-Fehler im MQTT-Handler beim Factory-Reset behoben
- 🐛 **LED/DNS Kompilierung** - Kompilierungsfehler in LED- und DNS-Caching-Code behoben
- 🐛 **Memory Leaks** - Fehlerbehandlung verbessert und Memory Leaks beseitigt
- 🐛 **WebUI-Bugs** - Mehrere WebUI-Bugs in 10 Dateien behoben
- 🐛 **Doppelte Benachrichtigung** - Doppelte „Einstellungen gespeichert"-Benachrichtigung behoben
- 🐛 **HTTP-Warnungen** - Harmlose httpd_txrx-Warnungen werden jetzt unterdrückt
- 🔧 **CI/CD** - Überschreiben existierender Tags im Release-Workflow ermöglicht

### Dependencies
- 📦 Bumped `bootstrap-vue-next` von 0.43.0 auf 0.43.1
- 📦 Bumped `marked` von 17.0.1 auf 17.0.2
- 📦 Bumped `vue` von 3.5.27 auf 3.5.28
- 📦 Bumped `crate-ci/typos` von 1.43.3 auf 1.43.4

## [2.1.6] - 2026-02-12

### Added
- 🌟 **Update LED Control** - New setting to enable/disable the status LED blinking when a firmware update is available (default: enabled).
- 🔒 **Enhanced Password Security** - Enforced stronger password policy (min 8 chars, uppercase, lowercase, digit).

### Changed
- 📄 **Changelog Fetching** - Now fetched via backend proxy (`/api/changelog`) to avoid CORS issues and improve reliability.
- ⚡ **Performance** - Optimized idle timer events in WebUI to reduce CPU usage.
- 🔧 **Network Validation** - Stricter validation prevents saving invalid network settings.
- 🛑 **MQTT Stability** - Improved MQTT task termination sequence to prevent system restart delays.
- 🔒 **API Security** - Removed wildcard CORS headers from monitoring API.

### Fixed
- 🐛 **Settings Error Handling** - Proper error propagation when saving invalid network settings.
- ♿ **Accessibility** - Added `aria-hidden` attributes to decorative icons.

## 2.1.5 Final

### Added
- 🔄 **Restart Confirmation Modal** - Settings page now shows a confirmation dialog before restarting the device after saving settings.
- 🌐 **CCU IP Settings** - CCU IP address is now properly saved and reactive in the settings store.
- 🔃 **System Log Refresh Button** - Manual refresh button to recover if log polling stalls.

### Changed
- 📱 **Mobile Widget Layout** - Dashboard widgets now use a compact 3-column grid layout on mobile devices instead of horizontal scroll container.
- 📖 **Changelog Readability** - Darker text color (`#343a40`) in changelog modal for improved readability.

### Fixed
- 🐛 **System Log Download** - Fixed 401 Unauthorized error by using authenticated axios request instead of direct link.
- 🐛 **System Log Polling** - Added timeout to polling to prevent silent failures.
- 🐛 **Changelog Display** - Removed unnecessary headers from GitHub Raw request to fix CORS issues.
- 🐛 **Password Validation** - Fixed `PasswordChangeModal` validation blocking settings save by using `$stopPropagation` in `useVuelidate`.

## [2.1.5] - 2026-02-09

### Added
- 📋 **System Log Viewer** - New dedicated page (`systemlog.vue`) for viewing system logs with live polling every 3 seconds and log file download.
- 🔛 **Log Toggle** - Enable/disable switch for system log polling to reduce unnecessary network traffic when not needed.
- 📡 **Log Manager API** - New `/api/log` and `/api/log/download` backend endpoints for system log access.
- 🕐 **10-Minute Idle Logout** - Automatic logout after 10 minutes of inactivity, with cross-tab synchronization via `localStorage`.
- 💾 **Persistent Sessions** - Session tokens stored in `localStorage` instead of `sessionStorage`, surviving tab closures and page reloads.
- 🧭 **System Log Navigation** - Navigation links for System Log added to both desktop and mobile menus.
- 💖 **Sponsor Button** - Added a "Sponsor" button in the footer with options for PayPal, Buy Me a Coffee, and Tesla referral.
- 🌍 **System Log Translations** - Translations for the log viewer added to all 10 supported languages.

### Changed
- 🎨 **UI/UX Modernization** - Comprehensive visual overhaul with custom scrollbars, glassmorphism effects, layered shadows, and smooth page transitions.
- 📊 **Dashboard Widgets** - Gradient icons, hover effects, and horizontal scrolling on mobile for a polished dashboard experience.
- 🧭 **Navigation Header** - Floating notifications and hardware-accelerated animations for smoother interaction.
- 🔒 **Security Refactoring** - Extensive refactoring of JSON handling in System Info, Settings, and Login to use `cJSON` library, eliminating buffer overflow risks and injection vulnerabilities.
- 🧹 **Memory Safety** - Implemented `secure_zero` to securely clear sensitive data (tokens, passwords) from memory; shared `secure_utils.h` with NULL-safe, XOR-based constant-time `secure_strcmp`.
- 🚀 **Rate Limiting** - Improved Rate Limiter with full IPv6 support (`AF_INET6`) for dual-stack networks, using `inet_ntop` for human-readable logging.
- 🌐 **Translation Keys** - Replaced all hardcoded fallback strings with proper `t()` translation keys across all WebUI components (login, settings, header, change-password, firmware update, changelog modal).
- 🧹 **Code Deduplication** - Removed duplicate `secure_strcmp` from `webui.cpp` (uses shared header) and duplicate `compareVersions` from `updatecheck.cpp` (uses `semver.h`).
- ♿ **Accessibility** - Removed low-contrast `text-muted` classes and added `aria-hidden` attributes to decorative icons for better screen reader support.
- 📄 **Changelog Fetching** - Changed changelog fetching to use the raw GitHub URL to avoid CORS issues and improve reliability.
- 👁️ **High Contrast Logs** - System log viewer uses black text on white background with orange accent border for maximum readability.

### Fixed
- 🐛 **CCU Connection Stability** - Fixed critical bug where `portMAX_DELAY` in lwIP UDP receive callback could block the entire network stack when the queue was full; changed to non-blocking send with drop warning.
- 🐛 **UDP Keep-Alive** - Increased timeout from 5s to 10s to prevent false disconnects; immediate keep-alive response sent back to CCU for better health detection.
- 🐛 **UDP Queue** - Increased from 32 to 64 entries for better burst handling; reduced poll timeout from 100ms to 10ms for lower packet processing latency.
- 🐛 **Disconnect Handling** - Fixed stale `_connectionStarted` state not being reset on keep-alive timeout; fixed race condition in disconnect ordering.
- 🐛 **Memory Leak** - Fixed a memory leak in `RawUartUdpListener` by properly draining the packet queue before deletion.
- 🐛 **Compilation** - Fixed `delayed_restart_task` not declared in scope by moving definition before `post_settings_json_handler_func`.
- 🐛 **Validation** - Fixed strict IP validation in CheckMK agent configuration.
- 🐛 **Static Analysis** - Resolved various static analysis warnings (cppcheck) for better code quality.

### Dependencies
- 📦 Bumped `axios` from 1.13.4 to 1.13.5.
- 📦 Bumped `crate-ci/typos` from 1.42.3 to 1.43.3.

## [2.1.4] - 2026-02-06

### Fixed
- 🐛 **Memory Leak** - Fixed memory leak in OTA URL handler (`args` not freed on error)
- 🐛 **Code Quality** - Fixed variable shadowing in `UpdateCheck` class
- 🔧 **Const Correctness** - Improved C++ standards compliance in Settings class
- 🌍 **Localization** - Corrected version display in French translation

## [2.1.3]

### Added
- 📋 **Changelog Modal** - View full changelog directly in WebUI with markdown rendering
- 🔔 **Update Status Notifications** - Prominent banner showing available updates with version number
- 🌐 **Multi-language Changelog** - Changelog modal supports all UI languages
- 🔌 **Improved CCU 3 Reconnect** - Enhanced mDNS announcements and network stabilization after restart
  - 2-second delay before UDP listener starts to ensure network stability
  - mDNS service announcements sent after all services are fully started
  - Enhanced logging for connection troubleshooting

### Added
- 📋 **Changelog Modal** - View full changelog directly in WebUI with markdown rendering
- 🔔 **Update Status Notifications** - Prominent banner showing available updates with version number
- 🌐 **Multi-language Changelog** - Changelog modal supports all UI languages

### Changed
- 🔄 **Update Check** - Improved update detection using version.txt from GitHub
- 📦 **Migration to Vite** - Replaced Parcel with Vite 6.3.5 build system
  - Faster build times (~2s vs ~10s)
  - Better code optimization
  - Smaller bundle sizes
- 🔧 **Release Workflow** - Improved release automation
  - Better firmware file naming
  - Comprehensive release notes
  - All required files included in releases

### Fixed
- 🐛 **Critical Security Fix** - Fixed Origin Validation Error vulnerability in Parcel (CVE)
- 🐛 **Update Detection** - Fixed version comparison using semantic versioning instead of string comparison
- 🐛 **OTA Update** - Improved error handling prevents panic on failed updates
- 🔨 **Build System** - Fixed vite-plugin-compression import syntax
- 🌍 **Cross-platform Build** - Fixed UTF-8 encoding issues on Windows

### Security
- 🔒 Removed all vulnerable Parcel dependencies
- 🔒 Updated to Vite with better security practices

## [2.1.2]

### Added
- 🔐 **Enhanced Security**
  - Stronger password requirements (8 chars, mixed case, numbers)
  - Automatic logout after 5 minutes of inactivity
  - Timing-attack protected password comparisons
- 🎨 **UI/UX Improvements**
  - Password strength indicator
  - Dark/Light theme toggle
  - Multi-language support (10 languages)
  - LED brightness control (0-100%)
- 📊 **Reset Reasons** - Detailed display of last restart reason in WebUI
- 🌐 **OTA Update via URL** - Download firmware directly from network
- 🔧 **Factory Reset** - Can now be triggered from WebUI

### Changed
- 📝 **OTA Password** - Removed requirement, standard authentication is sufficient
- 🎯 **Update UX** - Automatic restart after successful firmware update
- 📦 **Modern WebUI** - Responsive design for desktop and mobile

### Fixed
- 🐛 **Update Failure** - Improved error handling prevents system panic on failed updates
- 🔧 **Recovery** - Better OTA abort handling when updates fail

## [2.1.1] - 2025-01-XX

### Added
- ✨ **ESP-IDF 5.x** - Updated to latest ESP-IDF framework
- 🔧 **Modern Toolchains** - Compatible with GCC 14.2.0
- 🎨 **Vue 3.5** - Modernized WebUI with latest Vue.js
- ⚡ **Performance** - Improved stability and performance
- 🔐 **Separate OTA Password** - Optional dedicated password for firmware updates

### Changed
- 📡 **API Updates** - Migrated to modern ESP-IDF APIs
  - ADC: `esp_adc_cal` → `adc_oneshot`
  - SNTP: Legacy → `esp_sntp_*`
  - mDNS: Integrated as managed component
- 🔨 **I2C Configuration** - Updated to ESP-IDF 5.1 syntax

### Fixed
- 🔧 **RTC Driver** - Fixed circular include dependencies
- 📦 **Build System** - Optimized CMake and PlatformIO configuration

## [2.0.0] - 2025-01-XX

### Major Changes - Modernized Fork by Xerolux

This version represents a comprehensive modernization of the HB-RF-ETH firmware,
originally developed by Alexander Reinert.

#### Framework & Toolchain
- **ESP-IDF 5.1** - Updated from older ESP-IDF to version 5.1.0
- **Modern Toolchain** - Compatibility with GCC 14.2.0
- **Newlib 4.x** - Full support for modern newlib version
- **Automated Patching** - Intelligent build system for framework compatibility

#### WebUI Modernization
- **Vue 3.5.13** - Latest Vue.js with Composition API
- **Pinia 2.3.1** - Modern state management
- **Bootstrap Vue Next 0.28.5** - Current UI component library
- **Improved UX** - Better user guidance for firmware updates

#### API Modernizations
- **ADC API** - Migrated from deprecated `esp_adc_cal` to `adc_oneshot`
- **SNTP API** - Updated to `esp_sntp_*` functions
- **mDNS** - Integrated as managed component

#### Code Improvements
- **Better Error Handling** - More robust error handling across all components
- **Memory Optimization** - More efficient memory usage
- **Compilation** - All 21 source files compile without errors

### Technical Details

**Firmware Sizes:**
- Firmware: 918 KB
- Bootloader: 24 KB

**Memory Usage:**
- RAM: 18.9 KB / 327.6 KB (5.8%)
- Flash: 918 KB / 1.9 MB (48.3%)

**Framework Versions:**
- ESP-IDF: 5.1.0
- Platform: espressif32 ^6.9.0
- Toolchain: xtensa-esp-elf 14.2.0

### Known Limitations
- No automatic reconnect after board restart (CCU software restart required)
- Power supply via RPI-RF-MOD only without other power sources

### Acknowledgments

Special thanks to **Alexander Reinert** for developing the original HB-RF-ETH firmware and hardware.
This version builds on his excellent work and modernizes it for current development environments.

---

## Earlier Versions

For changes in versions before 2.0.0, see the [Original Repository](https://github.com/alexreinert/HB-RF-ETH).

[Unreleased]: https://github.com/Xerolux/HB-RF-ETH-ng/compare/v2.1.9...HEAD
[2.1.9]: https://github.com/Xerolux/HB-RF-ETH-ng/compare/v2.1.8...v2.1.9
[2.1.8]: https://github.com/Xerolux/HB-RF-ETH-ng/compare/v2.1.7...v2.1.8
[2.1.7]: https://github.com/Xerolux/HB-RF-ETH-ng/compare/v2.1.6...v2.1.7
[2.1.6]: https://github.com/Xerolux/HB-RF-ETH-ng/compare/v2.1.5...v2.1.6
[2.1.5]: https://github.com/Xerolux/HB-RF-ETH-ng/compare/v2.1.4...v2.1.5
[2.1.4]: https://github.com/Xerolux/HB-RF-ETH-ng/compare/v2.1.3...v2.1.4
[2.1.3]: https://github.com/Xerolux/HB-RF-ETH-ng/compare/v2.1.2...v2.1.3
[2.1.2]: https://github.com/Xerolux/HB-RF-ETH-ng/compare/v2.1.1...v2.1.2
[2.1.1]: https://github.com/Xerolux/HB-RF-ETH-ng/compare/v2.1.0...v2.1.1
[2.0.0]: https://github.com/Xerolux/HB-RF-ETH-ng/releases/tag/v2.0.0
