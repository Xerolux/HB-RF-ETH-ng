#!/usr/bin/env python3
"""Build and validate the slim update manifest consumed by the device.

The HB-RF-ETH-ng board is an ESP32-WROOM-32 without PSRAM. It must never parse
a large or open-ended document, so the manifest it fetches is deliberately flat,
short-keyed and hard-capped at MAX_BYTES. Generation therefore fails the release
build rather than publishing something the firmware would have to reject.

The slim manifest is derived from the full manifests (`latest.json`, `beta.json`)
that the release workflows already produce, so the two can never drift apart.

Usage:
    device_manifest.py build <full-manifest.json> <output.json>
    device_manifest.py check <manifest.json> [<manifest.json> ...]
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

# Hard limit. The firmware aborts the read past this many bytes without parsing
# anything, so a manifest above it is unusable in the field. See
# docs/AUTO_UPDATE_PLAN.md, rule 1.
MAX_BYTES = 1024

SCHEMA = 1

# Only release assets from GitHub are installable. Anything else would let a
# manifest point a device at a third-party host.
ALLOWED_HOST_SUFFIXES = ("github.com", "githubusercontent.com")

# The migration gate: the oldest firmware allowed to jump straight to this
# release. It lives next to version.txt because it is hand-maintained release
# policy, not a build artifact - release/ is overwritten by the release workflow.
# The file is absent by default, and absent means "no gate": set it in the very
# pull request that breaks the NVS or partition layout, where it is reviewable.
# See docs/AUTO_UPDATE_PLAN.md 11.4.
MIN_UPGRADE_FROM_FILE = Path("min_upgrade_from.txt")

SEMVER = re.compile(r"^(\d+)\.(\d+)\.(\d+)(?:-([A-Za-z]+)\.(\d+))?$")
SHA256_HEX = re.compile(r"^[0-9a-f]{64}$")

REQUIRED_STRINGS = ("fw", "fwUrl", "fwSha", "ui", "uiUrl", "uiSha", "uiMinFw", "notes")
REQUIRED_INTS = ("s", "fwSize", "uiSize", "uiApi")
URL_KEYS = ("fwUrl", "uiUrl", "notes")
SHA_KEYS = ("fwSha", "uiSha")
VERSION_KEYS = ("fw", "ui", "uiMinFw")
OPTIONAL_KEYS = ("fwMinFrom",)

ALLOWED_KEYS = frozenset(REQUIRED_STRINGS + REQUIRED_INTS + OPTIONAL_KEYS)


class ManifestError(SystemExit):
    def __init__(self, message: str) -> None:
        super().__init__(f"device manifest: {message}")


def require_semver(value: str, field: str) -> None:
    if not SEMVER.fullmatch(value):
        raise ManifestError(f"{field} is not a semantic version: {value!r}")


def require_https_github(url: str, field: str) -> None:
    if not url.startswith("https://"):
        raise ManifestError(f"{field} must use https: {url!r}")
    authority = url[len("https://"):].split("/", 1)[0]
    # userinfo would let "https://github.com@evil.example" read as trusted at a
    # glance, so the host is whatever follows the last "@".
    host = authority.rsplit("@", 1)[-1].lower()
    # We never emit an explicit port; one appearing here means the URL was not
    # produced by this generator.
    if ":" in host:
        raise ManifestError(f"{field} must not carry an explicit port: {authority}")
    if not any(host == suffix or host.endswith("." + suffix) for suffix in ALLOWED_HOST_SUFFIXES):
        raise ManifestError(f"{field} points at a host outside the allowlist: {host}")


def read_min_upgrade_from(root: Path) -> str | None:
    """Read the optional migration gate; absent or empty means no gate."""
    path = root / MIN_UPGRADE_FROM_FILE
    if not path.exists():
        return None
    value = path.read_text(encoding="utf-8").strip()
    if not value:
        return None
    require_semver(value, "fwMinFrom")
    return value


def build(source: Path, output: Path, root: Path) -> None:
    full = json.loads(source.read_text(encoding="utf-8"))
    webui = full.get("webui")
    if not isinstance(webui, dict):
        raise ManifestError(f"{source} has no webui section")

    manifest = {
        "s": SCHEMA,
        "fw": full["version"],
        "fwUrl": full["downloadUrl"],
        "fwSha": full["sha256"],
        "fwSize": require_size(full, source),
        # Placeholder so an optional gate lands with the other fw* fields
        # instead of at the end; dropped below when no gate is configured.
        "fwMinFrom": None,
        "ui": webui["version"],
        "uiUrl": webui["downloadUrl"],
        "uiSha": webui["sha256"],
        "uiSize": int(webui["size"]),
        "uiApi": int(webui["apiVersion"]),
        "uiMinFw": webui["minFirmwareVersion"],
        "notes": full.get("notesUrl") or full["releaseUrl"],
    }

    gate = read_min_upgrade_from(root)
    if gate is None:
        del manifest["fwMinFrom"]
    else:
        manifest["fwMinFrom"] = gate

    validate(manifest, str(output))

    output.parent.mkdir(parents=True, exist_ok=True)
    # Compact: whitespace counts against MAX_BYTES and the file is never
    # hand-edited. The readable full manifests stay in the repository root.
    output.write_text(serialise(manifest), encoding="utf-8")


def require_size(full: dict, source: Path) -> int:
    """The device compares Content-Length against this before writing flash."""
    size = full.get("size")
    if not isinstance(size, int) or isinstance(size, bool) or size <= 0:
        raise ManifestError(
            f"{source} carries no usable firmware size; the release workflow "
            f"must write a positive 'size' into the full manifest"
        )
    return size


def serialise(manifest: dict) -> str:
    return json.dumps(manifest, separators=(",", ":"), sort_keys=False) + "\n"


def validate(manifest: dict, name: str) -> None:
    if not isinstance(manifest, dict):
        raise ManifestError(f"{name} is not a JSON object")

    unknown = sorted(set(manifest) - ALLOWED_KEYS)
    if unknown:
        raise ManifestError(f"{name} has unknown keys: {', '.join(unknown)}")

    for key in REQUIRED_STRINGS:
        value = manifest.get(key)
        if not isinstance(value, str) or not value:
            raise ManifestError(f"{name} is missing string field {key}")
    for key in REQUIRED_INTS:
        value = manifest.get(key)
        if not isinstance(value, int) or isinstance(value, bool) or value <= 0:
            raise ManifestError(f"{name} is missing positive integer field {key}")

    if manifest["s"] != SCHEMA:
        raise ManifestError(f"{name} has schema {manifest['s']}, expected {SCHEMA}")

    for key in VERSION_KEYS:
        require_semver(manifest[key], key)
    if "fwMinFrom" in manifest:
        require_semver(manifest["fwMinFrom"], "fwMinFrom")
    for key in URL_KEYS:
        require_https_github(manifest[key], key)
    for key in SHA_KEYS:
        if not SHA256_HEX.fullmatch(manifest[key]):
            raise ManifestError(f"{name}: {key} is not 64 lowercase hex characters")

    size = len(serialise(manifest).encode("utf-8"))
    if size > MAX_BYTES:
        raise ManifestError(
            f"{name} is {size} bytes, over the {MAX_BYTES}-byte device limit"
        )


def check(paths: list[Path]) -> None:
    for path in paths:
        raw = path.read_bytes()
        if len(raw) > MAX_BYTES:
            raise ManifestError(
                f"{path} is {len(raw)} bytes, over the {MAX_BYTES}-byte device limit"
            )
        try:
            manifest = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as error:
            raise ManifestError(f"{path} is not valid UTF-8 JSON: {error}") from error
        validate(manifest, str(path))
        print(f"{path}: ok ({len(raw)} bytes, {MAX_BYTES - len(raw)} to spare)")


def main(argv: list[str]) -> int:
    if len(argv) >= 4 and argv[1] == "build":
        source = Path(argv[2])
        output = Path(argv[3])
        root = Path(argv[4]) if len(argv) > 4 else Path.cwd()
        build(source, output, root)
        print(f"{output}: written ({output.stat().st_size} bytes)")
        return 0
    if len(argv) >= 3 and argv[1] == "check":
        check([Path(item) for item in argv[2:]])
        return 0
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
