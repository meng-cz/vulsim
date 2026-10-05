#pragma once

#include "header.hpp"

INTERFACE() {
    SERVICE(submit, handshake=1, ARG(ConvCommand) command);
    SERVICE(dma_read, handshake=1, ARG(uint32_t) address);
    REQUEST(dma_data, ARG(uint64_t) data);
}

HELPER() {
inline Int<32> sign_extend_product(Int<16> value) {
    return Cat(Repeat<16>(value.at<15>()), value);
}

inline Int<8> select_pixel(Int<64> row, int32_t col) {
    if (col < 0 || col >= 8) return 0;
    return Int<8>(row.pick<8>(col * 8));
}
}

QUEUE(input_queue, ConvCommand, 16);
QUEUE(dma_read_queue, uint32_t, 16);

BRAM(data_mem, Int<64>, BRAM_DEPTH, 1, 1);
ROM(kernel_rom, 72, KERNEL_COUNT, 1, kernels.hex);

REGISTER(busy, bool) { busy = false; }
REGISTER(done, bool) { done = false; }
REGISTER(completed_count, uint32_t) { completed_count = 0; }
REGISTER(current_input, uint32_t) { current_input = 0; }
REGISTER(current_output, uint32_t) { current_output = 0; }
REGISTER(current_kernel, uint32_t) { current_kernel = 0; }
REGISTER(kernel_coeffs, Int<72>);

REGISTER(state, uint32_t) { state = 0; }
REGISTER(next_input_row, uint32_t) { next_input_row = 0; }
REGISTER(read_gap, uint32_t) { read_gap = 0; }
REGISTER(row0, Int<64>);
REGISTER(row1, Int<64>);
REGISTER(row2, Int<64>);
REGISTER(output_row, uint32_t) { output_row = 0; }
REGISTER(output_pair, uint32_t) { output_pair = 0; }

REGISTER(read_meta, ReadMeta) {
    read_meta.owner = 0;
    read_meta.row = 0;
}

REGISTER(window_reg, WindowStage);
REGISTER(product_reg, ProductStage);
REGISTER(sum_reg, SumStage);

SERVICE(submit, handshake=1, ready=input_queue.enqready(), ARG(ConvCommand) command) {
    input_queue.enqnext(command);
}

SERVICE(dma_read, handshake=1, ready=dma_read_queue.enqready(), ARG(uint32_t) address) {
    dma_read_queue.enqnext(address);
}

QUERY(status, ConvStatus) {
    ConvStatus result;
    result.busy = busy;
    result.done = done;
    result.completed = completed_count;
    result.current_input = current_input;
    result.current_output = current_output;
    result.current_kernel = current_kernel;
    result.input_empty = !input_queue.deqvalid();
    result.read_empty = !dma_read_queue.deqvalid();
    result.read_pending = (read_meta.get().owner == 2);
    return result;
}

TICK_IMPL() {
    const uint32_t kIdle = 0;
    const uint32_t kKernelWait = 1;
    const uint32_t kRows = 2;
    const uint32_t kDrain = 3;
    const uint32_t kNoRead = 0;
    const uint32_t kEngineRead = 1;
    const uint32_t kDmaRead = 2;

    ReadMeta old_meta = read_meta.get();
    Int<64> returned_row = 0;
    bool engine_row_valid = false;
    uint32_t returned_row_index = 0;

    if (old_meta.owner == kEngineRead) {
        returned_row = data_mem.readdata<0>();
        engine_row_valid = true;
        returned_row_index = old_meta.row;
    } else if (old_meta.owner == kDmaRead) {
        uint64_t returned_data = data_mem.readdata<0>().to<uint64_t>();
        dma_data(returned_data);
    }

    WindowStage next_window{};
    ProductStage next_product{};
    SumStage next_sum{};

    bool compute_write = false;
    if (sum_reg.get().valid) {
        SumStage value = sum_reg.get();
        Int<64> packed = Cat(Int<32>(value.odd_value), Int<32>(value.even_value));
        data_mem.write<0>(Int<11>(value.output_addr), packed);
        compute_write = true;
    }

    if (product_reg.get().valid) {
        ProductStage value = product_reg.get();
        Int<32> a0 = sign_extend_product(Int<16>(value.products.at<15, 0>()));
        Int<32> a1 = sign_extend_product(Int<16>(value.products.at<31, 16>()));
        Int<32> a2 = sign_extend_product(Int<16>(value.products.at<47, 32>()));
        Int<32> a3 = sign_extend_product(Int<16>(value.products.at<63, 48>()));
        Int<32> a4 = sign_extend_product(Int<16>(value.products.at<79, 64>()));
        Int<32> a5 = sign_extend_product(Int<16>(value.products.at<95, 80>()));
        Int<32> a6 = sign_extend_product(Int<16>(value.products.at<111, 96>()));
        Int<32> a7 = sign_extend_product(Int<16>(value.products.at<127, 112>()));
        Int<32> a8 = sign_extend_product(Int<16>(value.products.at<143, 128>()));
        Int<32> b0 = sign_extend_product(Int<16>(value.products.at<159, 144>()));
        Int<32> b1 = sign_extend_product(Int<16>(value.products.at<175, 160>()));
        Int<32> b2 = sign_extend_product(Int<16>(value.products.at<191, 176>()));
        Int<32> b3 = sign_extend_product(Int<16>(value.products.at<207, 192>()));
        Int<32> b4 = sign_extend_product(Int<16>(value.products.at<223, 208>()));
        Int<32> b5 = sign_extend_product(Int<16>(value.products.at<239, 224>()));
        Int<32> b6 = sign_extend_product(Int<16>(value.products.at<255, 240>()));
        Int<32> b7 = sign_extend_product(Int<16>(value.products.at<271, 256>()));
        Int<32> b8 = sign_extend_product(Int<16>(value.products.at<287, 272>()));

        Int<32> a01 = Int<32>(a0 + a1);
        Int<32> a23 = Int<32>(a2 + a3);
        Int<32> a45 = Int<32>(a4 + a5);
        Int<32> a67 = Int<32>(a6 + a7);
        Int<32> a0123 = Int<32>(a01 + a23);
        Int<32> a4567 = Int<32>(a45 + a67);
        Int<32> a01234567 = Int<32>(a0123 + a4567);
        Int<32> sum_a = Int<32>(a01234567 + a8);

        Int<32> b01 = Int<32>(b0 + b1);
        Int<32> b23 = Int<32>(b2 + b3);
        Int<32> b45 = Int<32>(b4 + b5);
        Int<32> b67 = Int<32>(b6 + b7);
        Int<32> b0123 = Int<32>(b01 + b23);
        Int<32> b4567 = Int<32>(b45 + b67);
        Int<32> b01234567 = Int<32>(b0123 + b4567);
        Int<32> sum_b = Int<32>(b01234567 + b8);

        next_sum.even_value = sum_a.to<int32_t>();
        next_sum.odd_value = sum_b.to<int32_t>();
        next_sum.output_addr = value.output_addr;
        next_sum.valid = true;
    }

    if (window_reg.get().valid) {
        WindowStage value = window_reg.get();
        ProductStage next{};
        next.output_addr = value.output_addr;
        next.valid = true;
        Int<72> coeffs = kernel_coeffs.get();
        Int<8> x0 = Int<8>(value.even_pixels.at<7, 0>());
        Int<8> x1 = Int<8>(value.even_pixels.at<15, 8>());
        Int<8> x2 = Int<8>(value.even_pixels.at<23, 16>());
        Int<8> x3 = Int<8>(value.even_pixels.at<31, 24>());
        Int<8> x4 = Int<8>(value.even_pixels.at<39, 32>());
        Int<8> x5 = Int<8>(value.even_pixels.at<47, 40>());
        Int<8> x6 = Int<8>(value.even_pixels.at<55, 48>());
        Int<8> x7 = Int<8>(value.even_pixels.at<63, 56>());
        Int<8> x8 = Int<8>(value.even_pixels.at<71, 64>());
        Int<8> x9 = Int<8>(value.odd_pixels.at<7, 0>());
        Int<8> x10 = Int<8>(value.odd_pixels.at<15, 8>());
        Int<8> x11 = Int<8>(value.odd_pixels.at<23, 16>());
        Int<8> x12 = Int<8>(value.odd_pixels.at<31, 24>());
        Int<8> x13 = Int<8>(value.odd_pixels.at<39, 32>());
        Int<8> x14 = Int<8>(value.odd_pixels.at<47, 40>());
        Int<8> x15 = Int<8>(value.odd_pixels.at<55, 48>());
        Int<8> x16 = Int<8>(value.odd_pixels.at<63, 56>());
        Int<8> x17 = Int<8>(value.odd_pixels.at<71, 64>());
        next.products.at<15, 0>() = Int<16>(x0.sint() * Int<8>(coeffs.at<7, 0>()).sint());
        next.products.at<31, 16>() = Int<16>(x1.sint() * Int<8>(coeffs.at<15, 8>()).sint());
        next.products.at<47, 32>() = Int<16>(x2.sint() * Int<8>(coeffs.at<23, 16>()).sint());
        next.products.at<63, 48>() = Int<16>(x3.sint() * Int<8>(coeffs.at<31, 24>()).sint());
        next.products.at<79, 64>() = Int<16>(x4.sint() * Int<8>(coeffs.at<39, 32>()).sint());
        next.products.at<95, 80>() = Int<16>(x5.sint() * Int<8>(coeffs.at<47, 40>()).sint());
        next.products.at<111, 96>() = Int<16>(x6.sint() * Int<8>(coeffs.at<55, 48>()).sint());
        next.products.at<127, 112>() = Int<16>(x7.sint() * Int<8>(coeffs.at<63, 56>()).sint());
        next.products.at<143, 128>() = Int<16>(x8.sint() * Int<8>(coeffs.at<71, 64>()).sint());
        next.products.at<159, 144>() = Int<16>(x9.sint() * Int<8>(coeffs.at<7, 0>()).sint());
        next.products.at<175, 160>() = Int<16>(x10.sint() * Int<8>(coeffs.at<15, 8>()).sint());
        next.products.at<191, 176>() = Int<16>(x11.sint() * Int<8>(coeffs.at<23, 16>()).sint());
        next.products.at<207, 192>() = Int<16>(x12.sint() * Int<8>(coeffs.at<31, 24>()).sint());
        next.products.at<223, 208>() = Int<16>(x13.sint() * Int<8>(coeffs.at<39, 32>()).sint());
        next.products.at<239, 224>() = Int<16>(x14.sint() * Int<8>(coeffs.at<47, 40>()).sint());
        next.products.at<255, 240>() = Int<16>(x15.sint() * Int<8>(coeffs.at<55, 48>()).sint());
        next.products.at<271, 256>() = Int<16>(x16.sint() * Int<8>(coeffs.at<63, 56>()).sint());
        next.products.at<287, 272>() = Int<16>(x17.sint() * Int<8>(coeffs.at<71, 64>()).sint());
        next_product = next;
    }

    uint32_t current_state = state.get();
    bool start_task = false;
    ConvCommand command{};
    if (input_queue.deqvalid()) {
        command = input_queue.front();
        if (command.op == 1) {
            if (!compute_write) {
                data_mem.write<0>(Int<11>(command.address), Int<64>(command.data));
                input_queue.deqnext();
            }
        } else if (command.op == 2 && !busy && current_state == kIdle &&
                   !window_reg.get().valid && !product_reg.get().valid && !sum_reg.get().valid) {
            input_queue.deqnext();
            start_task = true;
        }
    }

    if (start_task) {
        current_input.setnext(command.input_base);
        current_output.setnext(command.output_base);
        current_kernel.setnext(command.kernel_index);
        busy.setnext(true);
        done.setnext(false);
        kernel_rom.readreq<0>(Int<7>(command.kernel_index));
        state.setnext(kKernelWait);
        next_input_row.setnext(0);
        read_gap.setnext(0);
        output_row.setnext(0);
        output_pair.setnext(0);
    }

    bool issue_engine_read = false;
    uint32_t issue_row = 0;
    if (!start_task && current_state == kKernelWait) {
        kernel_coeffs.setnext(kernel_rom.readdata<0>());
        issue_engine_read = true;
        issue_row = 0;
        next_input_row.setnext(1);
        state.setnext(kRows);
    } else if (!start_task && current_state == kRows) {
        uint32_t requested = next_input_row.get();
        uint32_t gap = read_gap.get();
        if (requested == 1) {
            if (engine_row_valid && returned_row_index == 0) {
                issue_engine_read = true;
                issue_row = 1;
                next_input_row.setnext(2);
                read_gap.setnext(3);
            }
        } else if (requested < 8) {
            if (gap == 0) {
                issue_engine_read = true;
                issue_row = requested;
                next_input_row.setnext(requested + 1);
                read_gap.setnext(3);
            } else {
                read_gap.setnext(gap - 1);
            }
        }
    }

    if (engine_row_valid) {
        uint32_t slot = returned_row_index % 3;
        if (slot == 0) row0.setnext(returned_row);
        if (slot == 1) row1.setnext(returned_row);
        if (slot == 2) row2.setnext(returned_row);

        if (returned_row_index >= 1 && returned_row_index <= 7) {
            uint32_t y = returned_row_index - 1;
            output_row.setnext(y);
        }
    }

    bool launch_window = false;
    uint32_t win_y = output_row.get();
    uint32_t win_pair = output_pair.get();
    if (engine_row_valid && returned_row_index >= 1 && returned_row_index <= 7) {
        launch_window = true;
        win_y = returned_row_index - 1;
        win_pair = 0;
    } else if (current_state == kRows && next_input_row.get() >= 2 &&
               next_input_row.get() <= 8 && output_pair.get() < 4) {
        launch_window = true;
    }

    if (current_state == kRows && next_input_row.get() == 8 && output_row.get() == 6 &&
        output_pair.get() == 0 && !engine_row_valid) {
        output_row.setnext(7);
        output_pair.setnext(0);
        win_y = 7;
        win_pair = 0;
        launch_window = true;
    }

    if (launch_window) {
        Int<64> r0 = row0.get();
        Int<64> r1 = row1.get();
        Int<64> r2 = row2.get();
        if (engine_row_valid) {
            if (returned_row_index % 3 == 0) r0 = returned_row;
            if (returned_row_index % 3 == 1) r1 = returned_row;
            if (returned_row_index % 3 == 2) r2 = returned_row;
        }
        Int<64> top = 0;
        Int<64> middle = 0;
        Int<64> bottom = 0;
        if (win_y == 0) {
            middle = r0;
            bottom = r1;
        } else if (win_y == 7) {
            if ((win_y - 1) % 3 == 0) top = r0;
            if ((win_y - 1) % 3 == 1) top = r1;
            if ((win_y - 1) % 3 == 2) top = r2;
            if (win_y % 3 == 0) middle = r0;
            if (win_y % 3 == 1) middle = r1;
            if (win_y % 3 == 2) middle = r2;
        } else {
            if ((win_y - 1) % 3 == 0) top = r0;
            if ((win_y - 1) % 3 == 1) top = r1;
            if ((win_y - 1) % 3 == 2) top = r2;
            if (win_y % 3 == 0) middle = r0;
            if (win_y % 3 == 1) middle = r1;
            if (win_y % 3 == 2) middle = r2;
            if ((win_y + 1) % 3 == 0) bottom = r0;
            if ((win_y + 1) % 3 == 1) bottom = r1;
            if ((win_y + 1) % 3 == 2) bottom = r2;
        }

        WindowStage w{};
        w.out_row = win_y;
        w.pair = win_pair;
        w.output_addr = current_output.get() + win_y * 4 + win_pair;
        w.valid = true;
        int32_t first_col = static_cast<int32_t>(win_pair * 2);
        w.even_pixels.at<7, 0>() = select_pixel(top, first_col + 0 - 1);
        w.odd_pixels.at<7, 0>() = select_pixel(top, first_col + 0 - 1 + 1);
        w.even_pixels.at<15, 8>() = select_pixel(top, first_col + 1 - 1);
        w.odd_pixels.at<15, 8>() = select_pixel(top, first_col + 1 - 1 + 1);
        w.even_pixels.at<23, 16>() = select_pixel(top, first_col + 2 - 1);
        w.odd_pixels.at<23, 16>() = select_pixel(top, first_col + 2 - 1 + 1);
        w.even_pixels.at<31, 24>() = select_pixel(middle, first_col + 0 - 1);
        w.odd_pixels.at<31, 24>() = select_pixel(middle, first_col + 0 - 1 + 1);
        w.even_pixels.at<39, 32>() = select_pixel(middle, first_col + 1 - 1);
        w.odd_pixels.at<39, 32>() = select_pixel(middle, first_col + 1 - 1 + 1);
        w.even_pixels.at<47, 40>() = select_pixel(middle, first_col + 2 - 1);
        w.odd_pixels.at<47, 40>() = select_pixel(middle, first_col + 2 - 1 + 1);
        w.even_pixels.at<55, 48>() = select_pixel(bottom, first_col + 0 - 1);
        w.odd_pixels.at<55, 48>() = select_pixel(bottom, first_col + 0 - 1 + 1);
        w.even_pixels.at<63, 56>() = select_pixel(bottom, first_col + 1 - 1);
        w.odd_pixels.at<63, 56>() = select_pixel(bottom, first_col + 1 - 1 + 1);
        w.even_pixels.at<71, 64>() = select_pixel(bottom, first_col + 2 - 1);
        w.odd_pixels.at<71, 64>() = select_pixel(bottom, first_col + 2 - 1 + 1);
        next_window = w;
        if (win_pair < 3) {
            output_pair.setnext(win_pair + 1);
        } else {
            output_pair.setnext(0);
            if (win_y < 7) output_row.setnext(win_y + 1);
            else state.setnext(kDrain);
        }
    }

    if (issue_engine_read) {
        Int<11> input_base = Int<11>(current_input.get());
        Int<11> row_offset = Int<11>(issue_row);
        Int<12> wide_input_address = input_base + row_offset;
        Int<11> input_address = Int<11>(wide_input_address.at<10, 0>());
        data_mem.readreq<0>(input_address);
        ReadMeta meta{};
        meta.owner = kEngineRead;
        meta.row = issue_row;
        read_meta.setnext(meta);
    } else if (dma_read_queue.deqvalid()) {
        uint32_t address = dma_read_queue.front();
        data_mem.readreq<0>(Int<11>(address));
        dma_read_queue.deqnext();
        ReadMeta meta{};
        meta.owner = kDmaRead;
        meta.row = 0;
        read_meta.setnext(meta);
    } else {
        ReadMeta meta{};
        meta.owner = kNoRead;
        meta.row = 0;
        read_meta.setnext(meta);
    }

    window_reg.setnext(next_window);
    product_reg.setnext(next_product);
    sum_reg.setnext(next_sum);

    if (current_state == kDrain && !next_window.valid && !next_product.valid && !next_sum.valid) {
        busy.setnext(false);
        done.setnext(true);
        completed_count.setnext(completed_count.get() + 1);
        state.setnext(kIdle);
    }
}
