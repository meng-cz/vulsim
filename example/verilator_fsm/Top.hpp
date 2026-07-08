#pragma once

#include <defhelper.hpp>

#include "header.hpp"

REGISTER(state, uint8_t) {
    state = FSM_IDLE;
}

REGISTER(count, uint8_t) {
    count = 0;
}

REQUEST(event, ARG(uint8_t) state, ARG(uint8_t) count);

SERVICE(command, ARG(uint8_t) opcode) {
    uint8_t next_state = state;

    if (opcode == CMD_START) {
        next_state = FSM_RUNNING;
    } else if (opcode == CMD_PAUSE) {
        if (state == FSM_RUNNING) {
            next_state = FSM_PAUSED;
        }
    } else if (opcode == CMD_RESUME) {
        if (state == FSM_PAUSED) {
            next_state = FSM_RUNNING;
        }
    } else if (opcode == CMD_STOP) {
        next_state = FSM_DONE;
    }

    state.setnext(next_state);
}

QUERY(snapshot, FsmSnapshot) {
    FsmSnapshot value;
    value.state = state;
    value.count = count;
    value.active = (state == FSM_RUNNING);
    value.even = ((count & 1U) == 0U);
    return value;
}

TICK_IMPL() {
    if (state == FSM_RUNNING) {
        uint8_t next_count = count + 1U;
        count.setnext(next_count);
        if ((next_count & 3U) == 0U) {
            event(FSM_RUNNING, next_count);
        }
    }
}
