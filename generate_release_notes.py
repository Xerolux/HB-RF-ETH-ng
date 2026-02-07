#!/usr/bin/env python3
"""
Generate release notes for HB-RF-ETH-ng firmware releases.
Combines changelog, installation instructions, and funding information.

Usage:
    python generate_release_notes.py <version> <output_file>

Example:
    python generate_release_notes.py 2.1.0 release_notes.md
"""

import sys
import re
from pathlib import Path
from datetime import datetime

def extract_changelog(version: str) -> str:
    """Extract changelog section for specific version"""
    changelog_file = Path("CHANGELOG.md")

    if not changelog_file.exists():
        return "No changelog available."

    content = changelog_file.read_text()

    # Try to find the version section
    # Matches: "## 2.1.0", "## [2.1.0]", "## Version 2.1.0", "## Version [2.1.0]"
    pattern = rf"##\s+(?:Version\s+)?\[?{re.escape(version)}\]?"
    lines = content.split('\n')

    changelog_lines = []
    found = False

    for line in lines:
        if re.match(pattern, line):
            found = True
            continue
        elif found and line.startswith('## '):
            break
        elif found:
            changelog_lines.append(line)

    if changelog_lines:
        return '\n'.join(changelog_lines).strip()

    return "See CHANGELOG.md for version details."

def get_firmware_files() -> list:
    """Get list of firmware binary files"""
    release_dir = Path("release")

    if not release_dir.exists():
        return []

    files = []
    for file in sorted(release_dir.glob("*.bin")):
        files.append(file.name)

    return files

def generate_release_notes(version: str) -> str:
    """Generate complete release notes"""

    # Determine if this is a pre-release
    is_prerelease = any(tag in version.lower() for tag in ['alpha', 'beta', 'rc'])

    # Extract changelog
    changelog = extract_changelog(version)

    # Get firmware files
    firmware_files = get_firmware_files()

    # Build release notes
    notes = []

    # Header
    notes.append(f"# 🚀 HB-RF-ETH-ng Firmware v{version}")
    notes.append("")

    if is_prerelease:
        notes.append("> ⚠️ **Pre-Release Version** - This is a pre-release version for testing purposes.")
        notes.append("> Use at your own risk and report any issues on GitHub.")
        notes.append("")

    notes.append(f"**Release Date:** {datetime.now().strftime('%Y-%m-%d')}")
    notes.append("")

    # What's New
    notes.append("## 📋 What's New")
    notes.append("")
    notes.append(changelog)
    notes.append("")

    # Download Section
    notes.append("## 📥 Download & Installation")
    notes.append("")
    notes.append("### Complete Release Package")
    notes.append("")
    notes.append(f"Download the complete firmware package: **`hb-rf-eth-ng-firmware-v{version}.zip`**")
    notes.append("")
    notes.append("This package includes:")
    notes.append("- Firmware binary files")
    notes.append("- Bootloader and partition table")
    notes.append("- SHA256 checksums")
    notes.append("- README and CHANGELOG")
    notes.append("")

    # Individual Files
    if firmware_files:
        notes.append("### Individual Firmware Files")
        notes.append("")
        notes.append("| File | Description |")
        notes.append("|------|-------------|")
        for file in firmware_files:
            if file.startswith('firmware'):
                notes.append(f"| `{file}` | Main firmware binary |")
            elif file == 'bootloader.bin':
                notes.append(f"| `{file}` | ESP32 bootloader |")
            elif file == 'partitions.bin':
                notes.append(f"| `{file}` | Partition table |")
        notes.append("")

    # Installation Instructions
    notes.append("## 🔧 Installation Instructions")
    notes.append("")
    notes.append("### Method 1: WebUI Update (Recommended)")
    notes.append("")
    notes.append("1. Download the main firmware file (`firmware*.bin`)")
    notes.append("2. Login to your HB-RF-ETH-ng WebUI")
    notes.append("3. Navigate to **Firmware Update**")
    notes.append("4. Select the downloaded `.bin` file")
    notes.append("5. Click **Update** and wait for completion")
    notes.append("6. Device will automatically reboot with new firmware")
    notes.append("")
    notes.append("### Method 2: Serial Flash (Advanced)")
    notes.append("")
    notes.append("For initial installation or recovery:")
    notes.append("")
    notes.append("```bash")
    notes.append("# Using esptool.py")
    notes.append("esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 \\")
    notes.append("  --before default_reset --after hard_reset write_flash -z \\")
    notes.append("  --flash_mode dio --flash_freq 40m --flash_size 4MB \\")
    notes.append("  0x1000 bootloader.bin \\")
    notes.append("  0x8000 partitions.bin \\")
    notes.append("  0x10000 firmware*.bin")
    notes.append("```")
    notes.append("")

    # Verification
    notes.append("## 🔐 Verify Download Integrity")
    notes.append("")
    notes.append("SHA256 checksums are provided in `SHA256SUMS.txt`.")
    notes.append("")
    notes.append("**Linux/Mac:**")
    notes.append("```bash")
    notes.append("sha256sum -c SHA256SUMS.txt")
    notes.append("```")
    notes.append("")
    notes.append("**Windows (PowerShell):**")
    notes.append("```powershell")
    notes.append("Get-FileHash firmware*.bin -Algorithm SHA256")
    notes.append("```")
    notes.append("")

    # Important Notes
    notes.append("## ⚠️ Important Notes")
    notes.append("")
    notes.append("- **Backup your settings** before updating")
    notes.append("- **Default password:** `admin` (must be changed on first login)")
    notes.append("- After device restart, you may need to restart your CCU/debmatic/piVCCU3")
    notes.append("- Factory reset available via hardware button (see documentation)")
    notes.append("")

    # Features
    notes.append("## ✨ Key Features")
    notes.append("")
    notes.append("- 🌐 Ethernet connectivity for HomeMatic radio modules")
    notes.append("- 🔄 OTA firmware updates via WebUI")
    notes.append("- 🕐 Multiple time sources (NTP, GPS, DCF77, RTC)")
    notes.append("- 📊 SNMP monitoring support")
    notes.append("- 🔍 Check_MK agent integration")
    notes.append("- 🔒 Enforced password change on first login")
    notes.append("- 📱 Modern Vue 3 WebUI")
    notes.append("- ⚙️ ESP-IDF 5.x based")
    notes.append("")

    # Compatibility
    notes.append("## 🔌 Compatibility")
    notes.append("")
    notes.append("**Supported Radio Modules:**")
    notes.append("- RPI-RF-MOD")
    notes.append("- HM-MOD-RPI-PCB")
    notes.append("")
    notes.append("**Compatible Systems:**")
    notes.append("- piVCCU3 (≥ v3.51.6-41)")
    notes.append("- debmatic (≥ v3.51.6-46)")
    notes.append("- openCCU")
    notes.append("")

    # Support & Funding
    notes.append("## 💖 Support This Project")
    notes.append("")
    notes.append("If you find this firmware useful, please consider supporting its development:")
    notes.append("")
    notes.append("[![Buy Me A Coffee](https://img.shields.io/badge/buy%20me%20a%20coffee-donate-yellow.svg?style=for-the-badge)](https://www.buymeacoffee.com/xerolux)")
    notes.append("[![Tesla Referral](https://img.shields.io/badge/Tesla-Referral-red?style=for-the-badge&logo=tesla)](https://ts.la/sebastian564489)")
    notes.append("")
    notes.append("Your support helps maintain and improve this project! 🙏")
    notes.append("")

    # Links
    notes.append("## 🔗 Links & Resources")
    notes.append("")
    notes.append("- 📖 [Documentation](https://github.com/Xerolux/HB-RF-ETH-ng/blob/main/README.md)")
    notes.append("- 🐛 [Report Issues](https://github.com/Xerolux/HB-RF-ETH-ng/issues)")
    notes.append("- 💬 [Discussions](https://github.com/Xerolux/HB-RF-ETH-ng/discussions)")
    notes.append("- 📋 [Full Changelog](https://github.com/Xerolux/HB-RF-ETH-ng/blob/main/CHANGELOG.md)")
    notes.append("- 🔧 [Troubleshooting Guide](https://github.com/Xerolux/HB-RF-ETH-ng/blob/main/docs/TROUBLESHOOTING.md)")
    notes.append("")

    # Credits
    notes.append("## 👏 Credits")
    notes.append("")
    notes.append("This firmware is a modernized fork of the original [HB-RF-ETH](https://github.com/alexreinert/HB-RF-ETH) by **Alexander Reinert**.")
    notes.append("")
    notes.append("Special thanks to:")
    notes.append("- Alexander Reinert for the original firmware and hardware design")
    notes.append("- The HomeMatic community")
    notes.append("- All contributors and testers")
    notes.append("")

    # Footer
    notes.append("---")
    notes.append("")
    notes.append("**License:** [CC BY-NC-SA 4.0](https://github.com/Xerolux/HB-RF-ETH-ng/blob/main/LICENSE.md)")
    notes.append("")
    notes.append(f"**Version:** {version} | **Built:** {datetime.now().strftime('%Y-%m-%d %H:%M UTC')}")

    return '\n'.join(notes)

def main():
    if len(sys.argv) != 3:
        print("Usage: python generate_release_notes.py <version> <output_file>")
        print("Example: python generate_release_notes.py 2.1.0 release_notes.md")
        sys.exit(1)

    version = sys.argv[1].lstrip('v')
    output_file = sys.argv[2]

    print(f"📝 Generating release notes for version {version}")

    try:
        notes = generate_release_notes(version)

        Path(output_file).write_text(notes)

        print(f"✅ Release notes written to {output_file}")
        print(f"📄 {len(notes.split(chr(10)))} lines generated")

    except Exception as e:
        print(f"❌ Error generating release notes: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
