
#pragma once

#include <defhelper.hpp>

REQUEST_READY(deq, RESP(uint32_t) data);

REQUEST(output, ARG(uint32_t) data);

REGISTER(cycle, uint32_t) {
    cycle = 0;
}

TICK_IMPL() {
    cycle.setnext(cycle + 1);
    if ((cycle >> 1) % 2 == 0) {
        uint32_t enq_data = 0;
        if (deq(enq_data)) {
            output(enq_data);
        }
    }
}

