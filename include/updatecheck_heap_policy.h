/*
 *  updatecheck_heap_policy.h is part of the HB-RF-ETH firmware v2.0
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

// Admission policy for the outbound update-manifest fetch.
//
// Header-only and free of ESP-IDF dependencies so the exact boundaries are
// unit-tested on the host in CI (test/host/test_updatecheck_heap_policy.cpp).
// The values are not invented: they were calibrated against a live device
// under an active CCU session, which reported free=55 KiB / largest=32 KiB - a
// perfectly healthy state that an earlier one-dimensional 56-KiB rule rejected,
// leaving the WebUI to claim "no update" when it had in fact never looked.
//
// Two dimensions matter independently. Total free memory bounds whether a TLS
// session fits at all; the largest contiguous block decides whether it can
// actually be allocated. A fragmented heap with plenty of total space fails a
// handshake just as reliably as an exhausted one, so neither figure alone is a
// usable gate.
//
// Hence: a nominal case with a modest block requirement, plus a compensated
// case that admits slightly less total memory when the heap is notably less
// fragmented. Both branches sit above the floors below.
//
// Rejecting is never silent. The caller records the reason and surfaces it, so
// "skipped, too little memory" can never again be displayed as "up to date".

// Nominal: comfortable total, ordinary fragmentation.
#define UPDATE_CHECK_NOMINAL_FREE_BYTES  (56u * 1024u)
#define UPDATE_CHECK_NOMINAL_BLOCK_BYTES (18u * 1024u)

// Compensated: less total memory, but a markedly larger contiguous block.
#define UPDATE_CHECK_COMPENSATED_FREE_BYTES  (52u * 1024u)
#define UPDATE_CHECK_COMPENSATED_BLOCK_BYTES (28u * 1024u)

/**
 * @brief Decide whether a manifest fetch may start.
 *
 * @param free_bytes    Total free heap.
 * @param largest_block Largest allocatable contiguous block.
 * @return true when the fetch is admitted.
 */
static inline bool update_check_heap_allows(size_t free_bytes, size_t largest_block)
{
    const bool nominal = free_bytes >= UPDATE_CHECK_NOMINAL_FREE_BYTES &&
                         largest_block >= UPDATE_CHECK_NOMINAL_BLOCK_BYTES;
    const bool compensated = free_bytes >= UPDATE_CHECK_COMPENSATED_FREE_BYTES &&
                             largest_block >= UPDATE_CHECK_COMPENSATED_BLOCK_BYTES;
    return nominal || compensated;
}

// A device whose steady state sits below the gate is not out of luck: the
// monitoring workers hold the TLS sessions and task stacks that account for
// most of a fragmented heap, and the firmware already has a transaction to
// stop them (the same one the OTA path uses before a restart). Releasing
// them for the duration of one manual manifest fetch turns states like the
// issue #462 report (free=51 KiB / largest=27 KiB, one KiB short of both
// branches) into an admitted fetch, without weakening the floors for a
// fetch that would then fail mid-handshake.
//
// Hence the decision is two-phase. PROCEED starts the fetch. RELEASE_FIRST
// instructs the caller to stop the monitoring workers once and re-measure;
// the caller may also learn the workers are already stopped by another
// transaction, which counts as released but is not the caller's to undo.
// REFUSE is final and must carry the measured figures, so the WebUI can show
// why nothing was fetched.

typedef enum
{
    UPDATE_CHECK_HEAP_PROCEED = 0,
    UPDATE_CHECK_HEAP_RELEASE_FIRST,
    UPDATE_CHECK_HEAP_REFUSE
} update_check_heap_decision_t;

/**
 * @brief Plan the admission of a manifest fetch for the given heap figures.
 *
 * @param free_bytes          Total free heap.
 * @param largest_block       Largest allocatable contiguous block.
 * @param monitoring_released true once the monitoring workers have been
 *                            stopped (by this caller or another transaction)
 *                            and the heap has had time to settle.
 * @return PROCEED when the fetch may start, RELEASE_FIRST to stop the
 *         monitoring workers and ask again, REFUSE when the fetch stays
 *         below the gate even after a release.
 */
static inline update_check_heap_decision_t update_check_heap_decide(size_t free_bytes,
                                                                    size_t largest_block,
                                                                    bool monitoring_released)
{
    if (update_check_heap_allows(free_bytes, largest_block)) {
        return UPDATE_CHECK_HEAP_PROCEED;
    }
    return monitoring_released ? UPDATE_CHECK_HEAP_REFUSE : UPDATE_CHECK_HEAP_RELEASE_FIRST;
}
