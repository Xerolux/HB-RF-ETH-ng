/*
 *  secure_utils.h is part of the HB-RF-ETH firmware v2.0
 *
 *  Original work Copyright 2022 Alexander Reinert
 *  https://github.com/alexreinert/HB-RF-ETH
 *
 *  Modified work Copyright 2025 Xerolux
 *  Modernized fork - Updated to ESP-IDF 6.0 and modern toolchains
 *
 *  The HB-RF-ETH firmware is licensed under a
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 *
 *  You should have received a copy of the license along with this
 *  work.  If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

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
 * @brief Constant-time string comparison to prevent timing attacks.
 *
 * The comparison never short-circuits on the first mismatching byte. Like any
 * C-string comparison its runtime may reveal the public string lengths, but it
 * does not read beyond either terminating NUL byte.
 *
 * @param a First string (NULL-safe)
 * @param b Second string (NULL-safe)
 * @return 0 if strings are equal, non-zero otherwise
 */
static inline int secure_strcmp(const char *a, const char *b) {
    if (!a || !b) return 1;

    const size_t a_len = strlen(a);
    const size_t b_len = strlen(b);
    const size_t max_len = a_len > b_len ? a_len : b_len;
    size_t result = a_len ^ b_len;

    // Use zero for positions beyond the shorter string rather than indexing
    // past its terminator. The old lock-step loop kept dereferencing the short
    // input until the long input ended, which was undefined behaviour and
    // could fault when a credential ended at a memory-region boundary.
    for (size_t i = 0; i < max_len; ++i) {
        const unsigned char ca =
            i < a_len ? (unsigned char)a[i] : 0U;
        const unsigned char cb =
            i < b_len ? (unsigned char)b[i] : 0U;
        result |= (size_t)(ca ^ cb);
    }

    return result == 0 ? 0 : 1;
}

#endif // SECURE_UTILS_H
