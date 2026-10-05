#pragma once
#include "header.hpp"
PARAMETER(WORDS, 32);
PARAMETER(ADDR_BITS, clog2(WORDS));
PARAMETER(RESET_SEED, 0x1234);
REGISTER(words, uint32_t, dims=[WORDS]) {
    for (uint32_t i = 0; i < WORDS; ++i) words[i] = static_cast<uint32_t>(RESET_SEED) ^ (i * 0x9E3779B9U);
}
SERVICE(read, ARG(uint32_t) address, RESP(uint32_t) data) {
    Int<ADDR_BITS> index = address;
    data = words[index.to<uint32_t>()];
}
SERVICE(write, ARG(uint32_t) address, ARG(uint32_t) data) {
    Int<ADDR_BITS> index = address;
    words.setnext<0>(index.to<uint32_t>(), data);
}
QUERY(status, MemoryStatus) {
    MemoryStatus s{};
    s.words = WORDS; s.addr_bits = ADDR_BITS; s.seed = RESET_SEED;
    for (uint32_t i = 0; i < MEM_WORDS; ++i) s.data[i] = words[i];
    return s;
}
