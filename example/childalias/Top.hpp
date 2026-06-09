#pragma once

#include <defhelper.hpp>

#include "header.hpp"

REQUEST(output, ARG(uint32_t) data);

CHILD_INSTANCE(IncNode, node);

USE_CHILD_SERVICE_PORT(node, inc, node_inc);

TICK_IMPL() {
    uint32_t out = 0;
    node_inc(41, out);
    output(out);
}
