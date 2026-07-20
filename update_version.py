#!/usr/bin/env python3
"""Update firmware version numbers across the project.

The WebUI has an independent semantic version and is intentionally not touched
by this script. Use ``update_webui_version.py`` for WebUI-only releases.

Usage:
    python update_version.py <firmware-version>
"""

import json
import re
import sys
from pathlib import Path

VERSION_PATTERN = re.compile(r"^\d+\.\d+\.\d+(?:-[A-Za-z]+\.\d+)?$")


def update_version_txt(version: str) -> None:
    Path("version.txt").write_text(f"{version}\n", encoding="utf-8")
    print(f"Updated firmware version.txt to {version}")


def update_readme(version: str) -> None:
    path = Path("README.md")
    if not path.exists():
        return
    text = path.read_text(encoding="utf-8")
    text = re.sub(
        r"\*\*Version [\d.]+(?:-[A-Za-z]+\.\d+)? Änderungen:\*\*",
        f"**Version {version} Änderungen:**",
        text,
    )
    path.write_text(text, encoding="utf-8")


def update_locales(version: str) -> None:
    directory = Path("webui/src/locales")
    if not directory.exists():
        return
    for path in sorted(directory.glob("*.js")):
        text = path.read_text(encoding="utf-8")
        text = re.sub(
            r"(version: '.*? )([\d.]+(?:-[A-Za-z]+\.\d+)?)(\"|,')",
            rf"\g<1>{version}\g<3>",
            text,
        )
        text = re.sub(
            r"(versionInfo:.*v)(\d+\.\d+\.\d+(?:-[A-Za-z]+\.\d+)?)",
            rf"\g<1>{version}",
            text,
        )
        path.write_text(text, encoding="utf-8")


def update_openapi(version: str) -> None:
    path = Path("docs/openapi.yaml")
    if not path.exists():
        return
    text = path.read_text(encoding="utf-8")
    text = re.sub(
        r"version:\s*['\"]?[\d.]+(?:-[A-Za-z]+\.\d+)?['\"]?",
        f"version: '{version}'",
        text,
    )
    path.write_text(text, encoding="utf-8")


def update_troubleshooting(version: str) -> None:
    path = Path("docs/TROUBLESHOOTING.md")
    if not path.exists():
        return
    text = path.read_text(encoding="utf-8")
    text = re.sub(
        r"HB-RF-ETH-ng firmware v[\d.]+(?:-[A-Za-z]+\.\d+)?",
        f"HB-RF-ETH-ng firmware v{version}",
        text,
    )
    path.write_text(text, encoding="utf-8")


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python update_version.py <firmware-version>")

    version = sys.argv[1].lstrip("v")
    if not VERSION_PATTERN.fullmatch(version):
        raise SystemExit(f"Invalid firmware version: {version}")

    update_version_txt(version)
    update_readme(version)
    update_locales(version)
    update_openapi(version)
    update_troubleshooting(version)

    # Guard against accidentally coupling both release trains again.
    package = json.loads(Path("webui/package.json").read_text(encoding="utf-8"))
    print(f"Firmware updated to {version}; WebUI remains {package['version']}")


if __name__ == "__main__":
    main()
