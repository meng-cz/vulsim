#pragma once
#include "header.hpp"
INTERFACE() {
    PARAMETER(WIDTH, 13);
    PARAMETER(DEPTH, 2);
    PARAMETER(TAG, 23);
    PARAMETER(DERIVED, DEPTH + 3);
    SERVICE(load, ARG(uint32_t) data);
}
REGISTER(value, Int<WIDTH>) { value = TAG; }
QUEUE(queue, uint32_t, DEPTH);
SERVICE(load, ARG(uint32_t) data) {
    value.setnext(data);
    if (queue.enqready()) queue.enqnext(data);
}
QUERY(result, uint32_t) { return value.get().to<uint32_t>() + (DEPTH << 16) + (DERIVED << 24) + (queue.enqready() ? 0x80000000u : 0u); }
