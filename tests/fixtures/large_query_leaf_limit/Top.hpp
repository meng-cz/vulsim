#pragma once

#include "header.hpp"

INTERFACE() {
}

REGISTER_ARRAY1(regs, uint64_t, REG_COUNT, 1) {
    for (int i = 0; i < REG_COUNT; ++i) {
        regs[i] = static_cast<uint64_t>(i);
    }
}

// Model the unrelated state already present in a realistic CPU.  Without the
// query below this module fits below S7's leaf-symbol limit.  The old lowering
// cloned all 32 read-data ports once per queried register and crossed 4096.
REGISTER_ARRAY1(background, uint8_t, BACKGROUND_COUNT, 1) {
    for (int i = 0; i < BACKGROUND_COUNT; ++i) {
        background[i] = 0;
    }
}

QUERY(register_file, RegisterFileSnapshot) {
    RegisterFileSnapshot result{};
    result.values[0] = regs[0];
    result.values[1] = regs[1];
    result.values[2] = regs[2];
    result.values[3] = regs[3];
    result.values[4] = regs[4];
    result.values[5] = regs[5];
    result.values[6] = regs[6];
    result.values[7] = regs[7];
    result.values[8] = regs[8];
    result.values[9] = regs[9];
    result.values[10] = regs[10];
    result.values[11] = regs[11];
    result.values[12] = regs[12];
    result.values[13] = regs[13];
    result.values[14] = regs[14];
    result.values[15] = regs[15];
    result.values[16] = regs[16];
    result.values[17] = regs[17];
    result.values[18] = regs[18];
    result.values[19] = regs[19];
    result.values[20] = regs[20];
    result.values[21] = regs[21];
    result.values[22] = regs[22];
    result.values[23] = regs[23];
    result.values[24] = regs[24];
    result.values[25] = regs[25];
    result.values[26] = regs[26];
    result.values[27] = regs[27];
    result.values[28] = regs[28];
    result.values[29] = regs[29];
    result.values[30] = regs[30];
    result.values[31] = regs[31];
    return result;
}

TICK_IMPL() {
}
