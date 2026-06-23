#pragma once

#include "header.hpp"

REGISTER(state, CoreState) {
    state = CoreState::RESET;
}

REGISTER(code, uint32_t) {
    code = 0;
}

SERVICE(set_state, ARG(CoreState) next_state, ARG(uint32_t) next_code) {
    state.setnext(next_state);
    code.setnext(passthrough_code(next_code));
}

QUERY(current_state, CoreState) {
    return state;
}

QUERY(snapshot, StatusSnapshot) {
    StatusSnapshot s;
    s.state = state;
    s.code = passthrough_code(code);
    return s;
}
