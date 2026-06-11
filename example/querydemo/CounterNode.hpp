#pragma once

#include <defhelper.hpp>

#include "header.hpp"

REGISTER(sum, uint32_t) {
    sum = 0;
}

QUEUE(history, uint32_t, 4);

SERVICE(push, ARG(uint32_t) data) {
    if (history.enqready()) {
        history.enqnext(data);
    }
    sum.setnext(sum + data);
}

QUERY(snapshot, ChildSnapshot) {
    ChildSnapshot value;
    value.sum = sum;
    value.can_pop = history.deqvalid();
    return value;
}
