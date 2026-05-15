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
#include <array>

template<typename T, uint32_t Depth>
class VulQueue {
    static_assert(Depth >= 1, "Depth must be at least 1");
public:
    VulQueue() = default;

    bool enqready() const {
        return !enq_pending_ && size_ < Depth;
    }

    bool deqvalid() const {
        return deq_buf_valid_ && !deq_pending_;
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
    VulQueueMP() = default;

    uint32_t enqreqdy() const {
        if (enq_called_this_tick_) {
            return 0;
        }
        const uint32_t free_slots = Depth - size_;
        return free_slots < EnqWidth ? free_slots : EnqWidth;
    }

    uint32_t deqvalid() const {
        if (deq_called_this_tick_) {
            return 0;
        }
        return deq_valid_num_;
    }

    uint32_t enqnext(const std::array<T, EnqWidth> &values, const uint32_t num = EnqWidth) {
        if (enq_called_this_tick_) {
            return 0;
        }
        const uint32_t req = num < EnqWidth ? num : EnqWidth;
        const uint32_t rdy = enqreqdy();
        const uint32_t accepted = req < rdy ? req : rdy;
        for (uint32_t i = 0; i < accepted; ++i) {
            enq_buf_[i] = values[i];
        }
        enq_pending_num_ = accepted;
        enq_called_this_tick_ = true;
        return accepted;
    }

    const std::array<T, DeqWidth>& deqnext(const uint32_t num = DeqWidth) {
        if (!deq_called_this_tick_) {
            const uint32_t req = num < DeqWidth ? num : DeqWidth;
            const uint32_t valid = deqvalid();
            deq_pending_num_ = req < valid ? req : valid;
            deq_called_this_tick_ = true;
        }
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

        uint32_t pop_num = deq_pending_num_ < size_ ? deq_pending_num_ : size_;
        while (pop_num > 0) {
            if (++head_ == Depth) {
                head_ = 0;
            }
            --size_;
            --pop_num;
        }

        uint32_t can_push = Depth - size_;
        uint32_t push_num = enq_pending_num_ < can_push ? enq_pending_num_ : can_push;
        for (uint32_t i = 0; i < push_num; ++i) {
            data_[tail_] = enq_buf_[i];
            if (++tail_ == Depth) {
                tail_ = 0;
            }
            ++size_;
        }

        enq_pending_num_ = 0;
        deq_pending_num_ = 0;
        enq_called_this_tick_ = false;
        deq_called_this_tick_ = false;

        deq_valid_num_ = size_ < DeqWidth ? size_ : DeqWidth;
        uint32_t idx = head_;
        for (uint32_t i = 0; i < deq_valid_num_; ++i) {
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
    uint32_t size_ = 0;

    std::array<T, EnqWidth> enq_buf_{};
    uint32_t enq_pending_num_ = 0;
    bool enq_called_this_tick_ = false;

    std::array<T, DeqWidth> deq_buf_{};
    uint32_t deq_valid_num_ = 0;
    uint32_t deq_pending_num_ = 0;
    bool deq_called_this_tick_ = false;

    bool clr_pending_ = false;
};
