## [2.4.3-Beta.1] - 2026-11-23

> ⭐ [Star the project on GitHub](https://github.com/Xerolux/HB-RF-ETH-ng/stargazers)

### ⚠️ Upgrade notes

- **Beta test build for the web server boot fix.** Back up the gateway configuration before updating. The firmware installation restarts the gateway and briefly interrupts the CCU connection.

### 🐛 Bugfixes

- **The web server no longer takes requests while it is still registering its routes.** ESP-IDF publishes each route before it is fully initialised. A client that reconnects while the gateway boots - a poller, or an open WebUI tab that keeps retrying - could hit that window and crash the gateway with a `LoadProhibited` panic in the web server (#491). New connections now wait until the route table is complete; none is refused, and requests that arrive during boot are answered instead of turning into "404 Not Found".
- **The two-stage update marker is written without truncation.** The marker for "firmware now, WebUI after reboot" is measured before it is written, so a buffer that is too small is rejected instead of silently shortened. No change for existing installations; it also fixes a host test that no longer compiled with GCC 14.

### 🌐 Web Flasher

- The preparation checklist explains what to do when a macOS computer does not show the board's serial port (#508).

### ☕ Support the project

[GitHub Sponsors](https://github.com/sponsors/xerolux) · [Buy Me a Coffee](https://buymeacoffee.com/xerolux)

---

[Full changelog: v2.4.2...v2.4.3-Beta.1](https://github.com/Xerolux/HB-RF-ETH-ng/compare/v2.4.2...v2.4.3-Beta.1)
