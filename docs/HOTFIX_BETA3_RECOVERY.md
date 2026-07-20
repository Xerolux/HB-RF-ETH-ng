# Beta.3 recovery hotfix

This hotfix addresses issues found during the first hardware test of firmware
`2.2.5-Beta.3` and WebUI `1.0.0-Beta.1`:

- the standalone Recovery page did not execute its inline JavaScript because the
  normal Content Security Policy blocked it;
- the external WebUI used Brotli assets on the device's private-LAN HTTP origin;
- an external WebUI image could be selected on the firmware upload page;
- an empty primary IPv4 DNS setting prevented hostname-based NTP and OTA access.

The fixed external WebUI uses gzip and version `1.0.0-Beta.2`. Existing custom
DNS values remain unchanged; only missing or legacy `0.0.0.0` primary DNS values
are initialized to `1.1.1.1`.
