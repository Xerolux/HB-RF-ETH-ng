## 2025-12-12 - [DoS in OTA Handler]
**Vulnerability:** The OTA update handler unconditionally searches for `\r\n\r\n` (header terminator) in the request body to skip headers, but the frontend sends raw binary (`application/octet-stream`). If the binary data does not contain `\r\n\r\n` in the first chunk, `strstr` returns NULL, leading to a pointer arithmetic overflow and a crash (DoS).
**Learning:** Legacy code adapted for new purposes (multipart -> raw binary) can leave dangerous assumptions. Always verify if the data format matches the parsing logic.
**Prevention:** Verify Content-Type before parsing. For raw binary, do not try to parse headers.

## 2025-12-12 - [Timing Attack in Auth]
**Vulnerability:** The `validate_auth` function uses `strcmp` to compare the authentication token. `strcmp` returns early upon mismatch, allowing a timing attack to guess the token byte-by-byte.
**Learning:** Security-sensitive comparisons must be constant-time to avoid side-channel attacks.
**Prevention:** Use a constant-time comparison function (e.g., `sodium_memcmp` or a custom implementation) for secrets.
