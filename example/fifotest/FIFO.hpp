
#pragma once

#include <defhelper.hpp>

INTERFACE() {
    SERVICE(deq, handshake=1, RESP(uint32_t) data);
}

CONFIG(QUEUE_SIZE, 8);

QUEUE(q, uint32_t, QUEUE_SIZE);

SERVICE(deq, handshake=1, ready=q.deqvalid(), RESP(uint32_t) data) {
    data = q.front();
    q.deqnext();
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
