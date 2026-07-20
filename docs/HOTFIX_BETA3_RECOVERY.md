# Beta.3 recovery hotfix

This hotfix addresses issues found during the first hardware test of firmware
`2.2.5-Beta.3` and WebUI `1.0.0-Beta.1`:

- the standalone Recovery page did not execute its inline JavaScript because the
  normal Content Security Policy blocked it;
- the server sent Brotli assets even when the browser did not advertise Brotli
  support;
- an external WebUI image could be selected on the firmware upload page;
- an empty primary IPv4 DNS setting prevented hostname-based NTP and OTA access.

The standalone WebUI now uses gzip exclusively. Brotli assets and Brotli
negotiation are removed, so the same encoding is used on every private-LAN
browser. The updated WebUI version is `1.0.0-Beta.2`.

Existing custom DNS values remain unchanged. Only missing or legacy `0.0.0.0`
primary DNS values are initialized to `1.1.1.1`.


## Update-check policy

Firmware and WebUI metadata come from one combined manifest cache owned by the
ESP32. The browser only reads `/api/check_update`; it never contacts GitHub for
update metadata. A persistent NVS timestamp limits online update-manifest
requests to exactly one attempt per 24 hours, including across device reboots.
Page visits, local refresh buttons and MQTT status publication only read the
cached snapshot.


## Update-check policy

Firmware and WebUI metadata come from one combined manifest cache owned by the
ESP32. The browser only reads `/api/check_update`; it never contacts GitHub for
update metadata. A persistent NVS timestamp limits online update-manifest
requests to exactly one attempt per 24 hours, including across device reboots.
Page visits, local refresh buttons and MQTT status publication only read the
cached snapshot.


The daily update check is persisted in NVS and cannot be retriggered by page
visits or device reboots. A stable per-device stagger spreads fleets across a
2-60 minute window. Before creating the short-lived worker or opening TLS, the
firmware checks total free heap and the largest free block; an unsafe check is
skipped instead of risking a crash.
