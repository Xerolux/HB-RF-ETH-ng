## [2.4.2] - 2026-11-22

### Upgrade notes

- Back up the gateway configuration before updating. Installing the firmware restarts the gateway and briefly interrupts the CCU connection. This release ships with WebUI 1.2.0.

### Firmware fixes

- Manual and scheduled update searches can run with moderately fragmented memory, including configurations with MQTT discovery, the external WebUI and an active CCU connection.
- After releasing monitoring resources, the firmware gives the heap time to recover before rejecting a search. A scheduled search that was skipped because of low memory retries after one hour.

### Web Flasher fixes

- Fixed a JavaScript syntax error that prevented release selection and USB connection. The flasher loads its serial library from the project site and shows a message when the browser cannot select a serial port.
- GitHub Pages waits for the uploaded deployment artifact and includes the latest stable firmware in the flasher catalog.

[Full changelog: v2.4.1...v2.4.2](https://github.com/Xerolux/HB-RF-ETH-ng/compare/v2.4.1...v2.4.2)
