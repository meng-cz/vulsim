#include <cstdio>
#include <cstdlib>

#include <defhelper.hpp>
#include <run.hpp>

#include "header.hpp"

TOP("./Top.hpp");
PROJECT(".");

REQUEST(command, ARG(uint8_t) opcode);
QUERY(snapshot, FsmSnapshot);
SERVICE(event, ARG(uint8_t) state, ARG(uint8_t) count) {
    event_count++;
    last_event_state = state;
    last_event_count = count;
}

GLOBAL() {
    uint32_t event_count = 0;
    uint8_t last_event_state = FSM_IDLE;
    uint8_t last_event_count = 0;
}

SIMULATION() {
    auto fail = [&](const char *msg, uint32_t got, uint32_t expected) {
        std::printf("verilator_fsm failed: %s got=%u expected=%u\n", msg, got, expected);
        std::exit(1);
    };
    auto check = [&](bool cond, const char *msg, uint32_t got, uint32_t expected) {
        if (!cond) {
            fail(msg, got, expected);
        }
    };

    FsmSnapshot snap = snapshot();
    check(snap.state == FSM_IDLE, "initial state", snap.state, FSM_IDLE);
    check(snap.count == 0U, "initial count", snap.count, 0);
    check(!snap.active, "initial active", snap.active, 0);
    check(snap.even, "initial even", snap.even, 1);

    command(CMD_START);
    sim_nextcycle();

    snap = snapshot();
    check(snap.state == FSM_RUNNING, "started state", snap.state, FSM_RUNNING);
    check(snap.count == 0U, "started count", snap.count, 0);

    sim_nextcycle();
    snap = snapshot();
    check(snap.count == 1U, "first running tick", snap.count, 1);
    check(event_count == 0U, "no early event", event_count, 0);

    sim_nextcycle();
    snap = snapshot();
    check(snap.count == 2U, "second running tick", snap.count, 2);
    check(event_count == 0U, "second no event", event_count, 0);

    sim_nextcycle();
    snap = snapshot();
    check(snap.count == 3U, "third running tick", snap.count, 3);

    sim_nextcycle();
    snap = snapshot();
    check(snap.count == 4U, "fourth running tick", snap.count, 4);
    check(event_count == 1U, "first event count", event_count, 1);
    check(last_event_state == FSM_RUNNING, "first event state", last_event_state, FSM_RUNNING);
    check(last_event_count == 4U, "first event value", last_event_count, 4);

    command(CMD_PAUSE);
    sim_nextcycle();
    snap = snapshot();
    check(snap.state == FSM_PAUSED, "paused state", snap.state, FSM_PAUSED);
    check(snap.count >= 4U && snap.count <= 5U, "paused count range", snap.count, 4);
    uint8_t paused_count = snap.count;

    sim_nextcycle();
    snap = snapshot();
    check(snap.count == paused_count, "paused hold", snap.count, paused_count);
    check(event_count == 1U, "paused no event", event_count, 1);

    command(CMD_RESUME);
    sim_nextcycle();
    snap = snapshot();
    check(snap.state == FSM_RUNNING, "resumed state", snap.state, FSM_RUNNING);
    check(snap.count == paused_count, "resumed count", snap.count, paused_count);

    for (uint8_t target = paused_count; target < 8U; ++target) {
        sim_nextcycle();
    }
    snap = snapshot();
    check(snap.count == 8U, "resumed running count", snap.count, 8);
    check(event_count == 2U, "second event count", event_count, 2);
    check(last_event_count == 8U, "second event value", last_event_count, 8);

    command(CMD_STOP);
    sim_nextcycle();
    snap = snapshot();
    check(snap.state == FSM_DONE, "done state", snap.state, FSM_DONE);
    check(!snap.active, "done inactive", snap.active, 0);

    std::printf("verilator_fsm passed\n");
}
