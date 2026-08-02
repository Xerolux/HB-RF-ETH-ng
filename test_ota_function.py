#!/usr/bin/env python3
"""Upload a local HB-RF-ETH-ng firmware image through the manual OTA endpoint."""

import argparse
import getpass
import json
from pathlib import Path
import sys
import urllib.error
import urllib.request


LOGIN_TIMEOUT_SECONDS = 10
OTA_TIMEOUT_SECONDS = 600


def make_base_url(device):
    """Return a normalized device base URL."""
    value = device.strip().rstrip("/")
    if not value:
        raise ValueError("device address must not be empty")
    if "://" not in value:
        value = f"http://{value}"
    if not value.startswith(("http://", "https://")):
        raise ValueError("device address must use http:// or https://")
    return value


def decode_json(body, context):
    try:
        return json.loads(body.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        preview = body[:200].decode("utf-8", errors="replace")
        raise RuntimeError(f"{context} returned invalid JSON: {preview!r}") from exc


def http_error_message(exc):
    body = exc.read().decode("utf-8", errors="replace").strip()
    detail = f": {body}" if body else ""
    return f"HTTP {exc.code} {exc.reason}{detail}"


def get_token(base_url, username, password):
    url = f"{base_url}/login.json"
    data = json.dumps({"username": username, "password": password}).encode("utf-8")
    request = urllib.request.Request(
        url,
        data=data,
        headers={"Content-Type": "application/json"},
        method="POST",
    )

    try:
        with urllib.request.urlopen(request, timeout=LOGIN_TIMEOUT_SECONDS) as response:
            result = decode_json(response.read(), "login")
    except urllib.error.HTTPError as exc:
        raise RuntimeError(f"login failed: {http_error_message(exc)}") from exc
    except (urllib.error.URLError, TimeoutError) as exc:
        raise RuntimeError(f"login failed: {exc}") from exc

    token = result.get("token") if result.get("isAuthenticated") else None
    if not token:
        detail = result.get("error") or "invalid username or password"
        raise RuntimeError(f"login failed: {detail}")
    return token


def read_firmware(path):
    firmware_path = path.expanduser().resolve()
    if not firmware_path.is_file():
        raise RuntimeError(f"firmware file does not exist: {firmware_path}")
    if firmware_path.suffix.lower() != ".bin":
        raise RuntimeError("firmware file must have a .bin extension")

    try:
        image = firmware_path.read_bytes()
    except OSError as exc:
        raise RuntimeError(f"could not read firmware file: {exc}") from exc

    if not image:
        raise RuntimeError("firmware file is empty")
    if image[0] != 0xE9:
        raise RuntimeError(
            "file is not an ESP32 firmware image (expected magic byte 0xE9)"
        )
    return firmware_path, image


def upload_firmware(base_url, token, image):
    url = f"{base_url}/ota_update"
    request = urllib.request.Request(
        url,
        data=image,
        headers={
            "Authorization": f"Token {token}",
            "Content-Type": "application/octet-stream",
            "Content-Length": str(len(image)),
        },
        method="POST",
    )

    try:
        with urllib.request.urlopen(request, timeout=OTA_TIMEOUT_SECONDS) as response:
            result = decode_json(response.read(), "OTA upload")
    except urllib.error.HTTPError as exc:
        raise RuntimeError(f"OTA upload failed: {http_error_message(exc)}") from exc
    except (urllib.error.URLError, TimeoutError) as exc:
        raise RuntimeError(f"OTA upload failed: {exc}") from exc

    if not result.get("success"):
        detail = result.get("error") or result.get("message") or "unknown error"
        raise RuntimeError(f"OTA upload failed: {detail}")
    return result


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Upload a local firmware .bin file to the manual HB-RF-ETH-ng "
            "POST /ota_update endpoint."
        )
    )
    parser.add_argument(
        "device",
        help="device IP/hostname, optionally including http:// or https://",
    )
    parser.add_argument("firmware", type=Path, help="path to the local firmware .bin")
    parser.add_argument(
        "--username",
        default="admin",
        help="administrator username (default: admin)",
    )
    parser.add_argument(
        "--password",
        help="administrator password (omit to enter it without shell-history exposure)",
    )
    return parser.parse_args()


def main():
    args = parse_args()

    try:
        base_url = make_base_url(args.device)
        firmware_path, image = read_firmware(args.firmware)
        password = args.password
        if password is None:
            password = getpass.getpass("Administrator password: ")

        print(f"Authenticating as {args.username} at {base_url}...")
        token = get_token(base_url, args.username, password)
        print(
            f"Uploading {firmware_path.name} ({len(image)} bytes) "
            "to POST /ota_update..."
        )
        result = upload_firmware(base_url, token, image)
    except (RuntimeError, ValueError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    message = result.get("message") or "Firmware upload completed."
    print(message)
    print("The device should now restart and boot the uploaded firmware.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
