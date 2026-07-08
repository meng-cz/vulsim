
#include <defhelper.hpp>
#include <run.hpp>

GLOBAL() {

uint64_t test_tick = 0;

}

SERVICE(output, ARG(uint32_t) data) {
    printf("%ld: Output data: %u\n", test_tick, data);
}

SIMULATION() {
    constexpr uint64_t TestTick = 20;
    for (test_tick = 0; test_tick < TestTick; ++test_tick) {
        sim_nextcycle();
    }
}

