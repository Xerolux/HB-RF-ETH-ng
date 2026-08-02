# Beta.3 recovery hotfix

This hotfix addresses issues found during the first hardware test of firmware
`2.2.5-Beta.3` and WebUI `1.0.0-Beta.1`:

- Recovery login did not execute because the normal Content Security Policy
  blocked the page's self-contained JavaScript;
- the standalone WebUI used Brotli on a private HTTP origin and could render a
  white page when the browser did not decode its JavaScript bundle;
- the firmware upload page accepted a WebUI image before the backend rejected it;
- an empty primary IPv4 DNS setting affected outbound hostname resolution.

## WebUI and recovery

The standalone WebUI now uses gzip exclusively. Brotli assets and Brotli
negotiation are removed. WebUI version `1.0.0-Beta.2` is tracked independently
from the firmware version.

The Recovery page receives a route-specific security policy that permits only
its own embedded script. Normal New Design pages keep the stricter global
policy.

## Network default

Existing custom DNS values remain unchanged. Missing or legacy `0.0.0.0`
primary DNS values are initialized to `1.1.1.1`.

## Manual installation only

Firmware and WebUI release discovery is external to the device. The WebUI links
to GitHub Releases, but the ESP32 neither searches for releases nor downloads an
offered BIN file itself.

- Firmware is installed manually under **System → Firmware**.
- WebUI is installed manually under **System → WebUI**.
- MQTT does not provide update-search or update-install commands.
- The former automatic search, URL-OTA route, install-capable Home Assistant
  update entity, and firmware archive are not exposed or embedded.

The firmware upload page rejects a 320-KiB WebUI image and verifies the ESP32
image magic byte. The WebUI page rejects firmware filenames and requires the
exact WWW partition size.

## Fixed defaults

The New Design is the permanent user interface. Restart sync is permanently
active for every device and always holds the Ethernet PHY down long enough for
the CCU watchdog to observe the restart. Legacy backup/API fields remain
accepted for compatibility but cannot disable either default.

The Experimental page therefore contains only an informational message that no
experimental functions are currently available.

## Version reporting

The footer, system overview and MQTT status report the installed firmware and
WebUI versions separately. No latest-version or update-available state is
published.

The final verification covers the WebUI build, translation audit, gzip-only
320-KiB WWW image, ESP-IDF firmware build, size limits and security analysis.
