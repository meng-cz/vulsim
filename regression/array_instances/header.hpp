#pragma once
#include <defhelper.hpp>
STRUCT(ArraySnapshot) {
    uint32_t first[8];
    uint32_t second[4];
    uint32_t streamed[8];
};
