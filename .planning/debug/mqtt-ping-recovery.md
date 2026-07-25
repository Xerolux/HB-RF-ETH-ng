---
status: resolved
trigger: "schau dir die github issues an und fixe diese; updatesuche per mqtt geht nicht; wenn man über webui sucht wird das Update in mqtt angezeigt; die Notfallseite ist noch im alten Design; jetzt geht pingen aber es führt zum Absturz der Funkmodulfunktion"
created: 2026-07-25T18:10:00+02:00
updated: 2026-07-25T20:21:56+02:00
---

## Current Focus

hypothesis: "Confirmed: MQTT check_update was absent from the command dispatcher; ping returned on the success callback and freed callback state before ESP-IDF invoked on_ping_end; the self-contained recovery page still used its legacy visual tokens."
test: "Source/host regressions require the MQTT command and HA button, require ping cleanup only after PING_END_BIT, and enforce New-Design recovery tokens/layout."
expecting: "All targeted and full regressions pass, followed by a successful ESP-IDF firmware link."
next_action: "Publish the fix for device validation when requested; keep the unrelated long-uptime issue #362 open until current-firmware uptime evidence exists."

## Symptoms

expected: "MQTT command/check_update starts the same guarded update search as the WebUI and publishes the refreshed result; ping returns without rebooting or disrupting the radio/CCU connection; recovery uses the current New Design."
actual: "MQTT update search has no effect, while a WebUI-triggered search later appears on MQTT; after ping the firmware watchdog-reboots and the radio remains unusable until OpenCCU restarts; recovery still looks like the old design."
errors: "GitHub issue #393 reports Watchdog Reset (Interrupt Watchdog) after ping. MQTT logs are not supplied; the source routes unknown commands to command_rejected."
reproduction: "Publish to <prefix>/command/check_update; alternatively use WebUI and observe MQTT update topics. On the WebUI diagnostics page run one ping and observe the device/radio LEDs and OpenCCU communication. Open /recovery."
started: "MQTT regression predates Beta.16; ping/radio failure confirmed by multiple users on Beta.14, Beta.15 and Beta.16; recovery design feedback reported on Beta.16."

## Evidence

- timestamp: 2026-07-25T18:10:00+02:00
  checked: "Open GitHub issues and comments."
  found: "Issue #393 exactly reproduces the ping-triggered radio outage and records an interrupt-watchdog reboot; multiple users confirm it through Beta.16. Issue #362 remains open for historic long-uptime watchdog/reconnect failures."
  implication: "Ping is a current reproducible regression; #362 needs to be separated from the deterministic ping failure and checked against existing heap fixes."

- timestamp: 2026-07-25T18:10:00+02:00
  checked: "main/mqtt_handler.cpp and history around commit 3d23cff."
  found: "The current dispatcher handles only restart. check_update and its HA button were removed during a heap optimisation, although the manual timer-based low-heap-safe trigger was later reintroduced for the WebUI and the MQTT documentation still advertises it."
  implication: "MQTT search cannot work; it must call UpdateCheck::triggerManualFetch instead of restoring the old large MQTT worker."

- timestamp: 2026-07-25T18:10:00+02:00
  checked: "main/ping_service.cpp and ESP-IDF 6.1-beta1 ping_sock.c."
  found: "xEventGroupWaitBits waits for success OR end. On success it returns before ESP-IDF invokes on_ping_end, then deletes the event group and stack callback context. ping_sock.c invokes on_ping_end afterwards with those freed objects."
  implication: "The callback lifecycle has a deterministic use-after-free capable of causing the reported watchdog reboot."

- timestamp: 2026-07-25T18:54:00+02:00
  checked: "RED tests against the Beta.16 implementation."
  found: "The native ping lifecycle test failed because the service waited for BIT0|BIT1 instead of the final BIT1. The MQTT and recovery source regressions failed because check_update/HA discovery and the New-Design recovery contract were absent."
  implication: "All three regressions reproduced the missing or unsafe production behavior before the fixes."

- timestamp: 2026-07-25T20:10:00+02:00
  checked: "GREEN regression and build suite."
  found: "The ping host lifecycle test and monitoring host test pass; all 38 Playwright regressions pass; the WebUI builds; translation validation passes for 4 locales; JSON validation and git diff whitespace validation pass."
  implication: "The fixes preserve the existing backup, factory-reset, monitoring, update, responsive-layout and localization contracts."

- timestamp: 2026-07-25T20:16:00+02:00
  checked: "Clean ESP-IDF v6.1-beta1 ESP32 firmware build including the standalone WebUI image."
  found: "ping_service.cpp, mqtt_handler.cpp, updatecheck.cpp and system_overview_api.cpp compile and link successfully. HB-RF-ETH-ng.bin is 0x145eb0 bytes with 0x8a150 bytes (30%) free in the smallest app partition; spiffs.bin is exactly 0x50000 bytes."
  implication: "The production firmware and recovery image are buildable and remain within partition limits."

- timestamp: 2026-07-25T20:21:56+02:00
  checked: "Open issue #362 against the current history."
  found: "The latest report predates the Beta.8+ diagnostics and later resource reductions, has no deterministic steps, and has no confirmation on Beta.16. It describes long-uptime watchdog/reconnect behavior rather than the immediate ping lifecycle defect in #393."
  implication: "Do not close or claim #362 fixed without a current long-running device observation; it is tracked separately from this resolved regression."

## Eliminated

- hypothesis: "The ping failure is caused only by the four-second HTTP blocking timeout."
  evidence: "A successful ping returns on PING_SUCCESS_BIT before the ESP-IDF task finishes and exposes a direct freed-callback-state path independent of the HTTP timeout."
  timestamp: 2026-07-25T18:10:00+02:00

## Resolution

root_cause: "Ping cleanup raced ESP-IDF's asynchronous on_ping_end callback; MQTT never dispatched the restored safe manual update path; recovery deliberately retained legacy standalone styling."
fix: "Wait exclusively for PING_END_BIT before freeing callback state, with a defensive stop/acknowledgement path; route MQTT check_update and HA discovery through UpdateCheck::triggerManualFetch and republish status after the atomic snapshot update; replace the standalone recovery styling/structure with the New Design while retaining all recovery actions."
verification: "RED/GREEN host and source regressions, 38/38 Playwright regressions, WebUI build, translation/JSON/diff validation, and full ESP-IDF firmware/SPIFFS build."
files_changed: "main/ping_service.cpp, include/ping_service.h, main/mqtt_handler.cpp, main/updatecheck.cpp, main/system_overview_api.cpp, test/host/**, webui/tests/regressions.spec.js, .github/workflows/{build,release}.yml, CHANGELOG.md, docs/{WIKI,TROUBLESHOOTING}.md"
