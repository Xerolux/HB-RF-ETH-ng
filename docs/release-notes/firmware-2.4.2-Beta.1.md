## [2.4.2-Beta.1] - 2026-11-21

> ⭐ [Star the project on GitHub](https://github.com/Xerolux/HB-RF-ETH-ng/stargazers)

### ⚠️ Upgrade notes

- **Beta test build for the update-search fixes.** Back up the gateway configuration before updating. The firmware installation restarts the gateway and briefly interrupts the CCU connection.

### 🐛 Bugfixes

- **The update search works on fully equipped gateways again.** A gateway running MQTT with Home Assistant discovery, the mounted external WebUI and an active CCU session sits just below both admission thresholds of the update check, so every manual and scheduled search reported "Too little free memory" although the fetch would have fit. The admission gate gains a compact branch calibrated on that measured state, so the search proceeds after the monitoring workers are released for the moment of the fetch.
- **Update checks survive transient heap pressure.** The admission gate samples the heap for about two seconds after releasing the monitoring workers before refusing, and a scheduled check retries an hour later instead of waiting the full interval - the refused attempt never reached the network.

### ☕ Support the project

[GitHub Sponsors](https://github.com/sponsors/xerolux) · [Buy Me a Coffee](https://buymeacoffee.com/xerolux)

---

[Full changelog: v2.4.1...v2.4.2-Beta.1](https://github.com/Xerolux/HB-RF-ETH-ng/compare/v2.4.1...v2.4.2-Beta.1)
