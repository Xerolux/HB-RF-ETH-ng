# HB-RF-ETH REST API Documentation

## Overview

The HB-RF-ETH firmware provides a RESTful HTTP API for configuration and monitoring. All authenticated endpoints require a bearer token obtained through the login endpoint.

**Base URL:** `http://<device-ip>/`

**Authentication:** Token-based authentication using `Authorization: Token <token>` header

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
    "currentVersion": "2.1.0",
    "latestVersion": "2.1.0",
    "boardRevision": "REV 1.10 (PUB)",
    "uptimeSeconds": 345678,
    "resetReason": "Power-On Reset",
    "rawUartRemoteAddress": "192.168.1.50:2001",
    "memoryUsage": 45.32,
    "cpuUsage": 12.56,
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
    "timesource": 0,
    "dcfOffset": 40000,
    "gpsBaudrate": 9600,
    "ntpServer": "string",
    "ledBrightness": 100
  }
}
```

**Fields:**
- `hostname`: Device hostname (max 63 characters)
- `useDHCP`: Enable/disable DHCP
- `localIP`: Static IP address (used when DHCP is disabled)
- `netmask`: Network mask (used when DHCP is disabled)
- `gateway`: Default gateway (used when DHCP is disabled)
- `dns1`: Primary DNS server
- `dns2`: Secondary DNS server
- `timesource`: Time source (0 = NTP, 1 = DCF77, 2 = GPS)
- `dcfOffset`: DCF77 signal offset in microseconds (-60000 to 60000)
- `gpsBaudrate`: GPS module baud rate (4800, 9600, 19200, 38400, 57600, 115200)
- `ntpServer`: NTP server hostname or IP
- `ledBrightness`: LED brightness (0-100)

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
- Hostname: Must be valid DNS hostname (alphanumeric, hyphen, dot), max 63 chars
- IP addresses: Must be valid IPv4 addresses
- LED brightness: 0-100
- GPS baudrate: Must be one of: 4800, 9600, 19200, 38400, 57600, 115200
- DCF offset: -60000 to 60000
- NTP server: Valid hostname or IP, max 64 chars

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

Retrieve monitoring configuration for SNMP and CheckMK.

**Authentication:** Required

**Response (200 OK):**
```json
{
  "snmp": {
    "enabled": false,
    "community": "public",
    "location": "",
    "contact": "",
    "port": 161
  },
  "checkmk": {
    "enabled": false,
    "port": 6556,
    "allowedHosts": ""
  }
}
```

**Fields:**

**SNMP:**
- `enabled`: Enable/disable SNMP agent
- `community`: SNMP community string
- `location`: SNMP system location
- `contact`: SNMP system contact
- `port`: SNMP agent port (default: 161)

**CheckMK:**
- `enabled`: Enable/disable CheckMK agent
- `port`: CheckMK agent port (default: 6556)
- `allowedHosts`: Comma-separated list of allowed IP addresses/ranges

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
  "snmp": {
    "enabled": true,
    "community": "private",
    "location": "Server Room",
    "contact": "admin@example.com",
    "port": 161
  },
  "checkmk": {
    "enabled": true,
    "port": 6556,
    "allowedHosts": "192.168.1.0/24,10.0.0.5"
  }
}
```

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

### POST /ota_update

Upload and install a firmware update.

**Authentication:** Required

**Request:**
- Content-Type: `multipart/form-data`
- Form field: `file` (firmware binary)

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

**Note:** After successful upload, the device must be manually restarted for the firmware to take effect.

**Example:**
```bash
curl -X POST http://192.168.1.100/ota_update \
  -H "Authorization: Token YOUR_TOKEN_HERE" \
  -F "file=@firmware_2_1_0.bin"
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

2. **Token Storage**: Tokens are stored in sessionStorage and expire when the browser tab is closed.

3. **HTTPS**: The device does not use HTTPS by default. For production deployments, consider using a reverse proxy with SSL/TLS.

4. **SNMP Community**: The default SNMP community string is `public`. Change this to a secure value if SNMP is enabled.

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

# 5. Enable SNMP monitoring
curl -X POST http://192.168.1.100/api/monitoring \
  -H "Authorization: Token $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "snmp": {
      "enabled": true,
      "community": "private",
      "location": "Server Room",
      "contact": "admin@example.com"
    }
  }'
```

---

## WebSocket/Real-time Updates

Currently, the API does not support WebSocket connections or real-time updates. Clients must poll endpoints to retrieve updated information.

## Rate Limiting

- Login endpoint: Maximum 5 attempts per minute per IP address
- Other endpoints: No rate limiting (authenticated endpoints only)

## API Versioning

The current API version is **v2.0**. Breaking changes will be introduced in new major versions.

## Support

For issues, feature requests, or questions:
- GitHub: https://github.com/Xerolux/HB-RF-ETH-ng
- Original Project: https://github.com/alexreinert/HB-RF-ETH
