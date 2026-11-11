#!/usr/bin/env python3
"""Fail when a firmware HTTP route drifts from docs/openapi.yaml.

Two directions are checked against the handlers registered in main/*.cpp:

1. A new route without a spec entry fails CI, unless it is explicitly listed
   in LEGACY_UNDOCUMENTED below (routes that predate the spec). That list
   only ever shrinks - removing an entry means adding documentation.
2. A spec entry whose implementation disappeared fails unconditionally.

Usage: python3 scripts/check_openapi_drift.py
"""

import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent

# Routes that existed before the OpenAPI spec and are not yet documented.
# Do not add new entries here - document new endpoints instead.
LEGACY_UNDOCUMENTED = {
    "/api/backup",
    "/api/change-password",
    "/api/crash_log",
    "/api/factory-reset",
    "/api/log",
    "/api/log/disable",
    "/api/log/download",
    "/api/log/enable",
    "/api/log/status",
    "/api/log/stream",
    "/api/monitoring/test",
    "/api/ota_status",
    "/api/password-reset/complete",
    "/api/password-reset/start",
    "/api/password-reset/status",
    "/api/ping",
    "/api/restore",
    "/api/system/overview",
    "/api/system/relay-stats/reset",
    "/api/theme",
    "/api/update/install",
    "/api/webui/status",
    "/api/webui/update",
    "/recovery",
}


def firmware_routes():
    routes = set()
    for cpp in sorted((REPO / "main").glob("*.cpp")):
        text = cpp.read_text()
        for m in re.finditer(r'\.uri\s*=\s*"([^"]+)"', text):
            routes.add(m.group(1))
    return routes


def spec_paths():
    text = (REPO / "docs" / "openapi.yaml").read_text()
    return set(re.findall(r"^  (/[^:\s]+):", text, re.M))


def main():
    routes = firmware_routes()
    spec = spec_paths()

    if not routes:
        print("check_openapi_drift: no routes found in main/*.cpp - parser broken?")
        return 2

    new_undocumented = sorted(routes - spec - LEGACY_UNDOCUMENTED)
    obsolete_allowlist = sorted(LEGACY_UNDOCUMENTED - (routes - spec))
    unimplemented = sorted(spec - routes)

    problems = []
    if new_undocumented:
        problems.append(
            "routes without OpenAPI entry (add them to docs/openapi.yaml):\n  "
            + "\n  ".join(new_undocumented)
        )
    if obsolete_allowlist:
        problems.append(
            "LEGACY_UNDOCUMENTED entries that are documented now (remove them "
            "from scripts/check_openapi_drift.py):\n  " + "\n  ".join(obsolete_allowlist)
        )
    if unimplemented:
        problems.append(
            "spec entries without an implementation in main/*.cpp:\n  "
            + "\n  ".join(unimplemented)
        )

    if problems:
        print("OpenAPI drift detected:")
        for p in problems:
            print("- " + p)
        return 1

    documented = len(routes & spec)
    print(
        f"OpenAPI drift check passed: {len(routes)} firmware routes, "
        f"{documented} documented, "
        f"{len(LEGACY_UNDOCUMENTED)} legacy-allowed."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
