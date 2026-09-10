#pragma once

// Percent-decoding for URL query values. ESP-IDF's httpd_query_key_value()
// returns the raw query string slice without decoding, but browsers send
// values via encodeURIComponent() — a base64 admin token with '+', '/' and '='
// arrives as %2B, %2F and %3D and would never compare equal to the stored
// token. Kept header-only so the host unit tests can exercise it directly.

#ifdef __cplusplus
extern "C" {
#endif

static inline char url_hex_nibble(char c)
{
    if (c >= 'a') return (char)(c - 'a' + 10);
    if (c >= 'A') return (char)(c - 'A' + 10);
    return (char)(c - '0');
}

static inline int url_is_hex_digit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// Decodes %XX sequences in place. The result is never longer than the input.
// '+' is passed through unchanged: encodeURIComponent() encodes spaces as
// %20, so there is no application/x-www-form-urlencoded space folding to
// undo here. Invalid or truncated escapes are copied verbatim, leaving the
// value as-is (a later comparison then simply fails closed).
static inline void url_decode_in_place(char *value)
{
    if (!value) return;

    char *write = value;
    for (const char *read = value; *read; ++read) {
        if (read[0] == '%' && url_is_hex_digit(read[1]) && url_is_hex_digit(read[2])) {
            *write++ = (char)((url_hex_nibble(read[1]) << 4) | url_hex_nibble(read[2]));
            read += 2;
        } else {
            *write++ = *read;
        }
    }
    *write = '\0';
}

#ifdef __cplusplus
}
#endif
