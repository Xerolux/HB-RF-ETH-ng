#!/usr/bin/env python3
"""Verify migrate_original_firmware.py against a simulation of the ORIGINAL
HB-RF-ETH 1.3.x /ota_update handler (issue #461).

The simulation reproduces alexreinert/HB-RF-ETH 1.3.0 (master) src/webui.cpp
post_ota_update_handler_func() as literally as Python allows, including the
behaviour of the surrounding ESP-IDF pieces that make the handler fail:

- HTTPD_DEFAULT_CONFIG() recv_wait_timeout = 5 s: a recv() with no data
  returns HTTPD_SOCK_ERR_TIMEOUT (-2) after five seconds.
- The receive loop counts only the bytes of chunks AFTER the first one
  (content_received += recv_len lives in the else branch), so with the
  multipart body the original WebUI sends, content_received tops out at
  content_length - 1024 and the loop can never end on the byte counter.
  It only exits via recv() returning <= 0.
- On the timeout path the handler calls esp_ota_write() with the negative
  recv_len. As a size_t that is a huge value; the block/size checks in
  esp_ota_write turn it into an error (or, on a sector-aligned offset, into
  a corrupted stream write). Either way the handler aborts and
  esp_ota_end()/esp_ota_set_boot_partition() are never reached.
- Only recv() == 0 (client closed the connection after the body) exits the
  loop cleanly and runs the finalize sequence.

Two scenarios are asserted:

1. A browser-style upload (multipart with trailing boundary, connection kept
   open like axios does) fails to activate the update even though the image
   is completely written - this is issue #461.
2. The migration script (multipart part header + exact image + TCP
   half-close) finalizes the update and the written image is byte-identical
   to the .bin file.

Run:  python3 test_migrate_original_firmware.py
(Takes ~5 s per browser-style scenario because of the faithful 5 s timeout.)
"""

import json
import os
import socket
import tempfile
import threading
import unittest
from pathlib import Path

import migrate_original_firmware as migrator

# ESP-IDF httpd and flash constants the simulation models.
HTTPD_RECV_WAIT_TIMEOUT_SECONDS = 5  # HTTPD_DEFAULT_CONFIG recv_wait_timeout
HTTPD_SOCK_ERR_TIMEOUT = -2
OTA_PARTITION_SIZE = 0x1D0000  # partitions.csv ota_0/ota_1

SUCCESS_BODY = b"Firmware update completed, please restart to activate new firmware."

BOUNDARY = "----WebKitFormBoundarySim1234"


class OtaWriteError(Exception):
    """Modelled esp_ota_write() failure."""


class OriginalHandlerSim:
    """Single-threaded HTTP server re-enacting the original 1.3.x firmware."""

    def __init__(self, password, expected_image):
        self.password = password
        self.expected_image = expected_image
        self.token = "simulated-webui-token"
        # State of the last /ota_update request:
        self.flash = bytearray()
        self.finalized = False
        self.outcome = "no-request-handled"
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.bind(("127.0.0.1", 0))
        self._sock.listen(4)
        self.port = self._sock.getsockname()[1]
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._serve, daemon=True)
        self._thread.start()

    def shutdown(self):
        self._stop.set()
        socket.create_connection(("127.0.0.1", self.port), timeout=2).close()
        self._thread.join(timeout=5)
        self._sock.close()

    # --- server skeleton --------------------------------------------------

    def _serve(self):
        self._sock.settimeout(0.2)
        while not self._stop.is_set():
            try:
                conn, _ = self._sock.accept()
            except socket.timeout:
                continue
            try:
                self._handle_connection(conn)
            except (OSError, ValueError):
                pass
            finally:
                conn.close()

    def _read_request_head(self, conn):
        """Read the HTTP header block the way esp_http_server does before the
        handler runs; returns (method, path, headers, buffered_body_bytes)."""
        data = b""
        while b"\r\n\r\n" not in data:
            chunk = conn.recv(1024)
            if not chunk:
                raise ValueError("client closed before sending headers")
            data += chunk
        head, _, leftover = data.partition(b"\r\n\r\n")
        lines = head.decode("latin-1").split("\r\n")
        method, path, _version = lines[0].split(" ", 2)
        headers = {}
        for line in lines[1:]:
            name, _, value = line.partition(":")
            headers[name.strip().lower()] = value.strip()
        return method, path, headers, leftover

    def _send_response(self, conn, status, body):
        reason = {200: "OK", 401: "Not authorized", 404: "Not Found",
                  500: "Internal Server Error"}[status]
        payload = (
            f"HTTP/1.1 {status} {reason}\r\n"
            f"Content-Type: text/html\r\n"
            f"Content-Length: {len(body)}\r\n"
            f"\r\n"
        ).encode("latin-1") + body
        conn.sendall(payload)

    def _handle_connection(self, conn):
        method, path, headers, leftover = self._read_request_head(conn)
        if method == "POST" and path == "/login.json":
            self._handle_login(conn, headers, leftover)
        elif method == "POST" and path == "/ota_update":
            self._handle_ota_update(conn, headers, leftover)
        else:
            self._send_response(conn, 404, b"Nothing matches the given URI")

    # --- endpoints ---------------------------------------------------------

    def _handle_login(self, conn, headers, leftover):
        # Original post_login_json_handler_func: raw body read into a 1 KiB
        # buffer, password compared against the configured admin password.
        body = leftover
        while len(body) < int(headers.get("content-length", "0")):
            chunk = conn.recv(1024)
            if not chunk:
                break
            body += chunk
        try:
            supplied = json.loads(body.decode("utf-8")).get("password")
        except (UnicodeDecodeError, json.JSONDecodeError):
            supplied = None
        authenticated = supplied == self.password
        result = {"isAuthenticated": authenticated}
        if authenticated:
            result["token"] = self.token
        self._send_response(conn, 200, json.dumps(result).encode("utf-8"))

    def _handle_ota_update(self, conn, headers, leftover):
        # Original post_ota_update_handler_func, incl. validate_auth().
        if headers.get("authorization") != f"Token {self.token}":
            self._send_response(conn, 401, b"401 Not authorized")
            self.outcome = "unauthorized"
            return

        content_length = int(headers.get("content-length", "0"))
        self.flash.clear()
        self.finalized = False
        self.outcome = "incomplete"

        if self._ota_receive_loop(conn, content_length, leftover):
            if self._esp_ota_end():
                # esp_ota_set_boot_partition(update_partition)
                self.finalized = True
                self.outcome = "finalized"
                self._send_response(conn, 200, SUCCESS_BODY)
            else:
                self._send_response(conn, 500, b"Error finalizing OTA")
        # Every other outcome already sent its response (or deliberately sent
        # none, like the bare ESP_FAIL return on a socket error).

    # --- modelled ESP-IDF behaviour -----------------------------------------

    def _esp_ota_write(self, data, size=None):
        """Model esp_ota_write(handle, data, size) with C semantics,
        following IDF v4.4 components/app_update/esp_ota_ops.c: size 0 is a
        no-op, the magic byte is checked on the first write, everything else
        is handed to esp_partition_write().

        `size` lets a caller reproduce the original handler's
        esp_ota_write(ota_handle, ota_buff, recv_len) with a negative recv_len:
        as a size_t that becomes a huge value. The downstream
        esp_partition_write/spi_flash_write then either returns an error
        (handled by the handler's OTA_CHECK as "Error writing OTA") or starts
        an unbounded corrupting write that ends in a watchdog reset - in both
        cases the handler never reaches esp_ota_end(), which is the only
        property the simulation asserts on.
        """
        c_size = len(data) & 0xFFFFFFFF if size is None else size & 0xFFFFFFFF
        wrote = len(self.flash)
        if c_size == 0:
            return  # esp_ota_write returns ESP_OK for size 0
        if wrote == 0 and data[:1] != b"\xe9":
            raise OtaWriteError("ESP_ERR_OTA_VALIDATE_FAILED (magic byte)")
        if c_size > 0x100000 or (wrote + c_size) & 0xFFFFFFFF > OTA_PARTITION_SIZE:
            raise OtaWriteError("huge/negative size rejected downstream")
        self.flash.extend(data)

    def _esp_ota_end(self):
        """Model esp_ota_end() -> esp_image_verify(): the written image must
        be exactly the expected firmware (magic byte + checksum/SHA256)."""
        return bytes(self.flash) == self.expected_image

    def _ota_receive_loop(self, conn, content_length, buffered_body):
        """The original do/while receive loop, including its bugs.

        httpd_req_recv serves the bytes the HTTP header parser already
        buffered (they contain the multipart part header the handler strips)
        before touching the socket, exactly like esp_http_server does.
        """
        conn.settimeout(HTTPD_RECV_WAIT_TIMEOUT_SECONDS)

        pending = bytearray(buffered_body)

        def httpd_req_recv(buf_len):
            # Returns bytes, HTTPD_SOCK_ERR_TIMEOUT, or b"" (peer closed).
            if pending:
                out = bytes(pending[:buf_len])
                del pending[:buf_len]
                return out
            try:
                return conn.recv(buf_len)
            except socket.timeout:
                return HTTPD_SOCK_ERR_TIMEOUT

        content_received = 0
        is_req_body_started = False

        while True:
            chunk = httpd_req_recv(min(content_length, 1024))
            recv_len = (
                HTTPD_SOCK_ERR_TIMEOUT if chunk == HTTPD_SOCK_ERR_TIMEOUT else len(chunk)
            )

            if not is_req_body_started:
                is_req_body_started = True
                if recv_len <= 0:
                    # strstr() on a never-filled buffer: modelled as the
                    # NULL-dereference crash it would be in C.
                    self.outcome = "null_body_start_crash"
                    return False
                marker = chunk.find(b"\r\n\r\n")
                if marker < 0:
                    # strstr() returned NULL -> body_start_p = (char*)4.
                    self.outcome = "null_body_start_crash"
                    return False
                # esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, ...)
                self.flash.clear()
                try:
                    self._esp_ota_write(chunk[marker + 4 :])
                except OtaWriteError:
                    self._send_response(conn, 500, b"Error writing OTA")
                    self.outcome = "esp_ota_begin_write_failed"
                    return False
            elif recv_len == HTTPD_SOCK_ERR_TIMEOUT:
                # The else branch runs esp_ota_write with the negative
                # recv_len; esp_ota_end()/set_boot_partition() are skipped.
                try:
                    self._esp_ota_write(b"", size=0xFFFFFFFE)
                except OtaWriteError:
                    self._send_response(conn, 500, b"Error writing OTA")
                    self.outcome = "esp_ota_write_failed_timeout_path"
                    return False
            else:
                try:
                    self._esp_ota_write(chunk)
                except OtaWriteError:
                    self._send_response(conn, 500, b"Error writing OTA")
                    self.outcome = "esp_ota_write_failed"
                    return False
                content_received += recv_len

            # while ((recv_len > 0) && (content_received < content_length))
            if recv_len <= 0 or content_received >= content_length:
                break

        return True


# --- fixtures -----------------------------------------------------------------


def make_fake_esp32_image(size=65536):
    """Deterministic pseudo-firmware: 0xE9 magic, varied bytes, and a
    deliberate \\r\\n\\r\\n deep inside to prove the part-header marker governs
    the original handler's stripping, not the image content."""
    image = bytearray(b"\xe9")
    counter = 0
    while len(image) < size:
        image.append((counter * 7 + 13) & 0xFF)
        counter += 1
    image[1000:1004] = b"\r\n\r\n"
    return bytes(image)


def axios_style_multipart(image):
    """The exact body the ORIGINAL WebUI (FormData + axios) puts on the wire,
    including the trailing boundary the browser appends."""
    part_header = (
        f"--{BOUNDARY}\r\n"
        f'Content-Disposition: form-data; name="file"; filename="firmware.bin"\r\n'
        f"Content-Type: application/octet-stream\r\n"
        f"\r\n"
    ).encode("ascii")
    trailer = f"\r\n--{BOUNDARY}--\r\n".encode("ascii")
    return part_header, image + trailer


def raw_http_post(port, path, headers, body, part_header):
    """Upload like the browser does: one request on a raw socket, then keep
    the connection open waiting for a response (no half-close)."""
    with socket.create_connection(("127.0.0.1", port), timeout=30) as sock:
        head = (
            f"POST {path} HTTP/1.1\r\n"
            f"Host: 127.0.0.1\r\n"
            + "".join(f"{k}: {v}\r\n" for k, v in headers.items())
            + f"Content-Length: {len(part_header) + len(body)}\r\n"
            f"\r\n"
        ).encode("latin-1")
        sock.sendall(head)
        sock.sendall(part_header)
        sock.sendall(body)
        response = b""
        while b"\r\n\r\n" not in response:
            response += sock.recv(4096)
        head_part, _, rest = response.partition(b"\r\n\r\n")
        content_length = None
        for line in head_part.decode("latin-1").split("\r\n"):
            if line.lower().startswith("content-length:"):
                content_length = int(line.split(":", 1)[1].strip())
        while content_length is None or len(rest) < content_length:
            chunk = sock.recv(4096)
            if not chunk:
                break
            rest += chunk
        return head_part, rest


class MigrationSimulationTest(unittest.TestCase):
    password = "test-password-123"

    @classmethod
    def setUpClass(cls):
        cls.image = make_fake_esp32_image()
        cls.sim = OriginalHandlerSim(cls.password, cls.image)

    @classmethod
    def tearDownClass(cls):
        cls.sim.shutdown()

    def test_01_browser_upload_reproduces_issue_461(self):
        """A WebUI-style multipart upload does not activate the update."""
        part_header, body = axios_style_multipart(self.image)
        head, rest = raw_http_post(
            self.sim.port,
            "/ota_update",
            {"Authorization": f"Token {self.sim.token}"},
            body,
            part_header,
        )

        status_line = head.split(b"\r\n", 1)[0]
        self.assertIn(b"500", status_line)
        self.assertIn(b"Error writing OTA", rest)
        self.assertEqual(self.sim.outcome, "esp_ota_write_failed_timeout_path")
        self.assertFalse(self.sim.finalized)
        # The image was completely written (plus the trailing boundary the
        # browser appends), but the boot selector was never switched:
        self.assertEqual(bytes(self.sim.flash), self.image + b"\r\n--%s--\r\n" % BOUNDARY.encode("ascii"))

    def test_02_migration_script_finalizes(self):
        """The migration script drives the handler into its finalize path."""
        target = migrator.OriginalFirmwareTarget(f"127.0.0.1:{self.sim.port}")
        token = migrator.get_token(target, self.password)
        response = migrator.upload_image(target, token, "firmware.bin", self.image)
        status, body = migrator.parse_response(response)

        self.assertEqual(status, 200)
        self.assertIn("completed", body)
        self.assertEqual(self.sim.outcome, "finalized")
        self.assertTrue(self.sim.finalized)
        # The flashed image is byte-identical to the .bin file - no multipart
        # wrapper, no trailing boundary:
        self.assertEqual(bytes(self.sim.flash), self.image)

    def test_03_script_rejects_non_esp_image(self):
        handle, name = tempfile.mkstemp(suffix=".bin")
        try:
            with os.fdopen(handle, "wb") as stream:
                stream.write(b"\x00\x01\x02")
            with self.assertRaises(RuntimeError):
                migrator.read_firmware(Path(name))
        finally:
            os.unlink(name)


if __name__ == "__main__":
    unittest.main(verbosity=2)
