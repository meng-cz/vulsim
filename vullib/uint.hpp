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

#include "common.h"

#include <array>
#include <cassert>

using std::array;

#include <type_traits>

template <uint32_t BitWidth>
class UInt {
public:
    static constexpr uint32_t WORD_BITS = 64;
    static constexpr uint32_t NUM_WORDS = (BitWidth + WORD_BITS - 1) / WORD_BITS;
    static constexpr uint32_t EFFECTIVE_BITS = BitWidth % WORD_BITS == 0 ? WORD_BITS : BitWidth % WORD_BITS;

    using Storage = std::array<uint64_t, NUM_WORDS>;

    Storage data;

    FORCE_INLINE constexpr void mask_high_bits() {
        if constexpr (BitWidth % WORD_BITS != 0) {
            data.back() &= (uint64_t(1) << EFFECTIVE_BITS) - 1;
        }
    }

    class Slice {
    private:
        UInt<BitWidth>& parent;
        uint32_t hi;
        uint32_t lo;

    public:
        Slice(UInt<BitWidth>& p, uint32_t h, uint32_t l)
            : parent(p), hi(h), lo(l) {
            assert(h >= l);
            assert(h < BitWidth);
        }

        uint32_t width() const {
            return hi - lo + 1;
        }

        // ----------- 读取 -----------
        template <uint32_t ResultBitWidth>
        operator UInt<ResultBitWidth>() const {
            return UInt<ResultBitWidth>(parent.get_bits(hi, lo));
        }

        // ----------- 写入 UInt -----------
        Slice& operator=(const UInt<BitWidth>& rhs) {
            parent.set_bits(hi, lo, rhs);
            return *this;
        }

        // ----------- 写入整数 -----------
        Slice& operator=(uint64_t rhs) {
            parent.set_bits(hi, lo, UInt<BitWidth>(rhs));
            return *this;
        }
    };
    class Bit {
    private:
        UInt<BitWidth>& parent;
        uint32_t bit;
    public:
        Bit(UInt<BitWidth>& p, uint32_t b)
            : parent(p), bit(b) {
            assert(bit < BitWidth);
        }

        // ----------- 读取 -----------
        operator bool() const {
            uint32_t word = bit / WORD_BITS;
            uint32_t b = bit % WORD_BITS;
            return (parent.data[word] >> b) & 1;
        }

        // ----------- 写入 -----------
        Bit& operator=(bool value) {
            uint32_t word = bit / WORD_BITS;
            uint32_t b = bit % WORD_BITS;
            if (value) {
                parent.data[word] |= (uint64_t(1) << b);
            } else {
                parent.data[word] &= ~(uint64_t(1) << b);
            }
            return *this;
        }
    };

public:
    // Default constructor
    FORCE_INLINE constexpr UInt() : data{0} {}

    // Constructor from uint64_t
    FORCE_INLINE constexpr UInt(uint64_t value) : data{0} {
        data[0] = value;
        mask_high_bits();
    }

    // Constructor from another UInt
    template <uint32_t OtherBitWidth>
    FORCE_INLINE constexpr UInt(const UInt<OtherBitWidth>& other) : data{0} {
        constexpr uint32_t min_words = std::min(NUM_WORDS, UInt<OtherBitWidth>::NUM_WORDS);
        const auto& other_data = other.get_data();
        for (uint32_t i = 0; i < min_words; ++i) {
            data[i] = other_data[i];
        }
        mask_high_bits();
    }

    // Assignment from uint64_t
    FORCE_INLINE constexpr UInt& operator=(uint64_t value) {
        data = {};
        data[0] = value;
        mask_high_bits();
        return *this;
    }

    // Assignment from another UInt
    template <uint32_t OtherBitWidth>
    FORCE_INLINE constexpr UInt& operator=(const UInt<OtherBitWidth>& other) {
        constexpr uint32_t min_words = std::min(NUM_WORDS, UInt<OtherBitWidth>::NUM_WORDS);
        const auto& other_data = other.get_data();
        for (uint32_t i = 0; i < min_words; ++i) {
            data[i] = other_data[i];
        }
        for (uint32_t i = min_words; i < NUM_WORDS; ++i) {
            data[i] = 0;
        }
        mask_high_bits();
        return *this;
    }

    // Arithmetic operators
    FORCE_INLINE constexpr UInt operator+(const UInt& other) const {
        UInt result;
        uint64_t carry = 0;
        for (uint32_t i = 0; i < NUM_WORDS; ++i) {
            uint64_t sum = data[i] + other.data[i] + carry;
            result.data[i] = sum;
            carry = sum < data[i] || (sum == data[i] && carry) ? 1 : 0;
        }
        result.mask_high_bits();
        return result;
    }

    FORCE_INLINE constexpr UInt operator-(const UInt& other) const {
        UInt result;
        uint64_t borrow = 0;
        for (uint32_t i = 0; i < NUM_WORDS; ++i) {
            uint64_t diff = data[i] - other.data[i] - borrow;
            result.data[i] = diff;
            borrow = data[i] < other.data[i] + borrow ? 1 : 0;
        }
        result.mask_high_bits();
        return result;
    }

    FORCE_INLINE constexpr UInt operator*(const UInt& other) const {
        UInt result;
        for (uint32_t i = 0; i < NUM_WORDS; ++i) {
            uint64_t carry = 0;
            for (uint32_t j = 0; j < NUM_WORDS - i; ++j) {
                __int128 prod = (__int128)data[i] * other.data[j] + result.data[i + j] + carry;
                result.data[i + j] = prod;
                carry = prod >> 64;
            }
        }
        result.mask_high_bits();
        return result;
    }

    // Shift operators
    FORCE_INLINE constexpr UInt operator<<(uint32_t shift) const {
        UInt result;
        uint32_t word_shift = shift / WORD_BITS;
        uint32_t bit_shift = shift % WORD_BITS;
        for (uint32_t i = 0; i < NUM_WORDS; ++i) {
            if (i + word_shift < NUM_WORDS) {
                result.data[i + word_shift] |= data[i] << bit_shift;
            }
            if (bit_shift > 0 && i + word_shift + 1 < NUM_WORDS) {
                result.data[i + word_shift + 1] |= data[i] >> (WORD_BITS - bit_shift);
            }
        }
        result.mask_high_bits();
        return result;
    }

    FORCE_INLINE constexpr UInt operator>>(uint32_t shift) const {
        UInt result;
        uint32_t word_shift = shift / WORD_BITS;
        uint32_t bit_shift = shift % WORD_BITS;
        for (uint32_t i = 0; i < NUM_WORDS; ++i) {
            if (i >= word_shift) {
                result.data[i - word_shift] |= data[i] >> bit_shift;
            }
            if (bit_shift > 0 && i > word_shift) {
                result.data[i - word_shift - 1] |= data[i] << (WORD_BITS - bit_shift);
            }
        }
        return result;
    }

    // Bitwise operators
    FORCE_INLINE constexpr UInt operator&(const UInt& other) const {
        UInt result;
        for (uint32_t i = 0; i < NUM_WORDS; ++i) {
            result.data[i] = data[i] & other.data[i];
        }
        return result;
    }

    FORCE_INLINE constexpr UInt operator|(const UInt& other) const {
        UInt result;
        for (uint32_t i = 0; i < NUM_WORDS; ++i) {
            result.data[i] = data[i] | other.data[i];
        }
        return result;
    }

    FORCE_INLINE constexpr UInt operator^(const UInt& other) const {
        UInt result;
        for (uint32_t i = 0; i < NUM_WORDS; ++i) {
            result.data[i] = data[i] ^ other.data[i];
        }
        return result;
    }

    FORCE_INLINE constexpr UInt operator~() const {
        UInt result;
        for (uint32_t i = 0; i < NUM_WORDS; ++i) {
            result.data[i] = ~data[i];
        }
        result.mask_high_bits();
        return result;
    }

    // Comparison operators
    FORCE_INLINE constexpr bool operator==(const UInt& other) const {
        for (uint32_t i = 0; i < NUM_WORDS; ++i) {
            if (data[i] != other.data[i]) return false;
        }
        return true;
    }

    FORCE_INLINE constexpr bool operator!=(const UInt& other) const {
        return !(*this == other);
    }

    FORCE_INLINE constexpr bool operator<(const UInt& other) const {
        for (int32_t i = NUM_WORDS - 1; i >= 0; --i) {
            if (data[i] < other.data[i]) return true;
            if (data[i] > other.data[i]) return false;
        }
        return false;
    }

    FORCE_INLINE constexpr bool operator<=(const UInt& other) const {
        return *this < other || *this == other;
    }

    FORCE_INLINE constexpr bool operator>(const UInt& other) const {
        return !(*this <= other);
    }

    FORCE_INLINE constexpr bool operator>=(const UInt& other) const {
        return !(*this < other);
    }

    FORCE_INLINE constexpr Slice operator()(uint32_t hi, uint32_t lo) {
        return Slice(*this, hi, lo);
    }
    
    FORCE_INLINE constexpr Bit operator()(uint32_t bit) {
        return Bit(*this, bit);
    }

    // Helper to set a bit
    FORCE_INLINE constexpr void set_bit(uint32_t bit, bool value) {
        uint32_t word = bit / WORD_BITS;
        uint32_t b = bit % WORD_BITS;
        if (value) {
            data[word] |= (uint64_t(1) << b);
        } else {
            data[word] &= ~(uint64_t(1) << b);
        }
    }
    FORCE_INLINE constexpr void set_bits(uint32_t hibit, uint32_t lobit, const UInt<BitWidth>& value) {
        assert(hibit >= lobit);
        assert(hibit < BitWidth);

        const uint32_t width = hibit - lobit + 1;
        for (uint32_t i = 0; i < width; ++i) {
            const uint32_t src_word = i / WORD_BITS;
            const uint32_t src_bit = i % WORD_BITS;
            const bool bit_value = (value.data[src_word] >> src_bit) & 1;
            set_bit(lobit + i, bit_value);
        }
    }
    FORCE_INLINE constexpr UInt<BitWidth> get_bits(uint32_t hibit, uint32_t lobit) const {
        assert(hibit >= lobit);
        assert(hibit < BitWidth);

        UInt<BitWidth> result;
        const uint32_t width = hibit - lobit + 1;
        for (uint32_t i = 0; i < width; ++i) {
            const uint32_t src_word = (lobit + i) / WORD_BITS;
            const uint32_t src_bit = (lobit + i) % WORD_BITS;
            const bool bit_value = (data[src_word] >> src_bit) & 1;
            result.set_bit(i, bit_value);
        }
        return result;
    }

    // Access to data for testing
public:
    FORCE_INLINE const Storage& get_data() const { return data; }
};
