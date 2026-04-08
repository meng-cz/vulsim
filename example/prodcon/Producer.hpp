
#pragma once

#include <defhelper.hpp>

#include "header.hpp"

// Parameter

// Struct

// Register

REGISTER_INIT(cycle, uint8_t, 0);
REGISTER_INIT(count, uint8_t, 0);

// Port

REQUEST_PORT(send, bool, ARG(uint8_t) d);

// Logic block

TICK_IMPL() {
    cycle_setnext(cycle + 1);
    if (send(cycle)) {
        count_setnext(count + 1);
    }
}

