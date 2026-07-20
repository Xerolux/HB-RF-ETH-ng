# System overview, device themes, recovery, and log streaming

This feature set extends the New Design after the separate WebUI update
foundation described in `WEBUI_UPDATES.md`.

## System overview

The authenticated endpoint `GET /api/system/overview` returns diagnostics that
are useful when investigating memory pressure and update failures on the
ESP32-WROOM-32:

- ESP-IDF version and build target
- chip model, revision, features, and CPU-core count
- total, used, and free internal heap
- internal heap usage percentage
- minimum free heap observed since boot
- largest currently allocatable contiguous heap block
- PSRAM presence plus total/free PSRAM when available
- reset reason as a readable string
- detected flash size
- running and next OTA partition labels and sizes
- active WebUI source, version, mount state, and SPIFFS usage
- log capture state, ring capacity, currently available bytes, total stream
  offset, active subscriber count, and crash-tail availability

Values are calculated only when requested. No permanent diagnostic task,
statistics buffer, history recorder, or additional polling loop is created in
firmware. The New Design requests the data once when the page is opened and only
again when the user presses **Refresh**.

## Minimal recovery page

`GET /recovery` serves a small standalone HTML page embedded in the firmware. It
does not depend on Vue, Bootstrap, the SPIFFS WebUI image, external assets, or a
background task. It adds one route to the existing HTTP server and does not
create a second server instance.

The page uses the normal `/login.json` authentication flow and existing
`Authorization: Token ...` contract. After login it can:

- display on-demand RAM, reset, WebUI, and crash-tail status
- upload a raw WebUI image through `POST /api/webui/update`
- upload a raw firmware image through `POST /ota_update`
- download the current log through `GET /api/log/download`
- retrieve the one-shot crash tail through `GET /api/crash_log`
- restart the device through `POST /api/restart`

No privileged recovery-only API is introduced. All modifying actions remain
protected by the existing admin token.

## Device-wide theme configuration

`GET /api/theme` is public so the login page can adopt the device appearance.
`POST /api/theme` requires the existing admin token.

Stored values:

- `colorScheme`: `system`, `light`, or `dark`
- `primaryColor`: hexadecimal `#RRGGBB`

The browser applies its cached values before the request completes to avoid a
flash of the default theme. The device response then becomes authoritative and
is mirrored back to local storage. Theme settings are stored in the NVS
namespace `ui_theme` and therefore survive firmware and separate WebUI updates.

## Allocation-free log download

`GET /api/log/download` no longer constructs a complete `std::string` snapshot.
It reads the existing in-memory ring buffer in 1 KiB chunks and sends them using
HTTP chunked transfer encoding.

A reader whose absolute offset points to overwritten data is advanced to the
oldest byte still present. This keeps download memory constant while preserving
existing WebSocket live-log and syslog subscriber behavior.

The System Overview displays the same ring-buffer state without allocating a
log snapshot. Crash-tail presence is checked through the NVS blob length only;
the crash tail is not loaded or erased until the user explicitly requests it.

## Validation

Automated firmware, WebUI, size, security, and documentation checks are required
before merge. Physical 4 MiB ESP32-WROOM-32 validation is deferred to the first
beta test cycle and must be completed before promoting the feature to stable.
