
#include <defhelper.hpp>
#include <run.hpp>

#include "header.hpp"

// Variables

GLOBAL() {
    uint64_t tick_count = 0;
}

// Functions

SERVICE(output, ARG(uint8_t) s) {
    printf("%ld: Output: %u\n", tick_count, s);
}

SIMULATION() {
    for (tick_count = 0; tick_count < 20; ++tick_count) {
        sim_execute();
        sim_commit();
    }
}



