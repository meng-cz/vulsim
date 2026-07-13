#include <cstdio>
#include <cstdlib>

#include <defhelper.hpp>
#include <run.hpp>

#include "./header.hpp"

TOP("./Top.hpp");
PROJECT(".");

REQUEST(step, ARG(bool) in_valid, ARG(uint32_t) in_data, ARG(bool) stall, ARG(bool) flush);
QUERY(snapshot, PipelineSnapshot);

GLOBAL() {
    uint32_t check_index = 0;
}

SIMULATION() {
    auto check_stage = [&](bool valid, uint32_t data, bool expected_valid, uint32_t expected_data, const char *name) {
        if (valid != expected_valid || data != expected_data) {
            std::printf(
                "pipeline_holdreset failed at check %u: %s got=(%u,%u) expected=(%u,%u)\n",
                check_index,
                name,
                valid ? 1U : 0U,
                data,
                expected_valid ? 1U : 0U,
                expected_data
            );
            std::exit(1);
        }
    };
    auto check_snapshot = [&](const PipelineSnapshot &snap, bool s0_valid, uint32_t s0_data, bool s1_valid, uint32_t s1_data, bool s2_valid, uint32_t s2_data) {
        check_stage(snap.s0_valid, snap.s0_data, s0_valid, s0_data, "s0");
        check_stage(snap.s1_valid, snap.s1_data, s1_valid, s1_data, "s1");
        check_stage(snap.s2_valid, snap.s2_data, s2_valid, s2_data, "s2");
        ++check_index;
    };

    check_snapshot(snapshot(), false, 0, false, 0, false, 0);

    step(true, 11, false, false);
    sim_nextcycle();
    check_snapshot(snapshot(), true, 11, false, 0, false, 0);

    step(true, 22, false, false);
    sim_nextcycle();
    check_snapshot(snapshot(), true, 22, true, 11, false, 0);

    step(true, 33, true, false);
    sim_nextcycle();
    check_snapshot(snapshot(), true, 22, true, 11, false, 0);

    step(true, 44, false, false);
    sim_nextcycle();
    check_snapshot(snapshot(), true, 44, true, 22, true, 11);

    step(true, 55, true, true);
    sim_nextcycle();
    check_snapshot(snapshot(), false, 0, false, 0, false, 0);

    step(true, 66, false, false);
    sim_nextcycle();
    check_snapshot(snapshot(), true, 66, false, 0, false, 0);

    std::printf("pipeline_holdreset passed\n");
}
