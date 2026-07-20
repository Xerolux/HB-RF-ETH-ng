# Firmware and WebUI release versioning

HB-RF-ETH-ng uses two independent semantic-version release lines.

| Component | Version source | Tag example | Release asset |
|---|---|---|---|
| Firmware | `version.txt` | `v2.2.5-Beta.2` | `firmware_2.2.5-Beta.2.bin` |
| New Design WebUI | `webui/package.json` | `webui-v1.0.0-Beta.1` | `webui_1.0.0-Beta.1.bin` |

A firmware version change must not modify the WebUI version. A WebUI version
change must not modify `version.txt` or advertise a new firmware update.

## Full release

Use `.github/workflows/release.yml` when firmware changed or when publishing the
first migration-capable release.

The release contains:

- firmware image
- current compatible WebUI image
- bootloader and partition table
- SHA-256 checksums
- separate firmware and WebUI version files
- WebUI compatibility metadata

The repository's `latest.json` or `beta.json` firmware fields and `webui` block
are updated together. A stable full release may become GitHub's Latest release.

The WebUI image keeps its own version. Reusing an unchanged WebUI in a newer
firmware release does not create a fake WebUI version bump.

## WebUI-only release

Use `.github/workflows/release-webui.yml` for changes limited to Vue, CSS,
translations, themes, layout, or other browser-side behavior that uses the
existing firmware API.

The workflow performs a complete ESP-IDF build as a compatibility and size
check, but publishes only:

- `webui_<version>.bin`
- SHA-256 checksum
- WebUI version
- compatibility metadata

The tag is `webui-v<version>`. The release is never marked as GitHub's Latest
firmware release. Existing firmware fields in `latest.json` or `beta.json` are
preserved at the logical JSON-field level; only the `webui` block is replaced.

## Channels

- Stable full release: updates `latest.json` and `beta.json`.
- Beta full release: updates `beta.json` only.
- Stable WebUI-only release: replaces only the `webui` block in both manifests.
- Beta WebUI-only release: replaces only the `webui` block in `beta.json`.

## Compatibility contract

`webui/compatibility.json` defines:

- `apiVersion`: integer firmware/WebUI API contract
- `minFirmwareVersion`: minimum semantic firmware version

The same values are embedded into `webui-manifest.json` inside `spiffs.bin` and
published in the update manifest. The normal online updater checks both values
before downloading or erasing anything. The ESP32 then verifies image size,
SHA-256, product, design, image format and required assets before activating the
external WebUI.

The manual upload remains an expert/recovery path. Use only the image and
compatibility metadata from the matching GitHub release; normal users should use
the online updater.

Increment `apiVersion` only for an incompatible API change. Compatible API
additions keep the existing value and may raise `minFirmwareVersion` when the
new WebUI depends on those additions.

## Initial independent versions

- Firmware line: `2.2.x`
- New Design WebUI line: `1.0.0-Beta.1`
- WebUI API: `1`
