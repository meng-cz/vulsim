#pragma once

#include <defhelper.hpp>

#include "header.hpp"

REGISTER(cycle, uint32_t) {
    cycle = 0;
}

REQUEST(print, ARG(uint32_t) data);

CHILD_INSTANCE_ARRAY1(LineNode, lane, LEN);

USE_CHILD_SERVICE_PORT(lane[0], left_in, lane0_in);

CONNECT_CR_CS(lane[$], right_out, lane[$+1], left_in);
CONNECT_CR_R(lane[LEN-1], right_out, print);

TICK_IMPL() {
    if (cycle == 0) {
        lane0_in(5);
    }
    cycle.setnext(cycle + 1);
}
