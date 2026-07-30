
#pragma once

#include <defhelper.hpp>

#include "header.hpp"

INTERFACE() {
    REQUEST(output, ARG(uint8_t) s);
}

// Parameter

// Struct

// Register

// Port

// Logic block

// Child instance

CHILD_INSTANCE(Producer, prod);
CHILD_INSTANCE(Consumer, cons);

CONNECT_CR_CS(prod, send, cons, recv);

CONNECT_CR_R(cons, output, output);
