
#include <defhelper.hpp>
#include <run.hpp>

#include "header.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

GLOBAL() {

// variables here are visibal to both SERVICE and SIMULATION, and maintain their state across ticks.
std::vector<UInt<32>> expect_results;
uint64_t total_tests = 0;

}

REQUEST(input, ARG(UInt<16>) a, ARG(UInt<16>) b, ARG(UInt<16>) c, ARG(bool) init, ARG(bool) last);

SERVICE(output, ARG(UInt<32>) sum) {
    if (!expect_results.empty()) {
        UInt<32> expected = expect_results.front();
        expect_results.erase(expect_results.begin());
        if (sum != expected) {
            printf("%ld: Error: expected 0x%08x but got 0x%08x\n", total_tests, expected.get_u32(), sum.get_u32());
            sim_exit();
        } else {
            // printf("Got correct result: 0x%08x\n", sum.get_u32());
            total_tests++;
        }
    } else {
        printf("%ld: Error: got unexpected output 0x%08x\n", total_tests, sum.get_u32());
        sim_exit();
    }
}

SIMULATION() {
    auto fp16_to_fp32 = [](uint16_t h) -> float {
        uint32_t sign = (h >> 15) & 0x1u;
        uint32_t exp = (h >> 10) & 0x1fu;
        uint32_t frac = h & 0x3ffu;

        float mag = 0.0f;
        if (exp == 0) {
            if (frac != 0) {
                mag = std::ldexp(static_cast<float>(frac), -24);
            }
        } else if (exp == 31) {
            mag = (frac == 0) ? INFINITY : NAN;
        } else {
            mag = std::ldexp(1.0f + static_cast<float>(frac) / 1024.0f, static_cast<int>(exp) - 15);
        }
        return sign ? -mag : mag;
    };

    auto f32_bits = [](float v) -> uint32_t {
        uint32_t bits = 0;
        std::memcpy(&bits, &v, sizeof(bits));
        return bits;
    };

    auto push_expect_from_acc = [&](double acc) {
        float out = static_cast<float>(acc);
        expect_results.emplace_back(f32_bits(out));
    };

    auto one_tick = [&]() {
        sim_execute();
        sim_commit();
    };

    auto sanitize_fp16 = [](uint16_t x) -> uint16_t {
        // Keep random tests in finite domain by avoiding exp=31 (Inf/NaN).
        if ((x & 0x7c00u) == 0x7c00u) {
            x ^= 0x0400u;
        }
        return x;
    };

    auto send_seq = [&](const std::vector<uint16_t>& a_vec,
                        const std::vector<uint16_t>& b_vec,
                        uint16_t c_init) {
        if (a_vec.empty() || a_vec.size() != b_vec.size()) {
            printf("Error: invalid test vector size\n");
            sim_exit();
        }

        double acc = static_cast<double>(fp16_to_fp32(c_init));
        for (size_t i = 0; i < a_vec.size(); ++i) {
            bool init = (i == 0);
            bool last = (i + 1 == a_vec.size());
            input(UInt<16>(a_vec[i]), UInt<16>(b_vec[i]), UInt<16>(c_init), init, last);
            acc += static_cast<double>(fp16_to_fp32(a_vec[i])) * static_cast<double>(fp16_to_fp32(b_vec[i]));
            if (last) {
                push_expect_from_acc(acc);
            }
            one_tick();
        }
    };

    // Directed tests: zero / init path / sign mix / denormal path.
    send_seq({0x0000}, {0x0000}, 0x0000); // 0
    send_seq({0x3c00}, {0x3c00}, 0x0000); // 1*1
    send_seq({0x3c00}, {0x4000}, 0x3c00); // 1 + 1*2
    send_seq({0xbc00, 0x4000, 0x3c00}, {0x4000, 0xc000, 0xbc00}, 0x3c00); // signed accumulation
    send_seq({0x0001, 0x03ff, 0x0400}, {0x3c00, 0x3c00, 0x3c00}, 0x0000); // denormal + normal boundary

    // Random regression: fixed seed, varying sequence lengths.
    std::mt19937 rng(0x1234abcd);
    for (int tc = 0; tc < 120; ++tc) {
        int len = 1 + static_cast<int>(rng() % 8);
        std::vector<uint16_t> a_vec(len), b_vec(len);
        for (int i = 0; i < len; ++i) {
            a_vec[i] = sanitize_fp16(static_cast<uint16_t>(rng() & 0xffffu));
            b_vec[i] = sanitize_fp16(static_cast<uint16_t>(rng() & 0xffffu));
        }
        uint16_t c_init = sanitize_fp16(static_cast<uint16_t>(rng() & 0xffffu));
        send_seq(a_vec, b_vec, c_init);
    }

    // Flush pipeline and ensure all expected outputs are observed.
    for (int i = 0; i < 24; ++i) {
        one_tick();
    }

    if (!expect_results.empty()) {
        printf("Error: %zu expected outputs not observed\n", expect_results.size());
        sim_exit();
    }
}
