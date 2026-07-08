#include <defhelper.hpp>
#include <run.hpp>

SERVICE(output, ARG(uint32_t) data) {
    printf("childalias output: %u\n", data);
}

SIMULATION() {
    sim_nextcycle();
}
