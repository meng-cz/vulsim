#pragma once

#include <cstdint>
#include <defhelper.hpp>

CONFIG(IMAGE_WIDTH, 8);
CONFIG(IMAGE_HEIGHT, 8);
CONFIG(TASK_COUNT, 32);
CONFIG(BRAM_DEPTH, 2048);
CONFIG(KERNEL_COUNT, 128);

STRUCT(ConvCommand) {
    uint32_t op;
    uint32_t address;
    uint64_t data;
    uint32_t input_base;
    uint32_t output_base;
    uint32_t kernel_index;
};

STRUCT(ConvStatus) {
    bool busy;
    bool done;
    uint32_t completed;
    uint32_t current_input;
    uint32_t current_output;
    uint32_t current_kernel;
    bool input_empty;
    bool read_empty;
    bool read_pending;
};

STRUCT(WindowStage) {
    Int<72> even_pixels;
    Int<72> odd_pixels;
    uint32_t out_row;
    uint32_t pair;
    uint32_t output_addr;
    bool valid;
};

STRUCT(ProductStage) {
    Int<288> products;
    uint32_t output_addr;
    bool valid;
};

STRUCT(SumStage) {
    int32_t even_value;
    int32_t odd_value;
    uint32_t output_addr;
    bool valid;
};

STRUCT(ReadMeta) {
    uint32_t owner;
    uint32_t row;
};
