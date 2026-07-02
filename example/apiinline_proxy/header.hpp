#pragma once

#include <array>
#include <cstdint>
#include <defhelper.hpp>

CONFIG(PROXY_MEM_SIZE, 8);

STRUCT(ProxyMeta) {
    uint8_t tag;
    bool valid;
};

STRUCT(ProxyPayload) {
    uint32_t data;
    ProxyMeta meta;
};

ALIAS_ARRAY1(PayloadPair, ProxyPayload, 2);
