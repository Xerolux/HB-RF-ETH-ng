#!/usr/bin/env python3
"""
Local ESP-IDF wrapper for this repository.

Usage:
  ./idf.py build
  ./idf.py flash monitor

Behavior:
  - Uses ESP-IDF from $IDF_PATH if set
  - Falls back to ~/esp-idf
  - Applies repo defaults for target and sdkconfig defaults
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent
DEFAULT_IDF_PATH = Path.home() / "esp-idf"
SDKCONFIG_DEFAULTS = "sdkconfig.defaults;sdkconfig.hb-rf-eth-ng"


def main() -> int:
    idf_path = Path(os.environ.get("IDF_PATH", str(DEFAULT_IDF_PATH))).expanduser()
    real_idf_py = idf_path / "tools" / "idf.py"

    if not real_idf_py.exists():
        print(
            "ERROR: ESP-IDF not found.\n"
            "Set IDF_PATH or install ESP-IDF to ~/esp-idf.\n"
            "Expected: {}".format(real_idf_py),
            file=sys.stderr,
        )
        return 1

    env = os.environ.copy()
    env.setdefault("IDF_PATH", str(idf_path))
    env.setdefault("IDF_TARGET", "esp32")
    env.setdefault("SDKCONFIG_DEFAULTS", SDKCONFIG_DEFAULTS)

    cmd = [sys.executable, str(real_idf_py), *sys.argv[1:]]
    return subprocess.call(cmd, cwd=str(REPO_ROOT), env=env)


if __name__ == "__main__":
    raise SystemExit(main())
