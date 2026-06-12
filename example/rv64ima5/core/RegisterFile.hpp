#pragma once

#include "../header.hpp"

REGISTER_ARRAY1(regs, uint64_t, 32, 1) {
    for (int i = 0; i < 32; ++i) {
        regs[i] = 0;
    }
}

SERVICE(read2, ARG(uint32_t) rs1, ARG(uint32_t) rs2, RESP(RegReadPair) out) {
    out.rs1_val = (rs1 == 0U) ? 0ULL : regs[rs1];
    out.rs2_val = (rs2 == 0U) ? 0ULL : regs[rs2];
}

SERVICE(write, ARG(uint32_t) rd, ARG(uint64_t) data, ARG(bool) wen) {
    if (wen && rd != 0U) {
        regs.setnext<0>(rd, data);
    }
}

QUERY(zero_and_ra, RegReadPair) {
    RegReadPair out;
    out.rs1_val = 0;
    out.rs2_val = regs[1];
    return out;
}
