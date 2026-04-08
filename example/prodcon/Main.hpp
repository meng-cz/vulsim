
#pragma once

#include <defhelper.hpp>
#include <run.hpp>

#include "header.hpp"

// Port

SERVICE_PORT(output, void, ARG(uint8_t) s);

// Variables

VAR_INIT(uint64_t, tick_count, 0);

// Functions

SERVICE_LOGIC_IMPL(output, ARG(uint8_t) s) {
    printf("%ld: Output: %u\n", tick_count, s);
}

SIMULATION() {
    for (tick_count = 0; tick_count < 20; ++tick_count) {
        sim_execute();
        sim_commit();
    }
}



