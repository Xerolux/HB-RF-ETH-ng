# Changelog - HB-RF-ETH-ng Firmware

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Planned
- Enhanced monitoring capabilities

## [2.1.3] - 2025-02-08

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

## [2.1.2] - 2025-01-XX

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

[Unreleased]: https://github.com/Xerolux/HB-RF-ETH-ng/compare/v2.1.3...HEAD
[2.1.3]: https://github.com/Xerolux/HB-RF-ETH-ng/compare/v2.1.2...v2.1.3
[2.1.2]: https://github.com/Xerolux/HB-RF-ETH-ng/compare/v2.1.1...v2.1.2
[2.1.1]: https://github.com/Xerolux/HB-RF-ETH-ng/compare/v2.1.0...v2.1.1
[2.0.0]: https://github.com/Xerolux/HB-RF-ETH-ng/releases/tag/v2.0.0
