## 2024-05-23 - [ESP32 JSON Optimization]
**Learning:** For high-frequency API endpoints on embedded systems (like 1Hz polling), replacing robust but allocation-heavy JSON libraries (cJSON) with `snprintf` into a stack-allocated buffer can save significant CPU cycles and reduce heap fragmentation.
**Action:** Identify polled endpoints. If the structure is flat and predictable, switch to `snprintf`. Ensure buffer size is sufficient (stack usually has 4KB+ on ESP32 tasks).
