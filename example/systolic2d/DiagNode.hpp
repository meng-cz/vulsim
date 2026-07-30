#pragma once

#include <defhelper.hpp>

#include "header.hpp"

INTERFACE() {
    REQUEST(right_out, ARG(uint32_t) data);
    SERVICE(left_in, ARG(uint32_t) data);
}

REGISTER(datareg, uint32_t) {
    datareg = 0;
}

SERVICE(left_in, ARG(uint32_t) data) {
    datareg.setnext(data);
    right_out(data + 1);
}
