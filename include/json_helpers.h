#pragma once
#include <string.h>
#include <stdio.h>

/**
 * Escapes a string for use in a JSON value.
 *
 * @param input The input string to escape.
 * @param output The buffer to write the escaped string to.
 * @param output_size The size of the output buffer.
 * @return The number of characters written (excluding null terminator), or -1 if buffer is too small/invalid.
 *         On error/overflow, the output buffer is guaranteed to be null-terminated (possibly truncated or empty).
 */
static inline int escape_json_string(const char *input, char *output, size_t output_size) {
    if (!output || output_size == 0) return -1;
    output[0] = '\0'; // Ensure empty string on error start
    if (!input) return 0;

    size_t written = 0;
    for (size_t i = 0; input[i] != '\0'; i++) {
        char c = input[i];
        const char *esc = NULL;
        char hex_buf[7];

        switch (c) {
            case '\"': esc = "\\\""; break;
            case '\\': esc = "\\\\"; break;
            case '\b': esc = "\\b"; break;
            case '\f': esc = "\\f"; break;
            case '\n': esc = "\\n"; break;
            case '\r': esc = "\\r"; break;
            case '\t': esc = "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) {
                    snprintf(hex_buf, sizeof(hex_buf), "\\u%04x", (unsigned char)c);
                    esc = hex_buf;
                }
                break;
        }

        size_t len = (esc) ? strlen(esc) : 1;

        // Check for overflow (leaving space for null terminator)
        if (written + len >= output_size) {
            output[written] = '\0'; // Null-terminate what we have
            return -1;
        }

        if (esc) {
            strcpy(output + written, esc);
            written += len;
        } else {
            output[written++] = c;
        }
    }

    output[written] = '\0';
    return written;
}
