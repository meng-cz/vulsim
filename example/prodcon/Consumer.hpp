
#pragma once

#include <defhelper.hpp>

#include "header.hpp"

// Parameter

// Struct

// Register

REGISTER(cycle, uint8_t) {
    cycle = 0;
}
REGISTER(sum, uint8_t) {
    sum = 0;
}

// Port

REQUEST_PORT(output, void, ARG(uint8_t) s);

SERVICE_PORT(recv, bool, ARG(uint8_t) d);

// Logic block

TICK_IMPL() {
    cycle_setnext(cycle + 1);
}

SERVICE_COND_IMPL(recv, ARG(uint8_t) d) {
    return (cycle & 1) == 0;
}

SERVICE_LOGIC_IMPL(recv, ARG(uint8_t) d) {
    sum_setnext(sum + d);
    output(sum);
}

