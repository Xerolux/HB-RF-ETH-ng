## 2024-05-23 - [ESP32 JSON Optimization]
**Learning:** For high-frequency API endpoints on embedded systems (like 1Hz polling), replacing robust but allocation-heavy JSON libraries (cJSON) with `snprintf` into a stack-allocated buffer can save significant CPU cycles and reduce heap fragmentation.
**Action:** Identify polled endpoints. If the structure is flat and predictable, switch to `snprintf`. Ensure buffer size is sufficient (stack usually has 4KB+ on ESP32 tasks).

## 2025-05-24 - [Vue Component Tree Shaking]
**Learning:** Explicitly importing and registering only used components from large UI libraries (like `bootstrap-vue-next`) instead of using wildcard imports allows bundlers (Parcel) to effectively tree-shake unused code.
**Action:** Avoid `import * as Lib` and `app.use(Lib)`. Instead, import specific components and register them individually. In this case, it reduced the main bundle size by ~13% (37KB gzipped).

## 2024-05-24 - [ESP32 Stream Parsing]
**Learning:** For high-throughput serial data parsing (like UART), replacing byte-by-byte state machine loops with bulk `memcpy` (scanning for delimiters like `0xfd` with `memchr`) can significantly reduce CPU usage in interrupt-heavy environments.
**Action:** When optimizing state machines processing buffers, identify "run" states (like `RECEIVE_FRAME_DATA`) where bulk copying is safe, and use standard library functions (`memcpy`, `memchr`) to process chunks.

## 2026-01-03 - [Vue 3 Large List Reactivity]
**Learning:** For components visualizing high-frequency data (like WebSocket streams adding 100+ items/sec), standard Vue `ref([])` creates deep proxies for every object, causing massive memory and CPU overhead.
**Action:** Use `shallowRef([])` for the main list and manually trigger updates with `triggerRef(list)` after mutations (push/splice). This keeps the array reactive but leaves the elements as plain JS objects, significantly improving performance.
