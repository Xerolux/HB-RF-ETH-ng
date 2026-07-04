# HB-RF-ETH-ng Troubleshooting Guide

This guide helps you diagnose and resolve common issues with the HB-RF-ETH-ng firmware v2.2.3-Beta.6

## Table of Contents

- [General Troubleshooting](#general-troubleshooting)
- [Network Issues](#network-issues)
- [Radio Module Problems](#radio-module-problems)
- [WebUI Access](#webui-access)
- [Firmware Update Issues](#firmware-update-issues)
- [Time Synchronization](#time-synchronization)
- [LED Indicators](#led-indicators)
- [Performance Issues](#performance-issues)
- [Factory Reset](#factory-reset)
- [Diagnostic Information](#diagnostic-information)

---

## General Troubleshooting

### First Steps

When experiencing issues:

1. **Check LED indicators** - See [LED Indicators](#led-indicators) section
2. **Verify power supply** - Ensure stable 5V power via USB or PoE
3. **Check network connectivity** - Can you ping the device?
4. **Review serial console output** - Connect via USB and monitor at 115200 baud
5. **Check firmware version** - Ensure you're running the latest version

### Serial Console Access

Connect to the device via USB and use a serial terminal:

```bash
# Linux/Mac
screen /dev/ttyUSB0 115200

# Windows (using PuTTY)
# Set COM port and 115200 baud rate
```

Look for error messages, warnings, or crash logs.

---

## Network Issues

### Cannot Access Device via Network

**Symptoms:**
- Cannot ping device
- WebUI not accessible
- mDNS name not resolving

**Solutions:**

1. **Check DHCP vs Static IP**
   - Default: DHCP enabled
   - Check your router's DHCP leases for "HB-RF-ETH-ng-XXXXXX"
   - Access via mDNS: `http://hb-rf-eth-ng-XXXXXX.local`

2. **Verify Network Cable**
   - Check Ethernet cable connection
   - Try a different cable
   - Check link LED on Ethernet port

3. **Check Router/Switch**
   - Ensure switch port is enabled
   - Try different switch port
   - Check VLAN configuration if applicable

4. **IP Address Conflict**
   - If using static IP, ensure no conflict exists
   - Try resetting to DHCP mode (factory reset)

5. **Firewall Rules**
   - Check if firewall is blocking access
   - Temporarily disable firewall for testing

**Diagnostic Commands:**

```bash
# From your computer
ping 192.168.1.100       # Replace with device IP
nslookup hb-rf-eth-XXXXXX.local  # Test mDNS
arp -a | grep <MAC>      # Find device by MAC address
```

### Slow Network Performance

**Symptoms:**
- High latency
- WebUI slow to load
- Settings take long to save

**Solutions:**

1. **Check Network Load**
   - Monitor network utilization
   - Check for broadcast storms
   - Verify switch performance

2. **Check Device Resources**
   - Access `/sysinfo.json` to check memory and CPU usage
   - High CPU usage may indicate radio module issues

3. **Reduce LED Brightness**
   - High LED brightness can impact performance slightly
   - Reduce to 50-80% in settings

### mDNS Not Working

**Symptoms:**
- `hb-rf-eth-ng-XXXXXX.local` does not resolve

**Solutions:**

1. **Check mDNS Support**
   - Ensure your OS supports mDNS (Bonjour/Avahi)
   - Windows: Install Bonjour Print Services
   - Linux: Install `avahi-daemon`
   - Mac: Built-in support

2. **Network Configuration**
   - mDNS only works on local subnet
   - Some routers block multicast traffic
   - Check router multicast settings

3. **Use IP Address**
   - Access device directly via IP as fallback
   - Check DHCP leases on router

---

## Radio Module Problems

### Radio Module Not Detected

**Symptoms:**
- WebUI shows "Radio Module Type: -"
- Serial number shows "n/a"
- CCU cannot connect

**Solutions:**

1. **Check Module Connection**
   - Power off device
   - Reseat radio module firmly
   - Check for bent pins

2. **Verify Module Type**
   - Supported: RPI-RF-MOD, HM-MOD-RPI-PCB
   - Ensure module is compatible

3. **Check Serial Console**
   - Look for "Radio module detected" message
   - Check for error messages during boot

4. **Power Cycle**
   - Completely power off device
   - Wait 10 seconds
   - Power back on

### CCU Connection Fails

**Symptoms:**
- CCU cannot connect to HB-RF-ETH-ng
- Raw UART connection refused
- "Connection timeout" errors

**Solutions:**

1. **Check Raw UART Settings**
   - Default port: 2001
   - Verify CCU is configured correctly
   - Check `/sysinfo.json` for `rawUartRemoteAddress`

2. **Firewall Rules**
   - Ensure UDP port 2001 is not blocked
   - Check both device and CCU firewalls

3. **Network Path**
   - Ensure CCU and HB-RF-ETH-ng are on same network
   - Test connectivity: `ping` between devices

4. **Check Module Status**
   - Verify radio module is detected
   - Check LED indicators on radio module

### Reconnect After Restart

With current firmware versions, reconnect handling after restarts has been improved. If the CCU connection is still missing after a reboot:

**Checks:**
- Wait a short moment for Ethernet, mDNS and Raw UART services to finish starting
- Verify the configured CCU IP in the WebUI
- Check `/sysinfo.json` for `rawUartRemoteAddress`

**Workarounds if needed:**
- Restart the CCU/debmatic/piVCCU3 service
- Verify that no firewall or network ACL blocks the UDP path
- Check switch and link negotiation after boot

---

## WebUI Access

### Cannot Login to WebUI

**Symptoms:**
- "Authentication failed" message
- Correct password rejected

**Solutions:**

1. **Verify Password**
   - Default password: `admin`
   - Password is case-sensitive
   - Try clearing browser cache/cookies

2. **Session Token Issues**
   - Clear browser sessionStorage
   - Open browser DevTools → Application → Session Storage
   - Delete `hb-rf-eth-ng-pw` entry
   - Reload page and login again

3. **Browser Issues**
   - Try different browser (Chrome, Firefox, Edge)
   - Disable browser extensions
   - Try incognito/private mode

4. **Password Change Failed**
   - If password was changed but forgotten, perform factory reset

### WebUI Not Loading

**Symptoms:**
- Blank page
- "Connection refused"
- JavaScript errors in console

**Solutions:**

1. **Check HTTP Server**
   - Access `http://<device-ip>/` directly
   - Check serial console for HTTP server errors
   - Device may be rebooting

2. **Clear Browser Cache**
   ```
   Chrome: Ctrl+Shift+Delete
   Firefox: Ctrl+Shift+Delete
   ```

3. **Check Network**
   - Verify device is reachable
   - Try accessing `/login.json` directly
   - Check browser console for errors (F12)

4. **Firmware Corruption**
   - If WebUI never loads, firmware may be corrupted
   - Re-flash firmware via OTA or serial

### Settings Not Saving

**Symptoms:**
- Settings revert after save
- "Save failed" error
- Changes lost after reboot

**Solutions:**

1. **Check Validation Errors**
   - Browser console (F12) may show validation errors
   - Ensure values are within valid ranges:
     - LED brightness: 0-100
     - GPS baudrate: 4800, 9600, 19200, 38400, 57600, 115200
     - DCF offset: -60000 to 60000
     - Hostname: max 63 chars, alphanumeric + hyphen
     - IP addresses: valid IPv4 format

2. **NVS Storage Full**
   - Factory reset to clear NVS
   - Check serial console for NVS errors

3. **Authentication Expired**
   - Re-login if session expired
   - Check for 401 errors in browser console

---

## Firmware Update Issues

### Beta Update Not Shown

**Important:** If the device is already running the newest beta, the WebUI
correctly reports no update available. Check `currentVersion` and
`latestVersion` in `/api/check_update`: when both values are identical, there is
no newer beta to install.

**Checks:**

1. Enable the beta channel in Firmware / Network Update.
2. Press "Check now" to force a manifest refresh.
3. Verify the public beta manifest:
   `https://raw.githubusercontent.com/Xerolux/HB-RF-ETH-ng/main/beta.json?t=<timestamp>`.
4. In the firmware archive, use the Beta filter. On beta devices the WebUI
   should select this filter automatically.

**Developer note:** The firmware update check must use the raw `main/*.json`
manifest URL with a cache-busting query value. Avoid switching back to
`refs/heads/main` without testing immediately after a release, because GitHub
raw CDN edges can temporarily serve stale manifest content.

### Firmware Upload Fails

**Symptoms:**
- Upload stalls at certain percentage
- "Upload failed" error
- Device becomes unresponsive

**Solutions:**

1. **File Size/Format**
   - Verify firmware file is correct
   - File should be `.bin` format
   - Check file size matches expected (≈900KB)

2. **Network Stability**
   - Use wired connection to upload
   - Avoid WiFi for firmware updates
   - Ensure stable power during upload

3. **Browser Timeout**
   - Try different browser
   - Increase browser timeout settings
   - Use curl for upload:
     ```bash
     curl -X POST http://192.168.1.100/ota_update \
       -H "Authorization: Token YOUR_TOKEN" \
       -F "file=@firmware_2_1_0.bin"
     ```

4. **Insufficient Space**
   - OTA partition may be full
   - Factory reset before update

### Device Won't Boot After Update

**Symptoms:**
- LEDs blink rapidly
- Device stuck in boot loop
- WebUI never appears

**Solutions:**

1. **Wait for Rollback**
   - ESP32 may auto-rollback to previous version
   - Wait 5 minutes for automatic recovery

2. **Serial Console**
   - Connect via USB
   - Check boot errors
   - Look for "Boot failed" messages

3. **Manual Flash**
   - Flash firmware via USB/serial
   - Use esptool.py or ESP-IDF (`idf.py`)
   ```bash
   esptool.py --chip esp32 --port /dev/ttyUSB0 \
     write_flash 0x10000 firmware_2_1_0.bin
   ```

4. **Factory Reset**
   - Perform factory reset (see below)
   - Re-flash firmware

### Emergency Firmware Update (Rescue Script)

If the WebUI is inaccessible but the device is still reachable on the network (pingable), you can use the `test_ota_function.py` script included in the repository to trigger a firmware update via the API.

**Prerequisites:**
- Python 3 installed
- Device is reachable on the network
- Admin password is known

**Usage:**

The script is located in the root directory of the repository.

```bash
# Syntax
python3 test_ota_function.py <DEVICE_IP> <PASSWORD> [--url <FIRMWARE_URL>]

# Example (using default URL for latest firmware)
python3 test_ota_function.py 192.168.1.100 myPassword

# Example (using custom URL)
python3 test_ota_function.py 192.168.1.100 myPassword --url http://192.168.1.50/firmware.bin
```

The script will authenticate, trigger the OTA update, and monitor progress until the device restarts.

---

## MQTT Integration Issues

### MQTT Connection Fails

**Symptoms:**
- Device shows "MQTT disconnected" in WebUI
- No messages published to broker
- "Connection refused" or timeout errors

**Solutions:**

1. **Verify Broker Connectivity**
   ```bash
   # From device network
   ping <mqtt-server-ip>
   nc -zv <mqtt-server> 1883    # Check if port is open
   ```

2. **Check Configuration**
   - WebUI → Monitoring → MQTT
   - Verify server address/IP is correct
   - Verify port (default: 1883 for TCP, 8883 for TLS)
   - Check username/password if required

3. **Network/Firewall**
   - Ensure port 1883 (or 8883 for TLS) is allowed outbound
   - Check firewall rules
   - Try without TLS first to isolate issue

4. **TLS Certificate Issues**
   - If using custom CA: verify PEM format is correct
   - Try `tlsSkipVerify=true` temporarily for testing (not recommended for production)
   - Check certificate expiry

### Home Assistant Discovery Not Working

**Symptoms:**
- Devices don't appear in Home Assistant
- No MQTT entities created

**Solutions:**

1. **Enable Discovery**
   - WebUI → Monitoring → MQTT
   - Check "Home Assistant Discovery" checkbox
   - Save settings and restart device

2. **Check Discovery Topic**
   - Default prefix: `homeassistant/`
   - MQTT client should receive messages on `homeassistant/+/hb-rf-eth-*/#`
   - Example: `mosquitto_sub -h <broker> -t "homeassistant/#" -v`

3. **Verify MQTT Broker is Healthy**
   - Check broker logs for errors
   - Restart broker if needed
   - Ensure broker allows discovery topics

4. **Home Assistant MQTT Integration**
   - Verify HA's MQTT integration is configured and connected
   - Check HA's MQTT logs: Dev Tools → Logs

### Command Topics Not Working

**Symptoms:**
- Can't restart device via MQTT
- Commands are ignored

**Solutions:**

1. **Verify Commands Enabled**
   - WebUI → Monitoring → MQTT
   - Check "Enable Commands" checkbox
   - Device will only subscribe to `<prefix>/command/#` when enabled

2. **Check Command Token (if configured)**
   - If command token is set, payload must match exactly
   - Allowed characters: `A-Z a-z 0-9 - _ .`
   - Length: 8-63 characters
   - Publish: `mosquitto_pub -h <broker> -t "hb-rf-eth/command/restart" -m "<token>"`

3. **Topic Name Check**
   - Default prefix: `hb-rf-eth`
   - Check custom prefix if changed
   - Commands: `<prefix>/command/restart`, `<prefix>/command/factory_reset`, etc.

4. **Broker ACL**
   - Ensure device user has publish rights on `<prefix>/command/#`
   - Check broker ACL configuration
   - Consider restricting other users from command topics for security

### Sporadic MQTT Disconnections

**Symptoms:**
- MQTT frequently disconnects and reconnects
- Message gaps in Home Assistant

**Solutions:**

1. **Check Keep-Alive Settings**
   - Default: 120 seconds (ESP-IDF default)
   - If network drops packets, may need shorter timeout
   - Try adjusting on broker side if possible

2. **Network Stability**
   - Check for network issues: `ping -c 100 <broker>`
   - Look for packet loss
   - Check switch/router logs for errors

3. **Broker Load**
   - Monitor broker CPU/memory usage
   - Check for broker-side errors
   - Consider restarting broker during low-traffic period

4. **Reconnect Interval**
   - Current: 30 seconds between reconnect attempts
   - View in device logs: System Log → System Events

### No MQTT Messages Published

**Symptoms:**
- MQTT connected but no status messages
- Subscription shows no messages

**Solutions:**

1. **Verify MQTT is Enabled**
   - WebUI → Monitoring → MQTT
   - Toggle off/on to restart MQTT client

2. **Check Topic Prefix**
   - Default: `hb-rf-eth`
   - Subscribe to: `hb-rf-eth/status/#` (not retained)
   - Check: `mosquitto_sub -h <broker> -t "hb-rf-eth/status/#" -v`

3. **Publish Interval**
   - Messages published every 5 seconds (idle)
   - 1 second during OTA updates
   - Check broker is actually receiving: enable broker logging

4. **System Log**
   - Check device system log for MQTT errors
   - Share log via WebUI (System Log → Share button)
   - Review for connection errors or failed publishes

---

## Time Synchronization

### Time Not Syncing

**Symptoms:**
- System time incorrect
- NTP sync fails
- DCF77/GPS not working

**Solutions:**

**NTP Issues:**

1. **Check NTP Server**
   - Default: `pool.ntp.org`
   - Try alternative: `time.google.com`, `time.cloudflare.com`
   - Verify DNS resolution works

2. **Firewall**
   - Ensure UDP port 123 outbound is allowed
   - Check router/firewall settings

3. **Network Connectivity**
   - Verify internet connectivity
   - Test: `ping 8.8.8.8` from device network

**DCF77 Issues:**

1. **Signal Reception**
   - DCF77 only works in Central Europe
   - Check antenna placement
   - Avoid interference from electronics

2. **Offset Calibration**
   - Adjust `dcfOffset` in settings
   - Default: 40000 (40ms)
   - Try range: 30000-50000

**GPS Issues:**

1. **GPS Module Connection**
   - Check module wiring to J5 connector
   - Pin 1: VCC, Pin 2: TX, Pin 3: GND

2. **Baudrate Configuration**
   - Ensure GPS baudrate matches module
   - Common: 9600 (default)
   - Check GPS module datasheet

3. **GPS Fix**
   - GPS needs clear view of sky
   - Indoor GPS may not work
   - Wait 1-2 minutes for initial fix

---

## LED Indicators

### Green Power LED

| Pattern | Meaning |
|---------|---------|
| Solid ON | Device running normally |
| Alternating with red | System booting |
| OFF | No power or hardware failure |

### Red Status LED

| Pattern | Meaning | Action |
|---------|---------|--------|
| Solid OFF | Normal operation | None |
| Slow blink (1Hz) | Firmware update available | Update firmware |
| Fast blink, green OFF | Factory reset mode | See factory reset procedure |
| Fast blink, green ON | Firmware update in progress | Wait for completion |
| Alternating with green | System booting | Wait 30-60 seconds |

### Radio Module LEDs

See radio module documentation:
- **RPI-RF-MOD**: Green/red/blue LEDs indicate radio activity
- **HM-MOD-RPI-PCB**: Single green LED for activity

---

## Performance Issues

### High CPU Usage

**Symptoms:**
- CPU usage >50% (check `/sysinfo.json`)
- Slow response times
- WebUI sluggish

**Solutions:**

1. **Check Radio Activity**
   - High radio traffic increases CPU load
   - Monitor radio module LEDs
   - Check CCU for excessive polling

2. **Reduce Monitoring**
   - Disable unused monitoring services if not needed
   - Reduce CheckMK polling frequency
   - Lower LED brightness

3. **Network Issues**
   - Check for broadcast storms
   - Verify network switch health

### High Memory Usage

**Symptoms:**
- Memory usage >50% (check `/sysinfo.json`)
- Device crashes
- OOM (Out of Memory) errors in console

**Solutions:**

1. **Memory Leak**
   - Reboot device periodically
   - Update to latest firmware
   - Report issue on GitHub

2. **Fragmentation**
   - Factory reset to clear NVS
   - Reduces long-term fragmentation

---

## Factory Reset

### When to Perform Factory Reset

- Forgotten admin password
- Network settings preventing access
- Persistent errors/crashes
- Before firmware downgrade

### Factory Reset Procedure

1. **Disconnect power** from device
2. **Press and hold** the reset button
3. **Connect power** while holding button
4. **Wait ~4 seconds** - red LED blinks fast, green LED turns OFF
5. **Release button briefly**, then **press and hold again**
6. **Wait ~4 seconds** - both LEDs light solid for 1 second
7. **Release button** - device reboots with factory settings

### Post-Reset Configuration

After factory reset:
- **Hostname**: `HB-RF-ETH-ng-XXXXXX` (XXXXXX = last 3 MAC bytes)
- **Network**: DHCP enabled
- **Admin Password**: `admin` ⚠️ **Change immediately**
- **Time Source**: NTP (pool.ntp.org)
- **LED Brightness**: 100%
- **Monitoring**: Disabled

---

## Diagnostic Information

### Gathering Diagnostic Data

When reporting issues, include:

1. **Firmware Version**
   ```bash
   curl http://192.168.1.100/sysinfo.json | jq '.sysInfo.currentVersion'
   ```

2. **System Info**
   ```bash
   curl http://192.168.1.100/sysinfo.json \
     -H "Authorization: Token YOUR_TOKEN" | jq
   ```

3. **Serial Console Log**
   - Capture full boot log
   - Include errors and warnings

4. **Network Configuration**
   ```bash
   curl http://192.168.1.100/settings.json \
     -H "Authorization: Token YOUR_TOKEN" | jq '.settings'
   ```

5. **Browser Console Errors**
   - Open DevTools (F12)
   - Screenshot Console tab

### Automated Log Sharing (v2.2.0-Beta.8+)

**Easiest way to gather diagnostics:**

1. Open the WebUI and log in
2. Go to **System Log** page
3. Click the **Share** button (top right)
4. A comprehensive debug report is uploaded to MicroBin paste service
5. Share link is automatically copied to clipboard

**What's included in the report:**
- Complete system log
- System info (version, board revision, serial)
- Network configuration
- Ethernet status
- Radio module information
- MQTT/CheckMK configuration
- LED programs
- All diagnostic data needed for support

**Privacy:**
- Passwords, tokens, TLS keys are automatically redacted
- IP addresses and hostnames retained (needed for debugging)
- You can review the link before sharing

**Alternative (Manual API Call):**
```bash
curl -X POST http://192.168.1.100/api/log/share \
  -H "Authorization: Token YOUR_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{}'
```

### Common Error Messages

| Error Message | Meaning | Solution |
|---------------|---------|----------|
| `NVS: No free pages` | NVS storage full | Factory reset |
| `Radio module not detected` | Module connection issue | Reseat module |
| `ETH_START_BIT not found` | Ethernet PHY issue | Check hardware |
| `httpd_uri: uri /... not found` | WebUI routing error | Re-flash firmware |
| `OTA: Firmware too large` | OTA partition full | Check firmware size |
| `Failed to upload log` | Network/paste service issue | Check internet connectivity |

---

## Getting Help

If problems persist:

1. **Check GitHub Issues**
   - https://github.com/Xerolux/HB-RF-ETH-ng/issues
   - Search for similar problems

2. **Create New Issue**
   - Include diagnostic data (see above)
   - Describe steps to reproduce
   - Include firmware version

3. **Community Forums**
   - HomeMatic Forum
   - debmatic/piVCCU3 community

4. **Original Project**
   - https://github.com/alexreinert/HB-RF-ETH (Original)
   - https://github.com/Xerolux/HB-RF-ETH-ng (This fork)

---

## Preventive Maintenance

### Best Practices

1. **Regular Updates**
   - Check for firmware updates monthly
   - Subscribe to GitHub releases

2. **Backups**
   - Document your settings
   - Take screenshots of configuration

3. **Power Protection**
   - Use UPS for critical deployments
   - Avoid power cycling during updates

4. **Monitoring**
   - Enable CheckMK or MQTT only where needed
   - Monitor uptime and resource usage

5. **Change Default Password**
   - ⚠️ **Critical**: Change from `admin`
   - Use strong password (12+ chars)

6. **Network Security**
   - Isolate management interface
   - Use firewall rules
   - Disable unused monitoring services

7. **MQTT Command Security (Phase A)**

   The MQTT user/password configured in the WebUI authenticates the **device**
   against the broker - it does NOT prevent other clients from publishing to
   `<prefix>/command/#`. Anyone with publish rights on that topic can
   restart the device, trigger OTA, or even factory-reset it. Three
   complementary levers are available:

   - **Broker ACL (most important).** Configure your broker so that only the
     HB-RF-ETH-ng user may publish to `<prefix>/command/#`. Example
     Mosquitto ACL:
     ```text
     user hb-rf-eth
     topic hb-rf-eth/# rw
     topic homeassistant/# rw
     ```
   - **Command token (device-side).** In *Monitoring → MQTT → Command
     Topics* set a shared-secret token. Every command payload must then
     match the token exactly. Allowed charset: `A-Z a-z 0-9 - _ .`,
     length 8–63. Without the token the device rejects the command and
     publishes an event to `<prefix>/event/command_rejected`.
   - **Disable commands entirely.** Uncheck *Enable* under *Command Topics*
     to stop the device from subscribing to the command tree. Restart /
     Factory Reset / OTA are then only possible via WebUI.

   TLS / mTLS can be enabled independently in *Monitoring → MQTT → TLS / SSL*
   to encrypt the broker connection and to authenticate the device with a
   client certificate. TLS is optional and is NOT required for the
   command token to work.

   When a command token is configured, the HA discovery JSON publishes the
   token verbatim as `payload_press` / `payload_install` so the HA buttons
   continue to work. Lock down write access to `homeassistant/#` on the
   broker so other clients cannot read the token via the discovery topic.
