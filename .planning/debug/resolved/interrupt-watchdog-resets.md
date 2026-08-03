---
status: resolved
trigger: "schau dir den code an und fixe ihn, er muss stabil laufen jahre lang ohne fehler !!! das ist dein ziel"
created: 2026-08-02T13:22:00+02:00
updated: 2026-08-02T19:11:22+02:00
---

## Current Focus

hypothesis: "The reset is a composite real-time regression introduced in 2.2.x: always-on log fan-out ran unsafe/heavy subscriber work in arbitrary producer tasks, hot Raw-UART/metrics paths used ESP32-emulated 64-bit atomics backed by a global interrupt-disabling lock, and Ethernet was opted into the broader cache-disabled/IRAM interrupt contract. Forced worker deletion during network/TLS operations added independent mutex/resource-corruption risks."
test: "Remove every 64-bit atomic from runtime code in favor of native lock-free 32-bit state, make log subscribers bounded and race-free, stop network workers cooperatively with I/O deadlines, restore default cache-aware EMAC behavior, keep the 300 ms IWDT active, pin stable ESP-IDF v6.0.2, and stress the rollover/concurrency paths before a fresh production build."
expecting: "No application object imports 8-byte atomic helpers, log producers never perform Syslog formatting/network work, task shutdown cannot strand a global mutex, and the firmware builds with the original watchdog sensitivity intact."
next_action: "field-soak the verified Beta.4 firmware on affected boards under Raw-UART plus MQTT/Syslog/Prometheus traffic; retain serial panic output if any reset recurs"

## Symptoms

expected: "The HB-RF-ETH-ng firmware remains continuously available for years under normal CCU Raw-UART traffic with optional MQTT, Syslog, logging, CheckMK, events, WebUI, and network activity enabled."
actual: "Devices running the current Beta.3 firmware reboot unpredictably after minutes to hours with an Interrupt Watchdog reset; the CCU/radio connection is disrupted. Multiple devices and board revisions reproduce it while 2.1.10 is reported stable."
errors: "Boot reset reason: Watchdog Reset (Interrupt Watchdog). Crash black-box examples: free=101260 largest=98304 min_ever=99704 internal=152148 uptime=66s low_streak=0; free=104508 largest=102400 min_ever=99508 internal=155396 uptime=366s."
started: "Reported throughout the 2.2.x series; deterministic ping-triggered use-after-free was fixed earlier, but the nondeterministic watchdog reset remains in 2.2.6-Beta.3."
reproduction: "No single deterministic action. Run an active OpenCCU Raw-UART session, often with MQTT and Syslog/log capture enabled, and wait; resets have occurred from roughly one minute through many hours."

## Evidence

- timestamp: 2026-08-02T13:22:00+02:00
  checked: "User boot log, crash black-box values, repository config, current GitHub issue #362 reports, and both later-accessible screenshots. The two image files are byte-identical (SHA-256 2b1b9d980e2a3a816aad0ca10b33569cde226a43e5b4de0844079595a1e4cdf2)."
  found: "Repeated ESP_RST_INT_WDT resets occur with about 99-104 KB free heap, a 94-100 KB largest block, and low_streak=0. The screenshot independently confirms an Interrupt Watchdog reset after a last sample at 366 seconds on firmware Beta.2, but contains no panic PC/backtrace. CONFIG_ESP_INT_WDT_TIMEOUT_MS is 300 and both CPUs are checked."
  implication: "Low total heap and heap fragmentation are eliminated as the immediate reset trigger; investigation must focus on interrupt starvation, long cache/flash-off regions, driver/ISR misuse, or earlier memory corruption."

- timestamp: 2026-08-02T13:22:00+02:00
  checked: "Current main after git pull."
  found: "main is at ee36860. Beta.3 removed automatic update checking, reduced MQTT churn, and made Syslog sockets persistent, yet field resets continue. version.txt still says 2.2.6-Beta.2."
  implication: "The Beta.3 heap-pressure mitigation did not fix the underlying reset. The displayed Beta.2 version is a release-versioning defect, not proof that Beta.3 was not installed."

- timestamp: 2026-08-02T13:31:00+02:00
  checked: "ESP-IDF interrupt-watchdog implementation and every application critical section, ISR, high-priority task, UART/UDP callback, NVS/OTA path, and periodic task."
  found: "The interrupt watchdog is fed by the per-CPU FreeRTOS tick ISR, not by a timer task. Application critical sections are short, the DCF ISR only performs an ISR-safe queue send, and the Raw-UART LwIP callback hands data to a queue. No application path contains a deliberate 300 ms interrupt mask."
  implication: "Changing task priority or increasing the watchdog timeout would hide the symptom. The remaining high-value search area is a driver interrupt that stays live across cache-disabled flash/NVS windows."

- timestamp: 2026-08-02T13:35:00+02:00
  checked: "Git history and source/config delta between reported-stable v2.1.10 and the 2.2.x line."
  found: "Commit 35c7996 first added ETH_MAC_FLAG_WORK_WITH_CACHE_DISABLE during the 2.2 migration; stable v2.1.10 left ETH_MAC_DEFAULT_CONFIG().flags at zero. Commit c2ee535 later enabled CONFIG_ETH_IRAM_OPTIMIZATION. Both settings are present in every currently affected 2.2.x build."
  implication: "This is the interrupt-mode regression that aligns with the release boundary and all affected hardware, unlike optional MQTT/Syslog features that can be disabled without eliminating resets."

- timestamp: 2026-08-02T13:38:00+02:00
  checked: "ESP-IDF v6.0.2 EMAC implementation and public flag definition."
  found: "ETH_MAC_FLAG_WORK_WITH_CACHE_DISABLE is an opt-in special mode, not a compatibility requirement. It allocates the driver object in internal RAM and registers the EMAC interrupt with ESP_INTR_FLAG_IRAM, so the interrupt remains enabled while flash cache is disabled. The default flags value is zero."
  implication: "The project enabled a broader interrupt execution contract without needing Ethernet service during cache-off operations. Restoring the default causes ESP-IDF to defer the EMAC interrupt during those critical windows."

- timestamp: 2026-08-02T13:42:00+02:00
  checked: "Crash black-box uptime cadence and 64-bit metric atomics."
  found: "Black-box heap samples occur every 60 seconds from service startup, so recorded uptimes ending in the same residue are sampling cadence, not evidence of a 60-second reset trigger. On Xtensa ESP32, std::atomic<uint64_t> is not lock-free: ESP-IDF's 8-byte atomic helpers serialize through one global lock and enter a critical section. These operations were used on high-frequency Raw-UART RX/TX/drop paths after the logging/metrics expansion."
  implication: "The apparent periodicity and heap values are observations, not causes. Emulated 64-bit atomics are a concrete interrupt-latency amplifier and must not be used in firmware hot paths."

- timestamp: 2026-08-02T14:12:00+02:00
  checked: "The exact 2.2.x logging/metrics regression boundary and subscriber execution model."
  found: "Commit fb899dd added permanent LogManager subscribers for live-log streaming, Syslog, and metrics. Subscriber registration/state was read concurrently without synchronization; Syslog parsed ANSI/severity, read Settings/time, formatted RFC5424, and queued a large message on the producer's stack. The live-log queue/worker/subscriber also existed when no WebSocket client was connected."
  implication: "Arbitrary ESP-IDF and application tasks inherited substantial stack and scheduling work on every log line. Combined with the global 64-bit atomic critical section, this is the strongest code-level regression matching the affected release line."

- timestamp: 2026-08-02T14:18:00+02:00
  checked: "Events, Supporter CRL, Syslog, and Prometheus stop paths."
  found: "Several stop paths force-deleted tasks that could own g_net_fetch_mutex, mbedTLS/HTTP allocations, or live sockets. A deletion at that point can permanently strand the mutex or leak/corrupt network state; Prometheus sends also lacked a complete bounded-client lifecycle."
  implication: "These are independent long-uptime failure modes even when they are not the instruction that triggered the supplied 66-second reset, so stable shutdown requires cooperative cancellation and bounded I/O."

- timestamp: 2026-08-02T14:24:00+02:00
  checked: "Conservative runtime/platform fixes and executable policy/stress regressions."
  found: "Runtime code now contains no 64-bit atomic type. Metrics preserve 64-bit exported values using native lock-free uint32_t low/high words plus active-writer and generation validation; Raw-UART keepalive uses task-local rollover-safe TickType_t. LogManager subscriber state is synchronized, live-log forwarding starts only for an active client, and Syslog producer callbacks only severity-filter and bounded-copy into a nonblocking queue. Network/TLS workers use cooperative stop and deadlines. The cache-disabled EMAC flag and ETH_IRAM optimization are removed, every build is pinned to v6.0.2, and IWDT remains 300 ms."
  implication: "The concrete application and platform mechanisms capable of extending interrupt starvation or corrupting shared network state are removed rather than hidden behind a longer watchdog. The final build below verifies the integrated result; physical soak remains the last validation stage."

- timestamp: 2026-08-02T19:11:22+02:00
  checked: "Final host regressions, repeated counter stress, ASan credential comparison, source/symbol/config policy checks, two independent P0-P2 re-audits, and a complete ESP-IDF v6.0.2 production build after the final lifecycle/NVS review."
  found: "All 32 source-policy tests pass. Monitoring-default, ping-lifecycle, secure-string, and deterministic rollover/concurrency host binaries pass with -Wall -Wextra -Werror; the counter test also completed 25 consecutive stress runs and the unequal-length credential comparison passed AddressSanitizer. The final audit found and the implementation closed a Raw-UART task-create pbuf leak, a dual-atomic operation-gate race, fail-open monitoring allowlists, stale-auth factory-marker recovery, and missing bulk-password reauthentication. libmain.a has no undefined __atomic_*_8 import, the generated config identifies IDF 6.0.2, ETH_IRAM_OPTIMIZATION is disabled, and IWDT remains 300 ms. The final Beta.4 application is 0x145d90 bytes (1,334,672 bytes) with 30% of the smallest OTA partition free. SHA-256 is 1621b40e683da5df251496ca78dda3c59bafadc039e4116dc387c0fef7a8aec7."
  implication: "The code-level fix is build- and regression-verified. Only a physical multi-day field soak can establish whether the nondeterministic hardware reset has disappeared under the user's exact traffic and peripherals."

## Eliminated

- hypothesis: "The interrupt watchdog is caused directly by exhausted or severely fragmented heap."
  evidence: "Every supplied Beta.3 pre-crash snapshot is healthy and low_streak remains zero; the largest free block is close to the total default-capability free heap."
  timestamp: 2026-08-02T13:22:00+02:00

- hypothesis: "The 60-second heap watchdog or a periodic application task directly triggers each reset."
  evidence: "Crash black-box uptime is the timestamp of the last periodic sample, not the exact reset time; all samples naturally share the watchdog task's startup offset modulo 60 seconds."
  timestamp: 2026-08-02T13:42:00+02:00

- hypothesis: "The reset can be fixed safely by increasing CONFIG_ESP_INT_WDT_TIMEOUT_MS."
  evidence: "ESP-IDF feeds the IWDT from the tick ISR; a larger timeout only lengthens an interrupt-starvation window. The fix keeps the 300 ms threshold and removes the interrupt-mode regression."
  timestamp: 2026-08-02T13:58:00+02:00

## Resolution

root_cause: "No panic backtrace identifies one exact instruction, so a single-source claim would be false precision. The evidence supports a composite 2.2.x real-time regression: permanent logging subscribers performed unsafe/heavy work in arbitrary producer tasks while new hot counters used ESP32-emulated 64-bit atomics with a global interrupt-disabling lock. Cache-disabled/IRAM EMAC settings amplified interrupt/cache risk, and force-deleted TLS/network workers created additional long-uptime corruption paths. Healthy pre-reset heap excludes exhaustion as the supplied crash's immediate cause."
fix: "Use native lock-free 32-bit atomic/task-local state throughout runtime hot paths; make logging fan-out synchronized, bounded, and demand-driven; move Syslog formatting to its worker; use cooperative worker shutdown with socket/TLS deadlines; restore default cache-aware EMAC behavior; disable Ethernet IRAM optimization; and align all build inputs on stable ESP-IDF v6.0.2 without increasing the watchdog timeout. A single 32-bit CAS now serializes configuration and flash/reset operations; NVS/auth recovery and unauthenticated-listener configuration fail closed."
verification: "Complete at code/build level: 32/32 policy tests and all strict host binaries pass, the metrics concurrency/rollover binary passed 25 additional consecutive runs, the credential comparison passed AddressSanitizer, the final ESP-IDF v6.0.2 target build succeeds, the application archive imports no 8-byte atomic helpers, the generated configuration keeps IWDT=300 ms with Ethernet IRAM optimization disabled, and the Beta.4 binary hash/size were recorded. Two final read-only audits report no remaining P0/P1/P2 findings. Hardware field soak remains required to validate multi-day reset absence."
files_changed: ".github/workflows/* firmware pipelines; project/release/docs/version inputs; include runtime, storage, monitoring, authentication and network lifecycle headers; main Ethernet, logging, Raw-UART, metrics, monitoring, MQTT, Settings, WebUI, reset, CRL, theme, Syslog, events and Prometheus sources; ESP-IDF sdkconfig defaults; host/Unity stability regressions; WebUI Beta.16 sources and build assets"
