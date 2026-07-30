#pragma once

#include <defhelper.hpp>

#include "header.hpp"

INTERFACE() {
    SERVICE(inc, ARG(uint32_t) in, RESP(uint32_t) out);
}

SERVICE(inc, ARG(uint32_t) in, RESP(uint32_t) out) {
    out = in + 1;
}
