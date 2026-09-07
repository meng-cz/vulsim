#include <cstdio>
#include <defhelper.hpp>
#include <run.hpp>

SERVICE(mixed,
    ARG(uint32_t) lhs,
    RESP(uint32_t) result,
    ARG(uint32_t) rhs) {
    result = lhs + rhs;
}

SERVICE(output, ARG(uint32_t) value) {
    std::printf("mixed output: %u\n", value);
}

SIMULATION() {
    sim_nextcycle();
}
