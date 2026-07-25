---
status: awaiting_human_verify
trigger: "Updatecheck per MQTT/HA reagiert nicht; Raw-UART optimieren; Ping-Button und Einstellungen reagieren nicht sauber responsiv"
created: 2026-07-25T21:32:25+02:00
updated: 2026-07-25T21:32:25+02:00
---

## Current Focus

hypothesis: "Confirmed in source and on the live device: Beta.17 rejects the HA/MQTT-triggered manual update search at 55 KB free heap despite a healthy 32 KB contiguous block. The settings tabs additionally wrap early because their container is capped at 700 px, and the ping action has no dedicated responsive layout."
test: "Admit the exact live 55/32-KB heap state through a two-dimensional policy with absolute floors; remove per-packet Raw-UART descriptor/vector allocation without changing the wire format; assert real desktop/mobile tab and ping geometry in Playwright."
expecting: "The 55/32-KB policy case passes while 51/32, 64/17, and 55/27 fail; ordinary Raw-UART packets need no heap allocation; settings tabs stay in one row at 1280 px and ping stacks at 390 px."
next_action: "Install the next beta only when requested, then verify the HA Check for Update button and monitor heap/CCU continuity without performing a factory reset."

## Symptoms

expected: "The Home Assistant Check for Update button refreshes retained update topics; Raw-UART runs without avoidable heap churn; settings tabs and ping controls use the available width at desktop and mobile breakpoints."
actual: "The live device retains Beta.15 update data after a manual attempt, Raw-UART allocates a descriptor and vector for each UDP datagram, six settings tabs wrap into two rows on a wide desktop, and the ping button remains disproportionately small."
errors: "Live API lastSkipReason: Update check skipped (low heap): free=55 KB largest=32 KB."
reproduction: "On Beta.17 press Check for Update from HA/MQTT; inspect GET /api/check_update and the device log. Open /settings at 1280 px and 390 px."
started: "Reported on Beta.17."

## Evidence

- timestamp: 2026-07-25T21:20:00+02:00
  checked: "Authenticated read-only GET /api/check_update and live log on 192.168.178.56."
  found: "The device runs 2.2.5-Beta.17, still exposes Beta.15 as the cached release, and records lastSkipReason plus a log line for free=55 KB/largest=32 KB. No setting, firmware, or device state was changed."
  implication: "The button reaches the manual path, but the old one-dimensional 56-KB admission boundary prevents the network fetch."

- timestamp: 2026-07-25T21:24:00+02:00
  checked: "Raw-UART listener and upstream piVCCU driver behaviour."
  found: "The firmware log contains a valid explicit type-1 disconnect followed by a reconnect, not the firmware's distinct 10-second timeout message. The receive path allocates udp_event_t and std::vector for each datagram."
  implication: "Do not ignore valid CCU disconnect commands or change protocol semantics. Heap churn can be removed independently and safely."

- timestamp: 2026-07-25T21:28:00+02:00
  checked: "RED/GREEN layout and Raw-UART regressions."
  found: "The desktop layout test first observed two settings-tab rows at 1280 px and the Raw-UART source test found both heap allocations. Both pass after the production changes; the mobile test additionally verifies two-column tabs and a full-width ping action below the input."
  implication: "The reported geometry and allocation patterns are directly covered rather than inferred from screenshots."

- timestamp: 2026-07-25T21:32:25+02:00
  checked: "Final automated and target-build verification."
  found: "42/42 Playwright regressions, all three native host tests, WebUI production build, and the ESP-IDF v6.1-beta1 target build pass. Firmware size is 0x148540 bytes with 29% app-partition headroom; the release WebUI image v1.0.0-Beta.15 uses 252411/327680 raw bytes."
  implication: "The combined firmware and WebUI changes are buildable and remain inside both partitions."

## Eliminated

- hypothesis: "The observed CCU reconnect was caused by the firmware's keepalive timeout."
  evidence: "That path logs 'connection timed out (no keep-alive for 10 seconds)'; the live log instead contains only the explicit 'CCU 3 disconnected' message produced by a valid type-1 packet."
  timestamp: 2026-07-25T21:24:00+02:00

- hypothesis: "The update failure is an inert Home Assistant button or missing MQTT command dispatcher."
  evidence: "The safe check_update dispatcher and HA discovery button are present in Beta.17; the live update snapshot records the downstream manual callback's exact low-heap skip reason."
  timestamp: 2026-07-25T21:20:00+02:00

## Resolution

root_cause: "The update guard used independent 56-KB total/18-KB block thresholds and rejected a nearly nominal but low-fragmentation 55/32-KB live state. Raw-UART created two ordinary-packet heap allocations. Settings navigation was capped at 700 px with generic flex wrapping, and ping controls inherited a one-size-fits-all flex row."
fix: "Use a tested nominal-or-compensated update heap policy (56/18 KB or 52/28 KB); queue udp_event_t by value and process packets up to 256 bytes on the task stack with a checked large-frame fallback; switch settings tabs to full-width breakpoint grids and ping controls to a proportional desktop/two-row mobile grid."
verification: "Exact boundary host assertions, RED/GREEN source and geometry regressions, 42/42 full Playwright cases, three native host tests, production WebUI build, and full ESP-IDF firmware/SPIFFS build. Real-device verification awaits installation of the next beta."
files_changed: "include/updatecheck_heap_policy.h, main/updatecheck.cpp, main/rawuartudplistener.cpp, webui/src/settings.vue, webui/tests/regressions.spec.js, test/host/test_updatecheck_heap_policy.cpp, .github/workflows/build.yml, .github/workflows/release.yml, CHANGELOG.md"
