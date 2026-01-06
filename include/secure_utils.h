#pragma once

#include <stddef.h>

/**
 * Securely clears memory to prevent sensitive data leakage.
 * Uses a volatile pointer to prevent the compiler from optimizing away the write.
 */
static inline void secure_zero(void *v, size_t n) {
    volatile unsigned char *p = (volatile unsigned char *)v;
    while (n--) *p++ = 0;
}
