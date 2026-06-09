#pragma once

#include <defhelper.hpp>

#include "header.hpp"

SERVICE(inc, ARG(uint32_t) in, RESP(uint32_t) out) {
    out = in + 1;
}
