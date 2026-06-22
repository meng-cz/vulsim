#include <cstdio>
#include <cstdlib>

#include <defhelper.hpp>
#include <run.hpp>

#include "../header.hpp"

TOP("../Top.hpp");
PROJECT("..");

REQUEST(set_state, ARG(CoreState) next_state, ARG(uint32_t) next_code);
QUERY(current_state, CoreState);
QUERY(snapshot, StatusSnapshot);

SIMULATION() {
    auto fail = [&](const char *msg, uint32_t got, uint32_t expected) {
        std::printf("enumdemo failed: %s got=%u expected=%u\n", msg, got, expected);
        std::exit(1);
    };
    auto check = [&](bool cond, const char *msg, uint32_t got, uint32_t expected) {
        if (!cond) fail(msg, got, expected);
    };

    check(static_cast<uint32_t>(CoreState::RESET) == 1U, "RESET value", static_cast<uint32_t>(CoreState::RESET), 1);
    check(static_cast<uint32_t>(CoreState::RUNNING) == 2U, "RUNNING value", static_cast<uint32_t>(CoreState::RUNNING), 2);
    check(static_cast<uint32_t>(CoreState::WAITING) == 7U, "WAITING value", static_cast<uint32_t>(CoreState::WAITING), 7);
    check(static_cast<uint32_t>(CoreState::HALTED) == 8U, "HALTED value", static_cast<uint32_t>(CoreState::HALTED), 8);

    CoreState st = current_state();
    check(st == CoreState::RESET, "initial state", static_cast<uint32_t>(st), 1);
    sim_execute();
    sim_commit();

    set_state(CoreState::RUNNING, 11);
    sim_execute();
    sim_commit();

    StatusSnapshot snap = snapshot();
    check(snap.state == CoreState::RUNNING, "running state", static_cast<uint32_t>(snap.state), 2);
    check(snap.code == 11U, "running code", snap.code, 11);
    sim_execute();
    sim_commit();

    set_state(CoreState::WAITING, 99);
    sim_execute();
    sim_commit();

    snap = snapshot();
    check(snap.state == CoreState::WAITING, "waiting state", static_cast<uint32_t>(snap.state), 7);
    check(snap.code == 99U, "waiting code", snap.code, 99);
    sim_execute();
    sim_commit();

    set_state(CoreState::HALTED, 123);
    sim_execute();
    sim_commit();

    st = current_state();
    check(st == CoreState::HALTED, "halted state", static_cast<uint32_t>(st), 8);
    snap = snapshot();
    check(snap.code == 123U, "halted code", snap.code, 123);

    std::printf("enumdemo passed\n");
}
