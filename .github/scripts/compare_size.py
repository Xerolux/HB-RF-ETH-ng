import sys
import re

def parse_size(content):
    # Standard pio run -t size output example:
    # RAM:   [=         ]  11.4% (used 37468 bytes from 327680 bytes)
    # Flash: [==        ]  21.6% (used 226496 bytes from 1048576 bytes)
    ram_match = re.search(r"RAM:\s+\[.*\]\s+[\d\.]+\%\s+\(used\s+(\d+)\s+bytes", content)
    flash_match = re.search(r"Flash:\s+\[.*\]\s+[\d\.]+\%\s+\(used\s+(\d+)\s+bytes", content)

    ram = int(ram_match.group(1)) if ram_match else 0
    flash = int(flash_match.group(1)) if flash_match else 0
    return ram, flash

def format_bytes(size):
    for unit in ['B', 'KB', 'MB']:
        if abs(size) < 1024.0:
            return f"{size:3.1f} {unit}"
        size /= 1024.0
    return f"{size:.1f} GB"

try:
    with open(sys.argv[1], 'r') as f:
        base_content = f.read()
    with open(sys.argv[2], 'r') as f:
        curr_content = f.read()

    base_ram, base_flash = parse_size(base_content)
    curr_ram, curr_flash = parse_size(curr_content)

    ram_diff = curr_ram - base_ram
    flash_diff = curr_flash - base_flash

    ram_icon = "🟢" if ram_diff <= 0 else "🔴"
    flash_icon = "🟢" if flash_diff <= 0 else "🔴"

    print("### Firmware Size Diff")
    print("| Metric | Base | Current | Diff |")
    print("|---|---|---|---|")
    print(f"| **RAM** | {base_ram:,} B | {curr_ram:,} B | {ram_icon} {ram_diff:+,} B |")
    print(f"| **Flash** | {base_flash:,} B | {curr_flash:,} B | {flash_icon} {flash_diff:+,} B |")

except Exception as e:
    print(f"Error calculating size diff: {e}")
