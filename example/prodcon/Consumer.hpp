
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

REQUEST(output, ARG(uint8_t) s);

SERVICE_READY(recv, (cycle & 1) == 0, ARG(uint8_t) d) {
    sum_setnext(sum + d);
    output(sum);
}

// tick

TICK_IMPL() {
    cycle_setnext(cycle + 1);
}

