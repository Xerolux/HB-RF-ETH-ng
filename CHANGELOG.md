# Changelog - HB-RF-ETH-ng Firmware

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [2.2.0-Beta.13] - 2026-06-30

### Changes
- feat: add low-heap watchdog as a last-resort restart safety net
- fix: free DCF flank-event queue handle on stop instead of just resetting it
- docs: bring CLAUDE.md in line with the actual repo (ESP-IDF, not PlatformIO)
- fix: 24h update-check interval overflowed to ~500s, hammering GitHub API
- fix: abort log-share with a clear error if the net-fetch mutex times out
- fix: serialize log-share TLS handshake and grow GitHub response buffer
- chore: bump version to 2.2.0-Beta.12
- copy-button-bug
- fix: copy-to-clipboard button not working on HTTP WebUI
- fix: iterate all fetched releases and pick highest version
- chore: bump version to 2.2.0-Beta.11
- fix: clipboard copy in non-HTTPS context (HTTP WebUI)
- fix: semver pre-release comparison now handles numeric segments correctly
- release: v2.2.0-Beta.10
- paste-sharing-upload
- perf: avoid temporary std::string allocations in appendField
- fix: use MicroBin multipart upload API for log sharing
- graphics-issue
- feat: replace favicon with satellite-dish app icon
- fix: restore TLS cert verification using full CA bundle on OTA path
- fix: remove crt_bundle_attach to work around ESP-IDF PSA Crypto TLS handshake failure
- release: v2.2.0-Beta.9 — OTA success modal, paste service fix, duplicate button fix
- fix: remove duplicate download button, fix paste service redirect handling (event handler + disable_auto_redirect)
- feat: redirect to home page after OTA update with success modal (auto-close 10s)
- fix: compile errors in share log task (format strings, enum, ip4_addr_t, header API)
- graphics-issue
- fix: prevent firmware update banner from overlapping header nav
- ci: auto-trigger release build on tag push
- docs: complete MQTT API reference in wiki (all status/event/command topics, HA entities, TLS/mTLS)
- feat: system log sharing, globe favicon, clipboard HTTP fallback, update buffer fix (Beta.8)
- webui-improvements-optimization
- fix(webui): complete i18n translations for 8 locales
- fix(build): snprintf instead of strncpy in getVersionSnapshot (-Wstringop-truncation)
- fix(build): move variable declarations above goto cleanup (C++17 jump-misses-init)
- docs: fold performance optimizations into Beta.7 changelog
- perf: lightweight getVersionSnapshot, GH_RESPONSE_CAP 12KB, net-fetch serialization, stack watermarks
- chore: bump version to 2.2.0-Beta.7
- fix: token NVS persistence (no 401 after reboot), task stack overflows, idle logout, log noise
- fix(webui): hide native file input to prevent double file dialog on manual firmware upload
- chore: bump version to 2.2.0-Beta.6
- fix(webui): show real error instead of 'up to date' when update check fails
- chore: bump version to 2.2.0-Beta.5
- fix: beta-channel JSON parse, updatecheck boot race, changelog proxy OOM retry
- fix(build): silence -Wstringop-truncation in OTA error snapshot (snprintf)
- fix(build): replace removed ESP_ERR_HTTPS_OTA_INCOMPLETE with private OTA error code
- fix(ci): make update_version.py idempotent; fix mangled TROUBLESHOOTING version string
- fix(build): resolve Beta.4 compile errors (NO_DATA macro clash, snprintf truncation)
- chore: bump version to 2.2.0-Beta.4
- fix(ci): make update_changelog.py idempotent to avoid duplicate sections
- chore: bump version to 2.2.0-Beta.4
- docs: rewrite CHANGELOG with full curated history (2.1.0 -> 2.2.0-Beta.4)
- Merge pull request #333 from Xerolux/dependabot/github_actions/actions/checkout-7
- Merge pull request #325 from Xerolux/dependabot/github_actions/crate-ci/typos-1.47.2
- mqtt-phase-a-b
- feat(mqtt): Phase A + B - LWT/Birth, command token, OTA state, extended status topics
- refactor(webui): unify icons and design-token usage across UI
- feat(update): switch to GitHub Releases API with optional beta channel
- chore: upgrade toolchain and dependencies to latest
- fix(webui): fix dirty-tracking, auth redirect, OTA feedback and version compare
- fix(firmware): harden memory safety and bounds checks
- chore(ci)(deps): bump actions/checkout from 6 to 7
- fix-firmware-hardening
- fix: harden monitoring and log handling
- chore: bump version to 2.2.0-Beta.3
- performance-stability-upgrade
- feat: async monitoring diagnostic, i18n for hardcoded UI strings
- perf(webui): move update-check/changelog proxies off the httpd task
- fix(webui): OTA upload timeout, 401 redirect for visitors, polling robustness
- fix: boot hang hardening, httpd/MQTT/NTP stability fixes
- fix: RTC detection broken on IDF 6.0 and perf-oriented build config
- chore(ci)(deps): bump crate-ci/typos from 1.45.1 to 1.47.2
- chore: bump version to 2.2.0-Beta.2
- chore: bump version to 2.2.0-Beta.2 - IDF 6.0.1 compat & deps upgrade
- Merge pull request #316 from EarlSneedSinclair/mqtt-tls
- feat(mqtt): add TLS/SSL support for MQTT configuration
- Merge pull request #310 from Xerolux/dependabot/npm_and_yarn/webui/vue-i18n-11.4.0
- chore(deps)(deps): bump vue-i18n from 11.3.2 to 11.4.0 in /webui
- fix: correct update banner showing when running version is newer than available
- fix: semver pre-release comparison and log polling stability
- feat: live LED brightness update without restart
- chore: bump version to 2.2.0-Beta.1
- chore: prepare v2.2.0-Beta.1 release

## [2.2.0-Beta.10] - 2026-06-29

### Fixed
- **Log sharing via multipart upload**: replaced URL-encoded form POST with proper multipart/form-data upload to MicroBin. Fixes the "Paste service returned HTTP 200" error — the paste service now correctly returns a 303 redirect with the share URL.
- **TLS certificate verification**: switched ESP-IDF CA bundle from CMN subset to full Mozilla root CA, covering the ECDSA root (ISRG Root X2) required by Let's Encrypt / GitHub CDN. Fixes persistent `PSA_ERROR_GENERIC_ERROR (0xffffff73)` / TLS handshake failures during update checks and OTA downloads.

## [2.2.0-Beta.9] - 2026-06-29

### Added
- **OTA success modal**: after a firmware update, the WebUI automatically redirects to the home page and shows a "Update to version X successful" dialog. Auto-closes after 10 seconds or on user confirmation.

### Fixed
- **TLS handshake failure with GitHub**: the ESP-IDF 6.x PSA Crypto CA bundle causes `PSA_ERROR_GENERIC_ERROR (0xffffff73)` when verifying newer Let's Encrypt / ISRG Root X2 certificates. Removed `crt_bundle_attach` from `configure_ota_http_client` — TLS encryption is retained but the server certificate chain is not verified. Fixes persistent `mbedtls_ssl_handshake -0x3000` / `-0x008D` errors when checking for updates and downloading firmware.
- **Paste service sharing**: MicroBin returns a 303 redirect, but ESP-IDF auto-followed it (returning 200). Now uses `disable_auto_redirect` + event handler to properly capture the `Location` response header.
- **Duplicate download button**: the system log page showed two download icons (one was the auto-scroll toggle using the wrong icon). Auto-scroll now uses a chevron icon.

## [2.2.0-Beta.8] - 2026-06-29

### Added
- **System Log sharing**: new "Share" button in the system log uploads a comprehensive debug report (system info, network config, radio module status, monitoring config, LED programs, and full log) to a MicroBin paste service. Passwords, MQTT command tokens, and TLS keys are automatically redacted as `****` or `<set>`.
- **Globe favicon**: replaced the generic favicon with a custom globe icon visible in browser tabs.

### Fixed
- **Log copy on HTTP**: the "Copy" button in the system log now falls back to `document.execCommand('copy')` when the Clipboard API is unavailable in non-HTTPS contexts (local ESP32 access).
- **Update check buffer**: increased `GH_RESPONSE_CAP` from 12 KB to 24 KB to avoid JSON truncation when a GitHub release has a large changelog body. The buffer was previously reduced to 12 KB in Beta.7 but proved insufficient for some releases.

### Changed
- Raised `max_uri_handlers` from 25 to 30 to accommodate the new share endpoint.

## [2.2.0-Beta.7] - 2026-06-28

### Added
- **Token persists across reboots**: the authentication token is now saved to NVS. "Remember me" / persistent login survives firmware updates and device restarts — no more spurious 401 / redirect-to-login after a reboot. Token is regenerated on password change and cleared on factory reset.

### Fixed
- **30-second reboot loop**: the UpdateCheck task copied the 5 KB `ReleaseInfo` struct (with its 4 KB `body` field) onto a 8 KB stack — tight enough that TLS stack frames could overflow it, triggering a watchdog reset exactly 30 s after boot (the wait-before-first-check delay). UpdateCheck task stack raised to 12 KB; MQTT publish task (also copies the struct) raised from 6→10 KB.
- **Immediate logout on page load**: the idle-timer `lastActivity` was loaded from `localStorage`, so a stale timestamp from hours before caused an instant logout on the first 30 s cycle. `lastActivity` now always starts fresh on page load.
- **Log noise**: `httpd_parse: parse_block: incomplete` and `esp-x509-crt-bundle: Certificate validated` (3–4× per boot) are suppressed at the log level.

### Optimized
- **Lightweight version snapshot**: `UpdateCheck::getVersionSnapshot()` returns a 160-byte struct instead of the full 5 KB `ReleaseInfo` — the MQTT publish task (every 5 s) and the background checker no longer copy the 4 KB body on their stack. The WebUI still uses the full `getReleaseInfo()` for the release-notes preview.
- **Smaller heap footprint**: GitHub response buffer reduced from 24→12 KB (a single-release fetch fits in ~8 KB after the `?per_page=1` change).
- **Serialized external HTTPS requests**: a shared mutex (`g_net_fetch_mutex`) ensures the UpdateCheck fetch and the changelog proxy never open two TLS handshakes at once on the ESP32's limited heap.
- **Stack high-water-mark logging**: the UpdateCheck task and MQTT publish task log their remaining stack at each (first) cycle for early detection of future tightness.

## [2.2.0-Beta.6] - 2026-06-28

### Fixed
- **WebUI falsely showed "Firmware is up to date" when the update check failed**: when the GitHub fetch errored (e.g. the beta-channel parse bug on Beta.4), the firmware reported `updateAvailable: false` and the firmware-update page fell back to "aktuell"/"up to date" instead of the actual error. The page now surfaces the real error ("Update-Prüfung fehlgeschlagen: …") with a retry button. New `firmware.checkFailed` translations added for all 10 locales.

## [2.2.0-Beta.5] - 2026-06-28

### Fixed
- **Beta update channel was broken**: the `/releases` payload (15+ entries with their full bodies) exceeded the 24 KB response cap and was truncated, so `cJSON_Parse` failed and beta-channel users never saw an update. The beta endpoint now fetches only the newest release (`?per_page=1`).
- **Bogus "Failed to determine latest version" error at boot**: `UpdateCheck::_taskFunc` treated a coalesced (already-in-progress) `refresh()` as a hard failure and logged an empty error string. It now uses the cached snapshot's validity as the authority and only logs an error when a real error is present.
- **Changelog proxy ran out of memory at boot**: opening the WebUI while the background update-check fetch was running opened two TLS connections at once and exhausted the heap (`HTTP_CLIENT: Allocation failed`). The proxy now retries `esp_http_client_init` once after a short delay so the changelog load self-heals.

## [2.2.0-Beta.4] - 2026-06-28

### Added
- **MQTT Last Will & Testament + Birth message**: the device publishes `status/online`, and the broker flips it to `offline` on unclean disconnects, so Home Assistant reliably detects the device going offline.
- **MQTT command-topic security**: optional shared-secret command token (`mqtt.command_token`, 8–63 chars, charset `A–Z a–z 0–9 - _ .`). When set, every command payload (`restart` / `update` / `check_update`) must match the token exactly. The token is write-only (never returned by `GET`) and published verbatim as HA `payload_press` / `payload_install` so HA buttons keep working. Broker ACL requirement documented in TROUBLESHOOTING.md and WIKI.md.
- **Command topics without HA discovery**: new `mqtt.command_enabled` flag (default on) lets plain-MQTT users trigger commands even without HA discovery.
- **OTA state machine with live progress**: OTA now reports `idle / checking / starting / downloading / flashing / success / failed` with real 0–100 % download progress. New MQTT topics `status/ota_state`, `status/ota_progress`, `status/ota_error` plus transient `event/*` events (`update_started`, `update_downloading`, `update_finished`, `command_rejected`, …).
- **Richer status topics**: Ethernet (link / speed / duplex / IP / gateway), radio module (type / serial / firmware), reset reason, detailed heap breakdown and NTP sync state.
- **`check_update` command** to refresh release info without flashing.
- **Optional Beta update channel** (`betaChannel`, default off): stable users stay on stable releases, beta testers opt into pre-releases. The update check now uses the public GitHub Releases API as the single source of truth, replacing the xerolux.de-hosted `version.txt` + firmware mirror.
- Release-notes preview, beta badge and collapsible changelog on the firmware-update page.
- New AppIcon SVG set (lock, radio, satellite, arrowRight, gitFork, link, package, list, externalLink).
- New i18n keys (beta, betaChannel, betaChannelHint, releaseNotesPreview, viewOnGithub, noDownloadUrl, …) across all 11 locales.

### Changed
- **OTA**: replaced the blocking `esp_https_ota()` with the advanced `esp_https_ota_begin/perform/finish/abort` API; the publish task now reacts to OTA state changes within ~1 s instead of the 60 s cycle and emits progress events every ~5 %. Publish-task stack raised 4 KB → 6 KB.
- **Update check is non-blocking and cached**: the 24 h background refresh and a manual "Check now" coalesce via a fetch lock to respect GitHub's unauthenticated rate limit (60 req/h/IP). Manual refresh runs via `httpd_req_async_handler` so the single-threaded server stays responsive.
- **WebUI**: replaced emoji icons with a unified AppIcon SVG system, replaced hardcoded colors with design tokens, and replaced hardcoded English strings with i18n keys.
- **API**: `GET/POST /api/monitoring` expose `commandEnabled` / `commandTokenSet` / `commandTokenClear`; documented `GET/POST /api/check_update`, `/api/changelog`, `/api/ota_url`, `/api/ota_status` and the `betaChannel` setting in `API.md` and `openapi.yaml`.

### Fixed
- **Heap overflow** in UART RX buffers (radio-module + GPS): buffers now match the driver RX-buffer size, so `event.size > 128 B` no longer corrupts the heap.
- CRC16 read/write now use `memcpy` (unaligned / strict-aliasing safe).
- `Settings::save()` / `clear()` now bail out on `nvs_open` failure instead of using an uninitialized handle.
- `StreamParser` discards oversized frames instead of processing a stale buffer (prevents stream desync).
- Replaced the VLA in `RadioModuleDetector::sendFrame` with a fixed buffer + bounds check.
- Guarded the `cJSON_Print` result in the OTA-status handler against OOM NULL-deref.
- Hardened monitoring and log handling.
- **WebUI**: LED-program edits now mark the form dirty (`deep:true`) — previously lost on navigation; the forced-change gate in `PasswordChangeModal` can no longer be bypassed via ESC/backdrop; TLS clear-flags reset when loading a fresh PEM; the OTA countdown interval is cleared; `/api/ota_url` errors are surfaced instead of swallowed; pre-release segments are compared numerically (`Beta.10 > Beta.3`); a `401` on public pages (`/about`) no longer bounces unauthenticated visitors to `/login`.
- Removed obsolete SNMP tests referencing a non-existent function.

### Internal
- npm: vue 3.5.39, vue-router 5.1.0, vite 8.1.0, axios 1.18.1, marked 18.0.5, bootstrap-vue-next 0.45.7, @playwright/test 1.61.1, @vitejs/plugin-vue 6.0.7, esbuild 0.28.1; transitive form-data 4.0.6 fixes a CRLF-injection issue (0 vulnerabilities).
- ESP-IDF CI pinned to the stable tag v6.0.1; managed components mdns ^1.11.2, mqtt ^1.0.0.
- GitHub Actions: checkout v6 → v7, crate-ci/typos 1.45.1 → 1.47.2.
- Build/CI: resolve Beta.4 compile errors (lwIP `NO_DATA` macro clash in `streamparser.h`, `snprintf` format-truncation in `mqtt_handler.cpp`, removed `ESP_ERR_HTTPS_OTA_INCOMPLETE`, `-Wstringop-truncation` in the OTA snapshot); make `update_changelog.py` and `update_version.py` idempotent so release re-runs neither duplicate changelog sections nor accumulate `-Beta.x` version suffixes.

## [2.2.0-Beta.3] - 2026-06-12

### Added
- Async monitoring diagnostic; i18n for previously hardcoded UI strings.

### Changed
- Performance & stability upgrade; moved the update-check / changelog proxies off the httpd task.

### Fixed
- OTA upload timeout, 401 redirect for visitors and log-polling robustness.
- Boot-hang hardening; httpd / MQTT / NTP stability fixes.
- RTC detection broken on IDF 6.0 and in the perf-oriented build config.

## [2.2.0-Beta.2] - 2026-05-27

### Added
- **MQTT TLS/SSL support** (CA certs, mTLS, skip-verify) — thanks @EarlSneedSinclair (PR #316).
- Live LED-brightness update without restart.
- ESP-IDF 6.0.1 build compatibility (I2C struct fields, `time.h` includes).

### Changed
- Upgraded WebUI npm dependencies; espressif/mdns ^1.9.1; added sdkconfig.defaults entries for partition table, flash size and FreeRTOS trace.

### Fixed
- Update banner showing when the running version is newer than the available one.
- Semver pre-release comparison and log-polling stability.
- Missing `#include <time.h>` in `systemclock.cpp`; I2C struct-initializer field order in `rtcdriver.cpp`; suppressed unused-variable warnings in `semver.h`; double version suffix in `TROUBLESHOOTING.md`.

## [2.2.0-Beta.1] - 2026-04-24

### Added
- **ESP-IDF 6.0 migration**: LAN87xx PHY driver from esp-eth-drivers, external RMII clock config for the LAN8720A, LEDC driver, CMake/sdkconfig updates; renamed `src/` to `main/`; moved mqtt/json to managed components.
- Improved WebUI error handling, timeouts and API-response validation.

### Changed
- Migrated FreeRTOS tick conversion to the IDF 6.0 API.
- WebUI responsive-design improvements across all viewports; stability, layout, performance and design polish.

### Fixed
- Ethernet-init debug logging; EMAC clock reverted to CLK_OUTPUT GPIO17 (correct for the HB-RF-ETH board).
- Integer overflow in DNS-cache time calculation after ~12 h; `sem_take` timeout units (was ms instead of seconds); static definition type widened to `uint64_t`.
- CI now uses the esp-idf `install.sh` for v6 setup.

## [2.1.10] - 2026-03-20

### Added
- **Working SNMP, MQTT and CheckMK monitoring** (previously non-functional); GitHub Pages landing page and deployment workflow.

### Changed
- Upgraded Vite to 8; dependency refresh.

### Fixed
- **Monitoring thread-safety**: complete refactor to eliminate hangs and crashes; monitoring-config save is now non-blocking (async task); services only stop/restart when config actually changed.
- Stack overflows in the monitoring POST response, the CheckMK agent and `apply_config_task` (stacks raised to 8192 bytes); `checkmk_stop()` crash when disabling CheckMK after it was running; mqtt/checkmk stop crashes and race conditions.
- I2C-master API and missing `i2c_port` field for ESP-IDF 5.5.3; UART warnings; C++ designated-initializer compilation errors.
- CCU reconnect with a stale endpoint identifier after device reboot (now accepted); watchdog reconnect logic restored.
- Monitoring-settings save redirecting to login without saving; saving monitoring config with empty strings; skipping validation of empty strings when the service is disabled.
- SNMP removed again (not supported in ESP-IDF 5.5.3 / `CONFIG_LWIP_SNMP` unavailable).

## [2.1.9] - 2026-03-09

### Added
- Privacy disclaimer for external update checks.

### Changed
- Simplified release/artifact naming (dropped "Firmware" from the release name); updated platform/framework versions.

### Fixed
- Dependency / security updates (axios, vue, vue-i18n, bootstrap-vue-next, marked, vue-router, rollup, typos, upload-artifact).
- Refreshed documentation and screenshots; changelog-button visibility on the firmware page.

## [2.1.8] - 2026-02-22

### Fixed
- OTA updates failing with GitHub redirects (Bug #235).
- CORS error in the manual update check.
- OTA check interval and upload error; switched the OTA update server to xerolux.de.

### Added
- OTA functional test script.

## [2.1.7] - 2026-02-21

### Added
- **Fully configurable LED programs** (replaces the fixed update-blink toggle).
- Home Assistant MQTT Auto-Discovery integration.

### Changed
- Refactored LED programs; automated version bump in the release workflow.

### Fixed
- LED programs not saved/loaded correctly; immediate LED-state update and settings caching.
- IPv6 validation: suppressed spurious warnings for hostname server addresses; IPv6 support in CCU frontend validation.
- Bugs in GPS, IPv6 validation, OTA and the CPU task; UDP listener, MQTT handler and DNS cache; settings persistence and security; factory-reset linker error in the MQTT handler; compilation errors in LED and DNS caching.

### Security
- String-length validation before `strncpy` to prevent bypass; CCU address validation; MQTT-server, SNMP-community and NTP-server validation.

## [2.1.6] - 2026-02-12

### Added
- Real OTA progress tracking with a status-polling endpoint.
- Switchable LED blink on firmware update.
- Backend proxy for the changelog.

### Changed
- Optimized WebUI idle timer, accessibility and JSON serialization; refreshed translations and the mobile language menu.
- Upgraded ESP-IDF from v5.5.0 to v5.5.2.

### Fixed
- **Bugfix release**: backup/restore missing settings; CORS removed from the monitoring API; log-offset sync on ring-buffer overflow; OTA task double response.
- MQTT password loss, `ccuIP` validation, OTA double response, hostname mismatch, MQTT race condition.
- `PasswordChangeModal` validation; firmware-update URL format; password-change failure and settings persistence.

## [2.1.5] - 2026-02-10

### Added
- Mobile-optimized WebUI + bug fixes.
- Log Manager WebUI with enable/disable toggle; high-contrast system-log viewer.
- 10-minute idle auto-logout and persistent session.
- Maintenance-action modals; loading states on backup/restore and async buttons.
- Full-system restart that resets peripherals on reboot.

### Changed
- Migrated the WebUI bundler from Parcel to Vite; modernized UI/UX.
- Replaced hardcoded fallbacks with translation keys; completed localization.

### Fixed
- Race conditions, resource leaks and long-term stability; hardened string operations, fixed a memory leak, removed dead code.
- CCU connection stability with automatic reconnect; UDP connection handling.
- Vite code-splitting causing a blank WebUI; compilation errors (`esp_http_client_get_content_length`, `mdns_service_register_all` for ESP-IDF 5.1).
- OTA panics on failed updates; memory leak and CI issues.
- IPv6 support in the RateLimiter (fixed IP-resolution warning).

### Changed
- Removed the OTA-password requirement for firmware updates.

## [2.1.2] - 2026-02-07

Re-stabilization release restoring the v2.1.1 codebase after the experimental
December line (2.1.3 / 2.1.4) was rolled back.

### Fixed
- **RaspberryMatic Raw UART UDP connection**: accept the client endpoint ID to sync persistent sessions; set `_connectionStarted = true` on persistent-session reconnect; restore reliable reconnect after restart.
- Simplified Raw-UART UDP connection handling (removed unnecessary port clearing).

## [2.1.4] - 2025-12-20

Experimental feature release (later rolled back; re-stabilized in 2.1.2 on 2026-02-07).

### Added
- Multi-variant firmware builds (HMLGW and Analyzer features via conditional compilation) with firmware-variant matching in the update check.
- HomeMatic LAN Gateway (HMLGW) emulation mode.
- Analyzer Light feature with RSSI and naming.
- Optional DTLS/TLS transport encryption for Raw-UART UDP (ESP-IDF 5.x / mbedTLS 3.x).
- Nextcloud WebDAV backup integration.
- Content-Security-Policy and HTTP security headers; ETag caching for embedded static files.
- Reboot-resistant system log; maintenance controls.

### Changed
- Performance phases: FreeRTOS stack optimizations (~5 KB RAM saved), HMLGW ring-buffer & busy-wait elimination (~10–20 % CPU reduction), analyzer WebSocket buffer pool, `StreamParser` bulk copy, small-buffer optimization for `AnalyzerFrame` (~20 KB → ~1.5 KB queue heap).
- Refactored WebUI JSON handling; standardized password inputs with visibility toggle; modals for maintenance actions; visual icons.

### Fixed
- Network-stack stall via non-blocking queue sends in UDP callbacks; CCU reconnection after restart.
- Stack overflows in CheckMK/Analyzer agents; race condition in HMLGW task termination; analyzer WebSocket delivery.

### Security
- Cache-Control headers on sensitive endpoints; plain-text password removed from backup JSON; OTA rollback prevention; password-complexity enforcement; IP-whitelist bypass in CheckMK fixed.

## [2.1.3] - 2025-12-19

### Added
- Cache-Control headers; ETag caching for embedded static files; maintenance controls; password visibility toggle.

### Changed
- Upgraded to ESP-IDF 5.5.1 (from 5.1.0); platform / dependency refresh.

### Fixed
- 16+ critical bugfixes and optimizations; UDP packet handling (removed heap allocations).

### Security
- Stack buffer overflow in the monitoring agent (CRITICAL); timing attack in auth (HIGH); IP-whitelist bypass in CheckMK; password-length validation enforced.

## [2.1.1] - 2025-12-15

### Added
- **MQTT support** for system-status monitoring.
- **Home Assistant MQTT Discovery** support.
- Online update check with toggle; early-updates toggle and release-notes display.
- WebUI: auto-logout and manual logout redirect; login button when unauthenticated.

### Changed
- i18n: translations updated for all 10 languages; openCCU compatibility and i18n language switching.
- Increased the max URI handlers for the WebUI.

### Fixed
- 401 Unauthorized on backup download; variable shadowing and compiler warnings; CppCheck warnings.

### Security
- Timing-attack-resistant auth comparison; accessibility of the theme toggle.

## [2.1.0] - 2025-12-14

### Added
- Initial public release of the HB-RF-ETH-ng fork: ESP32 firmware ported to ESP-IDF 5.x, based on the original HB-RF-ETH by Alexander Reinert. Connects HomeMatic radio modules (HM-MOD-RPI-PCB, RPI-RF-MOD) to debmatic / piVCCU3 over Ethernet.
