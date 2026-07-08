#include <cstdio>
#include <cstdlib>
#include <deque>

#include <defhelper.hpp>
#include <run.hpp>

#include "./header.hpp"

TOP("./MulU32.hpp");
PROJECT(".");

REQUEST(s0input, ARG(uint32_t) a, ARG(uint32_t) b);

GLOBAL() {
    uint64_t current_tick = 0;

    struct ExpectedOutput {
        uint64_t y;
        uint64_t tick;
    };
    std::deque<ExpectedOutput> expected_outputs;
};

SERVICE(s3output, ARG(uint64_t) y) {
    if (expected_outputs.empty()) {
        std::printf("mulu32 failed: unexpected output y=%lu at tick %lu\n", y, current_tick);
        std::exit(1);
    }
    ExpectedOutput expected = expected_outputs.front();
    expected_outputs.pop_front();
    if (y != expected.y) {
        std::printf("mulu32 failed: incorrect output y=%lu at tick %lu, expected %lu\n", y, current_tick, expected.y);
        std::exit(1);
    }
    if (current_tick != expected.tick) {
        std::printf("mulu32 failed: output y=%lu at tick %lu, expected tick %lu\n", y, current_tick, expected.tick);
        std::exit(1);
    }
}

SIMULATION() {

    constexpr uint64_t target_cycle = 1000;
    constexpr uint64_t random_seed = 114514;

    uint64_t random_cur = random_seed;
    auto random = [&]() -> uint32_t {
        // PCG32-like random number generator
        random_cur = random_cur * 6364136223846793005ULL + 1;
        uint32_t xorshifted = static_cast<uint32_t>(((random_cur >> 18) ^ random_cur) >> 27);
        uint32_t rot = static_cast<uint32_t>(random_cur >> 59);
        return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
    };

    for (current_tick = 0; current_tick < target_cycle; ++current_tick) {
        // Randomly generate inputs
        if (random() % 2 == 0) {
            uint32_t a = random();
            uint32_t b = random();
            s0input(a, b);
            // Calculate expected output
            uint64_t expected_y = static_cast<uint64_t>(a) * static_cast<uint64_t>(b);
            expected_outputs.push_back({expected_y, current_tick + 3}); // Output will be available after 3 ticks
        }
        sim_nextcycle();
    }

    printf("mulu32 passed\n");
}
