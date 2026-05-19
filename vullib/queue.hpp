// MIT License

// Copyright (c) 2025 Meng Chengzhen, in Shandong University

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "common.h"
#include "uint.hpp"
#include <array>

template<typename T, uint32_t Depth>
class VulQueue {
    static_assert(Depth >= 1, "Depth must be at least 1");
public:
    VulQueue() = default;

    bool enqready() const {
        return size_ < Depth;
    }

    bool deqvalid() const {
        return deq_buf_valid_;
    }

    bool enqnext(const T &value) {
        if (!enqready()) {
            return false;
        }
        enq_buf_ = value;
        enq_pending_ = true;
        return true;
    }

    const T& deqnext() {
        deq_pending_ = true;
        return deq_buf_;
    }

    void clrnext() {
        clr_pending_ = true;
    }

    void apply_next_tick() {
        if (clr_pending_) {
            head_ = 0;
            tail_ = 0;
            size_ = 0;
            clr_pending_ = false;
        }

        if (deq_pending_ && size_ > 0) {
            head_ = (head_ + 1) % Depth;
            size_--;
        }

        if (enq_pending_ && size_ < Depth) {
            data_[tail_] = enq_buf_;
            tail_ = (tail_ + 1) % Depth;
            size_++;
        }

        deq_pending_ = false;
        enq_pending_ = false;

        if (size_ > 0) {
            deq_buf_ = data_[head_];
            deq_buf_valid_ = true;
        } else {
            deq_buf_valid_ = false;
        }
    }

private:
    std::array<T, Depth> data_{};
    uint32_t head_ = 0;
    uint32_t tail_ = 0;
    uint32_t size_ = 0;

    T enq_buf_{};
    bool enq_pending_ = false;

    T deq_buf_{};
    bool deq_buf_valid_ = false;
    bool deq_pending_ = false;

    bool clr_pending_ = false;
};

template<typename T, uint32_t Depth, uint32_t EnqWidth, uint32_t DeqWidth>
class VulQueueMP {
    static_assert(Depth >= 1, "Depth must be at least 1");
    static_assert(EnqWidth >= 1, "EnqWidth must be at least 1");
    static_assert(DeqWidth >= 1, "DeqWidth must be at least 1");
public:
    static constexpr uint32_t EnqCntWidth = log2ceil(EnqWidth + 1);
    static constexpr uint32_t DeqCntWidth = log2ceil(DeqWidth + 1);
    static constexpr uint32_t DepthWidth = log2ceil(Depth + 1);

    using EnqCntUInt = UInt<EnqCntWidth>;
    using DeqCntUInt = UInt<DeqCntWidth>;
    using DepthCntUInt = UInt<DepthWidth>;

    VulQueueMP() = default;

    EnqCntUInt enqreqdy() const {
        const uint32_t free_slots = Depth - size_.get_u32();
        return free_slots < EnqWidth ? EnqCntUInt(free_slots) : EnqCntUInt(EnqWidth);
    }

    DeqCntUInt deqvalid() const {
        return deq_valid_num_;
    }

    EnqCntUInt enqnext(const std::array<T, EnqWidth> &values, const EnqCntUInt num = EnqWidth) {
        const EnqCntUInt req = num < EnqCntUInt(EnqWidth) ? num : EnqCntUInt(EnqWidth);
        const EnqCntUInt rdy = enqreqdy();
        const EnqCntUInt accepted = req < rdy ? req : rdy;
        const uint32_t accepted_u32 = accepted.get_u32();
        for (uint32_t i = 0; i < accepted_u32; ++i) {
            enq_buf_[i] = values[i];
        }
        enq_pending_num_ = accepted;
        return accepted;
    }

    const std::array<T, DeqWidth>& deqnext(const DeqCntUInt num = DeqWidth) {
        const DeqCntUInt req = num < DeqCntUInt(DeqWidth) ? num : DeqCntUInt(DeqWidth);
        const DeqCntUInt valid = deqvalid();
        deq_pending_num_ = req < valid ? req : valid;
        return deq_buf_;
    }

    void clrnext() {
        clr_pending_ = true;
    }

    void apply_next_tick() {
        if (clr_pending_) {
            head_ = 0;
            tail_ = 0;
            size_ = 0;
            clr_pending_ = false;
        }

        uint32_t pop_num = deq_pending_num_.get_u32();
        const uint32_t size_u32 = size_.get_u32();
        if (pop_num > size_u32) {
            pop_num = size_u32;
        }
        while (pop_num > 0) {
            if (++head_ == Depth) {
                head_ = 0;
            }
            size_ = size_ - DepthCntUInt(1);
            --pop_num;
        }

        const uint32_t can_push = Depth - size_.get_u32();
        uint32_t push_num = enq_pending_num_.get_u32();
        if (push_num > can_push) {
            push_num = can_push;
        }
        for (uint32_t i = 0; i < push_num; ++i) {
            data_[tail_] = enq_buf_[i];
            if (++tail_ == Depth) {
                tail_ = 0;
            }
            size_ = size_ + DepthCntUInt(1);
        }

        enq_pending_num_ = 0;
        deq_pending_num_ = 0;

        deq_valid_num_ = size_.get_u32() < DeqWidth ? DeqCntUInt(size_.get_u32()) : DeqCntUInt(DeqWidth);
        uint32_t idx = head_;
        const uint32_t deq_valid_num_u32 = deq_valid_num_.get_u32();
        for (uint32_t i = 0; i < deq_valid_num_u32; ++i) {
            deq_buf_[i] = data_[idx];
            if (++idx == Depth) {
                idx = 0;
            }
        }
    }

private:
    std::array<T, Depth> data_{};
    uint32_t head_ = 0;
    uint32_t tail_ = 0;
    DepthCntUInt size_ = 0;

    std::array<T, EnqWidth> enq_buf_{};
    EnqCntUInt enq_pending_num_ = 0;

    std::array<T, DeqWidth> deq_buf_{};
    DeqCntUInt deq_valid_num_ = 0;
    DeqCntUInt deq_pending_num_ = 0;

    bool clr_pending_ = false;
};
