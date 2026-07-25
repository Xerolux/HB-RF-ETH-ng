#pragma once

#include <cstdint>

using TickType_t = std::uint32_t;
using BaseType_t = int;

#define BIT0 (1U << 0)
#define BIT1 (1U << 1)
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(ms) (static_cast<TickType_t>(ms))
