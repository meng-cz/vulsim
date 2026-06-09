#include <defhelper.hpp>
#include <run.hpp>

GLOBAL() {
    uint32_t tick = 0;
}

SERVICE(print, ARRAY(HEI), ARG(uint32_t) data) {
    printf("systolic2d tick=%u row=%u data=%u\n", tick, IDX, data);
}

SERVICE(spill, ARG(uint32_t) data) {
    printf("systolic2d tick=%u spill=%u\n", tick, data);
}

SIMULATION() {
    for (tick = 0; tick < 4; ++tick) {
        sim_execute();
        sim_commit();
    }
}
