# HB-RF-ETH REST API Documentation

## Overview

The HB-RF-ETH firmware provides a RESTful HTTP API for configuration and monitoring. All authenticated endpoints require a bearer token obtained through the login endpoint.

**Base URL:** `http://<device-ip>/`

**Authentication:** Token-based authentication using `Authorization: Token <token>` header

**Current Version:** v2.0 (Firmware 2.2.0-Beta.10 or later)

### Recent API Additions

| Feature | Version | Endpoint |
|---------|---------|----------|
| Token Persistence | 2.2.0-Beta.7 | All endpoints (token survives reboots) |
| OTA Success Feedback | 2.2.0-Beta.9 | GET/POST `/api/check_update`, `POST /api/ota_url` |
| Enhanced TLS Support | 2.2.0-Beta.10 | All HTTPS endpoints (Mozilla CA bundle) |
| Prometheus Exporter | 2.3.0 | `GET /metrics` on configurable port (default 9100) |
| Syslog Forwarding | 2.3.0 | Configured via `GET/POST /api/monitoring` (`syslog` block) |
| Event Notifications | 2.3.0 | Webhook / Telegram / Email via `notify` block |
| WebSocket Live Log | 2.3.0 | `GET /api/log/stream` (WS upgrade, `?token=…`) |
| Expanded Monitoring Config | 2.3.0 | `GET/POST /api/monitoring` now carries `prometheus`, `syslog`, `notify` blocks |

## Authentication

### POST /login.json

Authenticate with the device and obtain an access token.

**Request:**
```json
{
  "password": "string"
}
```

**Response (200 OK):**
```json
{
  "isAuthenticated": true,
  "token": "string"
}
```

**Response (401 Unauthorized):**
```json
{
  "isAuthenticated": false
}
```

**Example:**
```bash
curl -X POST http://192.168.1.100/login.json \
  -H "Content-Type: application/json" \
  -d '{"password":"admin"}'
```

---

## System Information

### GET /sysinfo.json

Retrieve system information including firmware version, hardware details, and radio module information.

**Authentication:** Required

**Response (200 OK):**
```json
{
  "sysInfo": {
    "serial": "string",
    "currentVersion": "string",
    "latestVersion": "string",
    "rawUartRemoteAddress": "string",
    "memoryUsage": 0.0,
    "cpuUsage": 0.0,
    "supplyVoltage": null,
    "temperature": null,
    "uptimeSeconds": 0,
    "boardRevision": "string",
    "resetReason": "string",
    "ethernetConnected": false,
    "ethernetSpeed": 0,
    "ethernetDuplex": "string",
    "radioModuleType": "string",
    "radioModuleSerial": "string",
    "radioModuleFirmwareVersion": "string",
    "radioModuleBidCosRadioMAC": "string",
    "radioModuleHmIPRadioMAC": "string",
    "radioModuleSGTIN": "string"
  }
}
```

**Fields:**

**System Information:**
- `serial`: Device serial number
- `currentVersion`: Currently installed firmware version
- `latestVersion`: Latest available firmware version
- `boardRevision`: Hardware board revision (e.g., "REV 1.10 (PUB)", "REV 1.8 (SK)")
- `uptimeSeconds`: System uptime in seconds since last boot
- `resetReason`: Reason for last system reset (e.g., "Power-On Reset", "Software Reset", "Watchdog")

**Performance Metrics:**
- `memoryUsage`: Memory usage percentage (0-100)
- `cpuUsage`: CPU usage percentage (0-100)
- `supplyVoltage`: Always `null`; retained for API compatibility because the board has no voltage-sense circuit.
- `temperature`: Always `null`; retained for API compatibility because the classic ESP32 has no supported internal temperature sensor.

**Network Status:**
- `ethernetConnected`: Boolean - Ethernet link status
- `ethernetSpeed`: Link speed in Mbps (10 or 100)
- `ethernetDuplex`: Duplex mode ("Full" or "Half")
- `rawUartRemoteAddress`: Remote address for raw UART connection

**Radio Module:**
- `radioModuleType`: Type of radio module (e.g., "RPI-RF-MOD", "HM-MOD-RPI-PCB")
- `radioModuleSerial`: Radio module serial number
- `radioModuleFirmwareVersion`: Radio module firmware version (e.g., "2.8.6")
- `radioModuleBidCosRadioMAC`: BidCos radio MAC address
- `radioModuleHmIPRadioMAC`: HmIP radio MAC address
- `radioModuleSGTIN`: Radio module SGTIN

**Example:**
```bash
curl -X GET http://192.168.1.100/sysinfo.json \
  -H "Authorization: Token YOUR_TOKEN_HERE"
```

**Example Response:**
```json
{
  "sysInfo": {
    "serial": "A1B2C3D4E5F6",
    "currentVersion": "2.2.0",
    "latestVersion": "2.1.12",
    "boardRevision": "REV 1.10 (PUB)",
    "uptimeSeconds": 345678,
    "resetReason": "Power-On Reset",
    "rawUartRemoteAddress": "192.168.1.50:2001",
    "memoryUsage": 45.32,
    "cpuUsage": 12.56,
    "supplyVoltage": null,
    "temperature": null,
    "ethernetConnected": true,
    "ethernetSpeed": 100,
    "ethernetDuplex": "Full",
    "radioModuleType": "HM-MOD-RPI-PCB",
    "radioModuleSerial": "KEQ0123456",
    "radioModuleFirmwareVersion": "2.8.6",
    "radioModuleBidCosRadioMAC": "1A2B3C4D",
    "radioModuleHmIPRadioMAC": "ABCDEF01",
    "radioModuleSGTIN": "3014F711A0123456789ABC"
  }
}
```

`supplyVoltage` and `temperature` are compatibility placeholders. Older firmware
published floating ADC data and `-127` respectively; consumers must treat both
fields as unavailable.

**System Monitoring:**

- `uptimeSeconds`: Monitor device stability (frequent reboots indicate issues)
- `resetReason`: Diagnose unexpected reboots:
  - "Watchdog" resets → Software hang or crash
  - "Brownout Reset" → Power supply issues
  - "Panic" → Critical software error
- `boardRevision`: Identify hardware version for support purposes
- `ethernetSpeed`/`ethernetDuplex`: Verify network link quality
  - Expected: 100 Mbit/s Full-Duplex
  - Lower speeds may indicate cable or switch issues

---

## Settings Management

### GET /settings.json

Retrieve current device settings.

**Authentication:** Required

**Response (200 OK):**
```json
{
  "settings": {
    "hostname": "string",
    "useDHCP": true,
    "localIP": "string",
    "netmask": "string",
    "gateway": "string",
    "dns1": "string",
    "dns2": "string",
    "ccuIP": "string",
    "enableIPv6": false,
    "ipv6Mode": "auto",
    "ipv6Address": "string",
    "ipv6PrefixLength": 64,
    "ipv6Gateway": "string",
    "ipv6Dns1": "string",
    "ipv6Dns2": "string",
    "timesource": 0,
    "dcfOffset": 40000,
    "gpsBaudrate": 9600,
    "ntpServer": "string",
    "ledBrightness": 100,
    "updateLedBlink": true,
    "betaChannel": false,
    "flashPause": false,
    "systemLogEnabled": false
  }
}
```

**Fields:**

**Network Settings:**
- `hostname`: Device hostname (max 63 characters, alphanumeric and hyphens)
- `useDHCP`: Enable/disable DHCP for IPv4
- `localIP`: Static IPv4 address (used when DHCP is disabled)
- `netmask`: Network mask (used when DHCP is disabled)
- `gateway`: Default gateway (used when DHCP is disabled)
- `dns1`: Primary DNS server (IPv4)
- `dns2`: Secondary DNS server (IPv4)
- `ccuIP`: CCU IP address (supports both IPv4 and IPv6)

**IPv6 Settings:**
- `enableIPv6`: Enable/disable IPv6
- `ipv6Mode`: IPv6 configuration mode ("auto" for SLAAC or "static")
- `ipv6Address`: Static IPv6 address (when mode is "static")
- `ipv6PrefixLength`: IPv6 prefix length (1-128, typically 64)
- `ipv6Gateway`: IPv6 default gateway (when mode is "static")
- `ipv6Dns1`: Primary DNS server (IPv6, when mode is "static")
- `ipv6Dns2`: Secondary DNS server (IPv6, when mode is "static")

**Time Settings:**
- `timesource`: Time source (0 = NTP, 1 = DCF77, 2 = GPS)
- `dcfOffset`: DCF77 signal offset in microseconds (-60000 to 60000)
- `gpsBaudrate`: GPS module baud rate (4800, 9600, 19200, 38400, 57600, 115200)
- `ntpServer`: NTP server hostname or IP (max 64 characters)

**System Settings:**
- `ledBrightness`: LED brightness (0-100)
- `updateLedBlink`: Enable/disable LED blinking for update notifications
- `betaChannel`: When `true`, the firmware update check considers pre-release versions published on GitHub (see [Firmware Management](#firmware-management)). Stable channel (default) only reports the latest stable release.
- `flashPause`: When `true`, a system restart holds the Ethernet PHY in hardware reset for ~35 seconds before the ESP32 actually reboots. This lets a connected CCU's link-loss watchdog trigger a clean CCU restart so no stale radio-mod connections survive a firmware update. Optional, defaults to `false`. Set via the Settings page; persisted in NVS alongside the other settings.
- `systemLogEnabled`: When `true`, the in-device ring-buffer system log is active and can be read via `/api/log`. Managed from the System Log page rather than the Settings page, but returned here for visibility.

**Example:**
```bash
curl -X GET http://192.168.1.100/settings.json \
  -H "Authorization: Token YOUR_TOKEN_HERE"
```

### POST /settings.json

Update device settings.

**Authentication:** Required

**Request:**
```json
{
  "hostname": "HB-RF-ETH-001",
  "useDHCP": false,
  "localIP": "192.168.1.100",
  "netmask": "255.255.255.0",
  "gateway": "192.168.1.1",
  "dns1": "8.8.8.8",
  "dns2": "8.8.4.4",
  "timesource": 0,
  "dcfOffset": 40000,
  "gpsBaudrate": 9600,
  "ntpServer": "pool.ntp.org",
  "ledBrightness": 80
}
```

**Response (200 OK):**
```json
{
  "settings": {
    // Updated settings object
  }
}
```

**Validation:**

All settings are validated on the backend before being applied. Invalid values will be rejected with a 400 Bad Request response.

**Network Validation:**
- `hostname`: Must be valid DNS hostname (alphanumeric, hyphen, dot), max 32 chars
- `localIP`, `netmask`, `gateway`, `dns1`, `dns2`: Must be valid IPv4 addresses
- `ccuIP`: Must be valid IPv4 or IPv6 address
- `ipv6Address`, `ipv6Gateway`, `ipv6Dns1`, `ipv6Dns2`: Must be valid IPv6 addresses
- `ipv6PrefixLength`: Must be between 1 and 128

**Time Validation:**
- `ntpServer`: Valid hostname or IP address, max 64 chars, must comply with DNS naming rules
- `dcfOffset`: -60000 to 60000 microseconds
- `gpsBaudrate`: Must be one of: 4800, 9600, 19200, 38400, 57600, 115200

**System Validation:**
- `ledBrightness`: 0-100

**Security Note:** All string inputs are validated for length and content to prevent buffer overflow and injection attacks. The backend performs comprehensive validation including:
- String length validation before buffer operations
- IP address format validation (both IPv4 and IPv6)
- Hostname and domain name DNS compliance checking
- Port number range validation
- Monitoring input validation for MQTT and CheckMK

**Example:**
```bash
curl -X POST http://192.168.1.100/settings.json \
  -H "Authorization: Token YOUR_TOKEN_HERE" \
  -H "Content-Type: application/json" \
  -d @settings.json
```

---

## Monitoring Configuration

### GET /api/monitoring

Retrieve monitoring configuration for MQTT, CheckMK, Prometheus, Syslog forwarding, and event notifications.

**Authentication:** Required

**Response (200 OK):**
```json
{
  "mqtt": {
    "enabled": false,
    "server": "",
    "port": 1883,
    "user": "",
    "password": "",
    "topicPrefix": "hb-rf-eth",
    "haDiscoveryEnabled": false,
    "haDiscoveryPrefix": "homeassistant",
    "tlsEnable": false,
    "tlsSkipVerify": false,
    "tlsCaCertsSet": false,
    "tlsCertfileSet": false,
    "tlsKeyfileSet": false,
    "commandEnabled": true,
    "commandTokenSet": false
  },
  "checkmk": {
    "enabled": false,
    "port": 6556,
    "allowedHosts": ""
  },
  "prometheus": {
    "enabled": false,
    "port": 9100,
    "allowedHosts": ""
  },
  "syslog": {
    "enabled": false,
    "server": "",
    "port": 514,
    "transport": 0,
    "minSeverity": 5,
    "hostname": ""
  },
  "notify": {
    "enabled": false,
    "channels": 0,
    "cooldownSeconds": 300,
    "webhookUrl": "",
    "webhookSecretSet": false,
    "telegramTokenSet": false,
    "telegramChatId": "",
    "smtpServer": "",
    "smtpPort": 587,
    "smtpTls": 1,
    "smtpUser": "",
    "smtpPasswordSet": false,
    "smtpFrom": "",
    "smtpTo": ""
  }
}
```

**Fields:**

**MQTT:**
- `enabled`: Enable/disable MQTT client
- `server`: MQTT broker hostname or IP address
- `port`: MQTT broker port (default: 1883, range: 1-65535)
- `user`: Optional username
- `password`: Optional password; an empty string keeps the stored password unchanged
- `topicPrefix`: Topic prefix for published entities and status messages
- `haDiscoveryEnabled`: Enable Home Assistant auto-discovery payloads
- `haDiscoveryPrefix`: Discovery prefix used for Home Assistant
- `tlsEnable`: Connect to the broker via TLS (port 8883 by default)
- `tlsSkipVerify`: Skip TLS certificate / hostname verification (insecure; lab only)
- `tlsCaCertsSet`: `true` if a custom CA bundle is stored (PEM). Use `tlsCaCertsClear=true` to remove.
- `tlsCertfileSet`: `true` if a client certificate (mTLS) is stored
- `tlsKeyfileSet`: `true` if a client key (mTLS) is stored
- `commandEnabled`: When `false`, the device does **not** subscribe to `<prefix>/command/#` and silently drops every command. Default: `true`.
- `commandTokenSet`: `true` if a shared-secret token has been configured. The token itself is never returned by the API. Send `commandTokenClear=true` to remove it, or a new `commandToken` value to replace it.

**CheckMK:**
- `enabled`: Enable/disable CheckMK agent
- `port`: CheckMK agent port (default: 6556, range: 1-65535)
- `allowedHosts`: Comma-separated list of allowed IP addresses/ranges (validated for IPv4/IPv6 format)

**Prometheus:**
- `enabled`: Enable/disable the Prometheus metrics exporter
- `port`: HTTP port the `/metrics` endpoint listens on (default: 9100, range: 1-65535)
- `allowedHosts`: Comma-separated source-IP allowlist (Prometheus scrapes carry no auth; the IP filter is the gate)

**Syslog:**
- `enabled`: Enable/disable Syslog forwarding
- `server`: Syslog server hostname or IP
- `port`: Syslog server port (default: 514)
- `transport`: `0` = UDP, `1` = TCP, `2` = TLS-over-TCP. The TLS transport takes the shared net-fetch mutex, so it is briefly deferred while a firmware OTA download is in progress.
- `minSeverity`: Minimum severity to forward (`0` = EMERG … `7` = DEBUG)
- `hostname`: Override the hostname tag in forwarded messages; empty = device hostname

**Event Notifications:**
- `enabled`: Master switch for the notification subsystem
- `channels`: Bitmask of active channels (`1` = webhook, `2` = Telegram, `4` = email)
- `cooldownSeconds`: Per-event-type dedupe window; only one notification per event type is sent within this window
- `webhookUrl` / `webhookSecret` (`webhookSecretSet`): HTTP POST target and shared secret (sent as `X-HB-RF-ETH-Secret` header). Secret write-only.
- `telegramToken` (`telegramTokenSet`) / `telegramChatId`: Telegram bot token and target chat ID. Token write-only.
- `smtpServer` / `smtpPort` / `smtpTls` (`0` = none, `1` = STARTTLS, `2` = implicit TLS) / `smtpUser` / `smtpPassword` (`smtpPasswordSet`) / `smtpFrom` / `smtpTo`: SMTP relay configuration. Password write-only. Note: an SMTP send holds the net-fetch mutex for the duration of the SMTP session; the OTA path defers event delivery until the OTA completes.

**Example:**
```bash
curl -X GET http://192.168.1.100/api/monitoring \
  -H "Authorization: Token YOUR_TOKEN_HERE"
```

### POST /api/monitoring

Update monitoring configuration.

**Authentication:** Required

**Request:**
```json
{
    "mqtt": {
      "enabled": true,
      "server": "mqtt.local",
      "port": 1883,
      "user": "hb-rf-eth",
      "password": "secret",
      "topicPrefix": "hb-rf-eth",
      "haDiscoveryEnabled": true,
      "haDiscoveryPrefix": "homeassistant",
      "tlsEnable": false,
      "tlsSkipVerify": false,
      "commandEnabled": true,
      "commandToken": "my-shared-secret-123",
      "commandTokenClear": false
  },
  "checkmk": {
    "enabled": true,
    "port": 6556,
    "allowedHosts": "192.168.1.0/24,10.0.0.5"
  }
}
```

**Command token validation:** the token must be 8–63 characters long and may
only contain `A–Z a–z 0–9 - _ .`. It is embedded verbatim in MQTT payloads
and Home Assistant discovery JSON, so any other character is rejected.

**Response (200 OK):**
```json
{
  "success": true
}
```

**Example:**
```bash
curl -X POST http://192.168.1.100/api/monitoring \
  -H "Authorization: Token YOUR_TOKEN_HERE" \
  -H "Content-Type: application/json" \
  -d @monitoring-config.json
```

---

## Firmware Management

### GET /api/check_update

Return the cached snapshot of the latest release known to the firmware. The
snapshot is refreshed automatically every 24 h by a background task and on
demand via `POST /api/check_update`. No network request is triggered by GET,
so it is safe to poll.

The release data is sourced from static update manifests in this repository:
`https://raw.githubusercontent.com/Xerolux/HB-RF-ETH-ng/main/latest.json?t=<seconds>`
for the stable channel and `.../beta.json?t=<seconds>` for the beta channel.
GitHub Releases still host the firmware binary, but the device no longer parses
the GitHub Releases API. The query value prevents stale raw CDN entries shortly
after the release workflow updates the manifests.

The release workflow updates the manifests automatically. Stable releases write
both `latest.json` and `beta.json`; pre-releases write only `beta.json`. The
manifest `sha256` must match the firmware binary entry in the release
`SHA256SUMS.txt`.

**Authentication:** Required

**Response (200 OK):**
```json
{
  "currentVersion": "2.1.11",
  "latestVersion": "2.1.12",
  "updateAvailable": true,
  "isPrerelease": false,
  "releaseNotes": "## What's new\n\n- Fixed ...",
  "releaseUrl": "https://github.com/Xerolux/HB-RF-ETH-ng/releases/tag/v2.1.12",
  "downloadUrl": "https://github.com/Xerolux/HB-RF-ETH-ng/releases/download/v2.1.12/firmware_2.1.12.bin",
  "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
  "publishedAt": "2025-08-14T12:34:56Z",
  "fetchedAt": 1735300000000,
  "betaChannel": false,
  "fetchInProgress": false,
  "error": null
}
```

**Fields:**
- `currentVersion`: Firmware version currently running on the device.
- `latestVersion`: Latest version matching the selected channel (`"n/a"` before the first successful fetch).
- `updateAvailable`: `true` when `latestVersion > currentVersion` per semver.
- `isPrerelease`: Mirrors the manifest's pre-release flag.
- `releaseNotes`: Optional Markdown excerpt from the manifest.
- `releaseUrl`: Release page for the "View on GitHub" link.
- `downloadUrl`: Firmware binary URL advertised by the manifest.
- `sha256`: Expected SHA-256 of the firmware binary advertised by the manifest.
- `publishedAt`: ISO 8601 timestamp from the manifest.
- `fetchedAt`: Unix epoch (milliseconds) of the last successful fetch; `0` if never fetched.
- `betaChannel`: Current value of the `betaChannel` setting.
- `fetchInProgress`: `true` while a manifest fetch is in flight.
- `error`: Human-readable description of the last fetch failure, `null` if the snapshot is valid.

**Example:**
```bash
curl http://192.168.1.100/api/check_update \
  -H "Authorization: Token YOUR_TOKEN_HERE"
```

---

### POST /api/check_update

Trigger an immediate refresh from the static update manifest. This is the
backend behind the WebUI "Jetzt nach Updates suchen" button. It **ignores the
24 h automatic window** (the user explicitly asked for a check) but enforces a
60 s cooldown and refuses to stack onto a fetch that is already running, so
the GitHub manifest and the ESP32 TLS heap stay protected.

The actual manifest fetch runs in a detached task (a GitHub TLS handshake can
take several seconds); this handler therefore returns **202 Accepted
immediately** and the client polls `GET /api/check_update` (`fetchInProgress`)
until it clears, then reads the updated snapshot.

**Authentication:** Required

**Request:** Empty body accepted (`{}` is also fine).

**Response (202 Accepted):**
```json
{
  "triggered": true,
  "fetchInProgress": true
}
```

**Fields:**
- `triggered`: `true` when a new fetch was armed. `false` if a fetch is
  already running, or the 60 s manual cooldown has not elapsed — in either
  case the cached snapshot remains available via `GET /api/check_update`.
- `fetchInProgress`: `true` while a manifest fetch (manual or automatic) is in
  flight. Clients should poll `GET /api/check_update` until this becomes
  `false`.

**Example:**
```bash
curl -i -X POST http://192.168.1.100/api/check_update \
  -H "Authorization: Token YOUR_TOKEN_HERE"
# HTTP/1.1 202 Accepted
# {"triggered":true,"fetchInProgress":true}
```

**Client flow:** POST → poll GET every ~1 s until `fetchInProgress == false`
(≤ 20 s) → read the refreshed snapshot. The automatic 24 h check timer is not
affected by manual triggers.

---

### GET /api/changelog

Proxy to the raw `CHANGELOG.md` on GitHub
(`https://raw.githubusercontent.com/Xerolux/HB-RF-ETH-ng/main/CHANGELOG.md`).
Used by the WebUI to render the full release history in a modal. The fetch
runs in a detached task; only one upstream fetch is allowed at a time.

**Authentication:** Required

**Response (200 OK):** `text/markdown`, full file contents.

**Response (503 Service Unavailable):** Another external fetch is already in progress. Retry shortly.

---

### POST /api/ota_url

Download and install a firmware image from an HTTPS URL. The download runs
in a detached task while progress is reported via `GET /api/ota_status`.
Typically called with the `downloadUrl` returned by `GET /api/check_update`.

**Authentication:** Required

**Request:**
```json
{ "url": "https://github.com/.../firmware_2.1.12.bin" }
```

**Response (200 OK):**
```json
{ "success": true, "message": "OTA update started" }
```

**Response (200 OK, request rejected):**
```json
{ "success": false, "error": "OTA update already in progress" }
```

---

### GET /api/ota_status

Report progress of an OTA download started via `POST /api/ota_url` or a manual
file upload.

**Authentication:** Required

**Response (200 OK):**
```json
{
  "status": "downloading",
  "progress": 42,
  "error": null
}
```

`status` is one of `"idle"`, `"downloading"`, `"success"`, `"failed"`.

---

### POST /ota_update

Upload and install a firmware update from a local file. Use this when the
device cannot reach GitHub (e.g. air-gapped network) or for custom builds.

**Authentication:** Required

**Request:**
- Content-Type: `application/octet-stream`
- Body: raw firmware binary

**Response (200 OK):**
```json
{
  "status": "success"
}
```

**Response (400 Bad Request):**
```json
{
  "error": "Invalid firmware file"
}
```

**Note:** After a successful upload, the device restarts automatically after a short delay.

**Example:**
```bash
curl -X POST http://192.168.1.100/ota_update \
  -H "Authorization: Token YOUR_TOKEN_HERE" \
  -F "file=@firmware_2_1_0.bin"
```

### GET /api/firmware_archive

Retrieve the list of previously released firmware versions. Proxied from the project's `archive.json` manifest on GitHub.

**Authentication:** Required

**Response (200 OK):**
```json
{
  "schema": 1,
  "updatedAt": "2026-07-05T19:06:15Z",
  "releases": [
    {
      "version": "2.2.3-Beta.12",
      "channel": "beta",
      "isPrerelease": true,
      "publishedAt": "2026-07-05T19:06:15Z",
      "downloadUrl": "https://github.com/.../firmware_2.2.3-Beta.12.bin",
      "releaseUrl": "https://github.com/.../releases/tag/v2.2.3-Beta.12",
      "notesUrl": "https://github.com/.../releases/tag/v2.2.3-Beta.12",
      "tagName": "v2.2.3-Beta.12",
      "name": "HB-RF-ETH-ng v2.2.3-Beta.12",
      "assetName": "firmware_2.2.3-Beta.12.bin",
      "assetSize": 1234567,
      "notesExcerpt": "Short single-line preview of the release notes…"
    }
  ]
}
```

**Fields:**
- `version`: Semantic version string.
- `channel`: `"stable"` or `"beta"`.
- `downloadUrl`: Direct asset URL on GitHub (used to trigger an archive install via `POST /api/ota_url`).
- `notesExcerpt`: A short (~300 char) collapsed-whitespace preview of the release notes. The full release notes are not embedded in the manifest to keep it small enough to stream through the device; follow `notesUrl` to read them on GitHub.
- `notesUrl` / `releaseUrl`: GitHub URL for the release (full notes, assets, etc.).
- `assetSize`: Firmware binary size in bytes (absent on legacy manifest entries).

**Note:** The response is generated by the GitHub Actions `rebuild-archive.yml` / `release.yml` workflows. Older manifest entries may carry a `notes` field with the full markdown; the API and WebUI accept both shapes. The device streams the manifest via an async HTTPS proxy, so a request can return `503` if the HTTPS subsystem is busy (e.g. another fetch or OTA is in progress) — retry shortly afterwards.

---

## System Log Management

### GET /api/log

Retrieve the system log buffer as plain text.

**Authentication:** Required

**Query Parameters:**
- `offset` (optional): Byte offset to retrieve logs from (for pagination)

**Response Headers:**
- `X-Log-Total`: Total bytes written to the log buffer

**Response (200 OK):**
```
text/plain - Raw log content

boot: Formatted one FS partition
I (23) esp_image: segment 0: paddr=0x001000 vaddr=0x40080000 size=0x02f4ac ( 194732) map
...
```

**Example:**
```bash
curl -X GET http://192.168.1.100/api/log \
  -H "Authorization: Token YOUR_TOKEN_HERE"

# With offset parameter
curl -X GET "http://192.168.1.100/api/log?offset=1024" \
  -H "Authorization: Token YOUR_TOKEN_HERE"
```

---

## System Control

### POST /api/restart

Restart the device.

**Authentication:** Required

**Request:** Empty POST request

**Response (200 OK):**
```json
{
  "status": "restarting"
}
```

**Note:** Device will restart immediately. The HTTP connection will be closed.

**Example:**
```bash
curl -X POST http://192.168.1.100/api/restart \
  -H "Authorization: Token YOUR_TOKEN_HERE"
```

---

## Error Responses

All endpoints may return the following error responses:

### 401 Unauthorized
```json
{
  "error": "Unauthorized"
}
```
Returned when authentication is required but not provided or invalid.

### 400 Bad Request
```json
{
  "error": "Invalid request"
}
```
Returned when request data is malformed or validation fails.

### 500 Internal Server Error
```json
{
  "error": "Internal server error"
}
```
Returned when an internal error occurs.

---

## Security Considerations

1. **Default Password**: The default admin password is `admin`. **Change this immediately after first login.**

2. **Token Storage**: Tokens are stored in `localStorage` and expire on device reboot or after inactivity-based logout.

3. **HTTPS**: The device does not use HTTPS by default. For production deployments, consider using a reverse proxy with SSL/TLS.

4. **Monitoring Access**: Restrict CheckMK access to trusted hosts and protect MQTT broker credentials appropriately.

5. **Rate Limiting**: Login attempts are rate-limited to prevent brute-force attacks.

6. **Network Access**: Restrict network access to the device's management interface using firewall rules.

---

## Complete Example Workflow

```bash
# 1. Login and get token
TOKEN=$(curl -s -X POST http://192.168.1.100/login.json \
  -H "Content-Type: application/json" \
  -d '{"password":"admin"}' | jq -r '.token')

# 2. Get system info
curl -X GET http://192.168.1.100/sysinfo.json \
  -H "Authorization: Token $TOKEN" | jq

# 3. Get current settings
curl -X GET http://192.168.1.100/settings.json \
  -H "Authorization: Token $TOKEN" | jq

# 4. Update LED brightness
curl -X POST http://192.168.1.100/settings.json \
  -H "Authorization: Token $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "hostname": "HB-RF-ETH",
    "useDHCP": true,
    "timesource": 0,
    "ntpServer": "pool.ntp.org",
    "ledBrightness": 50
  }'

# 5. Enable MQTT monitoring
curl -X POST http://192.168.1.100/api/monitoring \
  -H "Authorization: Token $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "mqtt": {
      "enabled": true,
      "server": "mqtt.local",
      "port": 1883,
      "topicPrefix": "hb-rf-eth"
    }
  }'
```

---

## WebSocket / Real-time Updates

### GET /api/log/stream (WebSocket upgrade)

Live log stream pushed from the device to one or more browser tabs. Each
log line that enters the on-device ring buffer is sent immediately as a
WebSocket TEXT frame. Up to 4 concurrent subscribers are supported.

The browser cannot attach `Authorization` headers to a WebSocket upgrade,
so the auth token is passed in the query string and validated before the
protocol upgrade is accepted:

```
ws://<device>/api/log/stream?token=<token>&offset=<absolute-byte-offset>
```

After a successful upgrade, the server sends an atomic catch-up snapshot as
four application-level frames:

1. `stream snapshot <end-offset>\n`
2. `stream backlog <byte-count>\n`
3. the raw backlog (omitted when byte-count is zero)
4. `stream connected <end-offset>\n`

Clients mark the stream ready only after the final frame. Subsequent log frames
use `stream data <end-offset>\n<raw-log-bytes>`. The absolute byte range lets a
client discard stale queued frames and detect a dropped range; reconnecting
with its last offset retrieves that range from the ring buffer without
duplicating already displayed lines.

The server unregisters subscribers when the underlying HTTP session closes;
queue overflow closes affected streams so clients recover from the ring
snapshot instead of silently losing data. The firmware supports up to four
acknowledged subscribers at once.

Older firmware versions do not have this endpoint — clients should fall
back to polling `GET /api/log` if the upgrade fails.

### GET /metrics (Prometheus exporter, separate port)

Plain-text Prometheus exposition served on a dedicated HTTP listener
configured via `monitoring.prometheus.port` (default 9100). Auth is via
source-IP allowlist (`monitoring.prometheus.allowedHosts`) since Prometheus
cannot send bearer tokens.

Exposed metrics (non-exhaustive):
- `hbrfeth_info{version,project}` (gauge, =1)
- `hbrfeth_uptime_seconds` (counter)
- `hbrfeth_heap_free_bytes{type="internal|default"}` (gauge)
- `hbrfeth_heap_largest_free_block` (gauge)
- `hbrfeth_cpu_usage_percent`, `hbrfeth_memory_usage_percent` (gauge)
- `hbrfeth_eth_link_up`, `hbrfeth_mqtt_connected` (gauge)
- `hbrfeth_rf_module{type="..."}` (gauge)
- `hbrfeth_udp_rx_frames_total`, `hbrfeth_udp_tx_frames_total`,
  `hbrfeth_udp_keepalive_total`, `hbrfeth_udp_drop_total` (counter)
- `hbrfeth_notify_sent_total`, `hbrfeth_notify_failed_total`,
  `hbrfeth_notify_suppressed_total` (counter)

## Rate Limiting

- Login endpoint: Maximum 5 attempts per minute per IP address
- Other endpoints: No rate limiting (authenticated endpoints only)

## API Versioning

The current API version is **v2.0**. Breaking changes will be introduced in new major versions.

## Support

For issues, feature requests, or questions:
- GitHub: https://github.com/Xerolux/HB-RF-ETH-ng
- Original Project: https://github.com/alexreinert/HB-RF-ETH
