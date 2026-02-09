#ifndef SECURE_UTILS_H
#define SECURE_UTILS_H

#include <stddef.h>
#include <string.h>

/**
 * @brief Securely zeros out memory to prevent compiler optimization
 *
 * @param v Pointer to memory to zero
 * @param n Number of bytes to zero
 */
static inline void secure_zero(void *v, size_t n) {
    volatile unsigned char *p = (volatile unsigned char *)v;
    while (n--) *p++ = 0;
}

/**
 * @brief Constant-time string comparison to prevent timing attacks
 *
 * @param a First string (NULL-safe)
 * @param b Second string (NULL-safe)
 * @return 0 if strings are equal, non-zero otherwise
 */
static inline int secure_strcmp(const char *a, const char *b) {
    if (!a || !b) return 1;

    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    size_t min_len = len_a < len_b ? len_a : len_b;
    int result = 0;

    if (len_a != len_b) {
        result = 1;
    }

    for (size_t i = 0; i < min_len; i++) {
        result |= (a[i] ^ b[i]);
    }

    return result;
}

#endif // SECURE_UTILS_H
