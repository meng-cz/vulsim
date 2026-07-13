#pragma once

#include <cstdint>
#include <defhelper.hpp>

CONFIG(PIPE_DEPTH, 3);

STRUCT(PipeStage) {
    bool valid;
    uint32_t data;
};

STRUCT(PipelineSnapshot) {
    bool s0_valid;
    uint32_t s0_data;
    bool s1_valid;
    uint32_t s1_data;
    bool s2_valid;
    uint32_t s2_data;
};

