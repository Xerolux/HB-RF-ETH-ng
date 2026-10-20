/*
 *  update_url_policy.h is part of the HB-RF-ETH firmware v2.0
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

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>

// Host allowlist for online-install downloads (docs/AUTO_UPDATE_PLAN.md rule 7).
//
// The install endpoint never accepts a URL from the caller; the only URL it
// fetches comes from the device manifest, which scripts/device_manifest.py
// already restricts to the same allowlist at generation time. This header
// re-checks the rule on the device so a manifest that somehow slipped past
// the generator cannot point the device at a third-party host.
//
// Mirrors of the same rule, kept in step:
//   - scripts/device_manifest.py (ALLOWED_HOST_SUFFIXES, generation time)
//   - test/host/test_update_url_policy.cpp (boundary cases)
//
// Header-only and ESP-IDF-free so it is unit-testable on the host in CI.

static inline bool update_url_policy_allows(const char *url)
{
    if (url == NULL) return false;
    if (strncmp(url, "https://", 8) != 0) return false;

    // The authority ends at the first '/' (no '?' before it is possible in a
    // URL without a path; a '?' directly after the host still ends it).
    const char *authority = url + 8;
    size_t authority_len  = 0;
    while (authority[authority_len] != '\0' && authority[authority_len] != '/' &&
           authority[authority_len] != '?' && authority[authority_len] != '#') {
        authority_len++;
    }

    // userinfo would let "https://github.com@evil.example" read as trusted at
    // a glance, so the host is whatever follows the last '@'.
    size_t host_start = 0;
    for (size_t i = 0; i < authority_len; i++) {
        if (authority[i] == '@') host_start = i + 1;
    }
    if (host_start >= authority_len) return false;

    const size_t host_len = authority_len - host_start;

    // An explicit port never appears in a URL this generator produced.
    for (size_t i = host_start; i < authority_len; i++) {
        if (authority[i] == ':') return false;
    }

    static const char *const kAllowedSuffixes[] = {
        "github.com",
        "githubusercontent.com",
    };

    for (size_t s = 0; s < sizeof(kAllowedSuffixes) / sizeof(kAllowedSuffixes[0]); s++) {
        const size_t suffix_len = strlen(kAllowedSuffixes[s]);
        if (host_len == suffix_len &&
            strncasecmp(authority + host_start, kAllowedSuffixes[s], suffix_len) == 0) {
            return true;
        }
        // Subdomain: the suffix must be preceded by a dot, so
        // "github.com.evil.example" does not pass as github.com.
        if (host_len > suffix_len + 1 &&
            authority[host_start + host_len - suffix_len - 1] == '.' &&
            strncasecmp(authority + host_start + host_len - suffix_len, kAllowedSuffixes[s],
                        suffix_len) == 0) {
            return true;
        }
    }
    return false;
}
