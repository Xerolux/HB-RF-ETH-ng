## [2.4.1] - 2026-11-20

> ⭐ [Star the project on GitHub](https://github.com/Xerolux/HB-RF-ETH-ng/stargazers)

### ⚠️ Upgrade notes

- **The stable channel now offers firmware 2.4.1 and WebUI 1.2.0.** Back up the gateway configuration before updating. The firmware installation restarts the gateway and briefly interrupts the CCU connection.

### ✨ Improvements

- **Home Assistant can discover the gateway over mDNS.** Native MQTT discovery adds diagnostics and token-gated controls. The separate [Home Assistant integration](https://github.com/Xerolux/hb-rf-eth-hass) can supply file-based services without duplicating MQTT entities.
- **Checkmk reports real device readings.** The agent exposes CPU, heap, Ethernet, CCU connection, radio module and last restart as local checks.
- **ESP-IDF v6.1.1.** The firmware and the ESP32 errata workarounds build against Espressif’s patch release.

### 🐛 Bugfixes

- **MQTT allocates command storage as needed.** The gateway keeps more heap available for HTTPS update checks.
- **Monitoring updates reuse the staged configuration.** Checkmk was enabled on hardware with MQTT active, and all six checks responded before and after an MQTT-triggered update search.
- **Legacy WebUI URLs redirect to the current interface.** Log exports begin at a complete line when old data has rolled out of the buffer.

### ☕ Support the project

[GitHub Sponsors](https://github.com/sponsors/xerolux) · [Buy Me a Coffee](https://buymeacoffee.com/xerolux)

---

[Full changelog: v2.3.1...v2.4.1](https://github.com/Xerolux/HB-RF-ETH-ng/compare/v2.3.1...v2.4.1)
