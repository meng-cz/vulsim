
#pragma once

#include <defhelper.hpp>
#include <run.hpp>

#include "header.hpp"

// Port

REQUEST_PORT(input, bool, ARG(AESData) data, ARG(AESKey) key);

SERVICE_PORT(output, void, ARG(AESData) data);

// Functions

SERVICE_LOGIC_IMPL(output, ARG(AESData) data) {
    // output the result
    // ignore
}


SIMULATION() {
    
    std::array<uint8_t, 16> data{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    std::array<uint8_t, 16> key{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

    constexpr uint64_t TestTick = 1000000000UL; // 1G ticks

    uint64_t input_cnt = 0;

    for (uint64_t tick = 0; tick < TestTick; ++tick) {
        if (input(data, key)) {
        // if (!((tick / 128) & 1) && aes128ser.input(input, key)) {
            input_cnt++;
            // for (auto &byte : input) {
            //     byte = static_cast<uint8_t>(byte + 1);
            // }
        }
        sim_execute();
        sim_commit();
    }

    printf("Simulation finished after %lu ticks.\n", TestTick);
    printf("Input signals received: %lu\n", input_cnt);

}

