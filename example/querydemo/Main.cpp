#include <defhelper.hpp>
#include <run.hpp>

GLOBAL() {
    uint64_t tick_count = 0;
}

SERVICE(output, ARG(uint32_t) data) {
    printf("%lu: querydemo output=%u\n", tick_count, data);
}

SIMULATION() {
    for (tick_count = 0; tick_count < 8; ++tick_count) {
        sim_nextcycle();
    }
}
