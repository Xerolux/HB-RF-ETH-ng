# CLAUDE.md — AI Assistant Guide for HB-RF-ETH-ng

## Project Overview

**HB-RF-ETH-ng** is a modernized ESP32 firmware for the HB-RF-ETH network interface board (version 2.x). It connects HomeMatic radio modules (HM-MOD-RPI-PCB, RPI-RF-MOD) to debmatic / piVCCU3 installations over Ethernet. This is a fork of the original HB-RF-ETH firmware by Alexander Reinert, ported to ESP-IDF 5.x by Xerolux (2025).

- **Current version:** 2.1.11
- **License:** Creative Commons BY-NC-SA 4.0 (non-commercial)
- **MCU:** ESP32 (WROOM-32), 240 MHz, 4 MB Flash, 328 KB RAM
- **Ethernet PHY:** LAN8720A

---

## Repository Structure

```
HB-RF-ETH-ng/
├── src/                    # C++ firmware source files
├── include/                # C++ header files (shared across components)
│   └── pins.h              # GPIO pin definitions (canonical hardware map)
├── lib/                    # Additional libraries (if any)
├── webui/                  # Vue.js 3 web interface
│   ├── src/                # Vue components, stores, locales
│   └── dist/               # Built WebUI assets (generated, embedded in firmware)
├── test/                   # PlatformIO unit tests
├── boards/                 # Custom PlatformIO board definition
│   └── hb-rf-eth-ng.json
├── docs/                   # Reference documentation
│   ├── API.md              # REST API reference
│   ├── TROUBLESHOOTING.md
│   ├── openapi.yaml        # OpenAPI 3.x spec
│   └── ALT_BUILD_WORKFLOWS.md
├── .github/workflows/      # CI/CD pipelines
├── CMakeLists.txt          # ESP-IDF root CMake config
├── platformio.ini          # PlatformIO project config
├── partitions.csv          # Flash partition table
├── sdkconfig.hb-rf-eth-ng  # ESP-IDF SDK configuration
├── sdkconfig.defaults      # SDK config overrides
├── version.txt             # Single source of truth for version
└── CHANGELOG.md            # Auto-generated changelog
```

---

## Build System

### Toolchain

| Tool | Version |
|------|---------|
| PlatformIO | Latest |
| ESP-IDF framework | 5.5.3 (via `framework-espidf@~3.50503.0`) |
| Xtensa GCC toolchain | 14.2.0+20251107 |
| Platform package | `espressif32@^6.13.0` |

### Building the Firmware

```bash
# Full build (auto-builds WebUI first via pre-script)
pio run

# Build and upload to device
pio run --target upload

# Serial monitor (115200 baud)
pio device monitor

# Clean build artifacts
pio run --target clean
```

### WebUI Build (standalone)

The WebUI is built automatically via `build_webui.py` pre-script. To build manually:

```bash
cd webui
npm install
npm run build      # outputs to webui/dist/
```

Built assets are gzip-compressed and embedded into firmware via `board_build.embed_files` in `platformio.ini`.

### Pre-build Python Scripts

These run automatically before firmware compilation:

| Script | Purpose |
|--------|---------|
| `build_webui.py` | Builds the Vue.js WebUI |
| `rename_webui_files.py` | Gzip-compresses WebUI assets |
| `append_version_to_progname.py` | Appends version to output binary name |

---

## Hardware Pin Map (`include/pins.h`)

| Signal | GPIO |
|--------|------|
| HM UART RX | GPIO 35 |
| HM UART TX | GPIO 2 |
| HM I2C SDA | GPIO 18 |
| HM I2C SCL | GPIO 5 |
| HM Reset | GPIO 23 |
| HM Button | GPIO 34 |
| RGB Red LED | GPIO 15 |
| RGB Green LED | GPIO 14 |
| RGB Blue LED | GPIO 12 |
| Status LED | GPIO 4 |
| Power LED | GPIO 16 |
| DCF77 input | GPIO 39 |
| ETH PHY Power | GPIO 13 |
| ETH MDC | GPIO 32 |
| ETH MDIO | GPIO 33 |
| Board Rev sense | ADC1 / GPIO 36 |
| Supply voltage sense | ADC1 / GPIO 37 |

**Never change pin assignments without verifying hardware schematics.**

---

## Firmware Architecture (`src/`)

The firmware runs on FreeRTOS with separate tasks per subsystem. Key source files:

| File | Responsibility |
|------|---------------|
| `main.cpp` | Entry point, task orchestration, hardware init |
| `ethernet.cpp` | Ethernet interface, DNS cache, DHCP/static IP |
| `radiomoduledetector.cpp` | Auto-detects HM radio module type (UART/I2C probe) |
| `radiomoduleconnector.cpp` | UART/I2C bridge to radio module |
| `rawuartudplistener.cpp` | UDP↔UART bridge (the core protocol relay) |
| `webui.cpp` | HTTP server + all REST API endpoint handlers |
| `settings.cpp` | Persistent config via NVS Flash |
| `monitoring.cpp` | CheckMK agent and MQTT monitoring |
| `mqtt_handler.cpp` | MQTT client, reconnect logic, message dispatch |
| `monitoring_api.cpp` | REST endpoints for monitoring config |
| `ntpclient.cpp` | NTP time sync client |
| `ntpserver.cpp` | NTP server for downstream clients |
| `dcf.cpp` | DCF77 radio time decoding |
| `gps.cpp` | GPS-based precise time sync |
| `rtcdriver.cpp` | Hardware RTC driver |
| `systemclock.cpp` | Unified system time management |
| `led.cpp` | RGB + status LED control |
| `pushbuttonhandler.cpp` | Factory reset button logic |
| `updatecheck.cpp` | OTA firmware update checking |
| `ota_config.cpp` | OTA server configuration |
| `mdnsserver.cpp` | mDNS / Bonjour advertisement |
| `validation.cpp` | Input validation (IP, hostname, passwords, etc.) |
| `log_manager.cpp` | Ring-buffer system log |
| `sysinfo.cpp` | System info queries (heap, uptime, version) |
| `rate_limiter.cpp` | Request rate limiting |
| `hmframe.cpp` | HomeMatic frame parsing |
| `streamparser.cpp` | Generic stream frame parser |

### Key Conventions in C++ Code

- All source files include a copyright header (see `include/pins.h` for the template).
- Use `ESP_LOGI/LOGW/LOGE` macros for logging — never `printf` directly.
- NVS namespace keys are short strings (max 15 chars per ESP-IDF NVS constraint).
- Thread safety: mutexes (`SemaphoreHandle_t`) are used wherever data is shared across FreeRTOS tasks. The monitoring/MQTT subsystem was specifically refactored for thread-safety (v2.1.x).
- HTTP handler functions in `webui.cpp` follow the pattern `esp_err_t handle_<endpoint>(httpd_req_t *req)`.
- Settings persistence uses `settings.cpp` — avoid direct NVS calls elsewhere.

---

## WebUI Architecture (`webui/src/`)

Built with **Vue.js 3** + **Bootstrap 5** + **Vite**.

| File/Dir | Purpose |
|----------|---------|
| `main.js` | App entry, Vue app creation, plugin registration |
| `app.vue` | Root component, router-view, global layout |
| `stores.js` | Pinia stores (auth token, system state) |
| `login.vue` | Authentication page |
| `home.vue` | Dashboard |
| `settings.vue` | Network/device configuration |
| `monitoring.vue` | MQTT / CheckMK configuration |
| `firmwareupdate.vue` | OTA firmware update UI |
| `sysinfo.vue` | System information display |
| `systemlog.vue` | Live system log viewer |
| `about.vue` | About / version info page |
| `change-password.vue` | Password change form |
| `header.vue` | Navigation header component |
| `components/` | Reusable UI components |
| `locales/` | i18n translation files (vue-i18n) |
| `styles/` | Custom CSS overrides |

### WebUI Key Dependencies

```json
"vue": "^3.x",
"bootstrap": "^5.x",
"bootstrap-vue-next": "^0.x",
"axios": "^1.x",
"vue-router": "^5.x",
"vue-i18n": "^11.x",
"pinia": "^3.x",
"vuelidate": "^2.x"
```

### API Communication

All REST calls use `axios` with base URL `/`. Authentication uses `Authorization: Token <token>` header. The token is obtained via `POST /login.json` and stored in Pinia.

---

## REST API Summary

See `docs/API.md` and `docs/openapi.yaml` for the full specification.

**Authentication:** `POST /login.json` → returns `{ token: "..." }`
**All other endpoints** require header: `Authorization: Token <token>`

Key endpoint categories:
- `/sysinfo.json` — system status
- `/settings.json` — read/write device settings
- `/monitoring.json` — monitoring configuration
- `/log.json` — system log
- `/update` — OTA firmware upload
- `/resetpassword` — password reset

---

## Testing

### Firmware Unit Tests (PlatformIO)

Tests live in `test/` subdirectories and use the PlatformIO native test runner:

```bash
pio test
```

Test suites:
- `test_ccu_validation` — CCU connection parameter validation
- `test_ipv6_validation` — IPv6 address parsing/validation
- `test_static_ipv4_and_password` — Static IP and password config
- `test_ntp_validation` — NTP configuration validation
- `test_ota_config` — OTA update configuration
- `test_monitoring_validation` — Monitoring service parameters

### WebUI Tests (Playwright)

```bash
cd webui
npm run test     # runs Playwright end-to-end tests
```

### OTA Function Test

```bash
python3 test_ota_function.py --help
```

---

## CI/CD (`.github/workflows/`)

| Workflow | Trigger | Purpose |
|----------|---------|---------|
| `build.yml` | Push / PR | Build WebUI + firmware, run unit tests |
| `release.yml` | Tag push | Version bump, changelog, GitHub release |
| `pr-check.yml` | PR open/update | Lint, validate, label PRs |
| `security.yml` | Schedule / push | Security scanning |
| `pages.yml` | Push to main | Deploy docs to GitHub Pages |
| `docs.yml` | Push | Regenerate API docs |
| `labeler.yml` | PR events | Auto-label PRs by file changes |
| `stale.yml` | Schedule | Close stale issues/PRs |
| `dependabot.yml` | Schedule | Dependency updates |

---

## Release Process

Releases are fully automated via `release.yml`:

1. Tag the commit: `git tag v2.x.x && git push --tags`
2. The workflow runs:
   - `update_version.py` — updates `version.txt` and all references
   - `update_changelog.py` — generates `CHANGELOG.md` from git history
   - `generate_release_notes.py` — creates GitHub release notes
   - Full firmware build with versioned binary name
   - Creates a GitHub Release with firmware `.bin` attached

Manual version/changelog tools:
```bash
python3 update_version.py <new_version>
python3 update_changelog.py
python3 generate_release_notes.py
```

---

## Development Conventions

### File Headers

All `.cpp` and `.h` files must include the license header:
```cpp
/*
 *  <filename> is part of the HB-RF-ETH firmware v2.0
 *
 *  Original work Copyright 2022 Alexander Reinert
 *  Modified work Copyright 2025 Xerolux
 *
 *  Licensed under CC BY-NC-SA 4.0
 */
```

Use `update_headers.py` to apply/update headers in bulk.

### Coding Style

- C++: Follow ESP-IDF conventions; use `esp_err_t` return codes
- Avoid dynamic memory allocation in ISR context
- Prefer stack-allocated buffers for HTTP responses where possible
- WebUI: Vue 3 Composition API preferred for new components
- i18n: All user-facing strings must go through `vue-i18n` — no hardcoded UI text

### Commit Messages

Follow conventional commits format used in auto-changelog:
```
feat: add GPS time sync support
fix: resolve MQTT reconnect race condition
docs: update API reference
chore: bump ESP-IDF to 5.5.3
```

### Branch Strategy

- `main` — stable releases only
- Feature branches: `feature/<description>`
- Bug fixes: `fix/<description>`
- Claude AI branches: `claude/<description>-<session-id>`

---

## Common Tasks for AI Assistants

### Adding a New REST Endpoint

1. Add handler function in `src/webui.cpp` following the pattern of existing handlers
2. Register the URI in the `httpd_uri_t` array at the bottom of `webui.cpp`
3. Add corresponding frontend API call in the appropriate Vue component
4. Document the endpoint in `docs/API.md` and `docs/openapi.yaml`

### Adding a New Setting

1. Define key constant in `include/settings.h` (max 15 chars for NVS key)
2. Add read/write functions in `src/settings.cpp`
3. Expose via REST API in `src/webui.cpp`
4. Add UI control in `webui/src/settings.vue`
5. Add i18n label in `webui/src/locales/`

### Modifying the Monitoring System

- Thread safety is critical — all monitoring state access must be guarded by the monitoring mutex
- See `src/monitoring.cpp` for mutex pattern
- Both MQTT and CheckMK share the same configuration structure

### Updating the WebUI

After any WebUI change, run:
```bash
cd webui && npm run build
```
The `pio run` command will pick up the rebuilt `webui/dist/` assets automatically.

### Flashing a Device

```bash
# Build and flash
pio run --target upload

# Flash only (no rebuild)
pio run --target upload --disable-auto-clean

# Monitor after flash
pio device monitor --baud 115200
```

---

## Important Files to Know

| File | Why it matters |
|------|---------------|
| `include/pins.h` | GPIO assignments — consult before any hardware-related change |
| `src/settings.cpp` | All persistent config logic lives here |
| `src/webui.cpp` | All HTTP/REST API handlers — largest file |
| `src/monitoring.cpp` | MQTT + CheckMK logic; thread-safety critical |
| `src/radiomoduledetector.cpp` | Core HomeMatic protocol detection |
| `platformio.ini` | Build configuration, embedded asset list |
| `sdkconfig.hb-rf-eth-ng` | ESP-IDF kernel/driver configuration |
| `partitions.csv` | Flash layout — change with extreme caution |
| `version.txt` | Single source of truth for version string |
| `webui/package.json` | Frontend dependency versions |
