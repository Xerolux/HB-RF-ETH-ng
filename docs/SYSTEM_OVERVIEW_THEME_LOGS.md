# System overview, device themes, and log streaming

This feature set extends the New Design after the separate WebUI update
foundation described in `WEBUI_UPDATES.md`.

## System overview

The authenticated endpoint `GET /api/system/overview` returns diagnostics that
are useful when investigating memory pressure and update failures on the
ESP32-WROOM-32:

- ESP-IDF version and build target
- chip model, revision, features, and CPU-core count
- current free heap and free internal heap
- minimum free heap observed since boot
- largest currently allocatable contiguous heap block
- detected flash size
- running and next OTA partition labels and sizes
- active WebUI source, version, mount state, and SPIFFS usage

Values are calculated only when requested. No permanent diagnostic task or
additional polling loop is created in firmware.

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
