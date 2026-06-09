#pragma once

#include <defhelper.hpp>

#include "header.hpp"

REGISTER(cycle, uint32_t) {
    cycle = 0;
}

REGISTER(captured0, uint32_t) {
    captured0 = 0;
}

REGISTER(captured1, uint32_t) {
    captured1 = 0;
}

REQUEST(print, ARRAY(HEI), ARG(uint32_t) data);
REQUEST(spill, ARG(uint32_t) data);

SERVICE(output, ARRAY(HEI), ARG(uint32_t) data) {
    if constexpr (IDX == 0) {
        captured0.setnext(data);
    } else if constexpr (IDX == 1) {
        captured1.setnext(data);
    }
}

CHILD_INSTANCE_ARRAY2(DiagNode, mesh, HEI, WID);

USE_CHILD_SERVICE_PORT(mesh[*][0], left_in, mesh_in, ARG(uint32_t) data);

CONNECT_CR_CS(mesh[$][?], right_out, mesh[$+1][?+1], left_in);
CONNECT_CR_S(mesh[*][WID-1], right_out, output);
CONNECT_CR_R(mesh[HEI-1][0], right_out, spill);

TICK_IMPL() {
    if (cycle == 0) {
        mesh_in<0>(1);
        mesh_in<1>(10);
    }
    if (cycle == 1) {
        print<0>(captured0);
        print<1>(captured1);
    }
    cycle.setnext(cycle + 1);
}
