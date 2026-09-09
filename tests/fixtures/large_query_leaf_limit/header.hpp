#pragma once

#include <cstdint>
#include <defhelper.hpp>

CONFIG(REG_COUNT, 32);
CONFIG(BACKGROUND_COUNT, 150);

STRUCT(RegisterFileSnapshot) {
    uint64_t values[REG_COUNT];
};
