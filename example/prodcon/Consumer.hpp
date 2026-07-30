
#pragma once

#include <defhelper.hpp>

#include "header.hpp"

INTERFACE() {
    REQUEST(output, ARG(uint8_t) s);
    SERVICE(recv, handshake=1, ARG(uint8_t) d);
}

USE_VERSION(even_ready);

VERSION(even_ready) {
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

SERVICE(recv, handshake=1, ready=((cycle & 1) == 0), ARG(uint8_t) d) {
    sum.setnext(sum + d);
    output(sum);
}

// tick

TICK_IMPL() {
    cycle.setnext(cycle + 1);
}
}

VERSION(always_ready) {
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

SERVICE(recv, handshake=1, ready=true, ARG(uint8_t) d) {
    sum.setnext(sum + d);
    output(sum);
}

// tick

TICK_IMPL() {
    cycle.setnext(cycle + 1);
}
}
