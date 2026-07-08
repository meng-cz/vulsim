#pragma once

#include <cstdint>
#include <defhelper.hpp>

CONFIG(FSM_IDLE, 0);
CONFIG(FSM_RUNNING, 1);
CONFIG(FSM_PAUSED, 2);
CONFIG(FSM_DONE, 3);

CONFIG(CMD_START, 1);
CONFIG(CMD_PAUSE, 2);
CONFIG(CMD_RESUME, 3);
CONFIG(CMD_STOP, 4);

STRUCT(FsmSnapshot) {
    uint8_t state;
    uint8_t count;
    bool active;
    bool even;
};
