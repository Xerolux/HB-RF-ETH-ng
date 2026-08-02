# Separate New Design updates

HB-RF-ETH-ng uses the existing 320 KiB `spiffs` partition for independently
updatable New Design assets. The partition table remains unchanged, so devices
can migrate through the manual local firmware upload without USB access.

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
- The WebUI upload uses one bounded 2 KiB heap buffer instead of reserving that
  space on the HTTP server task stack.
- Wrapped route slots are reset when a new HTTP server instance is registered.
- A manifest exactly at the configured size limit remains valid; only additional
  trailing data is rejected as oversized.
- The image must exactly match the SPIFFS partition size.
- `webui-manifest.json` must identify `HB-RF-ETH-ng`, format `1`, design
  `newdesign`, the expected gzip encodings, its API version and its minimum
  firmware version.
- The image API must exactly match the API implemented by the running firmware,
  and the running firmware must meet the image's minimum version.
- Compatibility is checked at boot and after every upload. An incompatible
  external image is never served; the embedded New Design becomes active and
  shows a persistent repair warning.
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
partition usage, external and effective WebUI versions, active source, image and
supported API versions, minimum/running firmware versions,
`compatibilityStatus`, and the last storage error. Authentication is required.

### `POST /api/webui/update`

Uploads a raw full-size SPIFFS image as `application/octet-stream`.
Authentication is required. `multipart/form-data` is intentionally rejected.

Optional request header:

```text
X-WebUI-SHA256: <64 hexadecimal characters>
X-WebUI-API-Version: <positive integer>
X-WebUI-Min-Firmware-Version: <semantic firmware version>
```

Clients may provide all three headers when release metadata is available. The
ESP32 checks the compatibility headers before erasing flash, then checks the
internal manifest again after the upload. Manual uploads may omit them, but all
structural and compatibility validation still runs from the internal image
manifest before activation.

## Release artifacts

Each release publishes:

- `firmware_<version>.bin`
- `webui_<version>.bin`
- `SHA256SUMS.txt`

The device does not consume `latest.json`, `beta.json`, or another online-update
manifest. Users select the release-provided WebUI image locally.
