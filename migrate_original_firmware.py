#!/usr/bin/env python3
"""Migrate a device running the ORIGINAL HB-RF-ETH firmware (1.3.x) to HB-RF-ETH-ng.

Why this script exists
----------------------
The original firmware's POST /ota_update handler (src/webui.cpp in
alexreinert/HB-RF-ETH 1.3.x) finalizes an uploaded image - esp_ota_end() plus
esp_ota_set_boot_partition() - only when its receive loop terminates before
the HTTP server's 5-second socket timeout. That loop can never terminate on
its byte counter for the multipart body the original WebUI sends (the first
chunk's bytes are never counted), so a browser upload almost always ends in a
timeout, the handler aborts with "Error writing OTA", and the boot selector is
never switched: after the restart the old firmware is still running. Only when
the client happens to close the connection right after the request body does
recv() return 0, the loop exit cleanly, and the update get activated - which
is why retrying the identical upload occasionally "magically" worked (see
HB-RF-ETH-ng issue #461).

This script performs the upload in exactly the shape the original handler
needs to succeed deterministically:

1. a multipart-style body whose part header ends at the first "\r\n\r\n"
   (the handler strips everything up to that marker and writes the rest
   verbatim), followed by the exact firmware image with no trailing boundary,
   so the written image is byte-identical to the .bin file;
2. a clean half-close of the client side of the TCP connection after the
   body, which makes the handler's recv() return 0 and the update finalize.

After the script reports success, power-cycle the device. The 1.3.x WebUI has
no restart button, and on the next boot the HB-RF-ETH-ng firmware starts and
confirms itself during a short self-test window.
"""

import argparse
import getpass
import json
import socket
import sys
import urllib.error
import urllib.request
from pathlib import Path

LOGIN_TIMEOUT_SECONDS = 10
UPLOAD_TIMEOUT_SECONDS = 600
SEND_CHUNK_BYTES = 4096

# The original handler locates the payload with strstr(buf, "\r\n\r\n") + 4.
# This part header guarantees that marker is ours and short enough to sit in
# the handler's first 1024-byte read.
PART_HEADER_FORMAT = (
    "--HB-RF-ETH-ng-migration\r\n"
    'Content-Disposition: form-data; name="file"; filename="{filename}"\r\n'
    "Content-Type: application/octet-stream\r\n"
    "\r\n"
)


class OriginalFirmwareTarget:
    """Connection parameters for a device running the original firmware."""

    def __init__(self, device):
        host, port, path_prefix = self._parse_device(device)
        self.host = host
        self.port = port
        self.path_prefix = path_prefix
        self.base_url = f"http://{host}:{port}{path_prefix}"

    @staticmethod
    def _parse_device(device):
        """Return (host, port, path_prefix) for a device address string."""
        value = device.strip().rstrip("/")
        if not value:
            raise ValueError("device address must not be empty")
        if value.startswith("https://"):
            raise ValueError(
                "the original firmware serves plain HTTP only; drop https://"
            )
        if value.startswith("http://"):
            value = value[len("http://") :]
        elif "://" in value:
            raise ValueError("device address must use http://")

        path_prefix = ""
        if "/" in value:
            value, path_prefix = value.split("/", 1)
            path_prefix = "/" + path_prefix
        if not value:
            raise ValueError("device address must contain a host")

        if ":" in value:
            host, port_text = value.rsplit(":", 1)
            if not port_text.isdigit() or not 1 <= int(port_text) <= 65535:
                raise ValueError(f"invalid port in device address: {port_text!r}")
            port = int(port_text)
        else:
            host, port = value, 80

        if not host:
            raise ValueError("device address must contain a host")
        return host, port, path_prefix


def get_token(target, password):
    """Authenticate against POST /login.json of the original firmware."""
    url = f"{target.base_url}/login.json"
    data = json.dumps({"password": password}).encode("utf-8")
    request = urllib.request.Request(
        url,
        data=data,
        headers={"Content-Type": "application/json"},
        method="POST",
    )

    try:
        with urllib.request.urlopen(request, timeout=LOGIN_TIMEOUT_SECONDS) as response:
            result = json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"login failed: HTTP {exc.code} {exc.reason}: {body}") from exc
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError) as exc:
        raise RuntimeError(
            f"login failed ({url}): {exc}. Is the device running the original "
            "HB-RF-ETH firmware?"
        ) from exc

    token = result.get("token") if result.get("isAuthenticated") else None
    if not token:
        raise RuntimeError(
            "login failed: wrong administrator password (this script targets "
            "devices running the ORIGINAL HB-RF-ETH firmware 1.3.x)"
        )
    return token


def read_firmware(path):
    """Load and sanity-check the firmware image."""
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


def build_request(target, token, filename, image):
    """Build the HTTP request as (request_head, body) byte strings.

    The body is the multipart part header plus the exact image and NOTHING
    after it. The original handler writes every byte following the first
    "\r\n\r\n" verbatim into the OTA partition, so any trailing multipart
    boundary would end up in flash behind the image; omitting it keeps the
    written image byte-identical to the .bin file.
    """
    part_header = PART_HEADER_FORMAT.format(filename=filename).encode("ascii")
    body = part_header + image

    head = (
        f"POST {target.path_prefix}/ota_update HTTP/1.1\r\n"
        f"Host: {target.host}:{target.port}\r\n"
        f"Authorization: Token {token}\r\n"
        f"Content-Type: multipart/form-data; boundary=HB-RF-ETH-ng-migration\r\n"
        f"Content-Length: {len(body)}\r\n"
        f"Connection: close\r\n"
        f"\r\n"
    ).encode("ascii")
    return head, body


def upload_image(target, token, filename, image):
    """Upload the image so the original handler finalizes the update.

    Closing the write side of the connection right after the body makes the
    handler's recv() return 0; its receive loop then exits before the 5-second
    socket timeout and runs esp_ota_end() + esp_ota_set_boot_partition().
    """
    head, body = build_request(target, token, filename, image)

    try:
        with socket.create_connection(
            (target.host, target.port), timeout=LOGIN_TIMEOUT_SECONDS
        ) as sock:
            sock.settimeout(UPLOAD_TIMEOUT_SECONDS)
            sock.sendall(head)
            for offset in range(0, len(body), SEND_CHUNK_BYTES):
                sock.sendall(body[offset : offset + SEND_CHUNK_BYTES])
            sock.shutdown(socket.SHUT_WR)

            response = b""
            while True:
                chunk = sock.recv(SEND_CHUNK_BYTES)
                if not chunk:
                    break
                response += chunk
    except OSError as exc:
        raise RuntimeError(f"upload failed: {exc}") from exc

    return response


def parse_response(response):
    """Return (status_code, body_text) from a raw HTTP response."""
    header_text, _, body_text = response.partition(b"\r\n\r\n")
    status_line = header_text.split(b"\r\n", 1)[0].decode("latin-1", errors="replace")
    parts = status_line.split(" ", 2)
    if len(parts) < 2 or not parts[1].isdigit():
        raise RuntimeError(f"device sent an unparseable response: {status_line!r}")
    return int(parts[1]), body_text.decode("utf-8", errors="replace")


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Migrate a device running the original HB-RF-ETH firmware (1.3.x) "
            "to an HB-RF-ETH-ng firmware .bin over its WebUI API. Use this "
            "instead of the browser upload: the original firmware's update "
            "handler only activates an upload when the client closes the "
            "connection after the body, which this script does reliably."
        )
    )
    parser.add_argument(
        "device",
        help="device IP/hostname of the ORIGINAL firmware, optionally http://host[:port]",
    )
    parser.add_argument("firmware", type=Path, help="path to the local HB-RF-ETH-ng firmware .bin")
    parser.add_argument(
        "--password",
        help="administrator password (omit to enter it without shell-history exposure)",
    )
    return parser.parse_args()


def main():
    args = parse_args()

    try:
        target = OriginalFirmwareTarget(args.device)
        firmware_path, image = read_firmware(args.firmware)
        password = args.password
        if password is None:
            password = getpass.getpass("Administrator password: ")

        print(f"Authenticating at {target.base_url}...")
        token = get_token(target, password)
        print(
            f"Uploading {firmware_path.name} ({len(image)} bytes) "
            "to POST /ota_update..."
        )
        response = upload_image(target, token, firmware_path.name, image)
        status, body = parse_response(response)
    except (RuntimeError, ValueError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    if status != 200 or "completed" not in body:
        print(
            f"Error: upload not activated by the device (HTTP {status}): "
            f"{body.strip() or '<no response body>'}",
            file=sys.stderr,
        )
        return 1

    print("Firmware update completed and activated by the device.")
    print("Now power-cycle the device (unplug and reconnect the power supply).")
    print("After the reboot the HB-RF-ETH-ng WebUI is served at the same address.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
