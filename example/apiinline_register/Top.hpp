#pragma once

#include "header.hpp"

REGISTER(scalar_reg, uint32_t) {
    scalar_reg = 1;
}

REGISTER_MUL(multi_reg, uint32_t, 3) {
    multi_reg = 10;
}

REGISTER_ARRAY1(array_reg, uint16_t, REG_ARRAY_SIZE, 2) {
    for (int i = 0; i < REG_ARRAY_SIZE; ++i) {
        array_reg[i] = static_cast<uint16_t>(i);
    }
}

REGISTER(payload_reg, Payload) {
    payload_reg.data = 0;
    payload_reg.meta.tag = 3;
    payload_reg.meta.valid = false;
    payload_reg.flag = false;
}

REGISTER_ARRAY1(payload_array, Payload, REG_ARRAY_SIZE, 2) {
    for (int i = 0; i < REG_ARRAY_SIZE; ++i) {
        payload_array[i].data = static_cast<uint32_t>(i);
        payload_array[i].meta.tag = static_cast<uint8_t>(i + 1);
        payload_array[i].meta.valid = (i == 0);
        payload_array[i].flag = false;
    }
}

SERVICE(write_scalar, ARG(uint32_t) value) {
    scalar_reg.setnext(value);
}

SERVICE(write_payload, ARG(Payload) next_payload) {
    payload_reg.setnext(next_payload);
}

QUERY(summary, RegisterSummary) {
    RegisterSummary result;
    result.scalar = scalar_reg;
    result.multi = multi_reg.get();
    result.arr0 = array_reg[0];
    result.payload_data = payload_reg.get().data;
    result.payload_tag = payload_array[1].meta.tag;
    result.payload_valid = payload_array[2].meta.valid;
    return result;
}

TICK_IMPL() {
    uint32_t idx = scalar_reg & (REG_ARRAY_SIZE - 1);
    uint32_t scalar_now = scalar_reg.get();

    multi_reg.setnext<0>(scalar_now + 1);
    multi_reg.setnext<1>(multi_reg + 2);
    multi_reg.setnext<2>(array_reg[idx] + multi_reg.get());

    array_reg.setnext<0>(idx, static_cast<uint16_t>(array_reg[idx] + scalar_reg));
    array_reg.setnext<1>((idx + 1) & (REG_ARRAY_SIZE - 1), static_cast<uint16_t>(multi_reg.get()));

    Payload payload_next = payload_reg;
    payload_next.data = payload_reg.get().data + scalar_reg;
    payload_next.meta.tag = static_cast<uint8_t>(payload_array[idx].meta.tag + 1);
    payload_next.meta.valid = !payload_next.meta.valid;
    payload_next.flag = !payload_next.flag;
    payload_reg.setnext(payload_next);

    Payload array_payload_next = payload_array[idx];
    array_payload_next.data = multi_reg.get() + array_reg[idx];
    array_payload_next.meta.valid = true;
    payload_array.setnext<1>(idx, array_payload_next);
}
