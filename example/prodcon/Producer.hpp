
#pragma once

#include <defhelper.hpp>

#include "header.hpp"

// Parameter

// Struct

// Register

REGISTER(cycle, uint8_t) {
    cycle = 0;
}
REGISTER(count, uint8_t) {
    count = 0;
}

// Port

REQUEST_PORT(send, bool, ARG(uint8_t) d);

// Logic block

TICK_IMPL() {
    cycle_setnext(cycle + 1);
    if (send(cycle)) {
        count_setnext(count + 1);
    }
}

