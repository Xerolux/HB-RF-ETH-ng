#ifndef SECURE_UTILS_H
#define SECURE_UTILS_H

#include <stddef.h>

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

#endif // SECURE_UTILS_H
