
#pragma once

#include <defhelper.hpp>

#include "header.hpp"

// Parameter

// Struct

STRUCT(ReadStageReg) {
    Int<ADDR_WIDTH> addr;
    bool valid;
};

STRUCT(TagEntry) {
    Int<TAG_WIDTH> tag;
    bool valid;
};

// Register

BRAM(tag_array, TagEntry, INDEX_SIZE, 1, 1);
BRAM(data_array, Int<DATA_WIDTH>, INDEX_SIZE, 1, 1);

REGISTER(read_stage, ReadStageReg) {
    read_stage.addr = 0;
    read_stage.valid = false;
}

WIRE(read_inputed, bool) {
    read_inputed = false;
}

// Port

REQUEST(readresp_s1, ARG(bool) hit, ARG(Int<DATA_WIDTH>) data);

SERVICE(read_s0, ARG(Int<ADDR_WIDTH>) addr)  {
    ReadStageReg s0;
    s0.addr = addr;
    s0.valid = true;
    read_stage.setnext(s0);
    Int<ADDR_WIDTH> index = addr(INDEX_WIDTH - 1, 0);
    tag_array.readreq<0>(index);
    data_array.readreq<0>(index);
    read_inputed = true;
}

SERVICE(refill_s0, ARG(Int<ADDR_WIDTH>) addr, ARG(Int<DATA_WIDTH>) data) {
    Int<ADDR_WIDTH> index = addr(INDEX_WIDTH - 1, 0);
    Int<TAG_WIDTH> tag = addr(ADDR_WIDTH - 1, INDEX_WIDTH);
    TagEntry tag_entry;
    tag_entry.tag = tag;
    tag_entry.valid = true;
    tag_array.write<0>(index, tag_entry);
    data_array.write<0>(index, data);
}

// Logic block

TICK_IMPL() {
    bool hit = false;
    Int<DATA_WIDTH> read_data;
    if (read_stage.get().valid) {
        Int<TAG_WIDTH> tag = read_stage.get().addr(ADDR_WIDTH - 1, INDEX_WIDTH);
        TagEntry tag_entry = tag_array.readdata<0>();
        Int<TAG_WIDTH> read_tag = tag_entry.tag;
        bool valid = tag_entry.valid;
        if (valid && read_tag == tag) {
            hit = true;
            read_data = data_array.readdata<0>();
        }
        readresp_s1(hit, read_data);
    }
    if (!read_inputed) {
        ReadStageReg s0;
        s0.valid = false;
        read_stage.setnext(s0);
    }
}
