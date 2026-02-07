#!/usr/bin/env python3
"""
Generate release notes for HB-RF-ETH-ng firmware releases.
Produces a concise release body: short intro, changelog, and sponsor block.

Usage:
    python generate_release_notes.py <version> <output_file>

Example:
    python generate_release_notes.py 2.1.0 release/README.md
"""

import sys
import re
from pathlib import Path


def extract_changelog(version: str) -> str:
    """Extract changelog section for specific version from CHANGELOG.md"""
    changelog_file = Path("CHANGELOG.md")

    if not changelog_file.exists():
        return ""

    content = changelog_file.read_text()

    # Try to find the version section (handles various formats:
    # "## Version 2.0.0 (...)", "## [2.0.0]", "## v2.0.0", "## 2.0.0")
    pattern = rf"## (?:Version )?\[?v?{re.escape(version)}\]?"
    lines = content.split('\n')

    changelog_lines = []
    found = False

    for line in lines:
        if re.match(pattern, line, re.IGNORECASE):
            found = True
            continue
        elif found and line.startswith('## '):
            break
        elif found:
            changelog_lines.append(line)

    if changelog_lines:
        return '\n'.join(changelog_lines).strip()

    return ""


def extract_readme_changes(version: str) -> str:
    """Extract version-specific changes from README.md as fallback"""
    readme_file = Path("README.md")

    if not readme_file.exists():
        return ""

    content = readme_file.read_text()

    # Look for "Version X.Y.Z Änderungen:" section
    pattern = rf"\*\*Version {re.escape(version)} Änderungen:\*\*"
    lines = content.split('\n')

    change_lines = []
    found = False

    for line in lines:
        if re.match(pattern, line):
            found = True
            continue
        elif found and (line.startswith('**Version ') or line.startswith('### ')):
            break
        elif found:
            change_lines.append(line)

    if change_lines:
        return '\n'.join(change_lines).strip()

    return ""


def generate_release_notes(version: str) -> str:
    """Generate concise release notes: intro, changelog, sponsor block."""

    is_prerelease = any(tag in version.lower() for tag in ['alpha', 'beta', 'rc'])

    # Try changelog first, fall back to README changes
    changelog = extract_changelog(version)
    if not changelog:
        changelog = extract_readme_changes(version)

    notes = []

    # Short intro
    notes.append(f"# HB-RF-ETH-ng Firmware v{version}")
    notes.append("")

    if is_prerelease:
        notes.append("> **Pre-Release** - Testversion, Nutzung auf eigene Gefahr.")
        notes.append("")

    notes.append("Firmware-Update für die HB-RF-ETH Platine.")
    notes.append("")

    # What changed
    if changelog:
        notes.append(f"## Was ist neu in v{version}?")
        notes.append("")
        notes.append(changelog)
        notes.append("")

    # Brief install hint
    notes.append("## Installation")
    notes.append("")
    notes.append("Die `firmware_*.bin` Datei herunterladen und per WebUI hochladen oder per URL installieren.")
    notes.append("SHA256-Prüfsummen befinden sich in `SHA256SUMS.txt`.")
    notes.append("")

    # Sponsor block
    notes.append("## Unterstützung")
    notes.append("")
    notes.append("Dir gefällt dieses Projekt und du möchtest es unterstützen?")
    notes.append("")
    notes.append("[![Buy Me A Coffee](https://img.shields.io/badge/buy%20me%20a%20coffee-donate-yellow.svg?style=for-the-badge)](https://www.buymeacoffee.com/xerolux)")
    notes.append("[![Tesla Referral](https://img.shields.io/badge/Tesla-Referral-red?style=for-the-badge&logo=tesla)](https://ts.la/sebastian564489)")
    notes.append("")

    return '\n'.join(notes)


def main():
    if len(sys.argv) != 3:
        print("Usage: python generate_release_notes.py <version> <output_file>")
        print("Example: python generate_release_notes.py 2.1.0 release/README.md")
        sys.exit(1)

    version = sys.argv[1].lstrip('v')
    output_file = sys.argv[2]

    print(f"Generating release notes for version {version}")

    try:
        notes = generate_release_notes(version)
        Path(output_file).write_text(notes)
        print(f"Release notes written to {output_file}")
    except Exception as e:
        print(f"Error generating release notes: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
