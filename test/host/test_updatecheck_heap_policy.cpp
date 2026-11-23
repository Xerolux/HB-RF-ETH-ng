#include "updatecheck_heap_policy.h"

#include <cassert>

// The boundary cases below are the ones that matter in the field, not
// invented numbers. A live device under an active CCU session reported
// free=55 KiB / largest=32 KiB and was wrongly rejected by an earlier
// one-dimensional rule; the WebUI then showed "no update found" for a check
// that had never run. That exact state must be admitted.
//
// A second field finding shaped the compact branch: a fully equipped 2.4.1
// gateway (MQTT with Home Assistant discovery, external WebUI mounted, active
// CCU session) settled at free=52 KiB / largest=22 KiB after releasing its
// monitoring workers - missed by the nominal branch on the total axis and by
// the compensated branch on the block axis, so every update check was skipped
// for good. That exact state must now be admitted, and the near-misses around
// the floors must still be refused.

static constexpr size_t kib(size_t value)
{
    return value * 1024u;
}

int main()
{
    // --- update_check_heap_allows -----------------------------------------

    // The first calibration regression: healthy total, low fragmentation.
    // Must be admitted.
    assert(update_check_heap_allows(kib(55), kib(32)));

    // The second calibration regression: the fully equipped 2.4.1 field
    // gateway's steady state after releasing the monitoring workers.
    assert(update_check_heap_allows(kib(52), kib(22)));

    // Below the compact total floor, however contiguous the heap is.
    assert(!update_check_heap_allows(kib(47), kib(32)));

    // Plenty of total memory, but no block large enough for a TLS session.
    assert(!update_check_heap_allows(kib(64), kib(17)));

    // A near-miss under the compact branch on both axes stays refused.
    assert(!update_check_heap_allows(kib(47), kib(19)));

    // Each branch exactly at its boundary is admitted.
    assert(update_check_heap_allows(kib(56), kib(18)));
    assert(update_check_heap_allows(kib(52), kib(28)));
    assert(update_check_heap_allows(kib(48), kib(20)));

    // One byte under a branch boundary is refused where no other branch
    // covers the state: a 17 KiB block is too small for every branch, and
    // 55 KiB total misses both the nominal and the compact total floors.
    assert(!update_check_heap_allows(kib(56) - 1, kib(18)));
    assert(!update_check_heap_allows(kib(56), kib(18) - 1));
    assert(!update_check_heap_allows(kib(48) - 1, kib(20)));
    assert(!update_check_heap_allows(kib(48), kib(20) - 1));

    // The issue #496 report: a 2.4.1 gateway was refused for good at
    // free=49 KiB / largest=20 KiB, measured after the worker release. The
    // compact branch admits exactly that state, and one KiB less on either
    // axis stays refused.
    assert(update_check_heap_allows(kib(49), kib(20)));
    assert(!update_check_heap_allows(kib(49), kib(20) - 1));
    assert(!update_check_heap_allows(kib(47), kib(20)));

    // The compensated near-misses are deliberate overlap, not accidents:
    // once compact exists, a state like the issue #462 device (51/27) or a
    // one-byte-short compensated boundary (51/28, 52/27) is admitted, because
    // both axes still clear the compact floor with room for the handshake.
    assert(update_check_heap_allows(kib(51), kib(27)));
    assert(update_check_heap_allows(kib(51), kib(28)));
    assert(update_check_heap_allows(kib(52), kib(28) - 1));

    // A block can never exceed total free memory, but the policy must not
    // depend on the caller enforcing that.
    assert(!update_check_heap_allows(0, 0));
    assert(update_check_heap_allows(kib(200), kib(200)));

    // --- update_check_heap_decide -----------------------------------------
    //
    // A steady state below every branch is not final: the monitoring workers
    // hold the TLS sessions and task stacks that account for most of a
    // fragmented heap, so the caller gets one chance to release them and
    // re-measure (the same transaction the OTA path runs). A healthy device
    // is never disturbed at all.

    // The 2.4.1 field gateway before the release - free=34 KiB per its log;
    // the largest block was not recorded there, and any value below the gates
    // decides identically: not admitted, workers not yet released, so the
    // fetch must ask for a release first.
    assert(update_check_heap_decide(kib(34), kib(16), false) == UPDATE_CHECK_HEAP_RELEASE_FIRST);

    // After the release that same device settles at its logged 52/22 steady
    // state and is admitted - the outcome the release phase exists to reach.
    assert(update_check_heap_decide(kib(52), kib(22), true) == UPDATE_CHECK_HEAP_PROCEED);

    // A heap already inside the compact branch proceeds untouched: the
    // release phase must never stop MQTT for nothing.
    assert(update_check_heap_decide(kib(52), kib(22), false) == UPDATE_CHECK_HEAP_PROCEED);

    // The issue #462 device (free=51 KiB / largest=27 KiB, one KiB short of
    // both original branches) motivated the release phase and now clears the
    // compact branch outright.
    assert(update_check_heap_decide(kib(51), kib(27), false) == UPDATE_CHECK_HEAP_PROCEED);

    // The issue #496 device (free=49 KiB / largest=20 KiB) is admitted from
    // its steady state, so the search no longer depends on a release first.
    assert(update_check_heap_decide(kib(49), kib(20), false) == UPDATE_CHECK_HEAP_PROCEED);
    assert(update_check_heap_decide(kib(49), kib(20), true) == UPDATE_CHECK_HEAP_PROCEED);

    // Below the compact floor the release phase still has a job: refuse only
    // once the workers have been released and the heap did not recover.
    assert(update_check_heap_decide(kib(46), kib(19), false) == UPDATE_CHECK_HEAP_RELEASE_FIRST);
    assert(update_check_heap_decide(kib(46), kib(19), true) == UPDATE_CHECK_HEAP_REFUSE);

    // Healthy calibration states proceed without any release.
    assert(update_check_heap_decide(kib(55), kib(32), false) == UPDATE_CHECK_HEAP_PROCEED);
    assert(update_check_heap_decide(kib(52), kib(28), false) == UPDATE_CHECK_HEAP_PROCEED);
    assert(update_check_heap_decide(kib(56), kib(18), false) == UPDATE_CHECK_HEAP_PROCEED);

    // A release that freed enough room admits the fetch.
    assert(update_check_heap_decide(kib(60), kib(30), true) == UPDATE_CHECK_HEAP_PROCEED);

    // Trivial figures never proceed, with or without a release.
    assert(update_check_heap_decide(0, 0, false) == UPDATE_CHECK_HEAP_RELEASE_FIRST);
    assert(update_check_heap_decide(0, 0, true) == UPDATE_CHECK_HEAP_REFUSE);

    return 0;
}
