#!/usr/bin/env python3
"""Upload to original HB-RF-ETH 1.3.x and verify the running version after reboot.

An upload acknowledgement does not prove that the new application boots
(issue #461). The script waits for a manual power-cycle and checks sysinfo.json
against the version embedded in the supplied image. It never reboots the
device or retries a flash automatically. --verify-only performs no upload.

The legacy handler strips the first multipart header and writes the rest
verbatim. We omit the trailing boundary to preserve the exact image bytes.
Contrary to the initial diagnosis, ESP-IDF's httpd_req_recv() returns zero
when Content-Length is consumed, even if the TCP connection remains open.
"""

import argparse
import getpass
import json
import socket
import struct
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

LOGIN_TIMEOUT_SECONDS = 10
UPLOAD_TIMEOUT_SECONDS = 600
SEND_CHUNK_BYTES = 4096
OTA_PARTITION_BYTES = 0x1D0000
VERIFY_TIMEOUT_SECONDS = 180
VERIFY_STABLE_SECONDS = 20  # longer than the firmware's 15-second self-test
POLL_SECONDS = 2
SUCCESS_BODY = "Firmware update completed, please restart to activate new firmware."

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
    firmware_version(image)
    return firmware_path, image


def firmware_version(image):
    """Read esp_app_desc_t at the first segment's data offset (24 + 8)."""
    if len(image) > OTA_PARTITION_BYTES:
        raise RuntimeError("firmware image exceeds the original OTA partition size")
    if len(image) < 288 or image[0] != 0xE9 or image[1] == 0:
        raise RuntimeError("file is not a complete ESP32 application image")
    if struct.unpack_from("<H", image, 12)[0] != 0:
        raise RuntimeError("firmware targets a chip other than ESP32")
    if struct.unpack_from("<I", image, 28)[0] < 256:
        raise RuntimeError("application descriptor is missing from the first segment")
    if struct.unpack_from("<I", image, 32)[0] != 0xABCD5432:
        raise RuntimeError("application descriptor is missing (do not upload a bootloader)")
    try:
        version = image[48:80].split(b"\0", 1)[0].decode("ascii")
        project = image[80:112].split(b"\0", 1)[0].decode("ascii")
    except UnicodeDecodeError as exc:
        raise RuntimeError("invalid application descriptor") from exc
    if project != "HB-RF-ETH-ng" or not version or not version[0].isdigit():
        raise RuntimeError("image must be an HB-RF-ETH-ng firmware application")
    return version


def get_running_version(target, timeout=LOGIN_TIMEOUT_SECONDS):
    """Read the unauthenticated endpoint shared by original and ng firmware."""
    request = urllib.request.Request(
        f"{target.base_url}/sysinfo.json",
        headers={"Cache-Control": "no-cache"},
    )
    with urllib.request.urlopen(request, timeout=timeout) as response:
        data = json.loads(response.read().decode("utf-8"))
    info = data.get("sysInfo") if isinstance(data, dict) else None
    version = info.get("currentVersion") if isinstance(info, dict) else None
    if not isinstance(version, str) or not version:
        raise ValueError("sysinfo.json does not contain sysInfo.currentVersion")
    return version


def verify_running_firmware(target, expected_version, timeout):
    """Require the expected version to remain reachable across the self-test window.

    This observes the running application, not the private OTA validation state.
    A reset, rollback, wrong version or unreachable device breaks the window.
    """
    deadline = time.monotonic() + timeout
    first_match = None
    last_observation = "device not yet reachable"
    while time.monotonic() < deadline:
        try:
            version = get_running_version(
                target, timeout=min(LOGIN_TIMEOUT_SECONDS, max(0.1, deadline - time.monotonic()))
            )
            last_observation = f"running firmware {version}"
            if version == expected_version:
                if first_match is None:
                    first_match = time.monotonic()
                if time.monotonic() - first_match >= VERIFY_STABLE_SECONDS:
                    return
            else:
                first_match = None
        except (OSError, ValueError) as exc:
            first_match = None
            # Do not print response bodies, URLs or device identifiers from errors.
            last_observation = f"version query failed ({type(exc).__name__})"
        remaining = deadline - time.monotonic()
        if remaining > 0:
            time.sleep(min(POLL_SECONDS, remaining))
    raise RuntimeError(
        f"migration NOT verified: expected {expected_version}; {last_observation}. "
        "If still on 1.3.x after a power-cycle, do not keep repeating uploads. "
        "Capture the serial boot log (115200 baud) to diagnose boot failure/rollback; "
        "see docs/TROUBLESHOOTING.md. Verification can be retried with --verify-only "
        "without writing flash."
    )


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
    """Upload the exact image; the response acknowledges staging, not booting."""
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
            "to an HB-RF-ETH-ng firmware .bin over its WebUI API, then verify "
            "the running version after a manual power-cycle."
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
    parser.add_argument("--verify-only", action="store_true",
                        help="only check the running version; no login or upload")
    parser.add_argument("--wait-seconds", type=int, default=VERIFY_TIMEOUT_SECONDS,
                        help="time allowed for manual reboot and version verification (default: 180)")
    return parser.parse_args()


def main():
    args = parse_args()

    try:
        target = OriginalFirmwareTarget(args.device)
        firmware_path, image = read_firmware(args.firmware)
        expected_version = firmware_version(image)
        if args.wait_seconds <= VERIFY_STABLE_SECONDS:
            raise ValueError(f"--wait-seconds must exceed {VERIFY_STABLE_SECONDS}")
        if not args.verify_only:
            current_version = get_running_version(target)
            if not current_version.startswith("1.3."):
                raise RuntimeError(
                    f"device reports {current_version}, not original 1.3.x; "
                    "use --verify-only to check an already migrated device"
                )
            password = args.password
            if password is None:
                password = getpass.getpass("Administrator password: ")

            print(f"Authenticating at {target.base_url}...")
            token = get_token(target, password)
            print(f"Uploading {firmware_path.name} ({len(image)} bytes) to POST /ota_update...")
            response = upload_image(target, token, firmware_path.name, image)
            status, body = parse_response(response)
            if status != 200 or body.strip() != SUCCESS_BODY:
                raise RuntimeError(
                    f"upload was not acknowledged (HTTP {status}): "
                    f"{body.strip() or '<no response body>'}"
                )
            print("Upload acknowledged. The new firmware has NOT yet been verified running.")
            print("Now power-cycle the device once (unplug and reconnect the power supply).", flush=True)
        print(f"Waiting up to {args.wait_seconds}s for firmware {expected_version} "
              f"to remain reachable for {VERIFY_STABLE_SECONDS}s...", flush=True)
        verify_running_firmware(target, expected_version, args.wait_seconds)
    except (RuntimeError, ValueError, OSError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    print(f"Verified: device reports firmware {expected_version} across the observation window.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
