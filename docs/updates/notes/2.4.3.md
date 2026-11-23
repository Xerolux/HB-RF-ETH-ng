## [2.4.3] - 2026-11-23

### Upgrade notes

- Back up the gateway configuration before updating. Installing the firmware restarts the gateway and briefly interrupts the CCU connection. This release ships with WebUI 1.2.0.
- Identical to 2.4.3-Beta.1 apart from the version string.

### Firmware fixes

- **The web server no longer takes requests while it is still registering its routes at boot.** ESP-IDF publishes each route before it is fully initialised. A client that reconnects while the gateway boots - a poller, or an open WebUI tab that keeps retrying - could hit that window and crash the gateway with a `LoadProhibited` panic (#491). Requests that arrive during boot are now answered instead of turning into "404 Not Found".
- **The two-stage update marker is written without truncation.** No change for existing installations; it also fixes a host test that no longer compiled with GCC 14.

### Web Flasher

- The preparation checklist explains what to do when a macOS computer does not show the board's serial port (#508).

### Verification

- Installed on an HB-RF-ETH (board revision 1.8, RPI-RF-MOD, CCU connected) through the device's own online update from the beta channel: search, download, verification, restart. The first request after the restart was answered correctly, every WebUI route and the WebSocket log stream work, and a ten-minute session with live log polling and the WebSocket stream ran without an error or a restart. No crash transcript was recorded. This was done on 2.4.3-Beta.1; the reboot race itself was not stressed with repeated restarts.

[Full changelog: v2.4.2...v2.4.3](https://github.com/Xerolux/HB-RF-ETH-ng/compare/v2.4.2...v2.4.3)
