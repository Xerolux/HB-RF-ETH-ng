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
    """Generate comprehensive release notes: intro, changelog, features, installation, support."""

    is_prerelease = any(tag in version.lower() for tag in ['alpha', 'beta', 'rc'])

    # Try changelog first, fall back to README changes
    changelog = extract_changelog(version)
    if not changelog:
        changelog = extract_readme_changes(version)

    notes = []

    # Header with version and badges
    notes.append(f"# 🚀 HB-RF-ETH-ng Firmware v{version}")
    notes.append("")
    notes.append("[![License](https://img.shields.io/github/license/Xerolux/HB-RF-ETH-ng)](LICENSE.md)")
    notes.append("[![Downloads](https://img.shields.io/github/downloads/Xerolux/HB-RF-ETH-ng/total)](https://github.com/Xerolux/HB-RF-ETH-ng/releases)")
    notes.append("")

    if is_prerelease:
        notes.append("> ⚠️ **Pre-Release** - Testversion, Nutzung auf eigene Gefahr.")
        notes.append("")

    # Overview
    notes.append("## 📋 Überblick")
    notes.append("")
    notes.append("HB-RF-ETH-ng ist eine modernisierte Fork der originalen HB-RF-ETH Firmware von Alexander Reinert.")
    notes.append("Diese Firmware ermöglicht es, ein Homematic Funkmodul (HM-MOD-RPI-PCB oder RPI-RF-MOD) per Netzwerk")
    notes.append("an eine CCU-Installation (piVCCU3, debmatic, OpenCCU) anzubinden.")
    notes.append("")

    # What changed
    if changelog:
        notes.append(f"## 🆕 Was ist neu in v{version}?")
        notes.append("")
        notes.append(changelog)
        notes.append("")
    else:
        notes.append(f"## 🆕 Was ist neu in v{version}?")
        notes.append("")
        notes.append("Siehe [Commit-Historie](https://github.com/Xerolux/HB-RF-ETH-ng/commits/main) für alle Änderungen.")
        notes.append("")

    # Key features
    notes.append("## ✨ Hauptfunktionen")
    notes.append("")
    notes.append("- **Moderne WebUI** mit Responsive Design, Dark/Light Theme und 10 Sprachen")
    notes.append("- **Netzwerk-Updates** - Firmware direkt per URL herunterladen")
    notes.append("- **MQTT-Support** mit Home Assistant Auto-Discovery")
    notes.append("- **SNMP & Check_MK Monitoring** für Integration in Monitoringsysteme")
    notes.append("- **IPv6-Support** mit Auto-Konfiguration")
    notes.append("- **Sichere Authentifizierung** mit automatischem Session-Timeout")
    notes.append("- **Verbesserte OTA-Updates** mit besserer Fehlerbehandlung")
    notes.append("- **LED-Helligkeitssteuerung** (0-100%)")
    notes.append("- **Konfigurations-Backup/Restore** über WebUI")
    notes.append("")

    # Installation
    notes.append("## 📥 Installation")
    notes.append("")
    notes.append("### Update über WebUI")
    notes.append("")
    notes.append("1. Die `firmware_*.bin` Datei aus diesem Release herunterladen")
    notes.append("2. In der WebUI zu **Firmware Update** navigieren")
    notes.append("3. Die .bin Datei hochladen")
    notes.append("4. Auf Abschluss des Updates und automatischen Neustart warten")
    notes.append("")
    notes.append("### Update per URL")
    notes.append("")
    notes.append("Alternativ kann das Update direkt aus diesem Release per URL in der WebUI durchgeführt werden.")
    notes.append("")
    notes.append("### Prüfsummen")
    notes.append("")
    notes.append("SHA256-Prüfsummen befinden sich in `SHA256SUMS.txt`.")
    notes.append("")

    # Important notes
    notes.append("## ⚠️ Wichtige Hinweise")
    notes.append("")
    notes.append("- **Backup der Einstellungen** vor dem Update erstellen (Einstellungen → Backup)")
    notes.append("- **Nicht abschalten** während des Update-Vorgangs")
    notes.append("- Bei sehr alten Versionen kann ein **Werksreset** erforderlich sein")
    notes.append("- Nach erfolgreichem Update startet das Gerät **automatisch neu**")
    notes.append("")

    # What's included
    notes.append("## 📦 Im Release enthalten")
    notes.append("")
    notes.append(f"- **Firmware-Binary** (`firmware_{version}.bin`)")
    notes.append("- **Bootloader** (`bootloader.bin`)")
    notes.append("- **Partitionstabelle** (`partitions.bin`)")
    notes.append("- **SHA256-Prüfsummen** (`SHA256SUMS.txt`)")
    notes.append("- **Versionsinformationen** (`version.txt`)")
    notes.append("")

    # Compatible systems
    notes.append("## 🔗 Kompatible CCU-Systeme")
    notes.append("")
    notes.append("- **[OpenCCU](https://openccu.de/)** - Open-Source CCU-Betriebssystem")
    notes.append("- **[piVCCU3](https://github.com/leon-vi/piVccu)** - Homematic auf Raspberry Pi")
    notes.append("- **[debmatic](https://github.com/leopes91/debmatic)** - Homematic auf Debian-basierten Systemen")
    notes.append("")

    # Support
    notes.append("## 💬 Support & Community")
    notes.append("")
    notes.append("- **Issues**: Bitte [Fehler melden](https://github.com/Xerolux/HB-RF-ETH-ng/issues)")
    notes.append("- **Discussions**: [Community-Diskussionen](https://github.com/Xerolux/HB-RF-ETH-ng/discussions)")
    notes.append("- **Dokumentation**: Siehe [README.md](https://github.com/Xerolux/HB-RF-ETH-ng/blob/main/README.md)")
    notes.append("")

    # Sponsor block
    notes.append("## 🙏 Unterstützung")
    notes.append("")
    notes.append("Dir gefällt dieses Projekt und du möchtest es unterstützen?")
    notes.append("")
    notes.append("[![Buy Me A Coffee][buymeacoffee-badge]][buymeacoffee]")
    notes.append("[![Tesla Referral](https://img.shields.io/badge/Tesla-Referral-red?style=for-the-badge&logo=tesla)](https://ts.la/sebastian564489)")
    notes.append("")
    notes.append("[buymeacoffee]: https://www.buymeacoffee.com/xerolux")
    notes.append("[buymeacoffee-badge]: https://img.shields.io/badge/buy%20me%20a%20coffee-donate-yellow.svg?style=for-the-badge")
    notes.append("")

    # License
    notes.append("## 📄 Lizenz")
    notes.append("")
    notes.append("Diese Firmware steht unter [Creative Commons Attribution-NonCommercial-ShareAlike 4.0](LICENSE.md).")
    notes.append("")

    # Footer
    notes.append("---")
    notes.append("")
    notes.append("**Vielen Dank an alle Beitragenden!** 🙏")
    notes.append("")
    notes.append("*Diese Firmware basiert auf der originalen Arbeit von [Alexander Reinert](https://github.com/ja-ra). ")
    notes.append("Die modernisierte Fork wird von [Xerolux](https://github.com/Xerolux) gewartet.*")
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
