
#pragma once

#include <defhelper.hpp>

ALIAS_ARRAY1(AESData, uint8_t, 16);
ALIAS_ARRAY1(AESKey, uint8_t, 16);

CONFIG(WAY, 4);
CONFIG(ADDRW, 8);
CONFIG(DATW, 64);
CONFIG(MEMSIZE, (1ULL << ADDRW) * WAY);

STRUCT(MemItem) {
    uint64_t addr;
    uint32_t tag;
    bool     valid;
};

ALIAS_ARRAY1(MemWay, MemItem, WAY);



