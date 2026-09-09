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

    return 0;
}
