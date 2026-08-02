#!/usr/bin/env python3
"""
Legacy helper that generates the retired embedded firmware archive.

The current firmware does not embed or serve this archive, and the former
``/api/firmware_archive`` endpoint was removed. The script remains only for
historical maintenance of old release branches; its output is unused by the
current build.

Usage:
    python scripts/update_archive.py

Run from the repository root.
"""

import gzip
import sys
from pathlib import Path

# Resolve paths relative to the repo root (parent of this script's directory).
REPO_ROOT = Path(__file__).resolve().parent.parent
SOURCE = REPO_ROOT / "archive.json"
OUT_DIR = REPO_ROOT / "main" / "generated"
OUT_FILE = OUT_DIR / "archive.json.gz"


def main() -> int:
    if not SOURCE.exists():
        print(f"✗ {SOURCE} not found — run the release tooling to generate it first.", file=sys.stderr)
        return 1

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    raw = SOURCE.read_bytes()
    if not raw.strip():
        print(f"✗ {SOURCE} is empty — refusing to embed an empty archive.", file=sys.stderr)
        return 1

    # Sanity check: archive.json must be valid JSON before we embed it, so a
    # malformed release file never makes it into a firmware image.
    import json
    try:
        json.loads(raw.decode("utf-8"))
    except (json.JSONDecodeError, UnicodeDecodeError) as e:
        print(f"✗ {SOURCE} is not valid JSON: {e}", file=sys.stderr)
        return 1

    gz = gzip.compress(raw, compresslevel=9)
    OUT_FILE.write_bytes(gz)

    raw_kb = len(raw) / 1024
    gz_kb = len(gz) / 1024
    print(f"✓ Embedded firmware archive → {OUT_FILE.relative_to(REPO_ROOT)}")
    print(f"  archive.json: {raw_kb:.1f} KB raw → {gz_kb:.1f} KB gzipped ({len(gz)} bytes in flash)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
