#pragma once

#include "header.hpp"

QUEUE_MP(q, uint32_t, Q_DEPTH, Q_ENQ_WIDTH, Q_DEQ_WIDTH);

SERVICE(push, ARG(U32x3) values, ARG(uint32_t) num) {
    q.enqnext(values, num);
}

SERVICE(pop, ARG(uint32_t) num, RESP(uint32_t) valid_before, RESP(U32x2) values) {
    valid_before = q.deqvalid();
    values = q.front(num);
    q.deqnext(num);
}

SERVICE(pop_push,
        ARG(uint32_t) pop_num,
        ARG(U32x3) values,
        ARG(uint32_t) push_num,
        RESP(uint32_t) ready_before,
        RESP(uint32_t) valid_before,
        RESP(uint32_t) ready_after,
        RESP(U32x2) popped) {
    ready_before = q.enqreqdy();
    valid_before = q.deqvalid();
    popped = q.front(pop_num);
    q.deqnext(pop_num);
    ready_after = q.enqreqdy();
    q.enqnext(values, push_num);
}

SERVICE(clear_push, ARG(U32x3) values, ARG(uint32_t) num) {
    q.clrnext();
    q.enqnext(values, num);
}

QUERY(status, QueueMPStatus) {
    QueueMPStatus s;
    s.enq_rdy = q.enqreqdy();
    s.deq_valid = q.deqvalid();
    return s;
}
