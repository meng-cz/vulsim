#pragma once

#include "header.hpp"

REGISTER_ARRAY1(pipe, PipeStage, PIPE_DEPTH, 1) {
    for (int i = 0; i < PIPE_DEPTH; ++i) {
        pipe[i].valid = false;
        pipe[i].data = 0;
    }
}

SERVICE(step, ARG(bool) in_valid, ARG(uint32_t) in_data, ARG(bool) stall, ARG(bool) flush) {
    PipeStage next0;
    next0.valid = in_valid;
    next0.data = in_data;

    pipe.setnext<0>(2, pipe[1]);
    pipe.setnext<0>(1, pipe[0]);
    pipe.setnext<0>(0, next0);

    if (stall) {
        pipe.holdnext();
    }
    if (flush) {
        pipe.resetnext();
    }
}

QUERY(snapshot, PipelineSnapshot) {
    PipelineSnapshot result;
    result.s0_valid = pipe[0].valid;
    result.s0_data = pipe[0].data;
    result.s1_valid = pipe[1].valid;
    result.s1_data = pipe[1].data;
    result.s2_valid = pipe[2].valid;
    result.s2_data = pipe[2].data;
    return result;
}

TICK_IMPL() {
}

