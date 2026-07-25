#pragma once

#include <stddef.h>

// A manifest check creates a temporary 9 KB worker and then opens one
// serialized HTTPS/TLS connection. The established nominal admission boundary
// remains 56 KB total plus an 18 KB largest block. A slightly lower total is
// accepted only when the heap is substantially less fragmented, giving the
// worker/TLS allocations a contiguous block with compensating headroom.
constexpr size_t UPDATE_CHECK_NOMINAL_FREE_HEAP = 56 * 1024;
constexpr size_t UPDATE_CHECK_MIN_FREE_HEAP = 52 * 1024;
constexpr size_t UPDATE_CHECK_MIN_LARGEST_BLOCK = 18 * 1024;
constexpr size_t UPDATE_CHECK_COMPENSATING_LARGEST_BLOCK = 28 * 1024;

constexpr bool update_check_heap_is_safe(size_t free_heap,
                                         size_t largest_block)
{
    const bool nominal_capacity =
        free_heap >= UPDATE_CHECK_NOMINAL_FREE_HEAP &&
        largest_block >= UPDATE_CHECK_MIN_LARGEST_BLOCK;
    const bool low_fragmentation_compensation =
        free_heap >= UPDATE_CHECK_MIN_FREE_HEAP &&
        largest_block >= UPDATE_CHECK_COMPENSATING_LARGEST_BLOCK;
    return nominal_capacity || low_fragmentation_compensation;
}
