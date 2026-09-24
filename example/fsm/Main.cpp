#include <defhelper.hpp>
#include <run.hpp>

#include "header.hpp"

GLOBAL() {
    uint64_t tick_count = 0;
}

QUERY(snapshot, FsmSnapshot);

SIMULATION() {
    for (tick_count = 0; tick_count < 24; ++tick_count) {
        sim_nextcycle();
        FsmSnapshot value = snapshot();
        printf("%lu: state=%u f=%u d=%016lx\n", tick_count,
               static_cast<unsigned>(value.state), static_cast<unsigned>(value.f), value.d);
    }
}
