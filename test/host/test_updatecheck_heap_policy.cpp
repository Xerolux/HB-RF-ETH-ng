#include "updatecheck_heap_policy.h"

#include <cassert>

// The four boundary cases below are the ones that matter in the field, not
// invented numbers. A live device under an active CCU session reported
// free=55 KiB / largest=32 KiB and was wrongly rejected by an earlier
// one-dimensional rule; the WebUI then showed "no update found" for a check
// that had never run. That exact state must be admitted, and the three
// near-misses around it must still be refused.

static constexpr size_t kib(size_t value)
{
    return value * 1024u;
}

int main()
{
    // The regression case: healthy total, low fragmentation. Must be admitted.
    assert(update_check_heap_allows(kib(55), kib(32)));

    // Too little total memory, however contiguous the heap is.
    assert(!update_check_heap_allows(kib(51), kib(32)));

    // Plenty of total memory, but no block large enough for a TLS session.
    assert(!update_check_heap_allows(kib(64), kib(17)));

    // Below the nominal total and below the compensated block requirement:
    // neither branch applies.
    assert(!update_check_heap_allows(kib(55), kib(27)));

    // Both branches exactly at their boundary are admitted.
    assert(update_check_heap_allows(kib(56), kib(18)));
    assert(update_check_heap_allows(kib(52), kib(28)));

    // One byte under either boundary is not.
    assert(!update_check_heap_allows(kib(56) - 1, kib(18)));
    assert(!update_check_heap_allows(kib(56), kib(18) - 1));
    assert(!update_check_heap_allows(kib(52) - 1, kib(28)));
    assert(!update_check_heap_allows(kib(52), kib(28) - 1));

    // A block can never exceed total free memory, but the policy must not
    // depend on the caller enforcing that.
    assert(!update_check_heap_allows(0, 0));
    assert(update_check_heap_allows(kib(200), kib(200)));

    // --- update_check_heap_decide -----------------------------------------
    //
    // The issue #462 case: a device in a perfectly ordinary steady state
    // (free=51 KiB / largest=27 KiB, one KiB short of both branches) used to
    // be refused for good, with no way for the user to ever get past the
    // message. The decision is therefore two-phase: an unreleased heap gets
    // one chance to release the monitoring workers before the refusal is
    // final, and a healthy device is never disturbed at all.

    // The issue #462 steady state: not admitted, but the monitoring workers
    // have not been released yet - the fetch must ask for a release first.
    assert(update_check_heap_decide(kib(51), kib(27), false) == UPDATE_CHECK_HEAP_RELEASE_FIRST);

    // The same figures after a release that did not free enough: now the
    // refusal is final and carries the measured figures.
    assert(update_check_heap_decide(kib(51), kib(27), true) == UPDATE_CHECK_HEAP_REFUSE);

    // A healthy device (the calibration regression case) proceeds without
    // any release - a manual check must not stop MQTT for nothing.
    assert(update_check_heap_decide(kib(55), kib(32), false) == UPDATE_CHECK_HEAP_PROCEED);

    // Exactly at a gate boundary: proceeds even before any release.
    assert(update_check_heap_decide(kib(52), kib(28), false) == UPDATE_CHECK_HEAP_PROCEED);
    assert(update_check_heap_decide(kib(56), kib(18), false) == UPDATE_CHECK_HEAP_PROCEED);

    // A release that freed enough room admits the fetch.
    assert(update_check_heap_decide(kib(60), kib(30), true) == UPDATE_CHECK_HEAP_PROCEED);

    // Trivial figures never proceed, with or without a release.
    assert(update_check_heap_decide(0, 0, false) == UPDATE_CHECK_HEAP_RELEASE_FIRST);
    assert(update_check_heap_decide(0, 0, true) == UPDATE_CHECK_HEAP_REFUSE);

    return 0;
}
