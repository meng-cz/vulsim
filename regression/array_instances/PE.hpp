#pragma once
#include "header.hpp"
INTERFACE() {
    PARAMETER(ROW, 3);
    SERVICE(submit, ARG(uint32_t) data);
    REQUEST(stream, ARG(uint32_t) data);
}
CHILD_INSTANCE(FPU, fpu, PARAM(WIDTH)=ROW+9, PARAM(DEPTH)=ROW+1);
USE_CHILD_SERVICE(fpu, load, load, ARG(uint32_t) data);
USE_CHILD_QUERY(fpu, result, child_result, uint32_t);
SERVICE(submit, ARG(uint32_t) data) { load(data); }
QUERY(result, uint32_t) { return child_result(); }
TICK_IMPL() { stream(child_result()); }
