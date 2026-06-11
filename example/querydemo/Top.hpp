#pragma once

#include <defhelper.hpp>

#include "header.hpp"

REQUEST(output, ARG(uint32_t) data);

REGISTER(mirror, uint32_t) {
    mirror = 0;
}

CHILD_INSTANCE(CounterNode, node);

USE_CHILD_SERVICE_PORT(node, push, node_push, ARG(uint32_t) data);
USE_CHILD_QUERY(node, snapshot, node_snapshot, ChildSnapshot);

QUERY(snapshot, TopSnapshot) {
    TopSnapshot value;
    value.child = node_snapshot();
    value.mirror = mirror;
    value.mirror_even = ((mirror & 1U) == 0U);
    return value;
}

TICK_IMPL() {
    TopSnapshot snap = snapshot();
    if (snap.child.can_pop && snap.mirror_even) {
        output(snap.child.sum);
    }
    node_push(3);
    mirror.setnext(snap.child.sum);
}
