#!/usr/bin/env python3
"""Generate release notes for HB-RF-ETH-ng WebUI-only releases.

WebUI releases are versioned independently of firmware (tag ``webui-v<version>``).
They never ship a firmware change, so the firmware ``CHANGELOG.md`` has no
per-WebUI-version section. Instead, WebUI-relevant changes accumulate under the
shared ``## [Unreleased]`` section and are tagged with ``(webui)``.

This script reads ``[Unreleased]``, keeps the WebUI-relevant bullets, drops
firmware-only bullets, and wraps them in a WebUI-specific header.

Usage:
    python generate_webui_release_notes.py <webui-version> <min-firmware> <api-version> <output_file>

Example:
    python generate_webui_release_notes.py 1.0.0-Beta.4 2.2.5-Beta.1 1 release/RELEASE_NOTES.md
"""

import re
import sys
from pathlib import Path


UNRELEASED_HEADING = "## [Unreleased]"
FIRMWARE_ONLY_TAGS = ("(mqtt)", "(diag)", "(ota)", "(security)", "(system)")


def extract_unreleased() -> str:
    """Return the body of the ``[Unreleased]`` section in CHANGELOG.md, or ''."""
    changelog = Path("CHANGELOG.md")
    if not changelog.exists():
        return ""
    lines = changelog.read_text(encoding="utf-8").splitlines()

    body: list[str] = []
    in_section = False
    for line in lines:
        if line.strip() == UNRELEASED_HEADING:
            in_section = True
            continue
        if not in_section:
            continue
        # The next top-level version heading ends the section.
        if line.startswith("## "):
            break
        body.append(line)

    return "\n".join(body).strip()


def filter_webui_bullets(block: str) -> list[str]:
    """Pick the WebUI-relevant bullet lines from the [Unreleased] body.

    A bullet is WebUI-relevant if it carries the ``(webui)`` scope tag, or if it
    has no firmware-only scope tag. Generic bullets (``- fix:``, ``- chore:``,
    ``- feat:`` without a scope) are kept so WebUI users still see broadly
    relevant notes; firmware-only scopes are dropped because the WebUI image
    does not change firmware behaviour.
    """
    bullets: list[str] = []
    for line in block.splitlines():
        stripped = line.strip()
        if not stripped.startswith("- "):
            continue
        # Strip the leading "- " to inspect the scope tag.
        body = stripped[2:]
        lowered = body.lower()
        if "(webui)" in lowered:
            bullets.append(stripped)
            continue
        if any(tag in lowered for tag in FIRMWARE_ONLY_TAGS):
            continue
        # No scope tag, or an unrecognised scope — keep it so WebUI users see
        # broadly-relevant notes. Anything we wrongly include is informational
        # only; anything we wrongly drop is recoverable from CHANGELOG.md.
        bullets.append(stripped)
    return bullets


def generate(version: str, min_firmware: str, api_version: str) -> str:
    unreleased = extract_unreleased()
    bullets = filter_webui_bullets(unreleased)

    lines: list[str] = []
    lines.append(f"# HB-RF-ETH-ng WebUI v{version}")
    lines.append("")
    lines.append(
        "**WebUI-only Release.** Enthält keine Firmware-Änderung. "
        f"Kompatibel mit Firmware ab `{min_firmware}` (API-Version {api_version})."
    )
    lines.append("")

    if bullets:
        lines.append("## Was sich geändert hat")
        lines.append("")
        lines.extend(bullets)
        lines.append("")
    else:
        lines.append("## Was sich geändert hat")
        lines.append("")
        lines.append(
            "Keine WebUI-relevanten Einträge unter `[Unreleased]` in `CHANGELOG.md` gefunden. "
            "Siehe die [Commit-Historie](https://github.com/Xerolux/HB-RF-ETH-ng/commits/main) für Details."
        )
        lines.append("")

    lines.append("## Technische Details")
    lines.append("")
    lines.append("- Design: New Design")
    lines.append(f"- WebUI API: {api_version}")
    lines.append(f"- Mindestens Firmware: {min_firmware}")
    lines.append("- Image-Größe: 327680 Bytes")
    lines.append("")
    lines.append(
        "Die lokal ausgewählte WebUI-Datei wird vor der Installation geprüft. "
        "Der ESP32 verifiziert Image-Größe, optionales SHA-256, Produkt, Design und benötigte Assets, "
        "bevor die separate WWW-Partition aktiviert wird."
    )
    lines.append("")
    lines.append(
        "Vollständige Entwicklungshistorie: "
        "[`CHANGELOG.md`](https://github.com/Xerolux/HB-RF-ETH-ng/blob/main/CHANGELOG.md) "
        "(Sektion `[Unreleased]`)."
    )
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    if len(sys.argv) != 5:
        print(
            "Usage: python generate_webui_release_notes.py "
            "<webui-version> <min-firmware> <api-version> <output_file>"
        )
        sys.exit(1)

    version = sys.argv[1].removeprefix("webui-v").removeprefix("v")
    min_firmware = sys.argv[2]
    api_version = sys.argv[3]
    output_file = Path(sys.argv[4])

    notes = generate(version, min_firmware, api_version)
    output_file.parent.mkdir(parents=True, exist_ok=True)
    output_file.write_text(notes, encoding="utf-8")
    print(f"WebUI release notes written to {output_file}")


if __name__ == "__main__":
    main()
