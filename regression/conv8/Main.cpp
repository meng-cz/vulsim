#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>

#include <defhelper.hpp>
#include <run.hpp>

#include "header.hpp"

TOP("./Conv8.hpp");
PROJECT(".");

REQUEST(submit, handshake=1, ARG(ConvCommand) command);
REQUEST(dma_read, handshake=1, ARG(uint32_t) address);
QUERY(status, ConvStatus);

GLOBAL() {
    struct ExpectedWord {
        uint64_t value;
        uint32_t task;
        uint32_t address;
    };

    uint64_t current_tick = 0;
    uint32_t dma_return_count = 0;
    std::deque<ExpectedWord> expected_dma_words;
}

SERVICE(dma_data, ARG(uint64_t) data) {
    if (expected_dma_words.empty()) {
        std::printf("conv8 regression failed: unexpected DMA return 0x%016llx at tick %llu\n",
                    static_cast<unsigned long long>(data),
                    static_cast<unsigned long long>(current_tick));
        std::exit(1);
    }
    const ExpectedWord expected = expected_dma_words.front();
    expected_dma_words.pop_front();
    if (data != expected.value) {
        std::printf("conv8 regression failed: task %u output address %u mismatch at tick %llu; expected 0x%016llx, got 0x%016llx\n",
                    expected.task, expected.address,
                    static_cast<unsigned long long>(current_tick),
                    static_cast<unsigned long long>(expected.value),
                    static_cast<unsigned long long>(data));
        std::exit(1);
    }
    ++dma_return_count;
}

SIMULATION() {
    constexpr uint32_t kBatchSize = 32;
    constexpr uint32_t kBatchCount = 4;
    constexpr uint32_t kWritesPerTask = 8;
    constexpr uint32_t kRecordsPerTask = 9;
    constexpr uint32_t kWordsPerTask = 32;
    constexpr uint32_t kWordsPerBatch = kBatchSize * kWordsPerTask;
    constexpr uint64_t kBatchTimeout = 100000;

    auto fail_timeout = [&](const char *phase, uint32_t batch) {
        std::printf("conv8 regression failed: timeout during %s in batch %u at tick %llu\n",
                    phase, batch, static_cast<unsigned long long>(current_tick));
        std::exit(1);
    };
    auto kernel_coeff = [](uint32_t kernel, uint32_t index) -> int8_t {
        const uint32_t mixed = (kernel * 11 + index * 7 + index * index * 3) % 9;
        return static_cast<int8_t>(static_cast<int32_t>(mixed) - 4);
    };
    auto next_random = [](uint64_t &state) -> uint32_t {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return static_cast<uint32_t>(state >> 16);
    };
    auto pack_input_row = [](const std::array<int8_t, 64> &image, uint32_t row) -> uint64_t {
        uint64_t packed = 0;
        for (uint32_t col = 0; col < 8; ++col) {
            const uint64_t byte = static_cast<uint8_t>(image[row * 8 + col]);
            packed |= byte << (col * 8);
        }
        return packed;
    };
    auto reference_conv = [&](const std::array<int8_t, 64> &image,
                              uint32_t kernel,
                              std::array<int32_t, 64> &output) {
        for (uint32_t y = 0; y < 8; ++y) {
            for (uint32_t x = 0; x < 8; ++x) {
                int32_t acc = 0;
                for (int32_t u = 0; u < 3; ++u) {
                    for (int32_t v = 0; v < 3; ++v) {
                        const int32_t iy = static_cast<int32_t>(y) + u - 1;
                        const int32_t ix = static_cast<int32_t>(x) + v - 1;
                        if (iy >= 0 && iy < 8 && ix >= 0 && ix < 8) {
                            acc += static_cast<int32_t>(image[iy * 8 + ix]) *
                                   static_cast<int32_t>(kernel_coeff(kernel, u * 3 + v));
                        }
                    }
                }
                output[y * 8 + x] = acc;
            }
        }
    };

    sim_reset();
    uint64_t rng = 0xA32F'91C7'5B8D'04E1ULL;

    for (uint32_t batch = 0; batch < kBatchCount; ++batch) {
        std::array<std::array<int8_t, 64>, kBatchSize> images{};
        std::array<std::array<int32_t, 64>, kBatchSize> expected{};
        std::array<uint32_t, kBatchSize> kernels{};

        for (uint32_t task = 0; task < kBatchSize; ++task) {
            kernels[task] = batch * kBatchSize + task;
            for (uint32_t i = 0; i < 64; ++i) {
                const uint32_t raw = next_random(rng);
                images[task][i] = static_cast<int8_t>(raw & 0xffU);
            }
            if (task == 0) {
                for (uint32_t i = 0; i < 64; ++i) {
                    images[task][i] = static_cast<int8_t>(static_cast<int32_t>((i * 13 + 17) % 127) - 63);
                }
            }
            reference_conv(images[task], kernels[task], expected[task]);
        }

        const uint32_t completion_base = status().completed;
        uint32_t record_index = 0;
        uint64_t elapsed = 0;
        while (record_index < kBatchSize * kRecordsPerTask) {
            const uint32_t task = record_index / kRecordsPerTask;
            const uint32_t sub = record_index % kRecordsPerTask;
            const uint32_t task_base = task * 40;
            ConvCommand command{};
            if (sub < kWritesPerTask) {
                command.op = 1;
                command.address = task_base + sub;
                command.data = pack_input_row(images[task], sub);
            } else {
                command.op = 2;
                command.input_base = task_base;
                command.output_base = task_base + 8;
                command.kernel_index = kernels[task];
            }

            if (submit(command)) {
                ++record_index;
            }
            sim_nextcycle();
            ++current_tick;
            if (++elapsed > kBatchTimeout) fail_timeout("DMA input and command submission", batch);
        }

        elapsed = 0;
        for (;;) {
            const ConvStatus snapshot = status();
            if (snapshot.completed >= completion_base + kBatchSize &&
                !snapshot.busy && snapshot.input_empty) {
                break;
            }
            sim_nextcycle();
            ++current_tick;
            if (++elapsed > kBatchTimeout) fail_timeout("convolution completion", batch);
        }

        for (uint32_t task = 0; task < kBatchSize; ++task) {
            const uint32_t output_base = task * 40 + 8;
            for (uint32_t word = 0; word < kWordsPerTask; ++word) {
                const uint32_t row = word / 4;
                const uint32_t pair = word % 4;
                const uint32_t even_col = pair * 2;
                const uint64_t expected_word =
                    static_cast<uint64_t>(static_cast<uint32_t>(expected[task][row * 8 + even_col])) |
                    (static_cast<uint64_t>(static_cast<uint32_t>(expected[task][row * 8 + even_col + 1])) << 32);
                const uint32_t address = output_base + word;
                if (!dma_read(address)) {
                    std::printf("conv8 regression failed: DMA read queue unexpectedly full at tick %llu\n",
                                static_cast<unsigned long long>(current_tick));
                    std::exit(1);
                }
                ExpectedWord item;
                item.value = expected_word;
                item.task = task;
                item.address = address;
                expected_dma_words.push_back(item);
                sim_nextcycle();
                ++current_tick;
            }
        }

        elapsed = 0;
        while (!expected_dma_words.empty() || !status().read_empty || status().read_pending) {
            sim_nextcycle();
            ++current_tick;
            if (++elapsed > kBatchTimeout) fail_timeout("DMA output readback", batch);
        }
        if (dma_return_count != (batch + 1) * kWordsPerBatch) {
            std::printf("conv8 regression failed: batch %u returned %u words, expected %u\n",
                        batch, dma_return_count, (batch + 1) * kWordsPerBatch);
            std::exit(1);
        }
        std::printf("conv8 batch %u passed: 32 tasks, 1024 BRAM words checked\n", batch);
    }

    std::printf("conv8 regression passed: %u tasks, %u ordered DMA result words\n",
                kBatchSize * kBatchCount, dma_return_count);
}
