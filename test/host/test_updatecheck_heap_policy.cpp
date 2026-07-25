#include <cassert>
#include <cstddef>

#include "updatecheck_heap_policy.h"

int main()
{
    constexpr std::size_t KB = 1024;

    // Preserve the established nominal boundary.
    static_assert(update_check_heap_is_safe(56 * KB, 18 * KB));

    // A slightly lower total is safe when fragmentation is substantially
    // better. This is the exact class of live-device state that previously
    // rejected a manual update check at 55 KB / 32 KB.
    static_assert(update_check_heap_is_safe(55 * KB, 32 * KB));

    // Neither a healthy largest block nor ample total memory may compensate
    // for crossing an absolute floor.
    static_assert(!update_check_heap_is_safe(51 * KB, 32 * KB));
    static_assert(!update_check_heap_is_safe(64 * KB, 17 * KB));

    // Below the nominal total, the stronger contiguous-block requirement is
    // mandatory.
    static_assert(!update_check_heap_is_safe(55 * KB, 27 * KB));

    assert(update_check_heap_is_safe(55 * KB, 32 * KB));
    return 0;
}
