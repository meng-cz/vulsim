#include <defhelper.hpp>
#include <run.hpp>

GLOBAL() {
    uint32_t tick = 0;
}

SERVICE(print, ARG(uint32_t) data) {
    printf("array1d tick=%u data=%u\n", tick, data);
}

SIMULATION() {
    for (tick = 0; tick < 3; ++tick) {
        sim_execute();
        sim_commit();
    }
}
