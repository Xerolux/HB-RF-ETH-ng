---
status: resolved
trigger: "Publish: Losing qos0 data when client not connected (wiederholt vor MQTT_EVENT_CONNECTED)"
created: 2026-07-25T20:45:00+02:00
updated: 2026-07-25T21:32:25+02:00
---

## Current Focus

hypothesis: "Confirmed: the periodic MQTT publish task started with the client runtime while every publish guard checked mqtt_running instead of mqtt_connected, so whole QoS 0 status batches were submitted during broker connection/reconnection."
test: "A source regression requires all public/internal publish entry points and the periodic task to defer while mqtt_connected is false, while preserving the immediate initial publish from MQTT_EVENT_CONNECTED."
expecting: "No esp_mqtt_client_publish call remains outside the centralized connected-state wrapper, and the complete regression/build suite remains green."
next_action: "Validate broker reconnect behaviour on the next beta without changing the live device during this investigation."

## Symptoms

expected: "No MQTT publish calls are submitted until MQTT_EVENT_CONNECTED; after connection, retained status and Home Assistant discovery are published normally."
actual: "Dozens of QoS 0 publishes run before the broker connection completes and ESP-MQTT logs 'Publish: Losing qos0 data when client not connected'."
errors: "Repeated mqtt_client warning from 28.1 s through 33.3 s; MQTT_EVENT_CONNECTED occurs later at 37.3 s."
reproduction: "Boot or restart with MQTT enabled while the broker connection takes several seconds. Observe one full status batch every publish cycle before MQTT_EVENT_CONNECTED."
started: "Observed immediately after installing v2.2.5-Beta.17."

## Evidence

- timestamp: 2026-07-25T20:45:00+02:00
  checked: "User device log ordering."
  found: "QoS 0 publish-loss warnings precede MQTT_EVENT_CONNECTED by several seconds and occur in dense groups matching mqtt_handler_publish_status."
  implication: "The broker is not yet connected when the periodic publisher emits status batches."

- timestamp: 2026-07-25T20:45:00+02:00
  checked: "main/mqtt_handler.cpp lifecycle and publish guards."
  found: "mqtt_running is set immediately after esp_mqtt_client_start and starts mqtt_publish_task. mqtt_connected is only set in MQTT_EVENT_CONNECTED, but publish_status, publish_ota_state, publish_event, publish_task_stacks and HA discovery check only mqtt_running/client."
  implication: "The code already tracks the correct broker state but does not use it to gate publishing."

- timestamp: 2026-07-25T21:32:25+02:00
  checked: "RED/GREEN MQTT lifecycle regression and final source scan."
  found: "The regression failed on the Beta.17 guards, then passed after centralizing publishing behind mqtt_can_publish. Exactly one raw esp_mqtt_client_publish call remains and it is inside the connected-state wrapper."
  implication: "Startup and reconnect status batches can no longer reach ESP-MQTT before MQTT_EVENT_CONNECTED."

- timestamp: 2026-07-25T21:32:25+02:00
  checked: "Complete verification suite."
  found: "42/42 Playwright regressions, all three native host tests, the production WebUI build, and a full ESP-IDF v6.1-beta1 firmware/SPIFFS build pass. The final firmware image is 0x148540 bytes with 0x87ac0 bytes (29%) free in the smallest app partition."
  implication: "The lifecycle fix compiles in the target firmware and does not regress the surrounding MQTT, update, recovery, backup, factory-reset, ping, or responsive-layout contracts."

## Eliminated

- hypothesis: "The broker or credentials reject QoS 0 packets after a successful login."
  evidence: "The warnings stop once MQTT_EVENT_CONNECTED occurs, after which subscription and Home Assistant discovery proceed normally."
  timestamp: 2026-07-25T20:45:00+02:00

## Resolution

root_cause: "mqtt_running represented client/task lifetime, not broker authentication. Every publishing path treated that weaker state as permission to publish, so the task submitted complete QoS-0 batches before MQTT_EVENT_CONNECTED and during reconnects."
fix: "Added mqtt_can_publish and a single mqtt_publish_connected wrapper requiring running, connected, and a valid client. All status/event/OTA/task-stack/HA paths use it; the periodic task sleeps while disconnected, and startup sets lifecycle flags before esp_mqtt_client_start to close the fast-event race."
verification: "Targeted regression failed RED and passed GREEN; final suite passes 42/42 Playwright cases, three native host tests, WebUI production build, and full ESP-IDF firmware/SPIFFS build."
files_changed: "main/mqtt_handler.cpp, webui/tests/regressions.spec.js, CHANGELOG.md"
