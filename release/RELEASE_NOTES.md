## [2.4.4] - 2026-11-23

### Firmware fixes

- The online installer downloads firmware and WebUI images in verified 4 KiB HTTP ranges. This reduces peak TLS memory use on gateways where a full-size response previously failed with a connection error.
- After pausing monitoring, the installer allows up to two seconds for network buffers and worker memory to be released. The system log records free memory and the largest contiguous block before the manifest fetch.

### Hardware verification

- A gateway with board revision 1.8 and an RPI-RF-MOD ran the corrected installer and updated to the published 2.4.3 firmware through the device's online installation endpoint. The gateway restarted, reported the expected firmware version and reconnected to the CCU.
- Devices running an earlier installer can install this firmware once through the manual upload in the WebUI if the online download fails. The bundled WebUI version is 1.2.0.
