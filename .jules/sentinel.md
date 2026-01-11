## 2025-12-12 - [DoS in OTA Handler]
**Vulnerability:** The OTA update handler unconditionally searches for `\r\n\r\n` (header terminator) in the request body to skip headers, but the frontend sends raw binary (`application/octet-stream`). If the binary data does not contain `\r\n\r\n` in the first chunk, `strstr` returns NULL, leading to a pointer arithmetic overflow and a crash (DoS).
**Learning:** Legacy code adapted for new purposes (multipart -> raw binary) can leave dangerous assumptions. Always verify if the data format matches the parsing logic.
**Prevention:** Verify Content-Type before parsing. For raw binary, do not try to parse headers.

## 2025-12-12 - [Timing Attack in Auth]
**Vulnerability:** The `validate_auth` function uses `strcmp` to compare the authentication token. `strcmp` returns early upon mismatch, allowing a timing attack to guess the token byte-by-byte.
**Learning:** Security-sensitive comparisons must be constant-time to avoid side-channel attacks.
**Prevention:** Use a constant-time comparison function (e.g., `sodium_memcmp` or a custom implementation) for secrets.

## 2025-05-18 - [IP Whitelist Bypass in Monitoring]
**Vulnerability:** The CheckMK agent used `strstr` to validate client IPs against the allowed hosts list. This allowed unauthorized IPs (e.g., "192.168.1.1") to bypass the check if they were a substring of an allowed IP (e.g., "192.168.1.100").
**Learning:** Partial string matching is insufficient for security controls. Access control lists must rely on exact matches or proper tokenization.
**Prevention:** Parse delimiters and perform strict equality checks for each token in access control lists.

## 2025-12-12 - [Unchecked snprintf Buffer Overflow]
**Vulnerability:** The monitoring agent used `len += snprintf(...)` to append to a fixed-size stack buffer without checking if `snprintf` truncated the output. This caused `sizeof(buf) - len` to underflow when the buffer filled up, allowing subsequent writes to overflow the stack.
**Learning:** `snprintf` returns the size *needed*, not the size *written*. Unchecked arithmetic on the return value is dangerous.
**Prevention:** Always verify `snprintf` return values against remaining buffer space. Use a helper function for safe string building.

## 2025-12-12 - [Stack Exhaustion in Tasks]
**Vulnerability:** Large buffers (e.g., 2KB) allocated on the stack in FreeRTOS tasks with limited stack size (e.g., 4KB) can lead to stack overflows, causing crashes or undefined behavior.
**Learning:** Embedded tasks have limited stack space. Large buffers should always be heap-allocated to prevent stack overflows, especially when using stack-heavy functions like `vsnprintf`.
**Prevention:** Use `malloc`/`free` or `std::vector` for buffers larger than 256-512 bytes in tasks.

## 2025-05-18 - [Network Stack Stall via Blocking Queue Send]
**Vulnerability:** The UDP packet receiver used `xQueueSend(..., portMAX_DELAY)` within the lwIP callback context. If the processing task was slow or the queue was full, the call would block indefinitely, stalling the entire TCP/IP thread and causing a system-wide Denial of Service (DoS).
**Learning:** Never use blocking calls (like `portMAX_DELAY`) in interrupt handlers or high-priority system callbacks (like lwIP).
**Prevention:** Use a timeout of `0` in `xQueueSend` within callbacks. It is better to drop a packet than to crash the network stack.

## 2025-02-18 - [JSON Injection in SysInfo API]
**Vulnerability:** The `/sysinfo.json` endpoint used `snprintf` to construct a JSON response using string values from various sources (GitHub API, Hardware, System) without escaping them. This allowed injection of arbitrary JSON fields or corruption of the JSON structure if the input contained double quotes or backslashes.
**Learning:** Even "trusted" sources like GitHub API (release tags) or hardware identifiers should be treated as untrusted input when constructing structured data like JSON or SQL. Optimizations that bypass standard serialization libraries (like `cJSON`) must include manual sanitation.
**Prevention:** Use a helper function to escape JSON-sensitive characters (`"`, `\`, control chars) before using strings in manual JSON construction, or stick to standard libraries despite minor performance overhead. I implemented a `escape_json_string` helper to maintain the performance benefit of `snprintf` while securing the output.
