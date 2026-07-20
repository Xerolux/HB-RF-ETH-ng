# Separate New Design updates

HB-RF-ETH-ng uses the existing 320 KiB `spiffs` partition for independently
updatable New Design assets. The partition table remains unchanged, so devices
can migrate through the normal firmware OTA process without USB access.

## Migration

1. Install the transition firmware using the existing firmware updater.
2. After reboot, the embedded New Design remains the active and fully usable UI.
3. Open **WebUI** and install the release-provided `webui_<version>.bin` image.
4. Once SHA-256 and manifest validation succeed, the device serves the New
   Design from SPIFFS. Firmware and WebUI updates are independent afterwards.

## Safety model

- Firmware and WebUI flash operations are mutually exclusive.
- The browser sends the release SHA-256; the ESP32 verifies it again while
  streaming the image directly into flash.
- The image must exactly match the SPIFFS partition size.
- `webui-manifest.json` must identify `HB-RF-ETH-ng`, format `1`, design
  `newdesign`, and the expected Brotli encodings.
- Invalid or incomplete images have their filesystem header erased.
- A persistent NVS transaction marker is committed before the first destructive
  flash operation. If power is lost, the next boot discards the unverified
  image before mounting SPIFFS.
- The embedded New Design remains the recovery UI in every failure path.
- Network settings, credentials, monitoring configuration, and all other user
  settings remain in NVS and are not part of the WebUI image.

## API

### `GET /api/webui/status`

Returns partition availability, mount and validation state, active update state,
partition usage, installed WebUI version, design, and the last storage error.
Authentication is required.

### `POST /api/webui/update`

Uploads a raw full-size SPIFFS image as `application/octet-stream`.
Authentication is required. `multipart/form-data` is intentionally rejected.

Optional request header:

```text
X-WebUI-SHA256: <64 hexadecimal characters>
```

Release-driven updates always provide this header. Manual test uploads may omit
it, but product/design/format and filesystem validation still run on the device.

## Release artifacts

Each release publishes:

- `firmware_<version>.bin`
- `webui_<version>.bin`
- `SHA256SUMS.txt`

`latest.json` and `beta.json` carry the WebUI version, design, URL, SHA-256, and
exact image size used by the online updater.
