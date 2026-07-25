---
status: awaiting_human_verify
trigger: "nach update suchen in firmware und in webui findet kein update"
created: 2026-07-22T20:07:35+02:00
updated: 2026-07-22T22:08:00+02:00
---

## Current Focus

hypothesis: Once either page already renders an up-to-date result, a manual check has no durable per-action feedback because outcome-specific state lives only in an auto-expiring toast; the cached fetchedAt display cannot acknowledge cooldown/error and is not an explicit result message.
test: Have the user deploy/run the rebuilt WebUI on the device and click manual search once on Firmware, then WebUI during the shared cooldown.
expecting: Firmware retains an inline up-to-date result and advances the displayed successful-check time; WebUI retains a cooldown explanation instead of appearing inert. Repeating after 60 seconds should retain an up-to-date result on WebUI and advance its check time.
next_action: Await explicit real-device confirmation; do not archive or mark resolved until the user reports "confirmed fixed".
reasoning_checkpoint:
  hypothesis: "Manual check outcomes disappear because checkNow returns an outcome string without retaining it, and both component handlers convert that string only into auto-expiring toasts; when the page was already current, the durable DOM returns to its pre-click state."
  confirming_evidence:
    - "Source tracing shows the shared store retains isChecking and accepted-completion state but no final outcome, while both click handlers map every outcome exclusively to a timed toast."
    - "Four Playwright RED cases observe the correct POST and toast for Firmware success, WebUI success, cooldown, and error, then fail only because durable manual-check feedback is absent."
    - "Backend fetchedAt changes only when refresh runs; cooldown correctly leaves it unchanged, so it cannot serve as universal click feedback."
  falsification_test: "After retaining the outcome, all four focused tests must pass; accepted success must advance the visible successful-check time, while cooldown/error must leave that time unchanged. A failure of either invariant falsifies or over-broadens the proposed fix."
  fix_rationale: "Retaining the final outcome addresses the state-loss boundary directly. A separate lastSuccessfulManualCheckAt timestamp makes accepted completion visible without mislabeling cooldown/error as successful checks; inline alerts reuse the existing translated outcome messages."
  blind_spots: "The exact live browser DOM is unavailable, and a device-side failure after a previously valid snapshot may have separate error-publication behavior. The focused tests cover the reported interaction contract and explicit frontend network errors, but not every firmware transport failure mode."
fault_tree:
  - "event branch: button click is not dispatched or handler is disabled"
  - "in-flight branch: request runs but loading feedback is absent or imperceptibly brief"
  - "success branch: accepted no-update completes but durable UI remains byte-for-byte unchanged"
  - "cooldown/error branch: skipped or failed checks surface only transient feedback"

## Symptoms

expected: Clicking the manual search on Firmware or WebUI should refresh the manifest and show the newest applicable version or an explicit up-to-date result.
actual: Both searches find no update; the Firmware status remains "Noch kein Prüfergebnis vorhanden" after the action.
errors: No visible error message in the supplied screenshot.
reproduction: Open Updates, select Firmware or WebUI, and click the respective manual update-search button.
started: Not specified.

## Eliminated

- hypothesis: A successful beta fetch loses its betaChannel marker, causing the API's cacheMatchesChannel guard to mask the firmware and WebUI releases.
  evidence: UpdateCheck::_doFetch sets out->betaChannel to the configured beta value after every successful parse, before refresh atomically publishes the ReleaseInfo. The device log confirms this exact successful beta parse path.
  timestamp: 2026-07-22T20:11:51+02:00

- hypothesis: The installed WebUI 1.0.0-Beta.7 was built before the completed/no-update rendering and polling behavior existed.
  evidence: Remote tag webui-v1.0.0-Beta.7 points to commit 5e59ef6 (2026-07-22 17:18Z), and its exact stores.js, firmwareupdate.vue, and webuiupdate.vue contain checkNow polling, the no-update outcome, "Firmware ist aktuell", and the WebUI up-to-date alert.
  timestamp: 2026-07-22T20:14:26+02:00

- hypothesis: Deployed firmware 2.2.5-Beta.9 uses a different release publication or GET response contract than current HEAD.
  evidence: Remote tag v2.2.5-Beta.9 resolves to commit 06a3dcb (2026-07-22 17:22Z). Its exact updatecheck.cpp and webui.cpp use the same successful betaChannel assignment, atomic ReleaseInfo publication, cacheMatchesChannel serialization, latestVersion/webui fields, and POST contract as current HEAD.
  timestamp: 2026-07-22T20:15:45+02:00

- hypothesis: Axios globally unwraps responses, so update code incorrectly reads response.data and falls back to n/a/null.
  evidence: webui/src/main.js response interceptor returns the full Axios response unchanged, and representative working stores consistently read response.data.
  timestamp: 2026-07-22T20:19:14+02:00

## Evidence

- timestamp: 2026-07-22T20:07:35+02:00
  checked: User-provided screenshot.
  found: Firmware search UI remains at "Noch kein Prüfergebnis vorhanden" with beta versions enabled and no displayed request error.
  implication: The action either does not refresh the backend result or the frontend discards/misreads the returned manifest state.

- timestamp: 2026-07-22T20:09:26+02:00
  checked: Device log /mnt/c/Users/basti/Downloads/hb-rf-eth-log.txt.
  found: Manual firmware checks arm and execute; beta manifest fetch and JSON parse succeed; latest and current firmware are both 2.2.5-Beta.9; backend logs "Firmware is up to date". Repeated clicks inside 60 seconds are rejected by cooldown. WebUI installation from 1.0.0-Beta.5 to 1.0.0-Beta.7 also succeeds.
  implication: Network fetch, manifest parsing, version selection, and installed-version detection work. The remaining defect is the frontend/API representation of a completed up-to-date result (plus possibly cooldown feedback), not update discovery or installation.

- timestamp: 2026-07-22T20:09:26+02:00
  checked: Live release manifests reported by the parent investigation.
  found: beta.json resolves to firmware 2.2.5-Beta.9 and embedded WebUI 1.0.0-Beta.7; webui-archive.json also resolves to 1.0.0-Beta.7.
  implication: Both device components are genuinely current, so the expected visible result is an explicit up-to-date state rather than an offered download.

- timestamp: 2026-07-22T20:10:34+02:00
  checked: Firmware/WebUI components, Pinia update store, and GET/POST /api/check_update handlers.
  found: The UI already renders an explicit current result whenever latestVersion is non-n/a, and WebUI renders current whenever the response contains a webui release. The backend intentionally masks both fields when ReleaseInfo.betaChannel does not equal the current beta setting (cacheMatchesChannel=false).
  implication: The screenshot's initial state is consistent with the API masking an otherwise valid fetched release because of a channel-marker mismatch; this is more specific than a generic frontend state bug and can be tested by tracing betaChannel assignment.

- timestamp: 2026-07-22T20:11:51+02:00
  checked: ReleaseInfo.betaChannel write path in UpdateCheck::_doFetch and publication in UpdateCheck::refresh.
  found: _doFetch assigns out->betaChannel=beta after a successful parse (duplicated identically on two consecutive lines), and refresh publishes that object under the state mutex before releasing the fetch lock.
  implication: A beta-channel marker mismatch is not produced by the successful manual-fetch path shown in the log. The duplicate assignment is harmless and unrelated.

- timestamp: 2026-07-22T20:12:29+02:00
  checked: Attempt to inspect the authenticated live API through the existing browser session.
  found: Browser control could not initialize because its host rejected the WSL workspace path as a non-local file URI before any browser access occurred.
  implication: No live payload was observed and no device/browser state was changed. Continue with artifact/source differential analysis rather than treating either API or UI as confirmed from the unavailable browser test.

- timestamp: 2026-07-22T20:14:26+02:00
  checked: Exact source at remote tag webui-v1.0.0-Beta.7 and local generated bundle strings.
  found: The installed Beta.7 tag includes the same manual POST/poll/final-GET flow and explicit up-to-date render conditions as current source. The older local SPIFFS image is Beta.3 but is irrelevant because the device log confirms a valid external Beta.7 WebUI is mounted.
  implication: The deployed client is not simply missing the feature; investigate the exact deployed firmware API implementation and client/server timing contract.

- timestamp: 2026-07-22T20:15:45+02:00
  checked: Exact source at remote tag v2.2.5-Beta.9 and LAN neighbor table.
  found: The deployed firmware source matches current backend response logic. The Windows ARP table contains 192.168.178.161 with MAC prefix 34:9f:7b, a likely ESP-class device candidate.
  implication: Static source comparison cannot explain the observed divergence. A read-only live endpoint probe is the remaining direct way to inspect actual API behavior before requesting user capture.

- timestamp: 2026-07-22T20:16:28+02:00
  checked: Read-only HTTP probes of 192.168.178.161 root and project API paths.
  found: The host serves a small generic root document but returns 404 for sysinfo.json, /api/webui/status, and /api/check_update.
  implication: 192.168.178.161 is not the target HB-RF-ETH-ng device; identify the target among the remaining explicit ARP neighbors.

- timestamp: 2026-07-22T20:17:15+02:00
  checked: Root HTTP documents on the remaining explicit ARP neighbors.
  found: Only 192.168.178.56 responds with a substantial gzip-compressed index document; .57, .104, .157, and .205 did not return an HTTP root within the short probe.
  implication: 192.168.178.56 is the probable target device and can be inspected further with GET-only requests.

- timestamp: 2026-07-22T20:17:59+02:00
  checked: Read-only live HTTP probe of 192.168.178.56.
  found: Root identifies HB-RF-ETH-ng and serves the external cache-busted New Design bundle; GET /api/check_update returns 401 without the browser's bearer token.
  implication: The target is confirmed, but the decisive API payload requires the user's authenticated browser context. Continue testing source-level response-shape transformations before requesting that capture.

- timestamp: 2026-07-22T20:19:14+02:00
  checked: Axios interceptors and representative store response consumption.
  found: Responses are not unwrapped or reshaped; the update store's response.data access matches the rest of the application.
  implication: Client-side generic response transformation is not responsible for latestVersion remaining null/n/a.

- timestamp: 2026-07-22T20:22:02+02:00
  checked: Read-only MQTT status snapshot from the device's configured broker.
  found: Retained/current messages report latest_firmware_version=2.2.5-Beta.9, latest_webui_version=1.0.0-Beta.7, firmware_update_available=false, and webui_update_available=false.
  implication: The device has a valid current release snapshot independently of the WebUI. The user-visible defect is failure to persist/render the completed no-update outcome, not manifest discovery or version comparison.

- timestamp: 2026-07-22T20:26:31+02:00
  checked: Focused Playwright regression run after the fix.
  found: Firmware persistent no-update, WebUI persistent no-update, and cooldown/no-fabricated-success tests all pass (3/3). Before the fix the first two failed and cooldown passed.
  implication: Persisting accepted manual completion directly fixes the reproduced UI contract gap without weakening cooldown behavior.

- timestamp: 2026-07-22T20:27:51+02:00
  checked: Full webui/tests/regressions.spec.js run.
  found: 10/14 pass, including all three new update-search cases and update navigation. Four failures are outside the changed production paths: three settings tests use an ambiguous hasText('Hostname') locator that now resolves two inputs, and one legacy firmware-archive test expects a request the current page no longer issues.
  implication: No adjacent update regression was found. The unrelated pre-existing failures are recorded but must not be "fixed" as part of this scoped debug change.

- timestamp: 2026-07-22T20:28:49+02:00
  checked: WebUI production build and final diff hygiene.
  found: npm run build succeeds (158 modules transformed); git diff --check reports no whitespace errors in the changed source/test/debug files. Focused update regressions remain 3/3 green.
  implication: The fix is self-verified and ready for real-device confirmation.

- timestamp: 2026-07-22T20:35:00+02:00
  checked: Human verification on the real device after the first fix.
  found: Firmware now persistently shows "Firmware ist aktuell" and WebUI shows installed=available 1.0.0-Beta.7 plus "Die installierte WebUI ist aktuell", but clicking either manual search produces no visible response, progress, or changed timestamp in the screenshots.
  implication: The first persistent-status fix works. The unresolved defect is interaction feedback for a new manual check, not discovery or retained up-to-date rendering.

- timestamp: 2026-07-22T20:42:00+02:00
  checked: Initial source and diff scan after failed human verification.
  found: Both buttons are wired to manual handlers and already expose busy labels/spinners; successful outcomes are routed to transient toast messages. The existing focused tests begin from "never checked" and therefore do not exercise a second accepted check when the page is already visibly current.
  implication: Click-dispatch failure is unlikely from static wiring. The missing coverage is precisely the real-device state where a completed current result exists before another manual click.

- timestamp: 2026-07-22T20:48:00+02:00
  checked: Exact update-store state transitions and both complete manual click handlers.
  found: checkNow sets only isChecking during the request and hasCompletedManualCheck after accepted success. It records no per-action outcome/time. Both components translate the returned outcome exclusively into a timed toast. Firmware's meta timestamp and WebUI's check card are derived from cached response.fetchedAt; cooldown/skipped/error do not update it.
  implication: The event is wired and the backend result is processed, but there is no durable inline state that can acknowledge every click while preserving the semantic difference between accepted success, cooldown, skip, and error.

- timestamp: 2026-07-22T20:55:00+02:00
  checked: Backend fetchedAt publication and global toast lifecycle.
  found: A successful device fetch replaces ReleaseInfo.fetchedAtMs; cooldown does not run refresh and therefore cannot change it. All manual-result toasts are globally rendered but automatically removed after 4-8 seconds.
  implication: fetchedAt is valid last-success data, not universal click acknowledgement. Durable interaction feedback requires separate frontend outcome state so cooldown/error can remain visible without pretending a successful fetch occurred.

- timestamp: 2026-07-22T21:08:00+02:00
  checked: Three new Playwright reproductions against unchanged production code.
  found: Firmware accepted-success, WebUI accepted-success, and Firmware cooldown all dispatched the request and displayed the expected transient toast. All three then failed at the same post-toast assertion because no manual-check-feedback element exists.
  implication: The hypothesis is directly confirmed: click and result processing work, but outcome state is discarded after the toast lifecycle. The failure is not request dispatch, polling, or version comparison.

- timestamp: 2026-07-22T21:16:00+02:00
  checked: Failed-POST Playwright reproduction against unchanged production code.
  found: The handler surfaced "TLS handshake failed" in its error toast, then the test failed because no durable inline error result exists.
  implication: Error handling has the same state-loss mechanism as success/cooldown. The fix must retain the semantic outcome without advancing the last-success timestamp.

- timestamp: 2026-07-22T21:29:00+02:00
  checked: Four focused Playwright GREEN cases after the production change.
  found: Firmware accepted-success, WebUI accepted-success, cooldown, and failed POST all pass (4/4). The two success cases remain visible beyond toast expiry and advance last-success time; cooldown/error retain their semantic inline result without advancing it.
  implication: The fix addresses the confirmed state-loss mechanism and preserves the required cooldown/error distinction.

- timestamp: 2026-07-22T21:35:00+02:00
  checked: Seven-case adjacent update-search regression run.
  found: The four new cases pass. Three older cases reach their intended result but fail Playwright strict mode because the same outcome message now correctly appears twice (inline plus toast), making their unscoped getByText locators ambiguous.
  implication: This is a test-locator adaptation caused by the intentional dual presentation, not a production behavior regression. Scope the old assertions to the toast they were designed to observe.

- timestamp: 2026-07-22T21:41:00+02:00
  checked: Seven-case update-search regression rerun after scoping the old toast locators.
  found: All seven cases pass, covering no-release accepted success, already-current success on both tabs, cooldown non-success behavior, and explicit network error behavior.
  implication: The new feedback state composes correctly with the first persistent-status fix and does not weaken cooldown or error semantics.

- timestamp: 2026-07-22T21:45:00+02:00
  checked: Production WebUI build and scoped diff hygiene.
  found: npm run build succeeds with 158 modules transformed; git diff --check reports no whitespace errors in the changed source, test, or debug-session files.
  implication: The implementation is production-buildable and mechanically clean; proceed to the full regression suite.

- timestamp: 2026-07-22T22:08:00+02:00
  checked: Complete webui/tests/regressions.spec.js run after adding the WebUI cooldown case.
  found: 15/19 pass. All nine update/navigation cases pass. The four failures exactly match the recorded pre-existing baseline: three ambiguous settings Hostname locators and one obsolete firmware-archive request expectation.
  implication: No adjacent regression from the update-feedback change was detected; remaining failures are outside the changed production paths and unchanged from before this continuation.

- timestamp: 2026-07-22T21:56:00+02:00
  checked: Final scoped source/test diff and working-tree overlap.
  found: The debug-session diff is limited to shared manual outcome/timestamp state, Firmware/WebUI inline renderers, and update regression coverage. Existing unrelated modifications in updates.vue, vite.config.js, screenshots, and Playwright artifacts were not touched.
  implication: The production change is minimal and isolated. Add one WebUI cooldown case to directly cover the cross-tab scenario before human verification.

- timestamp: 2026-07-22T22:05:00+02:00
  checked: WebUI-specific cooldown case and final focused update-search suite.
  found: The WebUI cooldown case passes alone; the final focused suite passes 8/8 in 44.2 seconds, including accepted success on both tabs, cooldown on both tabs, no fabricated success, and explicit error feedback.
  implication: Automated verification covers the reported two-tab workflow and the cooldown/error invariants. Only real-device visual confirmation remains.

## Resolution

root_cause: "The first fix retained only the fact that an accepted check had completed, not the outcome of each manual action. checkNow returned success/cooldown/skipped/error to component handlers, which rendered the outcome exclusively as an auto-expiring toast. When a page was already current—or when cooldown/error correctly left fetchedAt unchanged—the durable DOM after the toast was indistinguishable from the pre-click state."
fix: "Retained lastManualCheckOutcome and lastSuccessfulManualCheckAt in the shared update store. Accepted success advances the success timestamp; cooldown/skipped/error do not. Firmware and WebUI render the retained semantic result inline while preserving existing toasts, and both last-check displays prefer the accepted client completion time."
verification: "Four focused cases failed RED only on absent durable feedback, then passed GREEN. The final focused update-search suite passes 8/8, including cooldown on both tabs; production build and scoped diff check pass. The final complete regression run is 15/19 with all nine update/navigation cases green and the same four unrelated pre-existing failures recorded before this continuation. Awaiting real-device confirmation."
files_changed:
  - webui/src/stores.js
  - webui/src/firmwareupdate.vue
  - webui/src/webuiupdate.vue
  - webui/tests/regressions.spec.js
