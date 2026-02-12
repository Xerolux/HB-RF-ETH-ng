#!/usr/bin/env python3
"""
Update version numbers across the project for releases.
This script is called by the release workflow to ensure consistent versioning.

Usage:
    python update_version.py <version>

Example:
    python update_version.py 2.2.0
"""

import sys
import re
import json
from pathlib import Path

def update_version_txt(version: str):
    """Update version.txt file"""
    version_file = Path("version.txt")
    version_file.write_text(f"{version}\n")
    print(f"✓ Updated version.txt to {version}")

def update_readme(version: str):
    """Update README.md with new version"""
    readme_file = Path("README.md")
    content = readme_file.read_text()

    # Update the main title
    content = re.sub(
        r"# HB-RF-ETH-ng Firmware v[\d.]+",
        f"# HB-RF-ETH-ng Firmware v{version}",
        content
    )

    # Update version changes section if exists
    content = re.sub(
        r"\*\*Version [\d.]+ Änderungen:\*\*",
        f"**Version {version} Änderungen:**",
        content
    )

    readme_file.write_text(content)
    print(f"✓ Updated README.md to version {version}")

def update_locales(version: str):
    """Update version in locale files"""
    # Use full version for display (e.g. "Version 2.1.0")

    locales = ["cs.js", "de.js", "en.js", "es.js", "fr.js", "it.js", "nl.js", "no.js", "pl.js", "sv.js"]

    for locale in locales:
        file_path = Path(f"webui/src/locales/{locale}")
        if file_path.exists():
            content = file_path.read_text()
            # Update version: 'Word X.X' (preserves localized "Version" word)
            content = re.sub(
                r"(version: '.*? )([\d.]+)(',)",
                f"\\g<1>{version}\\g<3>",
                content
            )
            # Update versionInfo: '... v2.1.5 ...'
            content = re.sub(
                r"(versionInfo:.*v)(\d+\.\d+\.\d+)",
                f"\\g<1>{version}",
                content
            )
            file_path.write_text(content)
            print(f"✓ Updated {locale} to version {version}")

def update_about_vue(version: str):
    """Update about.vue with new version"""
    about_file = Path("webui/src/about.vue")
    content = about_file.read_text()

    # Extract major.minor version (e.g., "2.1" from "2.1.0")
    major_minor = '.'.join(version.split('.')[:2])

    # Update GitHub link (Fork vX.X)
    content = re.sub(
        r'<a href="https://github.com/Xerolux/HB-RF-ETH-ng" target="_new">GitHub Repository \(Fork v[\d.]+\)</a>',
        f'<a href="https://github.com/Xerolux/HB-RF-ETH-ng" target="_new">GitHub Repository (Fork v{major_minor})</a>',
        content
    )

    about_file.write_text(content)
    print(f"✓ Updated about.vue link to fork version {major_minor}")

def update_package_json(version: str):
    """Update webui/package.json with new version"""
    package_file = Path("webui/package.json")

    with open(package_file, 'r') as f:
        package_data = json.load(f)

    package_data['version'] = version

    with open(package_file, 'w') as f:
        json.dump(package_data, f, indent=2)
        f.write('\n')  # Add trailing newline

    print(f"✓ Updated package.json to version {version}")

def update_package_lock_json(version: str):
    """Update webui/package-lock.json with new version"""
    lock_file = Path("webui/package-lock.json")
    if not lock_file.exists():
        print("⚠ webui/package-lock.json not found, skipping")
        return

    with open(lock_file, 'r') as f:
        lock_data = json.load(f)

    lock_data['version'] = version
    if "" in lock_data.get('packages', {}):
         lock_data['packages'][""]['version'] = version

    with open(lock_file, 'w') as f:
        json.dump(lock_data, f, indent=2)
        f.write('\n')

    print(f"✓ Updated package-lock.json to version {version}")

def update_openapi_yaml(version: str):
    """Update docs/openapi.yaml with new version"""
    openapi_file = Path("docs/openapi.yaml")

    if not openapi_file.exists():
        print("⚠ docs/openapi.yaml not found, skipping")
        return

    content = openapi_file.read_text()

    # Update version in openapi spec
    content = re.sub(
        r"version: '[\d.]+'",
        f"version: '{version}'",
        content
    )

    openapi_file.write_text(content)
    print(f"✓ Updated openapi.yaml to version {version}")

def update_troubleshooting(version: str):
    """Update TROUBLESHOOTING.md with new version"""
    troubleshooting_file = Path("docs/TROUBLESHOOTING.md")

    if not troubleshooting_file.exists():
        print("⚠ docs/TROUBLESHOOTING.md not found, skipping")
        return

    content = troubleshooting_file.read_text()

    # Update version in header
    content = re.sub(
        r"HB-RF-ETH-ng firmware v[\d.]+",
        f"HB-RF-ETH-ng firmware v{version}",
        content
    )

    troubleshooting_file.write_text(content)
    print(f"✓ Updated TROUBLESHOOTING.md to version {version}")

def main():
    if len(sys.argv) != 2:
        print("Usage: python update_version.py <version>")
        print("Example: python update_version.py 2.2.0")
        sys.exit(1)

    version = sys.argv[1].lstrip('v')  # Remove leading 'v' if present

    # Validate version format (semantic versioning)
    if not re.match(r'^\d+\.\d+\.\d+$', version):
        print(f"Error: Invalid version format '{version}'. Expected format: X.Y.Z")
        sys.exit(1)

    print(f"\n🔄 Updating project to version {version}\n")

    try:
        update_version_txt(version)
        update_readme(version)
        update_locales(version)
        update_about_vue(version)
        update_package_json(version)
        update_package_lock_json(version)
        update_openapi_yaml(version)
        update_troubleshooting(version)

        print(f"\n✅ Successfully updated all files to version {version}")

    except Exception as e:
        print(f"\n❌ Error updating version: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
