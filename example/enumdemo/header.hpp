#pragma once

#include <cstdint>
#include <defhelper.hpp>

ENUM(CoreState) {
    RESET = 1,
    RUNNING,
    WAITING = 7,
    HALTED
};

STRUCT(StatusSnapshot) {
    CoreState state;
    uint32_t code;
};
