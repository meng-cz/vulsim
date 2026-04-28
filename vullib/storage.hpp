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
class VulStorageNextImpl {
public:
    static_assert(WRPortNum >= 1, "WRPortNum must be at least 1");
    static_assert(WRPortNum < 64, "WRPortNum must be less than 64");

    VulStorageNextImpl() : data_() {
        next_.fill(T{});
    }
    VulStorageNextImpl(const T &initial_value) : data_(initial_value) {
        next_.fill(initial_value);
    }

    void setnext(const T &value, uint32_t priority) {
        assert(priority < WRPortNum);
        if (priority < pending_write_ports_) {
            next_ = value;
            pending_write_ports_ = priority;
        }
    }

    T get() const {
        return data_;
    }

    operator const T&() const {
        return data_;
    }

    void apply_next_tick() {
        data_ = next_;
        pending_write_ports_ = WRPortNum;
    }

protected:
    T data_;
    T next_;
    uint32_t pending_write_ports_ = 0;
};

template<typename T>
class VulStorageNextImpl1 {

public:
    VulStorageNextImpl1() : data_(), next_buffer_() {}
    VulStorageNextImpl1(const T &initial_value) : data_(initial_value), next_buffer_(initial_value) {}

    void setnext(const T &value, uint32_t priority) {
        next_buffer_ = value;
    }

    void apply_next_tick() {
        data_ = next_buffer_;
    }

    operator const T&() const {
        return data_;
    }

protected:
    T next_buffer_;
    T data_;
};

template<typename T, uint32_t WRPortNum = 1>
class VulStorageNext {

    using ImplType = std::conditional_t<WRPortNum == 1,
                                        VulStorageNextImpl1<T>,
                                        VulStorageNextImpl<T, WRPortNum>>;
    ImplType impl_;

public:
    VulStorageNext() : impl_() {}
    VulStorageNext(const T &initial_value) : impl_(initial_value) {}

    void setnext(const T &value, uint32_t priority) {
        impl_.setnext(value, priority);
    }
    void setnext(const T &value) {
        impl_.setnext(value, 0);
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
};

template<typename T, uint32_t Size, uint32_t WRPortNum = 1>
class VulStorageNextArrayFullImpl {

    static_assert(Size >= 1, "Size must be at least 1");
    static_assert(WRPortNum >= 1, "WRPortNum must be at least 1");
    static_assert(WRPortNum < 64, "WRPortNum must be less than 64");

protected:
    std::array<VulStorageNext<T, WRPortNum>, Size> data_;

public:
    VulStorageNextArrayFullImpl() : data_() {}
    VulStorageNextArrayFullImpl(const T &initial_value) {
        for (auto &elem : data_) {
            elem = VulStorageNext<T, WRPortNum>(initial_value);
        }
    }

    void setnext(const uint32_t index, const T &value, uint32_t priority) {
        assert(index < Size);
        data_[index].setnext(value, priority);
    }
    void setnext(const uint32_t index, const T &value) {
        assert(index < Size);
        data_[index].setnext(value);
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

};

template<typename T, uint32_t Size, uint32_t WRPortNum = 1>
class VulStorageNextArrayDirtyImpl {

    static_assert(Size >= 1, "Size must be at least 1");
    static_assert(WRPortNum >= 1, "WRPortNum must be at least 1");
    static_assert(WRPortNum < 64, "WRPortNum must be less than 64");

protected:
    struct PendingSlot {
        T value;
        uint32_t best_prio;
        bool has_write;

        PendingSlot()
            : value(), best_prio(WRPortNum), has_write(false) {}
    };
    std::array<T, Size> curr_;
    std::array<PendingSlot, Size> pending_;

    std::array<uint32_t, Size> dirty_indices_{};
    uint32_t dirty_count_ = 0;
    std::array<uint8_t, Size> dirty_flags_{};

public:

    VulStorageNextArrayDirtyImpl() : curr_(), pending_() {}
    VulStorageNextArrayDirtyImpl(const T &initial_value) {
        for (auto &elem : curr_) {
            elem = initial_value;
        }
    }

    void setnext(const uint32_t index, const T &value, uint32_t priority) {
        assert(index < Size);
        assert(priority < WRPortNum);
        auto &slot = pending_[index];
        if (!slot.has_write || priority < slot.best_prio) {
            slot.value = value;
            slot.best_prio = priority;
            slot.has_write = true;
            if (dirty_flags_[index] == 0) {
                dirty_indices_[dirty_count_++] = index;
                dirty_flags_[index] = 1;
            }
        }
    }
    void setnext(const uint32_t index, const T &value) {
        setnext(index, value, 0);
    }
    void apply_next_tick() {
        for (uint32_t i = 0; i < dirty_count_; i++) {
            uint32_t index = dirty_indices_[i];
            curr_[index] = pending_[index].value;
            pending_[index].has_write = false;
            dirty_flags_[index] = 0;
        }
        dirty_count_ = 0;
    }
    const T& operator[](uint32_t index) const {
        assert(index < Size);
        return curr_[index];
    }
};

template<typename T, uint32_t Size, uint32_t WRPortNum = 1>
class VulStorageNextArray {

    static_assert(Size >= 1, "Size must be at least 1");
    static_assert(WRPortNum >= 1, "WRPortNum must be at least 1");
    static_assert(WRPortNum < 64, "WRPortNum must be less than 64");

    using ImplType = std::conditional_t<(Size <= 16),
                                        VulStorageNextArrayFullImpl<T, Size, WRPortNum>,
                                        VulStorageNextArrayDirtyImpl<T, Size, WRPortNum>>;
    ImplType impl_;

public:
    VulStorageNextArray() : impl_() {}
    VulStorageNextArray(const T &initial_value) : impl_(initial_value) {}

    void setnext(const uint32_t index, const T &value, uint32_t priority) {
        impl_.setnext(index, value, priority);
    }
    void setnext(const uint32_t index, const T &value) {
        impl_.setnext(index, value);
    }
    void apply_next_tick() {
        impl_.apply_next_tick();
    }
    const T& operator[](uint32_t index) const {
        return impl_[index];
    }
};

// constexpr uint32_t VulStorageNextArrayBlockSize = 1024;

// template<typename T, uint32_t Size>
// class VulStorageNextArray1DimImplAllCopy {
    
//     static_assert(Size >= 1, "Size must be at least 1");

// public:
//     VulStorageNextArray1DimImplAllCopy() {
//         next_buffer_.fill(T{});
//         data_.fill(T{});
//     }
//     VulStorageNextArray1DimImplAllCopy(const T &initial_value) {
//         next_buffer_.fill(initial_value);
//         data_.fill(initial_value);
//     }

//     void setnext(const T &value, uint32_t index) {
//         next_buffer_[index] = value;
//     }
//     T get(uint32_t index) const {
//         return data_[index];
//     }
//     void apply_next_tick() {
//         data_ = next_buffer_;
//     }

// protected:
//     std::array<T, Size> next_buffer_;
//     std::array<T, Size> data_;
// };

// template<typename T, uint32_t Size>
// class VulStorageNextArray1DimImplBlockCopy {

//     static_assert(Size >= 1, "Size must be at least 1");

// public:

//     VulStorageNextArray1DimImplBlockCopy() {
//         for (auto &block : blocks_) {
//             block = VulStorageNextArray1DimImplAllCopy<T, VulStorageNextArrayBlockSize>();
//         }
//     }
//     VulStorageNextArray1DimImplBlockCopy(const T &initial_value) {
//         for (auto &block : blocks_) {
//             block = VulStorageNextArray1DimImplAllCopy<T, VulStorageNextArrayBlockSize>(initial_value);
//         }
//     }

//     void setnext(const T &value, uint32_t index) {
//         uint32_t block_index = index / VulStorageNextArrayBlockSize;
//         uint32_t within_block_index = index % VulStorageNextArrayBlockSize;
//         blocks_[block_index].setnext(value, within_block_index);
//         block_updated_[block_index] = true;
//     }
//     T get(uint32_t index) const {
//         uint32_t block_index = index / VulStorageNextArrayBlockSize;
//         uint32_t within_block_index = index % VulStorageNextArrayBlockSize;
//         return blocks_[block_index].get(within_block_index);
//     }
//     void apply_next_tick() {
//         for (uint32_t i = 0; i < num_blocks_; i++) {
//             if (block_updated_[i]) {
//                 blocks_[i].apply_next_tick();
//                 block_updated_[i] = false;
//             }
//         }
//     }

// protected:
//     constexpr static uint32_t num_blocks_ = (Size + VulStorageNextArrayBlockSize - 1) / VulStorageNextArrayBlockSize;
//     std::array<VulStorageNextArray1DimImplAllCopy<T, VulStorageNextArrayBlockSize>, num_blocks_> blocks_;
//     std::array<bool, num_blocks_> block_updated_ = {false};
// };

// template<typename T, uint32_t Size>
// class VulStorageNextArray1DimImpl {
//     static_assert(Size >= 1, "Size must be at least 1");

//     using ImplType = std::conditional_t<Size <= VulStorageNextArrayBlockSize,
//                                         VulStorageNextArray1DimImplAllCopy<T, Size>,
//                                         VulStorageNextArray1DimImplBlockCopy<T, Size>>;
//     ImplType impl_;

// public:
//     VulStorageNextArray1DimImpl() : impl_() {}
//     VulStorageNextArray1DimImpl(const T &initial_value) : impl_(initial_value) {}
//     void setnext(const T &value, uint32_t index) {
//         impl_.setnext(value, index);
//     }
//     T get(uint32_t index) const {
//         return impl_.get(index);
//     }
//     void apply_next_tick() {
//         impl_.apply_next_tick();
//     }
// };


// template<typename T, uint32_t ... Dims>
// class VulStorageNextArray {
//     static_assert(sizeof...(Dims) >= 1, "At least one dimension is required");
//     constexpr static uint32_t total_size_ = (Dims * ...);
//     constexpr uint32_t get_flat_index_(const uint32_t ... indices) const {
//         std::array<uint32_t, sizeof...(Dims)> idx_array = {indices...};
//         uint32_t flat_index = 0;
//         uint32_t stride = 1;
//         for (int32_t i = sizeof...(Dims) - 1; i >= 0; --i) {
//             flat_index += idx_array[i] * stride;
//             stride *= std::get<i>(std::tuple<uint32_t, Dims...>{Dims...});
//         }
//         return flat_index;
//     }

// public:
//     VulStorageNextArray() : impl_() {}
//     VulStorageNextArray(const T &initial_value) : impl_(initial_value) {}
//     void setnext(const T &value, const uint32_t ... indices) {
//         uint32_t flat_index = get_flat_index_(indices...);
//         impl_.setnext(value, flat_index);
//     }
//     T get(const uint32_t ... indices) const {
//         uint32_t flat_index = get_flat_index_(indices...);
//         return impl_.get(flat_index);
//     }
//     void apply_next_tick() {
//         impl_.apply_next_tick();
//     }

// protected:
//     VulStorageNextArray1DimImpl<T, total_size_> impl_;
// };

} // namespace vulstorage

using vulstorage::VulStorageNext;
// using vulstorage::VulStorageNextArray;

