# Manual Firmware OTA Test Protocol

This checklist verifies the one remaining firmware-update path on real
HB-RF-ETH-ng hardware: an administrator uploads a **local firmware `.bin`** as
the raw request body of `POST /ota_update`.

The following former online-update features have been removed and are not part
of this test:

- automatic release/update searches;
- URL-based OTA downloads;
- MQTT or Home Assistant initiated OTA updates;
- the former `/api/check_update` and `/api/ota_url` endpoints.

The device does not need Internet access, DNS for GitHub, or a TLS connection to
a release server for a manual update.

## Prerequisites

- A test device reachable on the LAN.
- The administrator username and password. The default username is `admin`.
- A local HB-RF-ETH-ng **firmware** image (`*.bin`). Do not use the separate
  327680-byte WebUI/SPIFFS image on the firmware endpoint.
- Stable power and, preferably, a serial console for observing the upload,
  service shutdown, boot selection, and restart.
- Python 3 when using `test_ota_function.py`.

Use a known test build and record its SHA-256 before flashing:

```bash
sha256sum build/HB-RF-ETH-ng-<version>.bin
```

## Test with the helper script

The helper authenticates, validates the local file, and sends its bytes directly
as `application/octet-stream`. It does not contact GitHub or submit a download
URL.

```bash
python3 test_ota_function.py <device-ip> build/HB-RF-ETH-ng-<version>.bin
```

The password is requested interactively so it is not stored in shell history.
For non-interactive use, provide it explicitly:

```bash
python3 test_ota_function.py <device-ip> firmware.bin \
  --username admin --password '<admin-password>'
```

Expected output is similar to:

```text
Authenticating as admin at http://192.168.0.31...
Uploading firmware.bin (1320848 bytes) to POST /ota_update...
Firmware update completed, restarting in 3 seconds...
The device should now restart and boot the uploaded firmware.
```

The request remains open while the device receives, validates, and activates the
image. Allow up to ten minutes; do not interrupt the upload merely because it
takes longer than a normal API request.

## Equivalent curl test

`/ota_update` accepts a raw binary body, not `multipart/form-data`. In
particular, use `--data-binary`, not `-F`:

```bash
TOKEN=$(curl -fsS -X POST http://<device-ip>/login.json \
  -H 'Content-Type: application/json' \
  -d '{"username":"admin","password":"<admin-password>"}' \
  | python3 -c 'import json,sys; print(json.load(sys.stdin)["token"])')

curl -fsS -X POST http://<device-ip>/ota_update \
  -H "Authorization: Token $TOKEN" \
  -H 'Content-Type: application/octet-stream' \
  --data-binary @firmware.bin
```

On success, the endpoint returns JSON with `"success": true`, then the device
restarts after approximately three seconds.

## Pass criteria

- [ ] Authentication succeeds with the configured administrator credentials.
- [ ] The complete local `.bin` is accepted without a socket, flash-write, or
      image-validation error.
- [ ] The success response is sent only after the image has been finalized,
      background network services have stopped, and the new boot partition has
      been selected.
- [ ] The device restarts and reconnects to Ethernet.
- [ ] The expected new firmware version appears in the WebUI or
      `/sysinfo.json` after the restart.
- [ ] MQTT, CheckMK, Syslog, notifications, and raw-UART connectivity recover
      according to the saved configuration.
- [ ] The serial log contains no panic, watchdog reset, or heap-corruption
      message during upload, shutdown, and the first boot.

## Negative and recovery checks

Run destructive recovery tests only on a lab device with a known-good serial
flashing path.

- [ ] Upload without a valid `Authorization: Token ...` header: rejected with
      HTTP 401 and no partition change.
- [ ] Upload an empty file or a file without the ESP32 image magic byte `0xE9`:
      rejected and the running firmware remains active.
- [ ] Upload the 327680-byte WebUI/SPIFFS image: rejected with guidance to use
      the separate WebUI installer.
- [ ] Start two manual uploads concurrently: the second request is rejected as
      already in progress; the first image is not corrupted.
- [ ] Interrupt the client connection before the full content length is sent:
      the partial OTA write is aborted and the current boot partition remains
      selected.
- [ ] Remove power during an incomplete upload: after power is restored, the
      previously selected firmware still boots.

## Update search (manual, stage B1)

The search reports versions only; it never downloads or installs anything.
Everything below is read-only on the device apart from the outbound HTTPS
request itself.

**Memory gate — the acceptance criterion.** Read
`esp_get_minimum_free_heap_size()` (WebUI system overview, or
`status/min_free_heap`) before and after, and record the load state first,
because the floor depends on the baseline:

- On an **idle device** (no CCU session, optional services off; roughly
  110 KB free after boot), the manual search must not push the minimum free
  heap below **80 KB**.
- On a device **with an active CCU session** (idle free heap around 60–65 KB,
  where an 80 KB floor is unreachable by design), the transient minimum must
  stay above **15 KB** during the search's TLS handshake.

In both states, after twenty consecutive searches the free heap must return
to within 4 KB of its starting value. Field reference (2026-09-09, active
CCU on RPI-RF-MOD fw 4.4.22): idle free 63.6 KB / largest block 45.0 KB,
transient minimum 17.4 KB, free heap after 20 searches within 172 bytes of
the start; no relay impact, no watchdog reset.

- [ ] Press "Search for updates now" with the device idle: a result appears and
      the displayed check time advances.
- [ ] Press it again immediately: reported as cooldown, and the previously
      displayed result and check time are unchanged.
- [ ] Press it **while a CCU session is active and relaying**: the search
      completes or is skipped with a visible reason, and the serial log shows
      no UART overflow, no watchdog reset, and no CCU disconnect.
- [ ] Unplug the uplink and press it: an explicit failure message appears. It
      must not read as "up to date".
- [ ] Force a low-memory state (start monitoring, MQTT-TLS and syslog, attach a
      CCU) and press it: either it runs, or it is skipped with the free/largest
      figures shown. A skip must never be displayed as "no update found".
- [ ] Select the beta channel on a device running the newest beta: reported as
      up to date, and **no** downgrade to the older stable release is offered.
- [ ] Select the stable channel on that same beta device: still no downgrade.
- [ ] Two browsers press it simultaneously: the second is reported as busy or
      cooldown; only one outbound request is made.
- [ ] Press it while a firmware upload is running: skipped with a reason, and
      the upload is unaffected.
- [ ] Home Assistant: before the first search the four update entities read
      "unknown", not "off"/"no update". After a search they carry real values.
- [ ] Restart the device: the search result is gone (it is not persisted) and
      the card reads "no search has been run yet" rather than showing a stale
      result.
- [ ] Over an hour of normal CCU operation without pressing the button: no
      outbound connection to github.io appears at the uplink (`tcpdump`). The
      device must never search on its own.

## Long-run acceptance

After a successful update, leave the device under its normal MQTT, CheckMK,
Syslog, notification, and radio traffic for an extended soak. Record reset
reason, uptime, free heap, largest free block, minimum-ever heap, and worker stack
watermarks. A manual upload test passes the stability gate only when there are no
watchdog/panic resets and no sustained resource decline after services reconnect.
