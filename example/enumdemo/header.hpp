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

HELPER() {
inline uint32_t passthrough_code(uint32_t value) {
    return value;
}
}
