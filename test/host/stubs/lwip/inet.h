#pragma once

#include <cstdint>

#define IPADDR_TYPE_V4 0

struct ip4_addr_stub_t {
    std::uint32_t addr;
};

struct ip_addr_t {
    union {
        ip4_addr_stub_t ip4;
    } u_addr;
    std::uint8_t type;
};
