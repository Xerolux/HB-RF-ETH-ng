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
 * @brief Secure string comparison to prevent timing attacks
 *
 * @param a First string
 * @param b Second string
 * @return 0 if strings are equal, non-zero otherwise
 */
static inline int secure_strcmp(const char *a, const char *b) {
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    size_t min_len = len_a < len_b ? len_a : len_b;
    int result = len_a - len_b;

    for (size_t i = 0; i < min_len; i++) {
        result |= (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
    }

    return result;
}

#endif // SECURE_UTILS_H
