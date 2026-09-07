#pragma once

#include <defhelper.hpp>

REQUEST(mixed,
    ARG(uint32_t) lhs,
    RESP(uint32_t) result,
    ARG(uint32_t) rhs);

REQUEST(output, ARG(uint32_t) value);

TICK_IMPL() {
    uint32_t lhs = 11;
    uint32_t rhs = 22;
    uint32_t result = 0;

    mixed(lhs, result, rhs);
    output(result);
}
