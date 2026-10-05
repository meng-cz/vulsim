#include <cstdio>
#include <cstdlib>
#include <deque>

#include <defhelper.hpp>
#include <run.hpp>

#include "header.hpp"

TOP("./MulU32.hpp");
PROJECT(".");

REQUEST(s0input, ARG(uint32_t) a, ARG(uint32_t) b);

GLOBAL() {
    uint64_t current_tick = 0;

    struct ExpectedOutput {
        uint64_t value;
        uint64_t tick;
    };
    std::deque<ExpectedOutput> expected_outputs;
};

SERVICE(s3output, ARG(uint64_t) y) {
    if (expected_outputs.empty()) {
        std::printf("mulu32 regression failed: unexpected output %llu at tick %llu\n",
                    static_cast<unsigned long long>(y),
                    static_cast<unsigned long long>(current_tick));
        std::exit(1);
    }

    const ExpectedOutput expected = expected_outputs.front();
    expected_outputs.pop_front();
    if (y != expected.value) {
        std::printf("mulu32 regression failed: output %llu at tick %llu, expected %llu\n",
                    static_cast<unsigned long long>(y),
                    static_cast<unsigned long long>(current_tick),
                    static_cast<unsigned long long>(expected.value));
        std::exit(1);
    }
    if (current_tick != expected.tick) {
        std::printf("mulu32 regression failed: latency mismatch at tick %llu, expected tick %llu\n",
                    static_cast<unsigned long long>(current_tick),
                    static_cast<unsigned long long>(expected.tick));
        std::exit(1);
    }
}

SIMULATION() {
    constexpr uint64_t target_cycle = 100'000;
    constexpr uint64_t random_seed = 0x1'BF52;
    constexpr uint32_t directed[][2] = {
        {0U, 0U},
        {0U, 0xffffffffU},
        {1U, 0xffffffffU},
        {0xffffffffU, 0xffffffffU},
        {0x0000ffffU, 0x00010001U},
        {0x80000000U, 2U},
        {0x7fffffffU, 0x7fffffffU},
        {0x12345678U, 0x9abcdef0U},
    };

    uint64_t random_cur = random_seed;
    auto random = [&]() -> uint32_t {
        // PCG32-like deterministic generator.
        random_cur = random_cur * 6364136223846793005ULL + 1;
        uint32_t xorshifted = static_cast<uint32_t>(((random_cur >> 18) ^ random_cur) >> 27);
        uint32_t rot = static_cast<uint32_t>(random_cur >> 59);
        return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
    };

    for (current_tick = 0; current_tick < target_cycle; ++current_tick) {
        bool send = false;
        uint32_t a = 0;
        uint32_t b = 0;

        if (current_tick < sizeof(directed) / sizeof(directed[0])) {
            send = true;
            a = directed[current_tick][0];
            b = directed[current_tick][1];
        } else if ((random() & 1U) == 0U) {
            send = true;
            a = random();
            b = random();
        }

        if (send) {
            s0input(a, b);
            expected_outputs.push_back({
                static_cast<uint64_t>(a) * static_cast<uint64_t>(b),
                current_tick + 3,
            });
        }
        sim_nextcycle();
    }

    // Drain the three-stage pipeline and make sure every accepted input returned.
    for (current_tick = target_cycle; current_tick < target_cycle + 3; ++current_tick) {
        sim_nextcycle();
    }
    if (!expected_outputs.empty()) {
        std::printf("mulu32 regression failed: %llu expected outputs were not produced\n",
                    static_cast<unsigned long long>(expected_outputs.size()));
        std::exit(1);
    }

    std::printf("mulu32 regression passed: %llu cycles\n",
                static_cast<unsigned long long>(target_cycle));
}
