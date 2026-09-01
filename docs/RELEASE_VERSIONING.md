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

Prefer a manual workflow dispatch with the new firmware tag. The workflow then
updates `version.txt` and the changelog, commits those changes to `main`, creates
the tag, builds the artifacts and publishes the release. A release triggered by
pushing an existing `v*` tag never rewrites source metadata: the tagged commit's
`version.txt` must already match the tag exactly, or the workflow stops before
building any assets.

The release contains:

- firmware image
- current compatible WebUI image
- bootloader and partition table
- SHA-256 checksums
- separate firmware and WebUI version files
- WebUI compatibility metadata

The release workflow still updates the repository's `latest.json` or
`beta.json` metadata for release tooling and older external clients. The device
does not fetch these files and does not use them for automatic update searches.
A stable full release may become GitHub's Latest release.

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

Compatibility has two independent, authoritative sources:

- `main/webui_api_contract.json` defines `supportedApiVersion`, the exact WebUI
  API implemented by the firmware.
- `webui/compatibility.json` defines the WebUI's required `apiVersion` and
  `minFirmwareVersion`.

The contract is satisfied only when both conditions are true:

1. `apiVersion == supportedApiVersion` (exact match).
2. Running firmware `>= minFirmwareVersion` (semantic-version comparison,
   including prerelease identifiers).

`apiVersion` and `minFirmwareVersion` are embedded into
`webui-manifest.json` inside `spiffs.bin` and may also be published as release
metadata. The local-upload page sends only the raw image. The internal image
manifest is therefore authoritative and is checked after writing. The HTTP API
continues to accept optional compatibility and SHA-256 headers for external
upload clients, but the bundled WebUI neither searches a release manifest nor
adds those headers.

At every boot and after every WebUI upload, the firmware validates image size,
SHA-256 when supplied, product, design, image format, required assets, API
version and minimum firmware. An incompatible external WebUI is never served.
The firmware uses its embedded WebUI fallback and the fallback displays a
persistent compatibility warning with a link to the WebUI repair page.

Manual local upload is the supported installation path. Use only the WebUI
image from a compatible GitHub release. Because the bundled browser sends no
preflight metadata, the ESP32 validates the image's internal contract after
writing it. An incompatible image is invalidated and the embedded fallback
stays active.

### Mandatory change rules

- Increment both the firmware's `supportedApiVersion` and the WebUI's
  `apiVersion` for every incompatible REST, JSON, authentication, routing or
  behavioral contract change. Ship such a change as a full firmware release.
- Compatible API additions keep the current API number. Raise
  `minFirmwareVersion` if the WebUI begins to depend on that addition.
- A WebUI-only release must never change `apiVersion` unless support for that
  API already exists in the released/current firmware line.
- Never remove `apiVersion` or `minFirmwareVersion` from the standalone image
  manifest or update manifests.
- Never weaken the firmware-side boot and upload checks to a browser-only
  warning. The backend is the security and recovery boundary.

The WebUI packaging script, firmware CMake configuration and release workflows
fail when the two API contracts disagree or the current firmware is older than
the declared minimum. This is a release gate, not advisory documentation.

## Initial independent versions

- Firmware line: `2.2.x`
- New Design WebUI line: `1.0.0-Beta.1`
- WebUI API: `1`
