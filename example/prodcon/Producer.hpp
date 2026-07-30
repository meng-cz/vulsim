
#pragma once

#include <defhelper.hpp>

#include "header.hpp"

INTERFACE() {
    REQUEST(send, handshake=1, ARG(uint8_t) d);
}

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

// Logic block

TICK_IMPL() {
    cycle.setnext(cycle + 1);
    if (send(cycle)) {
        count.setnext(count + 1);
    }
}
