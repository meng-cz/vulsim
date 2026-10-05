#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>

#include <defhelper.hpp>
#include <run.hpp>

#include "header.hpp"

TOP("./AES1.hpp");
PROJECT(".");

REQUEST_READY(input, ARG(AESData) data, ARG(AESKey) key);

GLOBAL() {
    struct AESTestVector {
        AESData plaintext;
        AESKey key;
    };

    struct ExpectedOutput {
        AESData ciphertext;
        uint64_t due_tick;
        uint32_t vector_index;
    };

    uint64_t current_tick = 0;
    uint32_t output_count = 0;
    std::deque<ExpectedOutput> expected_outputs;

    static constexpr std::array<uint8_t, 256> aes_sbox = {
        0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
        0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
        0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
        0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
        0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
        0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
        0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
        0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
        0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
        0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
        0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
        0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
        0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
        0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
        0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
        0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};

    static constexpr std::array<uint8_t, 10> aes_rcon = {
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36};

    static uint8_t xtime(uint8_t value) {
        const uint8_t high_bit = static_cast<uint8_t>(value >> 7);
        return static_cast<uint8_t>((value << 1) ^ (0x1bU & (0U - high_bit)));
    }

    static AESData reference_aes128_encrypt(const AESData &plaintext, const AESKey &key) {
        std::array<uint8_t, 176> round_keys{};
        for (uint32_t i = 0; i < 16; ++i) {
            round_keys[i] = key[i];
        }

        uint32_t generated = 16;
        uint32_t rcon_index = 0;
        while (generated < round_keys.size()) {
            std::array<uint8_t, 4> temp = {
                round_keys[generated - 4], round_keys[generated - 3],
                round_keys[generated - 2], round_keys[generated - 1]};
            if ((generated % 16) == 0) {
                const uint8_t first = temp[0];
                temp[0] = aes_sbox[temp[1]];
                temp[1] = aes_sbox[temp[2]];
                temp[2] = aes_sbox[temp[3]];
                temp[3] = aes_sbox[first];
                temp[0] ^= aes_rcon[rcon_index++];
            }
            for (uint32_t i = 0; i < 4; ++i) {
                round_keys[generated] = round_keys[generated - 16] ^ temp[i];
                ++generated;
            }
        }

        AESData state = plaintext;
        for (uint32_t i = 0; i < 16; ++i) {
            state[i] ^= round_keys[i];
        }

        for (uint32_t round = 1; round <= 10; ++round) {
            for (uint32_t i = 0; i < 16; ++i) {
                state[i] = aes_sbox[state[i]];
            }

            AESData shifted{};
            for (uint32_t row = 0; row < 4; ++row) {
                for (uint32_t column = 0; column < 4; ++column) {
                    shifted[column * 4 + row] = state[((column + row) % 4) * 4 + row];
                }
            }
            state = shifted;

            if (round != 10) {
                for (uint32_t column = 0; column < 4; ++column) {
                    const uint32_t base = column * 4;
                    const uint8_t s0 = state[base];
                    const uint8_t s1 = state[base + 1];
                    const uint8_t s2 = state[base + 2];
                    const uint8_t s3 = state[base + 3];
                    const uint8_t total = s0 ^ s1 ^ s2 ^ s3;
                    state[base] = xtime(static_cast<uint8_t>(s0 ^ s1)) ^ s0 ^ total;
                    state[base + 1] = xtime(static_cast<uint8_t>(s1 ^ s2)) ^ s1 ^ total;
                    state[base + 2] = xtime(static_cast<uint8_t>(s2 ^ s3)) ^ s2 ^ total;
                    state[base + 3] = xtime(static_cast<uint8_t>(s3 ^ s0)) ^ s3 ^ total;
                }
            }

            const uint32_t key_offset = round * 16;
            for (uint32_t i = 0; i < 16; ++i) {
                state[i] ^= round_keys[key_offset + i];
            }
        }

        return state;
    }

    static void print_block(const AESData &block) {
        for (uint8_t byte : block) {
            std::printf("%02x", static_cast<unsigned>(byte));
        }
    }
};

SERVICE(output, ARG(AESData) data) {
    if (expected_outputs.empty()) {
        std::printf("aes1 regression failed: unexpected output at tick %llu: ",
                    static_cast<unsigned long long>(current_tick));
        print_block(data);
        std::printf("\n");
        std::exit(1);
    }

    const ExpectedOutput expected = expected_outputs.front();
    expected_outputs.pop_front();
    if (data != expected.ciphertext) {
        std::printf("aes1 regression failed: vector %u data mismatch at tick %llu; expected ",
                    expected.vector_index, static_cast<unsigned long long>(current_tick));
        print_block(expected.ciphertext);
        std::printf(", got ");
        print_block(data);
        std::printf("\n");
        std::exit(1);
    }
    if (current_tick != expected.due_tick) {
        std::printf("aes1 regression failed: vector %u latency mismatch at tick %llu, expected tick %llu\n",
                    expected.vector_index,
                    static_cast<unsigned long long>(current_tick),
                    static_cast<unsigned long long>(expected.due_tick));
        std::exit(1);
    }
    ++output_count;
}

SIMULATION() {
    constexpr uint64_t latency_cycles = 10;
    constexpr uint64_t issue_interval = 12;
    constexpr uint32_t random_vector_count = 64;
    constexpr std::array<AESTestVector, 4> fixed_vectors = {{
        {AESData{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff},
         AESKey{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f}},
        {AESData{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
         AESKey{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {AESData{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
         AESKey{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
        {AESData{0x2a, 0x7b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x81,
                 0x92, 0xa3, 0xb4, 0xc5, 0xd6, 0xe7, 0xf8, 0x09},
         AESKey{0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe,
                0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01}},
    }};
    constexpr size_t vector_count = fixed_vectors.size() + random_vector_count;

    std::array<AESTestVector, vector_count> test_vectors{};
    for (size_t i = 0; i < fixed_vectors.size(); ++i) {
        test_vectors[i] = fixed_vectors[i];
    }

    // Fixed-seed PCG32 stream keeps the randomized differential run repeatable.
    uint64_t random_state = 0x8f3d'9b71'4a26'c5e1ULL;
    auto next_random = [&]() -> uint32_t {
        random_state = random_state * 6364136223846793005ULL + 1;
        const uint32_t shifted = static_cast<uint32_t>(((random_state >> 18) ^ random_state) >> 27);
        const uint32_t rotation = static_cast<uint32_t>(random_state >> 59);
        return (shifted >> rotation) | (shifted << ((-rotation) & 31));
    };
    for (size_t i = fixed_vectors.size(); i < vector_count; ++i) {
        for (size_t byte = 0; byte < 16; ++byte) {
            test_vectors[i].plaintext[byte] = static_cast<uint8_t>(next_random());
            test_vectors[i].key[byte] = static_cast<uint8_t>(next_random());
        }
    }

    const AESData fips197_expected = {
        0x69, 0xc4, 0xe0, 0xd8, 0x6a, 0x7b, 0x04, 0x30,
        0xd8, 0xcd, 0xb7, 0x80, 0x70, 0xb4, 0xc5, 0x5a};

    std::array<AESData, vector_count> reference_results{};
    for (uint32_t i = 0; i < test_vectors.size(); ++i) {
        reference_results[i] = reference_aes128_encrypt(test_vectors[i].plaintext, test_vectors[i].key);
    }
    if (reference_results[0] != fips197_expected) {
        std::printf("aes1 regression failed: software reference did not match the FIPS-197 known-answer vector\n");
        std::exit(1);
    }

    constexpr uint64_t total_cycles =
        (vector_count - 1) * issue_interval + latency_cycles + 1;
    uint32_t next_vector = 0;
    for (current_tick = 0; current_tick < total_cycles; ++current_tick) {
        if (next_vector < test_vectors.size() && current_tick == next_vector * issue_interval) {
            const AESTestVector &test = test_vectors[next_vector];
            if (!input(test.plaintext, test.key)) {
                std::printf("aes1 regression failed: input vector %u was not accepted at tick %llu\n",
                            next_vector, static_cast<unsigned long long>(current_tick));
                std::exit(1);
            }
            expected_outputs.push_back({
                reference_results[next_vector], current_tick + latency_cycles, next_vector});
            ++next_vector;
        }
        sim_nextcycle();
    }

    if (next_vector != vector_count || !expected_outputs.empty() || output_count != vector_count) {
        std::printf("aes1 regression failed: sent %u/%zu vectors and received %u/%zu outputs\n",
                    next_vector, vector_count, output_count, vector_count);
        std::exit(1);
    }

    std::printf("aes1 regression passed: %zu vectors (%zu fixed, %u seeded-random), %llu-cycle latency\n",
                vector_count, fixed_vectors.size(), random_vector_count,
                static_cast<unsigned long long>(latency_cycles));
}
