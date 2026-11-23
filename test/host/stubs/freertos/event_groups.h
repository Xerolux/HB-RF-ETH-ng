#pragma once

#include <cstddef>

#include "freertos/FreeRTOS.h"

struct StubEventGroup;
using EventGroupHandle_t = StubEventGroup *;
using EventBits_t = std::uint32_t;

// Opaque application-allocated storage, like the real StaticEventGroup_t. The
// test decides what xEventGroupCreateStatic() constructs inside it.
struct StaticEventGroup_t {
    alignas(std::max_align_t) unsigned char storage[256];
};

EventGroupHandle_t xEventGroupCreate();
EventGroupHandle_t xEventGroupCreateStatic(StaticEventGroup_t *storage);
EventBits_t xEventGroupSetBits(EventGroupHandle_t group, EventBits_t bits);
EventBits_t xEventGroupWaitBits(EventGroupHandle_t group,
                                EventBits_t bits_to_wait_for,
                                BaseType_t clear_on_exit,
                                BaseType_t wait_for_all_bits,
                                TickType_t ticks_to_wait);
void vEventGroupDelete(EventGroupHandle_t group);
