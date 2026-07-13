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

#include <assert.h>

using std::array;

namespace vulstorage {

template<typename T, uint32_t WRPortNum>
class VulRegisterImpl {
public:
    static_assert(WRPortNum >= 1, "WRPortNum must be at least 1");
    static_assert(WRPortNum < 64, "WRPortNum must be less than 64");

    template <uint32_t P = 0>
    void setnext(const T &value) {
        static_assert(P < WRPortNum);
        if (reset_next_ || hold_next_) {
            return;
        }
        assert((issued_write_ports_ & (uint64_t(1) << P)) == 0 &&
               "VulRegister::setnext() may only be called once per cycle for each write port");
        issued_write_ports_ |= uint64_t(1) << P;
        if (P < pending_write_ports_) {
            next_ = value;
            pending_write_ports_ = P;
        }
    }

    T get() const {
        return data_;
    }

    operator const T&() const {
        return data_;
    }

    void apply_next_tick() {
        if (reset_next_) {
            data_ = reset_value_;
            next_ = reset_value_;
        } else if (hold_next_) {
            next_ = data_;
        } else {
            data_ = next_;
        }
        pending_write_ports_ = WRPortNum;
        issued_write_ports_ = 0;
        hold_next_ = false;
        reset_next_ = false;
    }

    void holdnext() {
        if (!reset_next_) {
            hold_next_ = true;
        }
    }

    void resetnext() {
        reset_next_ = true;
        hold_next_ = false;
    }

    void _set_reset_value(const T &value) {
        reset_value_ = value;
    }

    void _reset() {
        data_ = reset_value_;
        next_ = reset_value_;
        pending_write_ports_ = WRPortNum;
        issued_write_ports_ = 0;
        hold_next_ = false;
        reset_next_ = false;
    }

protected:
    T data_{};
    T next_{};
    T reset_value_{};
    uint32_t pending_write_ports_ = WRPortNum;
    uint64_t issued_write_ports_ = 0;
    bool hold_next_ = false;
    bool reset_next_ = false;
};

template<typename T>
class VulRegisterImpl1 {

public:
    template <uint32_t P = 0>
    void setnext(const T &value) {
        if (reset_next_ || hold_next_) {
            return;
        }
        assert(!write_issued_ &&
               "VulRegister::setnext() may only be called once per cycle for each write port");
        next_buffer_ = value;
        write_issued_ = true;
    }

    void apply_next_tick() {
        if (reset_next_) {
            data_ = reset_value_;
            next_buffer_ = reset_value_;
        } else if (hold_next_) {
            next_buffer_ = data_;
        } else {
            data_ = next_buffer_;
        }
        write_issued_ = false;
        hold_next_ = false;
        reset_next_ = false;
    }

    operator const T&() const {
        return data_;
    }

    void holdnext() {
        if (!reset_next_) {
            hold_next_ = true;
        }
    }

    void resetnext() {
        reset_next_ = true;
        hold_next_ = false;
    }

    void _set_reset_value(const T &value) {
        reset_value_ = value;
    }

    void _reset() {
        data_ = reset_value_;
        next_buffer_ = reset_value_;
        write_issued_ = false;
        hold_next_ = false;
        reset_next_ = false;
    }

protected:
    T next_buffer_{};
    T data_{};
    T reset_value_{};
    bool write_issued_ = false;
    bool hold_next_ = false;
    bool reset_next_ = false;
};

template<typename T, uint32_t WRPortNum = 1>
class VulRegister {

    using ImplType = std::conditional_t<WRPortNum == 1,
                                        VulRegisterImpl1<T>,
                                        VulRegisterImpl<T, WRPortNum>>;
    ImplType impl_;

public:
    template <uint32_t P = 0>
    void setnext(const T &value) {
        impl_.template setnext<P>(value);
    }
    void holdnext() {
        impl_.holdnext();
    }
    void resetnext() {
        impl_.resetnext();
    }
    void apply_next_tick() {
        impl_.apply_next_tick();
    }
    const T& get() const {
        return impl_;
    }
    operator const T&() const {
        return impl_;
    }
    void _set_reset_value(const T &value) {
        impl_._set_reset_value(value);
    }
    void _reset() {
        impl_._reset();
    }
};

template<typename T, uint32_t Size, uint32_t WRPortNum = 1>
class VulRegisterArrayFullImpl {

    static_assert(Size >= 1, "Size must be at least 1");
    static_assert(WRPortNum >= 1, "WRPortNum must be at least 1");
    static_assert(WRPortNum < 64, "WRPortNum must be less than 64");

protected:
    std::array<VulRegister<T, WRPortNum>, Size> data_;

public:
    template <uint32_t P = 0>
    void setnext(const uint32_t index, const T &value) {
        assert(index < Size);
        data_[index].template setnext<P>(value);
    }
    void holdnext(const uint32_t index) {
        assert(index < Size);
        data_[index].holdnext();
    }
    void holdnext() {
        for (auto &elem : data_) {
            elem.holdnext();
        }
    }
    void resetnext(const uint32_t index) {
        assert(index < Size);
        data_[index].resetnext();
    }
    void resetnext() {
        for (auto &elem : data_) {
            elem.resetnext();
        }
    }
    void apply_next_tick() {
        for (auto &elem : data_) {
            elem.apply_next_tick();
        }
    }
    const T& operator[](uint32_t index) const {
        assert(index < Size);
        return data_[index].get();
    }
    void _set_reset_value(const T &value) {
        for (auto &elem : data_) {
            elem._set_reset_value(value);
        }
    }
    void _set_reset_value(const array<T, Size> &values) {
        for (uint32_t i = 0; i < Size; i++) {
            data_[i]._set_reset_value(values[i]);
        }
    }
    void _reset() {
        for (auto &elem : data_) {
            elem._reset();
        }
    }
};

template<typename T, uint32_t Size, uint32_t WRPortNum = 1>
class VulRegisterArrayDirtyImpl {

    static_assert(Size >= 1, "Size must be at least 1");
    static_assert(WRPortNum >= 1, "WRPortNum must be at least 1");
    static_assert(WRPortNum < 64, "WRPortNum must be less than 64");

protected:
    struct PendingSlot {
        T value;
        uint32_t best_prio;
        bool has_write;
        bool hold_next;
        bool reset_next;
        uint64_t issued_write_ports;

        PendingSlot()
            : value(), best_prio(WRPortNum), has_write(false), hold_next(false),
              reset_next(false), issued_write_ports(0) {}
    };
    std::array<T, Size> curr_;
    std::array<T, Size> reset_values_;
    std::array<PendingSlot, Size> pending_;

    std::array<uint32_t, Size> dirty_indices_{};
    uint32_t dirty_count_ = 0;
    std::array<uint8_t, Size> dirty_flags_{};

public:

    VulRegisterArrayDirtyImpl() : curr_(), pending_() {}
    VulRegisterArrayDirtyImpl(const T &initial_value) : curr_(), pending_() {
        for (auto &elem : curr_) {
            elem = initial_value;
        }
        for (auto &elem : reset_values_) {
            elem = initial_value;
        }
        for (auto &slot : pending_) {
            slot.value = initial_value;
        }
    }

    template <uint32_t P = 0>
    void setnext(const uint32_t index, const T &value) {
        assert(index < Size);
        static_assert(P < WRPortNum);
        auto &slot = pending_[index];
        if (slot.reset_next || slot.hold_next) {
            return;
        }
        assert((slot.issued_write_ports & (uint64_t(1) << P)) == 0 &&
               "VulRegisterArray::setnext() may only be called once per cycle for each register write port");
        slot.issued_write_ports |= uint64_t(1) << P;
        if (!slot.has_write || P < slot.best_prio) {
            slot.value = value;
            slot.best_prio = P;
            slot.has_write = true;
            if (dirty_flags_[index] == 0) {
                dirty_indices_[dirty_count_++] = index;
                dirty_flags_[index] = 1;
            }
        }
    }
    void apply_next_tick() {
        for (uint32_t i = 0; i < dirty_count_; i++) {
            uint32_t index = dirty_indices_[i];
            if (pending_[index].reset_next) {
                curr_[index] = reset_values_[index];
                pending_[index].value = reset_values_[index];
            } else if (pending_[index].hold_next) {
                pending_[index].value = curr_[index];
            } else if (pending_[index].has_write) {
                curr_[index] = pending_[index].value;
            }
            pending_[index].has_write = false;
            pending_[index].best_prio = WRPortNum;
            pending_[index].hold_next = false;
            pending_[index].reset_next = false;
            pending_[index].issued_write_ports = 0;
            dirty_flags_[index] = 0;
        }
        dirty_count_ = 0;
    }
    const T& operator[](uint32_t index) const {
        assert(index < Size);
        return curr_[index];
    }
    void holdnext(uint32_t index) {
        assert(index < Size);
        auto &slot = pending_[index];
        if (slot.reset_next) {
            return;
        }
        slot.hold_next = true;
        slot.has_write = false;
        slot.best_prio = WRPortNum;
        slot.issued_write_ports = 0;
        mark_dirty(index);
    }
    void holdnext() {
        for (uint32_t i = 0; i < Size; i++) {
            holdnext(i);
        }
    }
    void resetnext(uint32_t index) {
        assert(index < Size);
        auto &slot = pending_[index];
        slot.reset_next = true;
        slot.hold_next = false;
        slot.has_write = false;
        slot.best_prio = WRPortNum;
        slot.issued_write_ports = 0;
        mark_dirty(index);
    }
    void resetnext() {
        for (uint32_t i = 0; i < Size; i++) {
            resetnext(i);
        }
    }
    void _set_reset_value(const T &value) {
        for (auto &elem : reset_values_) {
            elem = value;
        }
    }
    void _set_reset_value(const array<T, Size> &values) {
        reset_values_ = values;
    }
    void _reset() {
        for (auto &elem : curr_) {
            elem = T{};
        }
        curr_ = reset_values_;
        for (auto &slot : pending_) {
            slot.value = T{};
            slot.best_prio = WRPortNum;
            slot.has_write = false;
            slot.hold_next = false;
            slot.reset_next = false;
            slot.issued_write_ports = 0;
        }
        for (uint32_t i = 0; i < Size; i++) {
            pending_[i].value = reset_values_[i];
        }
        dirty_count_ = 0;
        dirty_flags_.fill(0);
    }

private:
    void mark_dirty(uint32_t index) {
        if (dirty_flags_[index] == 0) {
            dirty_indices_[dirty_count_++] = index;
            dirty_flags_[index] = 1;
        }
    }
};

template<typename T, uint32_t Size, uint32_t WRPortNum = 1>
class VulRegisterArray {

    static_assert(Size >= 1, "Size must be at least 1");
    static_assert(WRPortNum >= 1, "WRPortNum must be at least 1");
    static_assert(WRPortNum < 64, "WRPortNum must be less than 64");

    using ImplType = std::conditional_t<(Size <= 16),
                                        VulRegisterArrayFullImpl<T, Size, WRPortNum>,
                                        VulRegisterArrayDirtyImpl<T, Size, WRPortNum>>;
    ImplType impl_;

public:
    VulRegisterArray() : impl_() {}
    VulRegisterArray(const T &initial_value) : impl_(initial_value) {}

    template <uint32_t P = 0>
    void setnext(const uint32_t index, const T &value) {
        impl_.template setnext<P>(index, value);
    }
    void holdnext(const uint32_t index) {
        impl_.holdnext(index);
    }
    void holdnext() {
        impl_.holdnext();
    }
    void resetnext(const uint32_t index) {
        impl_.resetnext(index);
    }
    void resetnext() {
        impl_.resetnext();
    }
    void apply_next_tick() {
        impl_.apply_next_tick();
    }
    const T& operator[](uint32_t index) const {
        return impl_[index];
    }
    void _set_reset_value(const T &value) {
        impl_._set_reset_value(value);
    }
    void _set_reset_value(const array<T, Size> &values) {
        impl_._set_reset_value(values);
    }
    void _reset() {
        impl_._reset();
    }
};

} // namespace vulstorage

using vulstorage::VulRegister;
using vulstorage::VulRegisterArray;
