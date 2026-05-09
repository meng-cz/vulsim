
#pragma once

#include <defhelper.hpp>

#include "header.hpp"

// Parameter

// Struct

STRUCT(ReadStageReg) {
    UInt<ADDR_WIDTH> addr;
    bool valid = false;
};

// Register

BRAM(tag_array, TAG_WIDTH + 1, INDEX_WIDTH, 1, 1);
BRAM(data_array, DATA_WIDTH, INDEX_WIDTH, 1, 1);

REGISTER(read_stage, ReadStageReg) {
    read_stage.addr = 0;
    read_stage.valid = false;
}

WIRE(read_inputed, bool) {
    read_inputed = false;
}

// Port

SERVICE_PORT(read_s0, void, ARG(UInt<ADDR_WIDTH>) addr);
SERVICE_PORT(refill_s0, void, ARG(UInt<ADDR_WIDTH>) addr, ARG(UInt<DATA_WIDTH>) data);

REQUEST_PORT(readresp_s1, void, ARG(bool) hit, ARG(UInt<DATA_WIDTH>) data);

// Logic block

TICK_IMPL() {
    bool hit = false;
    UInt<DATA_WIDTH> read_data;
    if (read_stage_get().valid) {
        UInt<TAG_WIDTH> tag = read_stage_get().addr(ADDR_WIDTH - 1, INDEX_WIDTH);
        UInt<TAG_WIDTH + 1> tag_entry = tag_array.readdata<0>();
        UInt<TAG_WIDTH> read_tag = tag_entry(TAG_WIDTH, 1);
        bool valid = tag_entry(0);
        if (valid && read_tag == tag) {
            hit = true;
            read_data = data_array.readdata<0>();
        }
        readresp_s1(hit, read_data);
    }
    if (!read_inputed) {
        ReadStageReg s0;
        s0.valid = false;
        read_stage_setnext(s0);
    }
}

SERVICE_LOGIC_IMPL(read_s0, ARG(UInt<ADDR_WIDTH>) addr) {
    ReadStageReg s0;
    s0.addr = addr;
    s0.valid = true;
    read_stage_setnext(s0);
    UInt<ADDR_WIDTH> index = addr(INDEX_WIDTH - 1, 0);
    tag_array.readreq<0>(index);
    data_array.readreq<0>(index);
    read_inputed = true;
}

SERVICE_LOGIC_IMPL(refill_s0, ARG(UInt<ADDR_WIDTH>) addr, ARG(UInt<DATA_WIDTH>) data) {
    UInt<ADDR_WIDTH> index = addr(INDEX_WIDTH - 1, 0);
    UInt<TAG_WIDTH> tag = addr(ADDR_WIDTH - 1, INDEX_WIDTH);
    UInt<TAG_WIDTH + 1> tag_entry;
    tag_entry(TAG_WIDTH, 1) = tag;
    tag_entry(0) = 1; // valid bit
    tag_array.write<0>(index, tag_entry);
    data_array.write<0>(index, data);
}

