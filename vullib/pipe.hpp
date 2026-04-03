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

namespace vulpipe {

/**
 * Pipe Template Class Interface Definition
 * template<typename T, ...>
 * class xxxPipe {
 * public:
 *   bool can_push();
 *   bool push(const T &data);
 *   bool can_pop();
 *   bool pop(T &data);
 *   bool top(T &data);
 *   void clear(); // clear at next "apply_next_tick"
 * 
 *   void apply_next_tick();
 * }
 */

template<typename T, uint32_t Depth>
class PipeNoBlock {
    static_assert(Depth >= 1, "Depth must be at least 1");
private:
    std::deque<T> pending_;
    uint32_t ticks_to_ready_ = 0;
    bool has_data_ = false;
    bool pushed_this_tick_ = false;
    bool popped_this_tick_ = false;
    bool clear_called_ = false;
    bool clear_this_tick_ = false;

public:
    bool can_push() {
        return !clear_this_tick_ && !pushed_this_tick_;
    }

    bool push(const T &data) {
        if (!can_push()) return false;
        pending_.push_back(data);
        if (!has_data_) {
            ticks_to_ready_ = Depth;
            has_data_ = true;
        }
        pushed_this_tick_ = true;
        return true;
    }

    bool can_pop() {
        return has_data_ && ticks_to_ready_ == 0;
    }

    bool pop(T &data) {
        if (!can_pop() || popped_this_tick_) return false;
        data = pending_.front();
        pending_.pop_front();
        popped_this_tick_ = true;
        if (!pending_.empty()) {
            ticks_to_ready_ = 1; // 下一个数据剩余1周期
        } else {
            has_data_ = false;
        }
        return true;
    }

    bool top(T &data) {
        if (!can_pop()) return false;
        data = pending_.front();
        return true;
    }

    void clear() {
        clear_called_ = true;
        clear_this_tick_ = true;
    }

    void apply_next_tick() {
        if (clear_called_) {
            pending_.clear();
            has_data_ = false;
            ticks_to_ready_ = 0;
            clear_called_ = false;
        }
        if (has_data_ && ticks_to_ready_ > 0) {
            ticks_to_ready_--;
        }
        pushed_this_tick_ = false;
        popped_this_tick_ = false;
        clear_this_tick_ = false;
    }
};

// 特化Depth=1的版本，实现apply_next_tick完全无分支指令
template<typename T>
class PipeNoBlockDepth1 {
private:
    T pending_data_;
    bool has_pending_ = false;
    T ready_data_;
    bool has_ready_ = false;
    bool clear_pending_ = false;
    bool pushed_this_tick_ = false;
    bool popped_this_tick_ = false;
    bool clear_this_tick_ = false;
    bool clear_called_ = false;

public:
    bool can_push() {
        return !clear_this_tick_ && !pushed_this_tick_;
    }

    bool push(const T &data) {
        if (!can_push()) return false;
        pending_data_ = data;
        has_pending_ = true;
        pushed_this_tick_ = true;
        return true;
    }

    bool can_pop() {
        return has_ready_;
    }

    bool pop(T &data) {
        if (!can_pop() || popped_this_tick_) return false;
        data = ready_data_;
        has_ready_ = false;
        popped_this_tick_ = true;
        return true;
    }

    bool top(T &data) {
        if (!can_pop()) return false;
        data = ready_data_;
        return true;
    }

    void clear() {
        clear_pending_ = true;
        clear_called_ = true;
        clear_this_tick_ = true;
    }

    void apply_next_tick() {
        // 完全无分支指令
        ready_data_ = pending_data_;
        has_ready_ = has_pending_;
        pending_data_ = T{};
        has_pending_ = false;
        has_ready_ &= !clear_pending_;
        has_pending_ &= !clear_pending_;  // redundant but for consistency
        clear_pending_ = false;
        pushed_this_tick_ = false;
        popped_this_tick_ = false;
        clear_this_tick_ = false;
        clear_called_ = false;
    }
};

// 特化Depth=1的版本，使用PipeNoBlockDepth1
template<typename T>
class PipeNoBlock<T, 1> {
    PipeNoBlockDepth1<T> impl_;

public:
    bool can_push() { return impl_.can_push(); }
    bool push(const T &data) { return impl_.push(data); }
    bool can_pop() { return impl_.can_pop(); }
    bool pop(T &data) { return impl_.pop(data); }
    bool top(T &data) { return impl_.top(data); }
    void clear() { impl_.clear(); }
    void apply_next_tick() { impl_.apply_next_tick(); }
};

// 多端口无阻塞管道模板类
template<typename T, uint32_t Depth, uint32_t Width>
class PipeNoBlockMultiPort {
    static_assert(Depth >= 1, "Depth must be at least 1");
    static_assert(Width >= 1, "Width must be at least 1");
private:
    std::array<std::deque<T>, Depth> stages_;
    uint32_t push_count_this_tick_ = 0;
    uint32_t pop_count_this_tick_ = 0;
    bool clear_called_ = false;
    bool clear_this_tick_ = false;

public:
    bool can_push() {
        return !clear_this_tick_ && push_count_this_tick_ < Width;
    }

    bool push(const T &data) {
        if (!can_push()) return false;
        stages_[Depth - 1].push_back(data);
        push_count_this_tick_++;
        return true;
    }

    bool can_pop() {
        return !stages_[0].empty();
    }

    bool pop(T &data) {
        if (!can_pop() || pop_count_this_tick_ >= Width) return false;
        data = stages_[0].front();
        stages_[0].pop_front();
        pop_count_this_tick_++;
        return true;
    }

    bool top(T &data) {
        if (!can_pop()) return false;
        data = stages_[0].front();
        return true;
    }

    void clear() {
        clear_called_ = true;
        clear_this_tick_ = true;
    }

    void apply_next_tick() {
        if (clear_called_) {
            for (auto &s : stages_) s.clear();
            clear_called_ = false;
        }
        std::deque<T> temp = std::move(stages_[0]);
        for (size_t i = 0; i < Depth - 1; ++i) {
            stages_[i] = std::move(stages_[i + 1]);
        }
        stages_[Depth - 1] = std::move(temp);
        temp.clear();
        push_count_this_tick_ = 0;
        pop_count_this_tick_ = 0;
        clear_this_tick_ = false;
    }
};

template <typename T, uint32_t Depth>
class PipeNoBlockMultiPort<T, Depth, 1> {
    PipeNoBlock<T, Depth> impl_;

public:
    bool can_push() { return impl_.can_push(); }
    bool push(const T &data) { return impl_.push(data); }
    bool can_pop() { return impl_.can_pop(); }
    bool pop(T &data) { return impl_.pop(data); }
    bool top(T &data) { return impl_.top(data); }
    void clear() { impl_.clear(); }
    void apply_next_tick() { impl_.apply_next_tick(); }
};

/**
 * MUST GUARANTEE that pop/canpop/top calls are earlier than push/canpush calls in the same tick
 */
template<typename T>
class PipeDepth1 {
private:
    T data_;
    uint8_t has_data_ = 0;
    uint8_t push_count_ = 0;
    uint8_t pop_count_ = 0;
    uint8_t clear_called_ = 0;
public:
    bool can_push() {
        return (~has_data_ & 0xff) & (~push_count_ & 0xff);
    }

    bool push(const T &data) {
        uint8_t can = (~has_data_ & 0xff) & (~push_count_ & 0xff);
        data_ = can ? data : data_;
        has_data_ = can | (~can & has_data_);
        push_count_ = can | (~can & push_count_);
        return can;
    }

    bool can_pop() {
        return (has_data_ & 0xff) & (~pop_count_ & 0xff);
    }

    bool pop(T &data) {
        uint8_t can = (has_data_ & 0xff) & (~pop_count_ & 0xff);
        data = can ? data_ : data;
        has_data_ = (~can & 0xff) & has_data_;
        pop_count_ = can | (~can & pop_count_);
        return can;
    }

    bool top(T &data) {
        uint8_t can = has_data_ & 0xff;
        data = can ? data_ : data;
        return can;
    }

    void clear() {
        clear_called_ = 0xff;
    }

    void apply_next_tick() {
        uint8_t clear = clear_called_ & 0xff;
        has_data_ = (~clear & 0xff) & has_data_;
        clear_called_ = (~clear & 0xff) & clear_called_;
        push_count_ = 0;
        pop_count_ = 0;
    }
};

/**
 * Buffered FIFO with handshake, single push/pop per tick, no order constraint.
 */
template<typename T, uint32_t BufSize>
class PipeBufferedDepth1 {
    static_assert(BufSize >= 1, "BufSize must be at least 1");
private:
    std::deque<T> queue_;
    T pop_buffer_;
    bool has_pop_buffer_ = false;
    T pending_push_;
    bool has_pending_push_ = false;
    uint32_t push_count_this_tick_ = 0;
    uint32_t pop_count_this_tick_ = 0;
    bool clear_called_ = false;
    bool full_ = false;

public:
    bool can_push() {
        return !full_ && push_count_this_tick_ < 1;
    }

    bool push(const T &data) {
        if (!can_push()) return false;
        pending_push_ = data;
        has_pending_push_ = true;
        push_count_this_tick_++;
        return true;
    }

    bool can_pop() {
        return (has_pop_buffer_ || !queue_.empty()) && pop_count_this_tick_ < 1;
    }

    bool pop(T &data) {
        if (!can_pop()) return false;
        if (has_pop_buffer_) {
            data = pop_buffer_;
            has_pop_buffer_ = false;
        } else {
            data = queue_.front();
            queue_.pop_front();
        }
        pop_count_this_tick_++;
        return true;
    }

    bool top(T &data) {
        if (!can_pop()) return false;
        if (has_pop_buffer_) {
            data = pop_buffer_;
        } else {
            data = queue_.front();
        }
        return true;
    }

    void clear() {
        clear_called_ = true;
    }

    void apply_next_tick() {
        if (clear_called_) {
            queue_.clear();
            has_pop_buffer_ = false;
            clear_called_ = false;
        }
        if (has_pending_push_) {
            if (!has_pop_buffer_ && queue_.empty()) {
                pop_buffer_ = pending_push_;
                has_pop_buffer_ = true;
            } else {
                queue_.push_back(pending_push_);
            }
            has_pending_push_ = false;
        }
        uint32_t current_size = queue_.size() + (has_pop_buffer_ ? 1 : 0) + (has_pending_push_ ? 1 : 0);
        full_ = (current_size >= BufSize + 1);
        push_count_this_tick_ = 0;
        pop_count_this_tick_ = 0;
    }
};

template<typename T>
class PipeBufferedDepth1<T, 0> {
    PipeDepth1<T> impl_;
public:
    bool can_push() { return impl_.can_push(); }
    bool push(const T &data) { return impl_.push(data); }
    bool can_pop() { return impl_.can_pop(); }
    bool pop(T &data) { return impl_.pop(data); }
    bool top(T &data) { return impl_.top(data); }
    void clear() { impl_.clear(); }
    void apply_next_tick() { impl_.apply_next_tick(); }
};

/**
 * Buffered FIFO with handshake, multi-port, single depth, no order constraint.
 * Allows up to PushWidth pushes and PopWidth pops per tick.
 * Max elements: BufSize + 1 in queue.
 */
template<typename T, uint32_t PushWidth, uint32_t PopWidth, uint32_t BufSize>
class PipeBufferedMultiPortDepth1 {
    static_assert(PushWidth >= 1, "PushWidth must be at least 1");
    static_assert(PopWidth >= 1, "PopWidth must be at least 1");
    static_assert(BufSize >= 1, "BufSize must be at least 1");
private:
    std::deque<T> queue_;
    std::deque<T> pending_pushes_;
    uint32_t push_count_this_tick_ = 0;
    uint32_t pop_count_this_tick_ = 0;
    bool clear_called_ = false;

public:
    bool can_push() {
        uint32_t current_elements = queue_.size() + pending_pushes_.size();
        return push_count_this_tick_ < PushWidth && (current_elements + 1 <= BufSize + 1);
    }

    bool push(const T &data) {
        if (!can_push()) return false;
        pending_pushes_.push_back(data);
        push_count_this_tick_++;
        return true;
    }

    bool can_pop() {
        return !queue_.empty() && pop_count_this_tick_ < PopWidth;
    }

    bool pop(T &data) {
        if (!can_pop()) return false;
        data = queue_.front();
        queue_.pop_front();
        pop_count_this_tick_++;
        return true;
    }

    bool top(T &data) {
        if (!can_pop()) return false;
        data = queue_.front();
        return true;
    }

    void clear() {
        clear_called_ = true;
    }

    void apply_next_tick() {
        if (clear_called_) {
            queue_.clear();
            pending_pushes_.clear();
            clear_called_ = false;
        }
        for (const auto &pending : pending_pushes_) {
            queue_.push_back(pending);
        }
        pending_pushes_.clear();
        push_count_this_tick_ = 0;
        pop_count_this_tick_ = 0;
    }
};

template<typename T, uint32_t PushWidth, uint32_t PopWidth, uint32_t BufSize, uint32_t Depth, bool Handshake>
struct PipeSelector {
    using type = 
        std::conditional_t<Handshake && Depth == 1 && PushWidth == 1 && PopWidth == 1 && BufSize == 0, PipeDepth1<T>,
        std::conditional_t<Handshake && Depth == 1 && PushWidth == 1 && PopWidth == 1, PipeBufferedDepth1<T, BufSize>,
        std::conditional_t<Handshake && Depth == 1, PipeBufferedMultiPortDepth1<T, PushWidth, PopWidth, BufSize>,
        std::conditional_t<!Handshake && BufSize == 0 && PushWidth == 1 && PopWidth == 1 && Depth == 1, PipeNoBlockDepth1<T>,
        std::conditional_t<!Handshake && BufSize == 0 && PushWidth == 1 && PopWidth == 1, PipeNoBlock<T, Depth>,
        std::conditional_t<!Handshake && BufSize == 0 && PushWidth == PopWidth, PipeNoBlockMultiPort<T, Depth, PushWidth>,
        void>>>>>>;
    static_assert(!std::is_same_v<type, void>, "No matching pipe type for the given parameters");
};

template<typename T, uint32_t PushWidth, uint32_t PopWidth, uint32_t BufSize, uint32_t Depth, bool Handshake>
class VulPipe {
    using Impl = typename PipeSelector<T, PushWidth, PopWidth, BufSize, Depth, Handshake>::type;
    Impl impl_;

public:
    bool can_push() { return impl_.can_push(); }
    bool push(const T &data) { return impl_.push(data); }
    bool can_pop() { return impl_.can_pop(); }
    bool pop(T &data) { return impl_.pop(data); }
    bool top(T &data) { return impl_.top(data); }
    void clear() { impl_.clear(); }
    void apply_next_tick() { impl_.apply_next_tick(); }
};

} // namespace vulpipe

using vulpipe::VulPipe;


