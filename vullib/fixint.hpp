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
#include <cassert>
#include <type_traits>

namespace vulfixint {

template <uint32_t BitWidth>
class Int;

template <uint32_t ParentBitWidth, uint32_t SliceBitWidth>
class IntSliceRef;

template <uint32_t ParentBitWidth, uint32_t SliceBitWidth>
class IntConstSliceRef;

template <uint32_t ParentBitWidth>
class IntBitRef;

template <uint32_t ParentBitWidth>
class IntConstBitRef;

template <typename Operand>
class IntSignedView;

template <typename Operand>
struct IntOperandTraits;

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
             && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED)
constexpr auto operator+(const LhsOperand& lhs, const RhsOperand& rhs);

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
             && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED)
constexpr auto operator-(const LhsOperand& lhs, const RhsOperand& rhs);

template <typename Operand>
    requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID)
constexpr auto operator-(const Operand& operand);

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID)
constexpr auto operator*(const LhsOperand& lhs, const RhsOperand& rhs);

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
             && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED
             && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                    == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
constexpr auto operator&(const LhsOperand& lhs, const RhsOperand& rhs);

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
             && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED
             && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                    == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
constexpr auto operator|(const LhsOperand& lhs, const RhsOperand& rhs);

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
             && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED
             && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                    == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
constexpr auto operator^(const LhsOperand& lhs, const RhsOperand& rhs);

template <typename Operand>
    requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<Operand>>::IS_SIGNED)
constexpr auto operator~(const Operand& operand);

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
             && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED)
constexpr auto operator<<(const LhsOperand& lhs, const RhsOperand& rhs);

template <typename LhsOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr auto operator<<(const LhsOperand& lhs, T rhs);

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED)
constexpr auto operator>>(const LhsOperand& lhs, const RhsOperand& rhs);

template <typename LhsOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr auto operator>>(const LhsOperand& lhs, T rhs);

template <typename Operand>
    requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<Operand>>::IS_SIGNED)
constexpr bool ReduceAnd(const Operand& operand);

template <typename Operand>
    requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<Operand>>::IS_SIGNED)
constexpr bool ReduceOr(const Operand& operand);

template <typename Operand>
    requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<Operand>>::IS_SIGNED)
constexpr bool ReduceXor(const Operand& operand);

template <uint32_t N, typename Operand>
    requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<Operand>>::IS_SIGNED)
constexpr auto Repeat(const Operand& operand);

template <typename... Operands>
    requires(sizeof...(Operands) > 0
             && (... && IntOperandTraits<std::remove_cvref_t<Operands>>::VALID)
             && (... && !IntOperandTraits<std::remove_cvref_t<Operands>>::IS_SIGNED))
constexpr auto Cat(const Operands&... operands);

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
             && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED
             && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                    == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
constexpr bool operator==(const LhsOperand& lhs, const RhsOperand& rhs);

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
             && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED
             && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                    == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
constexpr bool operator!=(const LhsOperand& lhs, const RhsOperand& rhs);

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                    == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
constexpr bool operator<(const LhsOperand& lhs, const RhsOperand& rhs);

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                    == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
constexpr bool operator<=(const LhsOperand& lhs, const RhsOperand& rhs);

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                    == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
constexpr bool operator>(const LhsOperand& lhs, const RhsOperand& rhs);

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                    == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
constexpr bool operator>=(const LhsOperand& lhs, const RhsOperand& rhs);

inline constexpr uint64_t low_mask64(uint32_t bits) {
    if (bits == 0) {
        return 0;
    }
    if (bits >= 64) {
        return ~uint64_t(0);
    }
    return (uint64_t(1) << bits) - 1;
}

template <uint32_t ShiftBitWidth>
constexpr bool shift_amount_at_least_width(const Int<ShiftBitWidth>& shift, uint32_t width);

template <uint32_t BitWidth>
class Int {
public:
    static constexpr uint32_t WORD_BITS = 64;
    static constexpr uint32_t NUM_WORDS = (BitWidth + WORD_BITS - 1) / WORD_BITS;
    static constexpr uint32_t IDX_WIDTH = log2ceil(BitWidth);

private:
    static_assert(BitWidth > 0, "BitWidth must be greater than 0");
    static constexpr uint32_t LOW_MASK_BITS = BitWidth % WORD_BITS;

    std::array<uint64_t, NUM_WORDS> data;

    static constexpr uint64_t word_mask(uint32_t bits) {
        return low_mask64(bits);
    }

    constexpr void mask_high_bits() {
        if constexpr (LOW_MASK_BITS != 0) {
            data.back() &= word_mask(LOW_MASK_BITS);
        }
    }

    template <uint32_t DstBitWidth, uint32_t SrcBitWidth>
    static constexpr Int<DstBitWidth> cast_unsigned_operand(const Int<SrcBitWidth>& src) {
        return Int<DstBitWidth>(src);
    }

    template <uint32_t DstBitWidth, uint32_t SrcBitWidth>
    static constexpr Int<DstBitWidth> cast_signed_operand(const Int<SrcBitWidth>& src) {
        Int<DstBitWidth> out(0);

        constexpr uint32_t DST_WORDS = Int<DstBitWidth>::NUM_WORDS;
        constexpr uint32_t SRC_WORDS = Int<SrcBitWidth>::NUM_WORDS;
        constexpr uint32_t COPY_WORDS = (DST_WORDS < SRC_WORDS) ? DST_WORDS : SRC_WORDS;
        for (uint32_t i = 0; i < COPY_WORDS; ++i) {
            out.data[i] = src.data[i];
        }

        constexpr uint32_t SRC_SIGN_WORD = (SrcBitWidth - 1) / WORD_BITS;
        constexpr uint32_t SRC_SIGN_OFF = (SrcBitWidth - 1) % WORD_BITS;
        const bool sign = ((src.data[SRC_SIGN_WORD] >> SRC_SIGN_OFF) & uint64_t(1)) != 0;

        if constexpr (DstBitWidth > SrcBitWidth) {
            if (sign) {
                constexpr uint32_t EXT_START_WORD = SrcBitWidth / WORD_BITS;
                constexpr uint32_t EXT_START_OFF = SrcBitWidth % WORD_BITS;

                if constexpr (EXT_START_OFF != 0) {
                    out.data[EXT_START_WORD] |= ~low_mask64(EXT_START_OFF);
                    for (uint32_t i = EXT_START_WORD + 1; i < DST_WORDS; ++i) {
                        out.data[i] = ~uint64_t(0);
                    }
                } else {
                    for (uint32_t i = EXT_START_WORD; i < DST_WORDS; ++i) {
                        out.data[i] = ~uint64_t(0);
                    }
                }
            }
        } else if constexpr (DstBitWidth < SrcBitWidth) {
            constexpr uint32_t DST_SIGN_WORD = (DstBitWidth - 1) / WORD_BITS;
            constexpr uint32_t DST_SIGN_OFF = (DstBitWidth - 1) % WORD_BITS;
            const uint64_t sign_mask = uint64_t(1) << DST_SIGN_OFF;
            if (sign) {
                out.data[DST_SIGN_WORD] |= sign_mask;
            } else {
                out.data[DST_SIGN_WORD] &= ~sign_mask;
            }
        }

        out.mask_high_bits();
        return out;
    }

    template <uint32_t DstBitWidth, typename Operand>
        requires IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
    static constexpr Int<DstBitWidth> cast_fixint_operand(const Operand& operand) {
        using Traits = IntOperandTraits<std::remove_cvref_t<Operand>>;
        constexpr uint32_t SRC_BIT_WIDTH = Traits::BIT_WIDTH;
        const Int<SRC_BIT_WIDTH> src = Traits::get_int(operand);
        if constexpr (Traits::IS_SIGNED) {
            return cast_signed_operand<DstBitWidth>(src);
        } else {
            return cast_unsigned_operand<DstBitWidth>(src);
        }
    }

    constexpr Int& assign_from_same_width(const Int& value) {
        data = value.data;
        return *this;
    }

    template <uint32_t DstBitWidth>
    constexpr Int<DstBitWidth> get_bits(uint32_t lo) const {
        static_assert(DstBitWidth > 0, "DstBitWidth must be greater than 0");
        assert(lo + DstBitWidth <= BitWidth);

        constexpr uint32_t OUT_WORDS = Int<DstBitWidth>::NUM_WORDS;
        Int<DstBitWidth> out(0);

        if constexpr (BitWidth <= WORD_BITS && DstBitWidth <= WORD_BITS) {
            out.data[0] = (data[0] >> lo) & word_mask(DstBitWidth);
            return out;
        }

        for (uint32_t i = 0; i < OUT_WORDS; ++i) {
            const uint32_t src_bit = lo + i * WORD_BITS;
            const uint32_t src_word = src_bit / WORD_BITS;
            const uint32_t src_shift = src_bit % WORD_BITS;

            uint64_t chunk = data[src_word] >> src_shift;
            if (src_shift != 0 && src_word + 1 < NUM_WORDS) {
                chunk |= data[src_word + 1] << (WORD_BITS - src_shift);
            }
            out.data[i] = chunk;
        }

        if constexpr (DstBitWidth % WORD_BITS != 0) {
            out.data[OUT_WORDS - 1] &= word_mask(DstBitWidth % WORD_BITS);
        }
        return out;
    }

    template <uint32_t DstBitWidth, uint32_t SrcBitWidth>
    constexpr void set_bits(uint32_t lo, const Int<SrcBitWidth>& value) {
        static_assert(DstBitWidth > 0, "DstBitWidth must be greater than 0");
        assert(lo + DstBitWidth <= BitWidth);

        constexpr uint32_t COPY_BITS = (DstBitWidth < SrcBitWidth) ? DstBitWidth : SrcBitWidth;
        const uint32_t hi = lo + DstBitWidth - 1;
        const uint32_t first_word = lo / WORD_BITS;
        const uint32_t last_word = hi / WORD_BITS;

        auto clear_mask = [](uint32_t bit_in_word, uint32_t bits) constexpr {
            return word_mask(bits) << bit_in_word;
        };

        if (first_word == last_word) {
            data[first_word] &= ~clear_mask(lo % WORD_BITS, DstBitWidth);
        } else {
            data[first_word] &= ~clear_mask(lo % WORD_BITS, WORD_BITS - (lo % WORD_BITS));
            for (uint32_t w = first_word + 1; w < last_word; ++w) {
                data[w] = 0;
            }
            data[last_word] &= ~clear_mask(0, (hi % WORD_BITS) + 1);
        }

        if constexpr (BitWidth <= WORD_BITS && SrcBitWidth <= WORD_BITS) {
            data[0] |= (value.data[0] & word_mask(COPY_BITS)) << lo;
            return;
        }

        const uint32_t write_hi = lo + COPY_BITS - 1;
        const uint32_t write_first = lo / WORD_BITS;
        const uint32_t write_last = write_hi / WORD_BITS;

        for (uint32_t w = write_first; w <= write_last; ++w) {
            const uint32_t word_lo = w * WORD_BITS;
            const uint32_t seg_lo = (lo > word_lo) ? lo : word_lo;
            const uint32_t seg_hi = (write_hi < (word_lo + WORD_BITS - 1)) ? write_hi : (word_lo + WORD_BITS - 1);
            const uint32_t seg_bits = seg_hi - seg_lo + 1;
            const uint32_t dst_shift = seg_lo - word_lo;
            const uint32_t src_off = seg_lo - lo;
            const uint32_t src_word = src_off / WORD_BITS;
            const uint32_t src_shift = src_off % WORD_BITS;

            uint64_t chunk = value.data[src_word] >> src_shift;
            if (src_shift != 0 && src_word + 1 < Int<SrcBitWidth>::NUM_WORDS) {
                chunk |= value.data[src_word + 1] << (WORD_BITS - src_shift);
            }
            if (seg_bits < WORD_BITS) {
                chunk &= word_mask(seg_bits);
            }
            data[w] |= chunk << dst_shift;
        }
    }

    template <uint32_t DstBitWidth, typename T>
        requires std::is_integral_v<T>
    constexpr void set_bits_from_integral(uint32_t lo, T value) {
        set_bits<DstBitWidth>(lo, Int<sizeof(T) * 8>(value));
    }

    constexpr bool get_bit(uint32_t bit) const {
        assert(bit < BitWidth);
        return ((data[bit / WORD_BITS] >> (bit % WORD_BITS)) & uint64_t(1)) != 0;
    }

    constexpr void set_bit(uint32_t bit, bool value) {
        assert(bit < BitWidth);
        const uint32_t word = bit / WORD_BITS;
        const uint32_t off = bit % WORD_BITS;
        const uint64_t mask = uint64_t(1) << off;
        if (value) {
            data[word] |= mask;
        } else {
            data[word] &= ~mask;
        }
    }

public:
    constexpr Int() : data{} {}

    constexpr Int(bool value) : data{} {
        data[0] = value ? 1 : 0;
    }

    template <typename T>
        requires std::is_integral_v<T>
    constexpr Int(T value) : data{} {
        data[0] = static_cast<uint64_t>(value);
        if constexpr (std::is_signed_v<T> && NUM_WORDS > 1) {
            if (value < 0) {
                for (uint32_t i = 1; i < NUM_WORDS; ++i) {
                    data[i] = ~uint64_t(0);
                }
            }
        }
        mask_high_bits();
    }

    template <uint32_t OtherBitWidth>
    constexpr Int(const Int<OtherBitWidth>& other) : data{} {
        constexpr uint32_t COPY_WORDS = (NUM_WORDS < Int<OtherBitWidth>::NUM_WORDS)
                                            ? NUM_WORDS
                                            : Int<OtherBitWidth>::NUM_WORDS;
        for (uint32_t i = 0; i < COPY_WORDS; ++i) {
            data[i] = other.data[i];
        }
        mask_high_bits();
    }

    template <typename Operand>
        requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
                 && !std::is_same_v<std::remove_cvref_t<Operand>, Int>
                 && !std::is_integral_v<std::remove_cvref_t<Operand>>)
    constexpr Int(const Operand& operand) : data{} {
        assign_from_same_width(cast_fixint_operand<BitWidth>(operand));
    }

    template <typename T>
        requires std::is_integral_v<T>
    constexpr T to() const {
        constexpr uint32_t T_BITS = sizeof(T) * 8;
        static_assert(T_BITS <= 64, "to<T>() only supports T up to 64 bits");
        if constexpr (std::is_same_v<T, bool>) {
            return (data[0] & uint64_t(1)) != 0;
        } else {
            using U = std::make_unsigned_t<T>;
            U raw = static_cast<U>(data[0]);
            if constexpr (!std::is_signed_v<T>) {
                return static_cast<T>(raw);
            } else {
                if constexpr (BitWidth < T_BITS) {
                    const bool sign = ((raw >> (BitWidth - 1)) & U(1)) != 0;
                    if (sign) {
                        raw |= ~((U(1) << BitWidth) - U(1));
                    }
                }
                return static_cast<T>(raw);
            }
        }
    }

    constexpr auto sint() const {
        return IntSignedView<const Int&>(*this);
    }

    template <typename T>
        requires std::is_integral_v<T>
    constexpr Int& operator=(T value) {
        return assign_from_same_width(Int(value));
    }

    template <uint32_t OtherBitWidth>
    constexpr Int& operator=(const Int<OtherBitWidth>& value) {
        return assign_from_same_width(Int(value));
    }

    template <typename Operand>
        requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
                 && !std::is_same_v<std::remove_cvref_t<Operand>, Int>)
    constexpr Int& operator=(const Operand& operand) {
        return assign_from_same_width(cast_fixint_operand<BitWidth>(operand));
    }

    template <uint32_t Hi, uint32_t Lo>
    constexpr auto at() {
        static_assert(Hi >= Lo, "Hi must be >= Lo");
        static_assert(Hi < BitWidth, "Hi out of range");
        return IntSliceRef<BitWidth, Hi - Lo + 1>(*this, Lo);
    }

    template <uint32_t Hi, uint32_t Lo>
    constexpr auto at() const {
        static_assert(Hi >= Lo, "Hi must be >= Lo");
        static_assert(Hi < BitWidth, "Hi out of range");
        return IntConstSliceRef<BitWidth, Hi - Lo + 1>(*this, Lo);
    }

    template <uint32_t Idx>
    constexpr auto at() {
        static_assert(Idx < BitWidth, "Idx out of range");
        return IntBitRef<BitWidth>(*this, Idx);
    }

    template <uint32_t Idx>
    constexpr auto at() const {
        static_assert(Idx < BitWidth, "Idx out of range");
        return IntConstBitRef<BitWidth>(*this, Idx);
    }

    template <uint32_t DstBitWidth, typename IdxT>
        requires std::is_integral_v<std::remove_cvref_t<IdxT>>
    constexpr auto pick(IdxT idx) {
        static_assert(DstBitWidth > 0, "DstBitWidth must be greater than 0");
        const uint32_t lo = static_cast<uint32_t>(idx);
        return IntSliceRef<BitWidth, DstBitWidth>(*this, lo);
    }

    template <uint32_t DstBitWidth, typename IdxT>
        requires std::is_integral_v<std::remove_cvref_t<IdxT>>
    constexpr auto pick(IdxT idx) const {
        static_assert(DstBitWidth > 0, "DstBitWidth must be greater than 0");
        const uint32_t lo = static_cast<uint32_t>(idx);
        return IntConstSliceRef<BitWidth, DstBitWidth>(*this, lo);
    }

    template <uint32_t DstBitWidth>
    constexpr auto pick(const Int<IDX_WIDTH>& idx) {
        return pick<DstBitWidth>(idx.template to<uint32_t>());
    }

    template <uint32_t DstBitWidth>
    constexpr auto pick(const Int<IDX_WIDTH>& idx) const {
        return pick<DstBitWidth>(idx.template to<uint32_t>());
    }

    template <typename IdxT>
        requires std::is_integral_v<std::remove_cvref_t<IdxT>>
    constexpr auto pick(IdxT idx) {
        return IntBitRef<BitWidth>(*this, static_cast<uint32_t>(idx));
    }

    template <typename IdxT>
        requires std::is_integral_v<std::remove_cvref_t<IdxT>>
    constexpr auto pick(IdxT idx) const {
        return IntConstBitRef<BitWidth>(*this, static_cast<uint32_t>(idx));
    }

    constexpr auto pick(const Int<IDX_WIDTH>& idx) {
        return pick(idx.template to<uint32_t>());
    }

    constexpr auto pick(const Int<IDX_WIDTH>& idx) const {
        return pick(idx.template to<uint32_t>());
    }

    template <uint32_t, uint32_t>
    friend class IntSliceRef;
    template <uint32_t, uint32_t>
    friend class IntConstSliceRef;
    template <uint32_t>
    friend class IntBitRef;
    template <uint32_t>
    friend class IntConstBitRef;
    template <uint32_t>
    friend class Int;
    template <typename LhsOperand, typename RhsOperand>
        requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
                 && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
                 && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED)
    friend constexpr auto operator+(const LhsOperand& lhs, const RhsOperand& rhs);
    template <typename LhsOperand, typename RhsOperand>
        requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
                 && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
                 && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED)
    friend constexpr auto operator-(const LhsOperand& lhs, const RhsOperand& rhs);
    template <typename Operand>
        requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID)
    friend constexpr auto operator-(const Operand& operand);
    template <typename LhsOperand, typename RhsOperand>
        requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID)
    friend constexpr auto operator*(const LhsOperand& lhs, const RhsOperand& rhs);
    template <typename LhsOperand, typename RhsOperand>
        requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
                 && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
                 && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED
                 && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                        == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
    friend constexpr auto operator&(const LhsOperand& lhs, const RhsOperand& rhs);
    template <typename LhsOperand, typename RhsOperand>
        requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
                 && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
                 && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED
                 && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                        == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
    friend constexpr auto operator|(const LhsOperand& lhs, const RhsOperand& rhs);
    template <typename LhsOperand, typename RhsOperand>
        requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
                 && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
                 && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED
                 && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                        == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
    friend constexpr auto operator^(const LhsOperand& lhs, const RhsOperand& rhs);
    template <typename Operand>
        requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
                 && !IntOperandTraits<std::remove_cvref_t<Operand>>::IS_SIGNED)
    friend constexpr auto operator~(const Operand& operand);
    template <typename LhsOperand, typename RhsOperand>
        requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
                 && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
                 && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED)
    friend constexpr auto operator<<(const LhsOperand& lhs, const RhsOperand& rhs);
    template <typename LhsOperand, typename RhsOperand>
        requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
                 && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED)
    friend constexpr auto operator>>(const LhsOperand& lhs, const RhsOperand& rhs);
    template <uint32_t ShiftBitWidth>
    friend constexpr bool shift_amount_at_least_width(const Int<ShiftBitWidth>& shift, uint32_t width);
    template <typename Operand>
        requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
                 && !IntOperandTraits<std::remove_cvref_t<Operand>>::IS_SIGNED)
    friend constexpr bool ReduceAnd(const Operand& operand);
    template <typename Operand>
        requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
                 && !IntOperandTraits<std::remove_cvref_t<Operand>>::IS_SIGNED)
    friend constexpr bool ReduceOr(const Operand& operand);
    template <typename Operand>
        requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
                 && !IntOperandTraits<std::remove_cvref_t<Operand>>::IS_SIGNED)
    friend constexpr bool ReduceXor(const Operand& operand);
    template <uint32_t N, typename Operand>
        requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
                 && !IntOperandTraits<std::remove_cvref_t<Operand>>::IS_SIGNED)
    friend constexpr auto Repeat(const Operand& operand);
    template <typename... Operands>
        requires(sizeof...(Operands) > 0
                 && (... && IntOperandTraits<std::remove_cvref_t<Operands>>::VALID)
                 && (... && !IntOperandTraits<std::remove_cvref_t<Operands>>::IS_SIGNED))
    friend constexpr auto Cat(const Operands&... operands);
    template <typename LhsOperand, typename RhsOperand>
        requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
                 && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
                 && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED
                 && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                        == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
    friend constexpr bool operator==(const LhsOperand& lhs, const RhsOperand& rhs);
    template <typename LhsOperand, typename RhsOperand>
        requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
                 && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
                 && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED
                 && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                        == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
    friend constexpr bool operator!=(const LhsOperand& lhs, const RhsOperand& rhs);
    template <typename LhsOperand, typename RhsOperand>
        requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                        == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
    friend constexpr bool operator<(const LhsOperand& lhs, const RhsOperand& rhs);
    template <typename LhsOperand, typename RhsOperand>
        requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                        == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
    friend constexpr bool operator<=(const LhsOperand& lhs, const RhsOperand& rhs);
    template <typename LhsOperand, typename RhsOperand>
        requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                        == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
    friend constexpr bool operator>(const LhsOperand& lhs, const RhsOperand& rhs);
    template <typename LhsOperand, typename RhsOperand>
        requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                        == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
    friend constexpr bool operator>=(const LhsOperand& lhs, const RhsOperand& rhs);
};

template <uint32_t ParentBitWidth, uint32_t SliceBitWidth>
class IntSliceRef {
private:
    Int<ParentBitWidth>& parent;
    uint32_t lo;

public:
    static constexpr uint32_t WIDTH = SliceBitWidth;

    constexpr IntSliceRef(const IntSliceRef&) = default;

    constexpr IntSliceRef(Int<ParentBitWidth>& p, uint32_t l) : parent(p), lo(l) {
        assert(l + SliceBitWidth <= ParentBitWidth);
    }

    constexpr operator Int<SliceBitWidth>() const {
        return parent.template get_bits<SliceBitWidth>(lo);
    }

    constexpr Int<SliceBitWidth> to_int() const {
        return parent.template get_bits<SliceBitWidth>(lo);
    }

    constexpr auto sint() const {
        return IntSignedView<IntSliceRef>(*this);
    }

    template <typename T>
        requires std::is_integral_v<T>
    constexpr operator T() const {
        return to_int().template to<T>();
    }

    template <typename T>
        requires std::is_integral_v<T>
    constexpr IntSliceRef& operator=(T value) {
        parent.template set_bits<SliceBitWidth>(lo, Int<sizeof(T) * 8>(value));
        return *this;
    }

    constexpr IntSliceRef& operator=(const IntSliceRef& other) {
        parent.template set_bits<SliceBitWidth>(lo, other.to_int());
        return *this;
    }

    template <typename Operand>
        requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<Operand>>::BIT_WIDTH == SliceBitWidth)
    constexpr IntSliceRef& operator=(const Operand& operand) {
        parent.template set_bits<SliceBitWidth>(lo, Int<ParentBitWidth>::template cast_fixint_operand<SliceBitWidth>(operand));
        return *this;
    }

    template <uint32_t OtherParentBitWidth, uint32_t OtherSliceBitWidth>
        requires (OtherSliceBitWidth != SliceBitWidth)
    constexpr IntSliceRef& operator=(const IntSliceRef<OtherParentBitWidth, OtherSliceBitWidth>&) = delete;

    template <uint32_t OtherParentBitWidth, uint32_t OtherSliceBitWidth>
        requires (OtherSliceBitWidth != SliceBitWidth)
    constexpr IntSliceRef& operator=(const IntConstSliceRef<OtherParentBitWidth, OtherSliceBitWidth>&) = delete;

    template <typename Operand>
        requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<Operand>>::BIT_WIDTH != SliceBitWidth)
    constexpr IntSliceRef& operator=(const Operand&) = delete;
};

template <uint32_t ParentBitWidth, uint32_t SliceBitWidth>
class IntConstSliceRef {
private:
    const Int<ParentBitWidth>& parent;
    uint32_t lo;

public:
    static constexpr uint32_t WIDTH = SliceBitWidth;

    constexpr IntConstSliceRef(const IntConstSliceRef&) = default;

    constexpr IntConstSliceRef(const Int<ParentBitWidth>& p, uint32_t l) : parent(p), lo(l) {
        assert(l + SliceBitWidth <= ParentBitWidth);
    }

    constexpr operator Int<SliceBitWidth>() const {
        return parent.template get_bits<SliceBitWidth>(lo);
    }

    constexpr Int<SliceBitWidth> to_int() const {
        return parent.template get_bits<SliceBitWidth>(lo);
    }

    constexpr auto sint() const {
        return IntSignedView<IntConstSliceRef>(*this);
    }

    template <typename T>
        requires std::is_integral_v<T>
    constexpr operator T() const {
        return to_int().template to<T>();
    }

    friend class IntSliceRef<ParentBitWidth, SliceBitWidth>;
};

template <uint32_t ParentBitWidth>
class IntBitRef {
private:
    Int<ParentBitWidth>& parent;
    uint32_t bit;

public:
    static constexpr uint32_t WIDTH = 1;

    constexpr IntBitRef(const IntBitRef&) = default;

    constexpr IntBitRef(Int<ParentBitWidth>& p, uint32_t b) : parent(p), bit(b) {
        assert(b < ParentBitWidth);
    }

    constexpr operator bool() const {
        return parent.get_bit(bit);
    }

    constexpr operator Int<1>() const {
        return Int<1>(parent.get_bit(bit));
    }

    constexpr Int<1> to_int() const {
        return Int<1>(parent.get_bit(bit));
    }

    constexpr auto sint() const {
        return IntSignedView<IntBitRef>(*this);
    }

    template <typename T>
        requires std::is_integral_v<T>
    constexpr operator T() const {
        return Int<1>(*this).template to<T>();
    }

    constexpr IntBitRef& operator=(bool value) {
        parent.set_bit(bit, value);
        return *this;
    }

    template <typename T>
        requires std::is_integral_v<T>
    constexpr IntBitRef& operator=(T value) {
        return *this = Int<1>(value);
    }

    constexpr IntBitRef& operator=(const IntBitRef& other) {
        parent.set_bit(bit, other.to_int().template to<bool>());
        return *this;
    }

    template <typename Operand>
        requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<Operand>>::BIT_WIDTH == 1)
    constexpr IntBitRef& operator=(const Operand& operand) {
        parent.set_bit(bit, Int<ParentBitWidth>::template cast_fixint_operand<1>(operand).template to<bool>());
        return *this;
    }

    template <typename Operand>
        requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
                 && IntOperandTraits<std::remove_cvref_t<Operand>>::BIT_WIDTH != 1)
    constexpr IntBitRef& operator=(const Operand&) = delete;

    template <uint32_t>
    friend class Int;
};

template <uint32_t ParentBitWidth>
class IntConstBitRef {
private:
    const Int<ParentBitWidth>& parent;
    uint32_t bit;

public:
    static constexpr uint32_t WIDTH = 1;

    constexpr IntConstBitRef(const IntConstBitRef&) = default;

    constexpr IntConstBitRef(const Int<ParentBitWidth>& p, uint32_t b) : parent(p), bit(b) {
        assert(b < ParentBitWidth);
    }

    constexpr operator bool() const {
        return parent.get_bit(bit);
    }

    constexpr operator Int<1>() const {
        return Int<1>(parent.get_bit(bit));
    }

    constexpr Int<1> to_int() const {
        return Int<1>(parent.get_bit(bit));
    }

    constexpr auto sint() const {
        return IntSignedView<IntConstBitRef>(*this);
    }

    template <typename T>
        requires std::is_integral_v<T>
    constexpr operator T() const {
        return Int<1>(*this).template to<T>();
    }

    friend class IntBitRef<ParentBitWidth>;
    template <uint32_t>
    friend class Int;
};

template <typename Operand>
class IntSignedView {
private:
    using OperandType = std::remove_cvref_t<Operand>;

public:
    static_assert(IntOperandTraits<OperandType>::VALID,
                  "IntSignedView operand must be Int or an Int ref proxy");
    static constexpr uint32_t BIT_WIDTH = IntOperandTraits<OperandType>::BIT_WIDTH;

    Operand operand;

    constexpr explicit IntSignedView(Operand op) : operand(op) {}

    constexpr Int<BIT_WIDTH> to_int() const {
        return IntOperandTraits<OperandType>::get_int(operand);
    }
};

template <typename Operand>
struct IntOperandTraits {
    static constexpr bool VALID = false;
};

template <uint32_t BitWidth>
struct IntOperandTraits<Int<BitWidth>> {
    static constexpr bool VALID = true;
    static constexpr bool IS_SIGNED = false;
    static constexpr uint32_t BIT_WIDTH = BitWidth;

    static constexpr Int<BitWidth> get_int(const Int<BitWidth>& value) {
        return value;
    }
};

template <uint32_t ParentBitWidth, uint32_t SliceBitWidth>
struct IntOperandTraits<IntSliceRef<ParentBitWidth, SliceBitWidth>> {
    static constexpr bool VALID = true;
    static constexpr bool IS_SIGNED = false;
    static constexpr uint32_t BIT_WIDTH = SliceBitWidth;

    static constexpr Int<SliceBitWidth> get_int(const IntSliceRef<ParentBitWidth, SliceBitWidth>& value) {
        return value.to_int();
    }
};

template <uint32_t ParentBitWidth, uint32_t SliceBitWidth>
struct IntOperandTraits<IntConstSliceRef<ParentBitWidth, SliceBitWidth>> {
    static constexpr bool VALID = true;
    static constexpr bool IS_SIGNED = false;
    static constexpr uint32_t BIT_WIDTH = SliceBitWidth;

    static constexpr Int<SliceBitWidth> get_int(const IntConstSliceRef<ParentBitWidth, SliceBitWidth>& value) {
        return value.to_int();
    }
};

template <uint32_t ParentBitWidth>
struct IntOperandTraits<IntBitRef<ParentBitWidth>> {
    static constexpr bool VALID = true;
    static constexpr bool IS_SIGNED = false;
    static constexpr uint32_t BIT_WIDTH = 1;

    static constexpr Int<1> get_int(const IntBitRef<ParentBitWidth>& value) {
        return value.to_int();
    }
};

template <uint32_t ParentBitWidth>
struct IntOperandTraits<IntConstBitRef<ParentBitWidth>> {
    static constexpr bool VALID = true;
    static constexpr bool IS_SIGNED = false;
    static constexpr uint32_t BIT_WIDTH = 1;

    static constexpr Int<1> get_int(const IntConstBitRef<ParentBitWidth>& value) {
        return value.to_int();
    }
};

template <typename Operand>
struct IntOperandTraits<IntSignedView<Operand>> {
private:
    using BaseOperand = std::remove_cvref_t<Operand>;

public:
    static constexpr bool VALID = IntOperandTraits<BaseOperand>::VALID;
    static constexpr bool IS_SIGNED = true;
    static constexpr uint32_t BIT_WIDTH = IntOperandTraits<BaseOperand>::BIT_WIDTH;

    static constexpr Int<BIT_WIDTH> get_int(const IntSignedView<Operand>& value) {
        return value.to_int();
    }
};

template <typename Operand>
inline constexpr bool is_int_operand_v = IntOperandTraits<std::remove_cvref_t<Operand>>::VALID;

template <typename Operand>
inline constexpr bool is_int_operand_signed_v = IntOperandTraits<std::remove_cvref_t<Operand>>::IS_SIGNED;

template <typename Operand>
inline constexpr uint32_t int_operand_bit_width_v = IntOperandTraits<std::remove_cvref_t<Operand>>::BIT_WIDTH;

template <typename Operand>
using int_operand_int_t = Int<int_operand_bit_width_v<Operand>>;

template <typename Operand>
constexpr int_operand_int_t<Operand> get_int_operand_value(const Operand& operand) {
    using Traits = IntOperandTraits<std::remove_cvref_t<Operand>>;
    static_assert(Traits::VALID, "operand is not a supported fixint operand");
    return Traits::get_int(operand);
}

template <uint32_t ShiftBitWidth>
constexpr bool shift_amount_at_least_width(const Int<ShiftBitWidth>& shift, uint32_t width) {
    if constexpr (Int<ShiftBitWidth>::NUM_WORDS > 1) {
        for (uint32_t i = 1; i < Int<ShiftBitWidth>::NUM_WORDS; ++i) {
            if (shift.data[i] != 0) {
                return true;
            }
        }
    }
    return shift.data[0] >= uint64_t(width);
}

template <typename T>
constexpr uint64_t checked_integral_shift_amount(T value) {
    using RawT = std::remove_cvref_t<T>;
    static_assert(std::is_integral_v<RawT>, "shift amount must be integral");

    if constexpr (std::is_same_v<RawT, bool>) {
        return value ? 1 : 0;
    } else {
        if constexpr (std::is_signed_v<RawT>) {
            assert(value >= 0);
        }
        using UnsignedT = std::make_unsigned_t<RawT>;
        return static_cast<uint64_t>(static_cast<UnsignedT>(value));
    }
}

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
             && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED)
constexpr auto operator+(const LhsOperand& lhs_operand, const RhsOperand& rhs_operand) {
    constexpr uint32_t LHS_BIT_WIDTH = int_operand_bit_width_v<LhsOperand>;
    constexpr uint32_t RHS_BIT_WIDTH = int_operand_bit_width_v<RhsOperand>;
    constexpr uint32_t RESULT_BIT_WIDTH = ((LHS_BIT_WIDTH > RHS_BIT_WIDTH) ? LHS_BIT_WIDTH : RHS_BIT_WIDTH) + 1;
    constexpr uint32_t LHS_WORDS = Int<LHS_BIT_WIDTH>::NUM_WORDS;
    constexpr uint32_t RHS_WORDS = Int<RHS_BIT_WIDTH>::NUM_WORDS;
    constexpr uint32_t MAX_WORDS = (LHS_WORDS > RHS_WORDS) ? LHS_WORDS : RHS_WORDS;

    const Int<LHS_BIT_WIDTH> lhs = get_int_operand_value(lhs_operand);
    const Int<RHS_BIT_WIDTH> rhs = get_int_operand_value(rhs_operand);
    Int<RESULT_BIT_WIDTH> out(0);

    if constexpr (LHS_BIT_WIDTH <= 64 && RHS_BIT_WIDTH <= 64) {
        const uint64_t l = lhs.data[0];
        const uint64_t r = rhs.data[0];
        const uint64_t sum = l + r;
        out.data[0] = sum;
        if constexpr (RESULT_BIT_WIDTH > 64) {
            out.data[1] = (sum < l) ? 1 : 0;
        }
        out.mask_high_bits();
        return out;
    } else {
        uint64_t carry = 0;

        if constexpr (LHS_WORDS <= RHS_WORDS) {
            for (uint32_t i = 0; i < LHS_WORDS; ++i) {
                const uint64_t s1 = lhs.data[i] + rhs.data[i];
                const uint64_t c1 = (s1 < lhs.data[i]) ? 1 : 0;
                const uint64_t s2 = s1 + carry;
                const uint64_t c2 = (s2 < s1) ? 1 : 0;
                out.data[i] = s2;
                carry = c1 | c2;
            }
            for (uint32_t i = LHS_WORDS; i < RHS_WORDS; ++i) {
                const uint64_t s = rhs.data[i] + carry;
                out.data[i] = s;
                carry = (s < rhs.data[i]) ? 1 : 0;
            }
        } else {
            for (uint32_t i = 0; i < RHS_WORDS; ++i) {
                const uint64_t s1 = lhs.data[i] + rhs.data[i];
                const uint64_t c1 = (s1 < lhs.data[i]) ? 1 : 0;
                const uint64_t s2 = s1 + carry;
                const uint64_t c2 = (s2 < s1) ? 1 : 0;
                out.data[i] = s2;
                carry = c1 | c2;
            }
            for (uint32_t i = RHS_WORDS; i < LHS_WORDS; ++i) {
                const uint64_t s = lhs.data[i] + carry;
                out.data[i] = s;
                carry = (s < lhs.data[i]) ? 1 : 0;
            }
        }

        if constexpr (Int<RESULT_BIT_WIDTH>::NUM_WORDS > MAX_WORDS) {
            out.data[MAX_WORDS] = carry;
        }

        out.mask_high_bits();
        return out;
    }
}

template <typename IntOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<IntOperand>>::IS_SIGNED
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr auto operator+(const IntOperand& lhs_operand, T rhs) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<IntOperand>;
    return lhs_operand + Int<BIT_WIDTH>(rhs);
}

template <typename T, typename IntOperand>
    requires(std::is_integral_v<std::remove_cvref_t<T>>
             && IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<IntOperand>>::IS_SIGNED)
constexpr auto operator+(T lhs, const IntOperand& rhs_operand) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<IntOperand>;
    return Int<BIT_WIDTH>(lhs) + rhs_operand;
}

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
             && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED)
constexpr auto operator-(const LhsOperand& lhs_operand, const RhsOperand& rhs_operand) {
    constexpr uint32_t LHS_BIT_WIDTH = int_operand_bit_width_v<LhsOperand>;
    constexpr uint32_t RHS_BIT_WIDTH = int_operand_bit_width_v<RhsOperand>;
    constexpr uint32_t RESULT_BIT_WIDTH = (LHS_BIT_WIDTH > RHS_BIT_WIDTH) ? LHS_BIT_WIDTH : RHS_BIT_WIDTH;
    constexpr uint32_t LHS_WORDS = Int<LHS_BIT_WIDTH>::NUM_WORDS;
    constexpr uint32_t RHS_WORDS = Int<RHS_BIT_WIDTH>::NUM_WORDS;

    const Int<LHS_BIT_WIDTH> lhs = get_int_operand_value(lhs_operand);
    const Int<RHS_BIT_WIDTH> rhs = get_int_operand_value(rhs_operand);
    Int<RESULT_BIT_WIDTH> out(0);

    if constexpr (RESULT_BIT_WIDTH <= 64) {
        out.data[0] = lhs.data[0] - rhs.data[0];
        out.mask_high_bits();
        return out;
    } else {
        uint64_t borrow = 0;

        if constexpr (LHS_WORDS <= RHS_WORDS) {
            for (uint32_t i = 0; i < LHS_WORDS; ++i) {
                const uint64_t l = lhs.data[i];
                const uint64_t r = rhs.data[i];
                out.data[i] = l - r - borrow;
                const uint64_t rb = r + borrow;
                borrow = ((rb < r) ? 1 : 0) | ((l < rb) ? 1 : 0);
            }
            for (uint32_t i = LHS_WORDS; i < RHS_WORDS; ++i) {
                const uint64_t r = rhs.data[i];
                out.data[i] = uint64_t(0) - r - borrow;
                const uint64_t rb = r + borrow;
                borrow = ((rb < r) ? 1 : 0) | ((rb != 0) ? 1 : 0);
            }
        } else {
            for (uint32_t i = 0; i < RHS_WORDS; ++i) {
                const uint64_t l = lhs.data[i];
                const uint64_t r = rhs.data[i];
                out.data[i] = l - r - borrow;
                const uint64_t rb = r + borrow;
                borrow = ((rb < r) ? 1 : 0) | ((l < rb) ? 1 : 0);
            }
            for (uint32_t i = RHS_WORDS; i < LHS_WORDS; ++i) {
                const uint64_t l = lhs.data[i];
                out.data[i] = l - borrow;
                borrow = (l < borrow) ? 1 : 0;
            }
        }

        out.mask_high_bits();
        return out;
    }
}

template <typename IntOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<IntOperand>>::IS_SIGNED
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr auto operator-(const IntOperand& lhs_operand, T rhs) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<IntOperand>;
    return lhs_operand - Int<BIT_WIDTH>(rhs);
}

template <typename T, typename IntOperand>
    requires(std::is_integral_v<std::remove_cvref_t<T>>
             && IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<IntOperand>>::IS_SIGNED)
constexpr auto operator-(T lhs, const IntOperand& rhs_operand) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<IntOperand>;
    return Int<BIT_WIDTH>(lhs) - rhs_operand;
}

template <typename Operand>
    requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID)
constexpr auto operator-(const Operand& operand) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<Operand>;
    Int<BIT_WIDTH> value = get_int_operand_value(operand);
    Int<BIT_WIDTH> out(0);

    if constexpr (BIT_WIDTH <= 64) {
        out.data[0] = ~value.data[0] + 1;
    } else {
        uint64_t carry = 1;
        for (uint32_t i = 0; i < Int<BIT_WIDTH>::NUM_WORDS; ++i) {
            const uint64_t sum = ~value.data[i] + carry;
            out.data[i] = sum;
            carry = (carry != 0) && (sum == 0);
        }
    }

    out.mask_high_bits();
    return out;
}

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID)
constexpr auto operator*(const LhsOperand& lhs_operand, const RhsOperand& rhs_operand) {
    using LhsTraits = IntOperandTraits<std::remove_cvref_t<LhsOperand>>;
    using RhsTraits = IntOperandTraits<std::remove_cvref_t<RhsOperand>>;

    constexpr uint32_t LHS_BIT_WIDTH = LhsTraits::BIT_WIDTH;
    constexpr uint32_t RHS_BIT_WIDTH = RhsTraits::BIT_WIDTH;
    constexpr uint32_t RESULT_BIT_WIDTH = LHS_BIT_WIDTH + RHS_BIT_WIDTH;
    constexpr uint32_t LHS_WORDS = Int<LHS_BIT_WIDTH>::NUM_WORDS;
    constexpr uint32_t RHS_WORDS = Int<RHS_BIT_WIDTH>::NUM_WORDS;
    constexpr uint32_t OUT_WORDS = Int<RESULT_BIT_WIDTH>::NUM_WORDS;

    const Int<LHS_BIT_WIDTH> lhs = get_int_operand_value(lhs_operand);
    const Int<RHS_BIT_WIDTH> rhs = get_int_operand_value(rhs_operand);
    Int<RESULT_BIT_WIDTH> out(0);

    constexpr uint32_t LHS_SIGN_WORD = (LHS_BIT_WIDTH - 1) / 64;
    constexpr uint32_t LHS_SIGN_OFF = (LHS_BIT_WIDTH - 1) % 64;
    constexpr uint32_t RHS_SIGN_WORD = (RHS_BIT_WIDTH - 1) / 64;
    constexpr uint32_t RHS_SIGN_OFF = (RHS_BIT_WIDTH - 1) % 64;

    const bool lhs_negative = LhsTraits::IS_SIGNED && ((lhs.data[LHS_SIGN_WORD] >> LHS_SIGN_OFF) & 1U);
    const bool rhs_negative = RhsTraits::IS_SIGNED && ((rhs.data[RHS_SIGN_WORD] >> RHS_SIGN_OFF) & 1U);

    if constexpr (LHS_BIT_WIDTH <= 64 && RHS_BIT_WIDTH <= 64) {
        auto sext64 = [](uint64_t v, uint32_t bits) constexpr -> int64_t {
            if (bits == 64) {
                return static_cast<int64_t>(v);
            }
            const uint64_t mask = low_mask64(bits);
            v &= mask;
            const uint64_t sign = uint64_t(1) << (bits - 1);
            if (v & sign) {
                v |= ~mask;
            }
            return static_cast<int64_t>(v);
        };

        if constexpr (LhsTraits::IS_SIGNED || RhsTraits::IS_SIGNED) {
            const int128_t l = LhsTraits::IS_SIGNED ? static_cast<int128_t>(sext64(lhs.data[0], LHS_BIT_WIDTH))
                                                    : static_cast<int128_t>(lhs.data[0]);
            const int128_t r = RhsTraits::IS_SIGNED ? static_cast<int128_t>(sext64(rhs.data[0], RHS_BIT_WIDTH))
                                                    : static_cast<int128_t>(rhs.data[0]);
            const uint128_t prod = static_cast<uint128_t>(l * r);
            out.data[0] = static_cast<uint64_t>(prod);
            if constexpr (OUT_WORDS > 1) {
                out.data[1] = static_cast<uint64_t>(prod >> 64);
            }
        } else {
            const uint128_t prod = static_cast<uint128_t>(lhs.data[0]) * static_cast<uint128_t>(rhs.data[0]);
            out.data[0] = static_cast<uint64_t>(prod);
            if constexpr (OUT_WORDS > 1) {
                out.data[1] = static_cast<uint64_t>(prod >> 64);
            }
        }
        out.mask_high_bits();
        return out;
    } else {
        auto lhs_ext_word = [&](uint32_t i) constexpr -> uint64_t {
            if (i < LHS_WORDS) {
                uint64_t w = lhs.data[i];
                if constexpr (LhsTraits::IS_SIGNED) {
                    if (lhs_negative) {
                        constexpr uint32_t LAST = LHS_WORDS - 1;
                        constexpr uint32_t REM = LHS_BIT_WIDTH % 64;
                        if constexpr (REM != 0) {
                            if (i == LAST) {
                                w |= ~low_mask64(REM);
                            }
                        }
                    }
                }
                return w;
            }
            if constexpr (LhsTraits::IS_SIGNED) {
                return lhs_negative ? ~uint64_t(0) : uint64_t(0);
            } else {
                return uint64_t(0);
            }
        };

        auto rhs_ext_word = [&](uint32_t i) constexpr -> uint64_t {
            if (i < RHS_WORDS) {
                uint64_t w = rhs.data[i];
                if constexpr (RhsTraits::IS_SIGNED) {
                    if (rhs_negative) {
                        constexpr uint32_t LAST = RHS_WORDS - 1;
                        constexpr uint32_t REM = RHS_BIT_WIDTH % 64;
                        if constexpr (REM != 0) {
                            if (i == LAST) {
                                w |= ~low_mask64(REM);
                            }
                        }
                    }
                }
                return w;
            }
            if constexpr (RhsTraits::IS_SIGNED) {
                return rhs_negative ? ~uint64_t(0) : uint64_t(0);
            } else {
                return uint64_t(0);
            }
        };

        for (uint32_t i = 0; i < OUT_WORDS; ++i) {
            const uint64_t lw = lhs_ext_word(i);
            if (lw == 0) {
                continue;
            }

            uint128_t carry = 0;
            for (uint32_t j = 0; j + i < OUT_WORDS; ++j) {
                const uint32_t k = i + j;
                const uint128_t acc =
                    static_cast<uint128_t>(lw) * rhs_ext_word(j) + static_cast<uint128_t>(out.data[k]) + carry;
                out.data[k] = static_cast<uint64_t>(acc);
                carry = acc >> 64;
            }
        }

        out.mask_high_bits();
        return out;
    }
}

template <typename IntOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr auto operator*(const IntOperand& lhs_operand, T rhs) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<IntOperand>;
    if constexpr (std::is_signed_v<std::remove_cvref_t<T>>) {
        return lhs_operand * Int<BIT_WIDTH>(rhs).sint();
    } else {
        return lhs_operand * Int<BIT_WIDTH>(rhs);
    }
}

template <typename T, typename IntOperand>
    requires(std::is_integral_v<std::remove_cvref_t<T>>
             && IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID)
constexpr auto operator*(T lhs, const IntOperand& rhs_operand) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<IntOperand>;
    if constexpr (std::is_signed_v<std::remove_cvref_t<T>>) {
        return Int<BIT_WIDTH>(lhs).sint() * rhs_operand;
    } else {
        return Int<BIT_WIDTH>(lhs) * rhs_operand;
    }
}

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
             && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED
             && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                    == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
constexpr auto operator&(const LhsOperand& lhs_operand, const RhsOperand& rhs_operand) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<LhsOperand>;
    const Int<BIT_WIDTH> lhs = get_int_operand_value(lhs_operand);
    const Int<BIT_WIDTH> rhs = get_int_operand_value(rhs_operand);
    Int<BIT_WIDTH> out(0);

    for (uint32_t i = 0; i < Int<BIT_WIDTH>::NUM_WORDS; ++i) {
        out.data[i] = lhs.data[i] & rhs.data[i];
    }
    out.mask_high_bits();
    return out;
}

template <typename IntOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<IntOperand>>::IS_SIGNED
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr auto operator&(const IntOperand& lhs_operand, T rhs) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<IntOperand>;
    return lhs_operand & Int<BIT_WIDTH>(rhs);
}

template <typename T, typename IntOperand>
    requires(std::is_integral_v<std::remove_cvref_t<T>>
             && IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<IntOperand>>::IS_SIGNED)
constexpr auto operator&(T lhs, const IntOperand& rhs_operand) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<IntOperand>;
    return Int<BIT_WIDTH>(lhs) & rhs_operand;
}

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
             && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED
             && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                    == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
constexpr auto operator|(const LhsOperand& lhs_operand, const RhsOperand& rhs_operand) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<LhsOperand>;
    const Int<BIT_WIDTH> lhs = get_int_operand_value(lhs_operand);
    const Int<BIT_WIDTH> rhs = get_int_operand_value(rhs_operand);
    Int<BIT_WIDTH> out(0);

    for (uint32_t i = 0; i < Int<BIT_WIDTH>::NUM_WORDS; ++i) {
        out.data[i] = lhs.data[i] | rhs.data[i];
    }
    out.mask_high_bits();
    return out;
}

template <typename IntOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<IntOperand>>::IS_SIGNED
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr auto operator|(const IntOperand& lhs_operand, T rhs) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<IntOperand>;
    return lhs_operand | Int<BIT_WIDTH>(rhs);
}

template <typename T, typename IntOperand>
    requires(std::is_integral_v<std::remove_cvref_t<T>>
             && IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<IntOperand>>::IS_SIGNED)
constexpr auto operator|(T lhs, const IntOperand& rhs_operand) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<IntOperand>;
    return Int<BIT_WIDTH>(lhs) | rhs_operand;
}

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
             && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED
             && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                    == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
constexpr auto operator^(const LhsOperand& lhs_operand, const RhsOperand& rhs_operand) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<LhsOperand>;
    const Int<BIT_WIDTH> lhs = get_int_operand_value(lhs_operand);
    const Int<BIT_WIDTH> rhs = get_int_operand_value(rhs_operand);
    Int<BIT_WIDTH> out(0);

    for (uint32_t i = 0; i < Int<BIT_WIDTH>::NUM_WORDS; ++i) {
        out.data[i] = lhs.data[i] ^ rhs.data[i];
    }
    out.mask_high_bits();
    return out;
}

template <typename IntOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<IntOperand>>::IS_SIGNED
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr auto operator^(const IntOperand& lhs_operand, T rhs) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<IntOperand>;
    return lhs_operand ^ Int<BIT_WIDTH>(rhs);
}

template <typename T, typename IntOperand>
    requires(std::is_integral_v<std::remove_cvref_t<T>>
             && IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<IntOperand>>::IS_SIGNED)
constexpr auto operator^(T lhs, const IntOperand& rhs_operand) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<IntOperand>;
    return Int<BIT_WIDTH>(lhs) ^ rhs_operand;
}

template <typename Operand>
    requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<Operand>>::IS_SIGNED)
constexpr auto operator~(const Operand& operand) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<Operand>;
    const Int<BIT_WIDTH> value = get_int_operand_value(operand);
    Int<BIT_WIDTH> out(0);

    for (uint32_t i = 0; i < Int<BIT_WIDTH>::NUM_WORDS; ++i) {
        out.data[i] = ~value.data[i];
    }
    out.mask_high_bits();
    return out;
}

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
             && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED)
constexpr auto operator<<(const LhsOperand& lhs_operand, const RhsOperand& rhs_operand) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<LhsOperand>;
    constexpr uint32_t RHS_BIT_WIDTH = int_operand_bit_width_v<RhsOperand>;
    constexpr uint32_t NUM_WORDS = Int<BIT_WIDTH>::NUM_WORDS;

    const Int<BIT_WIDTH> lhs = get_int_operand_value(lhs_operand);
    const Int<RHS_BIT_WIDTH> rhs = get_int_operand_value(rhs_operand);
    Int<BIT_WIDTH> out(0);

    if (shift_amount_at_least_width(rhs, BIT_WIDTH)) {
        return out;
    }

    const uint32_t shift = static_cast<uint32_t>(rhs.data[0]);
    if (shift == 0) {
        return lhs;
    }

    const uint32_t word_shift = shift / 64;
    const uint32_t bit_shift = shift % 64;
    for (uint32_t i = NUM_WORDS; i > 0; --i) {
        const uint32_t dst_idx = i - 1;
        if (dst_idx < word_shift) {
            continue;
        }

        const uint32_t src_idx = dst_idx - word_shift;
        uint64_t word = lhs.data[src_idx] << bit_shift;
        if (bit_shift != 0 && src_idx > 0) {
            word |= lhs.data[src_idx - 1] >> (64 - bit_shift);
        }
        out.data[dst_idx] = word;
    }
    out.mask_high_bits();
    return out;
}

template <typename LhsOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr auto operator<<(const LhsOperand& lhs_operand, T rhs) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<LhsOperand>;
    const uint64_t shift = checked_integral_shift_amount(rhs);
    if (shift >= uint64_t(BIT_WIDTH)) {
        return Int<BIT_WIDTH>(0);
    }
    return lhs_operand << Int<64>(shift);
}

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED)
constexpr auto operator>>(const LhsOperand& lhs_operand, const RhsOperand& rhs_operand) {
    using LhsTraits = IntOperandTraits<std::remove_cvref_t<LhsOperand>>;
    constexpr uint32_t BIT_WIDTH = LhsTraits::BIT_WIDTH;
    constexpr uint32_t RHS_BIT_WIDTH = int_operand_bit_width_v<RhsOperand>;
    constexpr uint32_t NUM_WORDS = Int<BIT_WIDTH>::NUM_WORDS;
    constexpr uint32_t TOP_BITS = BIT_WIDTH % 64;
    constexpr uint32_t SIGN_WORD = (BIT_WIDTH - 1) / 64;
    constexpr uint32_t SIGN_OFF = (BIT_WIDTH - 1) % 64;

    const Int<BIT_WIDTH> lhs = get_int_operand_value(lhs_operand);
    const Int<RHS_BIT_WIDTH> rhs = get_int_operand_value(rhs_operand);
    Int<BIT_WIDTH> out(0);

    if (shift_amount_at_least_width(rhs, BIT_WIDTH)) {
        return out;
    }

    const uint32_t shift = static_cast<uint32_t>(rhs.data[0]);
    if (shift == 0) {
        return lhs;
    }

    const bool negative = LhsTraits::IS_SIGNED && (((lhs.data[SIGN_WORD] >> SIGN_OFF) & uint64_t(1)) != 0);
    const uint64_t fill_word = negative ? ~uint64_t(0) : uint64_t(0);
    auto word_at = [&](uint32_t idx) constexpr {
        if (idx >= NUM_WORDS) {
            return fill_word;
        }
        uint64_t word = lhs.data[idx];
        if constexpr (TOP_BITS != 0) {
            if (negative && idx == NUM_WORDS - 1) {
                word |= ~low_mask64(TOP_BITS);
            }
        }
        return word;
    };

    const uint32_t word_shift = shift / 64;
    const uint32_t bit_shift = shift % 64;
    for (uint32_t dst_idx = 0; dst_idx < NUM_WORDS; ++dst_idx) {
        const uint32_t src_idx = dst_idx + word_shift;
        uint64_t word = word_at(src_idx) >> bit_shift;
        if (bit_shift != 0) {
            word |= word_at(src_idx + 1) << (64 - bit_shift);
        }
        out.data[dst_idx] = word;
    }
    out.mask_high_bits();
    return out;
}

template <typename LhsOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr auto operator>>(const LhsOperand& lhs_operand, T rhs) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<LhsOperand>;
    const uint64_t shift = checked_integral_shift_amount(rhs);
    if (shift >= uint64_t(BIT_WIDTH)) {
        return Int<BIT_WIDTH>(0);
    }
    return lhs_operand >> Int<64>(shift);
}

template <typename Operand>
    requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<Operand>>::IS_SIGNED)
constexpr bool ReduceAnd(const Operand& operand) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<Operand>;
    constexpr uint32_t FULL_WORDS = BIT_WIDTH / 64;
    constexpr uint32_t REM_BITS = BIT_WIDTH % 64;
    const Int<BIT_WIDTH> value = get_int_operand_value(operand);

    for (uint32_t i = 0; i < FULL_WORDS; ++i) {
        if (value.data[i] != ~uint64_t(0)) {
            return false;
        }
    }
    if constexpr (REM_BITS != 0) {
        return value.data[FULL_WORDS] == low_mask64(REM_BITS);
    } else {
        return true;
    }
}

template <typename Operand>
    requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<Operand>>::IS_SIGNED)
constexpr bool ReduceOr(const Operand& operand) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<Operand>;
    const Int<BIT_WIDTH> value = get_int_operand_value(operand);

    for (uint32_t i = 0; i < Int<BIT_WIDTH>::NUM_WORDS; ++i) {
        if (value.data[i] != 0) {
            return true;
        }
    }
    return false;
}

template <typename Operand>
    requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<Operand>>::IS_SIGNED)
constexpr bool ReduceXor(const Operand& operand) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<Operand>;
    const Int<BIT_WIDTH> value = get_int_operand_value(operand);
    bool parity = false;

    for (uint32_t i = 0; i < Int<BIT_WIDTH>::NUM_WORDS; ++i) {
        uint64_t word = value.data[i];
        while (word != 0) {
            parity = !parity;
            word &= word - 1;
        }
    }
    return parity;
}

template <uint32_t N, typename Operand>
    requires(IntOperandTraits<std::remove_cvref_t<Operand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<Operand>>::IS_SIGNED)
constexpr auto Repeat(const Operand& operand) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<Operand>;
    static_assert(N > 0, "Repeat count must be greater than 0");
    static_assert(N <= ~uint32_t(0) / BIT_WIDTH, "Repeat result bit width overflows uint32_t");
    constexpr uint32_t RESULT_BIT_WIDTH = BIT_WIDTH * N;

    const Int<BIT_WIDTH> value = get_int_operand_value(operand);
    Int<RESULT_BIT_WIDTH> out(0);
    for (uint32_t i = 0; i < N; ++i) {
        out.template set_bits<BIT_WIDTH>(i * BIT_WIDTH, value);
    }
    return out;
}

template <typename... Operands>
    requires(sizeof...(Operands) > 0
             && (... && IntOperandTraits<std::remove_cvref_t<Operands>>::VALID)
             && (... && !IntOperandTraits<std::remove_cvref_t<Operands>>::IS_SIGNED))
constexpr auto Cat(const Operands&... operands) {
    constexpr uint32_t RESULT_BIT_WIDTH = (int_operand_bit_width_v<Operands> + ...);
    Int<RESULT_BIT_WIDTH> out(0);
    uint32_t lo = RESULT_BIT_WIDTH;

    auto append = [&]<typename Operand>(const Operand& operand) constexpr {
        constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<Operand>;
        lo -= BIT_WIDTH;
        out.template set_bits<BIT_WIDTH>(lo, get_int_operand_value(operand));
    };
    (append(operands), ...);
    return out;
}

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
             && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED
             && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                    == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
constexpr bool operator==(const LhsOperand& lhs_operand, const RhsOperand& rhs_operand) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<LhsOperand>;
    const Int<BIT_WIDTH> lhs = get_int_operand_value(lhs_operand);
    const Int<BIT_WIDTH> rhs = get_int_operand_value(rhs_operand);

    for (uint32_t i = 0; i < Int<BIT_WIDTH>::NUM_WORDS; ++i) {
        if (lhs.data[i] != rhs.data[i]) {
            return false;
        }
    }
    return true;
}

template <typename IntOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<IntOperand>>::IS_SIGNED
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr bool operator==(const IntOperand& lhs_operand, T rhs) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<IntOperand>;
    return lhs_operand == Int<BIT_WIDTH>(rhs);
}

template <typename T, typename IntOperand>
    requires(std::is_integral_v<std::remove_cvref_t<T>>
             && IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<IntOperand>>::IS_SIGNED)
constexpr bool operator==(T lhs, const IntOperand& rhs_operand) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<IntOperand>;
    return Int<BIT_WIDTH>(lhs) == rhs_operand;
}

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<LhsOperand>>::IS_SIGNED
             && !IntOperandTraits<std::remove_cvref_t<RhsOperand>>::IS_SIGNED
             && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                    == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
constexpr bool operator!=(const LhsOperand& lhs, const RhsOperand& rhs) {
    return !(lhs == rhs);
}

template <typename IntOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<IntOperand>>::IS_SIGNED
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr bool operator!=(const IntOperand& lhs, T rhs) {
    return !(lhs == rhs);
}

template <typename T, typename IntOperand>
    requires(std::is_integral_v<std::remove_cvref_t<T>>
             && IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && !IntOperandTraits<std::remove_cvref_t<IntOperand>>::IS_SIGNED)
constexpr bool operator!=(T lhs, const IntOperand& rhs) {
    return !(lhs == rhs);
}

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                    == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
constexpr bool operator<(const LhsOperand& lhs_operand, const RhsOperand& rhs_operand) {
    using LhsTraits = IntOperandTraits<std::remove_cvref_t<LhsOperand>>;
    using RhsTraits = IntOperandTraits<std::remove_cvref_t<RhsOperand>>;
    constexpr uint32_t BIT_WIDTH = LhsTraits::BIT_WIDTH;
    constexpr uint32_t SIGN_WORD = (BIT_WIDTH - 1) / 64;
    constexpr uint32_t SIGN_OFF = (BIT_WIDTH - 1) % 64;

    const Int<BIT_WIDTH> lhs = get_int_operand_value(lhs_operand);
    const Int<BIT_WIDTH> rhs = get_int_operand_value(rhs_operand);

    const bool lhs_negative = LhsTraits::IS_SIGNED && (((lhs.data[SIGN_WORD] >> SIGN_OFF) & uint64_t(1)) != 0);
    const bool rhs_negative = RhsTraits::IS_SIGNED && (((rhs.data[SIGN_WORD] >> SIGN_OFF) & uint64_t(1)) != 0);
    if (lhs_negative != rhs_negative) {
        return lhs_negative;
    }

    for (uint32_t i = Int<BIT_WIDTH>::NUM_WORDS; i > 0; --i) {
        const uint32_t idx = i - 1;
        if (lhs.data[idx] != rhs.data[idx]) {
            return lhs.data[idx] < rhs.data[idx];
        }
    }
    return false;
}

template <typename IntOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr bool operator<(const IntOperand& lhs_operand, T rhs) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<IntOperand>;
    if constexpr (std::is_signed_v<std::remove_cvref_t<T>>) {
        return lhs_operand < Int<BIT_WIDTH>(rhs).sint();
    } else {
        return lhs_operand < Int<BIT_WIDTH>(rhs);
    }
}

template <typename T, typename IntOperand>
    requires(std::is_integral_v<std::remove_cvref_t<T>>
             && IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID)
constexpr bool operator<(T lhs, const IntOperand& rhs_operand) {
    constexpr uint32_t BIT_WIDTH = int_operand_bit_width_v<IntOperand>;
    if constexpr (std::is_signed_v<std::remove_cvref_t<T>>) {
        return Int<BIT_WIDTH>(lhs).sint() < rhs_operand;
    } else {
        return Int<BIT_WIDTH>(lhs) < rhs_operand;
    }
}

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                    == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
constexpr bool operator<=(const LhsOperand& lhs, const RhsOperand& rhs) {
    return !(rhs < lhs);
}

template <typename IntOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr bool operator<=(const IntOperand& lhs, T rhs) {
    return !(rhs < lhs);
}

template <typename T, typename IntOperand>
    requires(std::is_integral_v<std::remove_cvref_t<T>>
             && IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID)
constexpr bool operator<=(T lhs, const IntOperand& rhs) {
    return !(rhs < lhs);
}

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                    == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
constexpr bool operator>(const LhsOperand& lhs, const RhsOperand& rhs) {
    return rhs < lhs;
}

template <typename IntOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr bool operator>(const IntOperand& lhs, T rhs) {
    return rhs < lhs;
}

template <typename T, typename IntOperand>
    requires(std::is_integral_v<std::remove_cvref_t<T>>
             && IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID)
constexpr bool operator>(T lhs, const IntOperand& rhs) {
    return rhs < lhs;
}

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<LhsOperand>>::BIT_WIDTH
                    == IntOperandTraits<std::remove_cvref_t<RhsOperand>>::BIT_WIDTH)
constexpr bool operator>=(const LhsOperand& lhs, const RhsOperand& rhs) {
    return !(lhs < rhs);
}

template <typename IntOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr bool operator>=(const IntOperand& lhs, T rhs) {
    return !(lhs < rhs);
}

template <typename T, typename IntOperand>
    requires(std::is_integral_v<std::remove_cvref_t<T>>
             && IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID)
constexpr bool operator>=(T lhs, const IntOperand& rhs) {
    return !(lhs < rhs);
}



} // namespace vulfixint

using vulfixint::Int;

using vulfixint::ReduceAnd;
using vulfixint::ReduceOr;
using vulfixint::ReduceXor;
using vulfixint::Repeat;
using vulfixint::Cat;
