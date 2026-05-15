
#pragma once

#include <defhelper.hpp>

CONST(QUEUE_SIZE, 8);

QUEUE(q, uint32_t, QUEUE_SIZE);

SERVICE_READY(deq, q.deqvalid(), RESP(uint32_t) data) {
    data = q.deqnext();
}

REGISTER(cycle, uint32_t) {
    cycle = 0;
}

TICK_IMPL() {
    cycle.setnext(cycle + 1);
    if (cycle % 2 == 0) {
        q.enqnext(cycle);
    }
}


