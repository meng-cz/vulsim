#pragma once

#include <cstdint>
#include <defhelper.hpp>

// The states deliberately use a sparse encoding so that the lowering path has
// to retain an if/else-if comparison chain instead of relying on a counter.
CONFIG(FSM_SEED, 3);
CONFIG(FSM_XORSHIFT_A, 9);
CONFIG(FSM_ADD_MIX, 14);
CONFIG(FSM_ROTATE, 22);
CONFIG(FSM_MULTIPLY, 37);
CONFIG(FSM_FEEDBACK, 45);
CONFIG(FSM_SCRAMBLE, 58);
CONFIG(FSM_WYHASH, 71);
CONFIG(FSM_AVALANCHE, 89);
CONFIG(FSM_SELECT, 104);
CONFIG(FSM_FINALIZE, 119);
CONFIG(FSM_RESEED, 127);

STRUCT(FsmSnapshot) {
    uint8_t state;
    bool f;
    uint64_t d;
};
