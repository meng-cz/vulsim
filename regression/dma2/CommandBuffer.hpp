#pragma once
#include "header.hpp"
PARAMETER(DEPTH, 2);
QUEUE(q, DmaCommand, DEPTH);
REGISTER(enqueued, uint32_t);
REGISTER(popped, uint32_t);
SERVICE(submit, ready=q.enqready(), ARG(DmaCommand) command) {
    q.enqnext(command);
    enqueued.setnext(enqueued + 1);
}
SERVICE(pop, RESP(DmaCommand) command) {
    command = q.front();
    q.deqnext();
    popped.setnext(popped + 1);
}
QUERY(status, BufferStatus) {
    BufferStatus s{};
    s.depth = DEPTH; s.enqueued = enqueued; s.popped = popped;
    s.can_push = q.enqready(); s.can_pop = q.deqvalid();
    return s;
}
