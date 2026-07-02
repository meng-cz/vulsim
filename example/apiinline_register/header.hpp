#pragma once

#include <cstdint>
#include <defhelper.hpp>

CONFIG(REG_ARRAY_SIZE, 4);

STRUCT(MetaInfo) {
    uint8_t tag;
    bool valid;
};

STRUCT(Payload) {
    uint32_t data;
    MetaInfo meta;
    bool flag;
};

STRUCT(RegisterSummary) {
    uint32_t scalar;
    uint32_t multi;
    uint16_t arr0;
    uint32_t payload_data;
    uint8_t payload_tag;
    bool payload_valid;
};
