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

#include <string>
#include <vector>
#include <set>
#include <array>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

using std::array;
using std::vector;
using std::list;
using std::deque;
using std::map;
using std::unordered_map;
using std::unordered_multimap;
using std::set;
using std::unordered_set;
using std::make_pair;
using std::move;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::shared_ptr;
using std::make_shared;
using std::make_unique;
using std::make_pair;

#include <type_traits>

template <uint32_t BitWidth>
class UInt {
public:
    static constexpr uint32_t WORD_BITS = 64;
    static constexpr uint32_t NUM_WORDS = (BitWidth + WORD_BITS - 1) / WORD_BITS;
    static constexpr uint32_t EFFECTIVE_BITS = BitWidth % WORD_BITS == 0 ? WORD_BITS : BitWidth % WORD_BITS;

    using Storage = std::array<uint64_t, NUM_WORDS>;

    Storage data;

public:
    FORCE_INLINE constexpr void mask_high_bits() {
        if constexpr (BitWidth % WORD_BITS != 0) {
            data.back() &= (uint64_t(1) << EFFECTIVE_BITS) - 1;
        }
    }

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

    // Division and modulo
    FORCE_INLINE constexpr UInt operator/(const UInt& other) const {
        if (!other) return UInt(0); // division by zero
        uint64_t dividend = data[0]; // assume BitWidth <= 64 for simplicity
        uint64_t divisor = other.data[0];
        uint64_t quotient = dividend / divisor;
        return UInt(quotient);
    }

    FORCE_INLINE constexpr UInt operator%(const UInt& other) const {
        if (!other) return *this; // division by zero
        uint64_t dividend = data[0];
        uint64_t divisor = other.data[0];
        uint64_t remainder = dividend % divisor;
        return UInt(remainder);
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

    // Conversion to bool
    FORCE_INLINE constexpr operator bool() const {
        for (uint32_t i = 0; i < NUM_WORDS; ++i) {
            if (data[i] != 0) return true;
        }
        return false;
    }

    // Conversion to built-in integer types
    FORCE_INLINE constexpr operator uint64_t() const {
        return data[0];
    }

    FORCE_INLINE constexpr operator uint32_t() const {
        return static_cast<uint32_t>(data[0]);
    }

    FORCE_INLINE constexpr operator uint16_t() const {
        return static_cast<uint16_t>(data[0]);
    }

    FORCE_INLINE constexpr operator uint8_t() const {
        return static_cast<uint8_t>(data[0]);
    }

    // Extract function (template version)
    template <uint32_t HiBit, uint32_t LoBit>
    FORCE_INLINE constexpr UInt<HiBit - LoBit + 1> extract() const {
        static_assert(HiBit >= LoBit && HiBit < BitWidth, "Invalid bit range");
        constexpr uint32_t ResultWidth = HiBit - LoBit + 1;
        UInt<ResultWidth> result = {};
        constexpr uint32_t start_word = LoBit / WORD_BITS;
        constexpr uint32_t start_bit = LoBit % WORD_BITS;
        constexpr uint32_t end_word = HiBit / WORD_BITS;
        constexpr uint32_t end_bit = HiBit % WORD_BITS;
        ExtractHelper<start_word, end_word, start_bit, end_bit, start_word, 0, UInt<ResultWidth>::NUM_WORDS>::apply(data, result.data);
        result.mask_high_bits();
        return result;
    }

    // Extract function (runtime version)
    FORCE_INLINE constexpr UInt<BitWidth> extract(uint32_t hi_bit, uint32_t lo_bit) const {
        UInt<BitWidth> result;
        uint32_t length = hi_bit - lo_bit + 1;
        uint32_t start_word = lo_bit / WORD_BITS;
        uint32_t start_bit = lo_bit % WORD_BITS;
        uint32_t end_word = hi_bit / WORD_BITS;
        uint32_t end_bit = hi_bit % WORD_BITS;
        uint32_t result_word = 0;
        uint32_t result_bit = 0;
        for (uint32_t w = start_word; w <= end_word; ++w) {
            uint64_t word = data[w];
            uint32_t bit_start = (w == start_word) ? start_bit : 0;
            uint32_t bit_end = (w == end_word) ? end_bit : WORD_BITS - 1;
            uint32_t bits = bit_end - bit_start + 1;
            uint64_t mask = (bits == WORD_BITS) ? ~uint64_t(0) : ((uint64_t(1) << bits) - 1);
            uint64_t part = (word & (mask << bit_start)) >> bit_start;
            // add to result
            if (result_bit + bits <= WORD_BITS) {
                result.data[result_word] |= part << result_bit;
                result_bit += bits;
                if (result_bit == WORD_BITS) {
                    result_word++;
                    result_bit = 0;
                }
            } else {
                uint32_t first_bits = WORD_BITS - result_bit;
                result.data[result_word] |= (part & ((uint64_t(1) << first_bits) - 1)) << result_bit;
                result_word++;
                uint32_t remaining_bits = bits - first_bits;
                result.data[result_word] |= part >> first_bits;
                result_bit = remaining_bits;
            }
        }
        result.mask_high_bits();
        return result;
    }

private:
    template <uint32_t StartWord, uint32_t EndWord, uint32_t StartBit, uint32_t EndBit, uint32_t CurrentWord, uint32_t ResultBitPos, uint32_t ResultNumWords>
    struct ExtractHelper {
        static constexpr void apply(const Storage& data, std::array<uint64_t, ResultNumWords>& result) {
            if constexpr (CurrentWord <= EndWord) {
                constexpr uint32_t bit_start = (CurrentWord == StartWord) ? StartBit : 0;
                constexpr uint32_t bit_end = (CurrentWord == EndWord) ? EndBit : WORD_BITS - 1;
                constexpr uint32_t bits = bit_end - bit_start + 1;
                constexpr uint64_t mask = (bits == WORD_BITS) ? ~uint64_t(0) : ((uint64_t(1) << bits) - 1) << bit_start;
                uint64_t part = (data[CurrentWord] & mask) >> bit_start;
                constexpr uint32_t result_word = ResultBitPos / WORD_BITS;
                constexpr uint32_t result_bit = ResultBitPos % WORD_BITS;
                if constexpr (result_bit + bits <= WORD_BITS) {
                    result[result_word] |= part << result_bit;
                    ExtractHelper<StartWord, EndWord, StartBit, EndBit, CurrentWord + 1, ResultBitPos + bits, ResultNumWords>::apply(data, result);
                } else {
                    constexpr uint32_t first_bits = WORD_BITS - result_bit;
                    result[result_word] |= (part & ((uint64_t(1) << first_bits) - 1)) << result_bit;
                    result[result_word + 1] |= part >> first_bits;
                    ExtractHelper<StartWord, EndWord, StartBit, EndBit, CurrentWord + 1, ResultBitPos + bits, ResultNumWords>::apply(data, result);
                }
            }
        }
    };

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

    // Access to data for testing
public:
    FORCE_INLINE const Storage& get_data() const { return data; }
};
