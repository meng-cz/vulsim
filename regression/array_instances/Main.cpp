#include <cstdlib>
#include <cstdio>
#include <defhelper.hpp>
#include <run.hpp>
#include "header.hpp"
TOP("./Top.hpp");
PROJECT(".");
REQUEST(submit1, array=8, ARG(uint32_t) data);
REQUEST(submit2, array=4, ARG(uint32_t) data);
QUERY(snapshot, ArraySnapshot);
SIMULATION() {
    uint32_t expected1[8]; uint32_t expected2[4];
    for (uint32_t i = 0; i < 8; ++i) expected1[i] = 23 + ((i+1) << 16) + ((i+4) << 24) + 0x80000000u;
    for (uint32_t i = 0; i < 4; ++i) expected2[i] = 23 + (4 << 16) + (7 << 24) + 0x80000000u;
    for (uint32_t phase = 0; phase < 2; ++phase) {
        sim_reset();
        for (uint32_t cycle = 0; cycle < 20; ++cycle) {
            ArraySnapshot before = snapshot();
            for (uint32_t i = 0; i < 8; ++i) {
                if (before.first[i] != expected1[i]) { printf("first[%u] cycle=%u actual=%u expected=%u\n", i, cycle, before.first[i], expected1[i]); std::exit(1); }
            }
            for (uint32_t i = 0; i < 4; ++i) if (before.second[i] != expected2[i]) { printf("second[%u] cycle=%u actual=%u expected=%u\n", i, cycle, before.second[i], expected2[i]); std::exit(2); }
            const uint32_t data = 0xA1935u + cycle * 1031;
            submit1<0>(data); submit1<1>(data+1); submit1<2>(data+2); submit1<3>(data+3);
            submit1<4>(data+4); submit1<5>(data+5); submit1<6>(data+6); submit1<7>(data+7);
            submit2<0>(data+20); submit2<1>(data+21); submit2<2>(data+22); submit2<3>(data+23);
            sim_nextcycle();
            ArraySnapshot after = snapshot();
            for (uint32_t i = 0; i < 8; ++i) {
                if (after.streamed[i] != expected1[i]) { printf("streamed[%u] cycle=%u actual=%u expected=%u\n", i, cycle, after.streamed[i], expected1[i]); std::exit(3); }
                expected1[i] = ((data+i) & ((1u << (i+9))-1)) + ((i+1) << 16) + ((i+4) << 24) + (cycle < i ? 0x80000000u : 0u);
            }
            for (uint32_t i = 0; i < 4; ++i) expected2[i] = ((data+20+i) & 4095) + (4 << 16) + (7 << 24) + (cycle < 3 ? 0x80000000u : 0u);
        }
        for (uint32_t i = 0; i < 8; ++i) expected1[i] = 23 + ((i+1) << 16) + ((i+4) << 24) + 0x80000000u;
        for (uint32_t i = 0; i < 4; ++i) expected2[i] = 23 + (4 << 16) + (7 << 24) + 0x80000000u;
    }
    printf("array_instances PASS\n");
}
