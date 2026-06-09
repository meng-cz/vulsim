#pragma once

#include <defhelper.hpp>

#include "header.hpp"

REQUEST(right_out, ARG(uint32_t) data);

SERVICE(left_in, ARG(uint32_t) data) {
    right_out(data + 1);
}
