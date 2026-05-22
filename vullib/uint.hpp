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
#include <bit>

using std::array;

#include <type_traits>

template <uint32_t BitWidth>
class IntRangeProxy;

template <uint32_t BitWidth>
class IntConstRangeProxy;

template <uint32_t BitWidth>
class IntBitProxy;

template <uint32_t BitWidth>
class IntConstBitProxy;

template <uint32_t BitWidth>
class IntSignedView;

template <uint32_t BitWidth>
class Int {

public:

    static_assert(BitWidth > 0, "BitWidth must be greater than 0");

    static constexpr uint32_t WORD_BITS = 64;
    static constexpr uint32_t NUM_WORDS = (BitWidth + WORD_BITS - 1) / WORD_BITS;
    static constexpr uint32_t IDX_WIDTH = log2ceil(BitWidth);

    std::array<uint64_t, NUM_WORDS> data;
    constexpr void mask_high_bits() {
        if constexpr (BitWidth % WORD_BITS != 0) {
            data.back() &= (uint64_t(1) << (BitWidth % WORD_BITS)) - 1;
        }
    }

    // constructors and assignment

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

    template <uint32_t OtherBitWidth>
    constexpr Int(const IntSignedView<OtherBitWidth>& other);
    
    template <typename T>
        requires std::is_integral_v<T>
    constexpr T to() const {
        constexpr uint32_t T_BITS = sizeof(T) * 8;
        using U = std::make_unsigned_t<T>;

        U raw = static_cast<U>(data[0]);
        if constexpr (T_BITS > 64) {
            if constexpr (NUM_WORDS > 1) {
                raw |= static_cast<U>(data[1]) << 64;
            }
        }

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

    constexpr IntSignedView<BitWidth> sint() const;

    // slice and bit access

    constexpr IntRangeProxy<BitWidth> operator()(uint32_t hi, uint32_t lo);

    constexpr IntConstRangeProxy<BitWidth> operator()(uint32_t hi, uint32_t lo) const;

    constexpr IntBitProxy<BitWidth> operator()(uint32_t bit);

    constexpr IntConstBitProxy<BitWidth> operator()(uint32_t bit) const;

    template <uint32_t DstBitWidth>
    constexpr IntRangeProxy<BitWidth> range_at(Int<IDX_WIDTH> idx);

    template <uint32_t DstBitWidth>
    constexpr IntConstRangeProxy<BitWidth> range_at(Int<IDX_WIDTH> idx) const;

    constexpr IntBitProxy<BitWidth> bit_at(Int<IDX_WIDTH> idx);

    constexpr IntConstBitProxy<BitWidth> bit_at(Int<IDX_WIDTH> idx) const;

    // other operations

    constexpr bool reduce_or() const;

    constexpr bool reduce_and() const;

    constexpr bool reduce_xor() const;

    template <uint32_t K>
    constexpr Int<BitWidth*K> repeat() const;

    template <uint32_t OtherBitWidth>
    constexpr Int<BitWidth + OtherBitWidth> cat(const Int<OtherBitWidth>& other) const;

private:
    template <uint32_t>
    friend class IntRangeProxy;
    template <uint32_t>
    friend class IntConstRangeProxy;
    template <uint32_t>
    friend class IntBitProxy;
    template <uint32_t>
    friend class IntConstBitProxy;

    template <uint32_t DstBitWidth>
    constexpr Int<DstBitWidth> get_bits(uint32_t hi, uint32_t lo) const;

    template <typename T>
        requires std::is_integral_v<T>
    constexpr T get_bits_as(uint32_t hi, uint32_t lo) const;

    template <uint32_t SrcBitWidth>
    constexpr void set_bits(uint32_t hi, uint32_t lo, const Int<SrcBitWidth>& value);

    template <uint32_t SrcBitWidth>
    constexpr void set_bits_from_range(uint32_t hi, uint32_t lo, const Int<SrcBitWidth>& value, uint32_t src_hi, uint32_t src_lo);

    template <typename T>
        requires std::is_integral_v<T>
    constexpr void set_bits_from(uint32_t hi, uint32_t lo, T value);

    constexpr bool get_bit(uint32_t bit) const;

    constexpr void set_bit(uint32_t bit, bool value);
};

template <uint32_t BitWidth>
class IntSignedView {
public:
    const Int<BitWidth>& parent;
    IntSignedView(const Int<BitWidth>& p) : parent(p) {}
};

template <uint32_t BitWidth>
template <uint32_t OtherBitWidth>
constexpr Int<BitWidth>::Int(const IntSignedView<OtherBitWidth>& other) : data{} {
    const auto& src = other.parent;

    constexpr uint32_t SRC_WORDS = Int<OtherBitWidth>::NUM_WORDS;
    constexpr uint32_t COPY_WORDS = (NUM_WORDS < SRC_WORDS) ? NUM_WORDS : SRC_WORDS;
    for (uint32_t i = 0; i < COPY_WORDS; ++i) {
        data[i] = src.data[i];
    }

    constexpr uint32_t SRC_SIGN_WORD = (OtherBitWidth - 1) / WORD_BITS;
    constexpr uint32_t SRC_SIGN_OFF = (OtherBitWidth - 1) % WORD_BITS;
    const bool sign = ((src.data[SRC_SIGN_WORD] >> SRC_SIGN_OFF) & uint64_t(1)) != 0;

    if constexpr (BitWidth > OtherBitWidth) {
        if (sign) {
            const uint32_t ext_start_word = OtherBitWidth / WORD_BITS;
            const uint32_t ext_start_off = OtherBitWidth % WORD_BITS;

            if (ext_start_off != 0) {
                data[ext_start_word] |= ~((uint64_t(1) << ext_start_off) - 1);
                for (uint32_t i = ext_start_word + 1; i < NUM_WORDS; ++i) {
                    data[i] = ~uint64_t(0);
                }
            } else {
                for (uint32_t i = ext_start_word; i < NUM_WORDS; ++i) {
                    data[i] = ~uint64_t(0);
                }
            }
        }
    } else if constexpr (BitWidth < OtherBitWidth) {
        constexpr uint32_t DST_SIGN_WORD = (BitWidth - 1) / WORD_BITS;
        constexpr uint32_t DST_SIGN_OFF = (BitWidth - 1) % WORD_BITS;
        const uint64_t sign_mask = uint64_t(1) << DST_SIGN_OFF;
        if (sign) {
            data[DST_SIGN_WORD] |= sign_mask;
        } else {
            data[DST_SIGN_WORD] &= ~sign_mask;
        }
    }

    mask_high_bits();
}

template <uint32_t BitWidth>
template <uint32_t DstBitWidth>
constexpr Int<DstBitWidth> Int<BitWidth>::get_bits(uint32_t hi, uint32_t lo) const {
    assert(hi >= lo);
    assert(hi < BitWidth);

    Int<DstBitWidth> out(0);

    const uint32_t range_bits = hi - lo + 1;
    const uint32_t copy_bits = (range_bits < DstBitWidth) ? range_bits : DstBitWidth;
    if (copy_bits == 0) {
        return out;
    }

    auto low_mask = [](uint32_t bits) constexpr -> uint64_t {
        if (bits == 0) {
            return 0;
        }
        if (bits >= 64) {
            return ~uint64_t(0);
        }
        return (uint64_t(1) << bits) - 1;
    };

    if constexpr (BitWidth <= WORD_BITS && DstBitWidth <= WORD_BITS) {
        out.data[0] = (data[0] >> lo) & low_mask(copy_bits);
        return out;
    }

    const uint32_t out_words = (copy_bits + WORD_BITS - 1) / WORD_BITS;
    for (uint32_t i = 0; i < out_words; ++i) {
        const uint32_t src_bit = lo + i * WORD_BITS;
        const uint32_t src_word = src_bit / WORD_BITS;
        const uint32_t src_shift = src_bit % WORD_BITS;

        uint64_t chunk = data[src_word] >> src_shift;
        if (src_shift != 0 && src_word + 1 < NUM_WORDS) {
            chunk |= data[src_word + 1] << (WORD_BITS - src_shift);
        }
        out.data[i] = chunk;
    }

    const uint32_t tail_bits = copy_bits % WORD_BITS;
    if (tail_bits != 0) {
        out.data[out_words - 1] &= low_mask(tail_bits);
    }

    return out;
}

template <uint32_t BitWidth>
template <typename T>
    requires std::is_integral_v<T>
constexpr T Int<BitWidth>::get_bits_as(uint32_t hi, uint32_t lo) const {
    constexpr uint32_t T_BITS = sizeof(T) * 8;
    const auto out = get_bits<T_BITS>(hi, lo);

    if constexpr (T_BITS <= 64) {
        return static_cast<T>(out.data[0]);
    } else {
        using U = std::make_unsigned_t<T>;
        U v = static_cast<U>(out.data[0]);
        v |= static_cast<U>(out.data[1]) << 64;
        return static_cast<T>(v);
    }
}

template <uint32_t BitWidth>
template <uint32_t SrcBitWidth>
constexpr void Int<BitWidth>::set_bits(uint32_t hi, uint32_t lo, const Int<SrcBitWidth>& value) {
    assert(hi >= lo);
    assert(hi < BitWidth);

    const uint32_t dst_bits = hi - lo + 1;
    const uint32_t copy_bits = (dst_bits < SrcBitWidth) ? dst_bits : SrcBitWidth;

    auto low_mask = [](uint32_t bits) constexpr -> uint64_t {
        if (bits == 0) {
            return 0;
        }
        if (bits >= 64) {
            return ~uint64_t(0);
        }
        return (uint64_t(1) << bits) - 1;
    };

    auto word_mask = [&](uint32_t bit_in_word, uint32_t bits) constexpr -> uint64_t {
        return low_mask(bits) << bit_in_word;
    };

    const uint32_t first_word = lo / WORD_BITS;
    const uint32_t last_word = hi / WORD_BITS;

    if (first_word == last_word) {
        data[first_word] &= ~word_mask(lo % WORD_BITS, dst_bits);
    } else {
        data[first_word] &= ~word_mask(lo % WORD_BITS, WORD_BITS - (lo % WORD_BITS));
        for (uint32_t w = first_word + 1; w < last_word; ++w) {
            data[w] = 0;
        }
        data[last_word] &= ~word_mask(0, (hi % WORD_BITS) + 1);
    }

    if (copy_bits == 0) {
        return;
    }

    if constexpr (BitWidth <= WORD_BITS && SrcBitWidth <= WORD_BITS) {
        const uint64_t src_chunk = value.data[0] & low_mask(copy_bits);
        data[0] |= src_chunk << lo;
        return;
    }

    const uint32_t write_hi = lo + copy_bits - 1;
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
            chunk &= low_mask(seg_bits);
        }

        data[w] |= (chunk << dst_shift);
    }
}

template <uint32_t BitWidth>
template <uint32_t SrcBitWidth>
constexpr void Int<BitWidth>::set_bits_from_range(uint32_t hi, uint32_t lo, const Int<SrcBitWidth>& value, uint32_t src_hi, uint32_t src_lo) {
    assert(hi >= lo);
    assert(hi < BitWidth);
    assert(src_hi >= src_lo);
    assert(src_hi < SrcBitWidth);

    const uint32_t src_range_bits = src_hi - src_lo + 1;
    const uint32_t dst_bits = hi - lo + 1;
    const uint32_t copy_bits = (dst_bits < src_range_bits) ? dst_bits : src_range_bits;

    auto low_mask = [](uint32_t bits) constexpr -> uint64_t {
        if (bits == 0) {
            return 0;
        }
        if (bits >= 64) {
            return ~uint64_t(0);
        }
        return (uint64_t(1) << bits) - 1;
    };

    auto word_mask = [&](uint32_t bit_in_word, uint32_t bits) constexpr -> uint64_t {
        return low_mask(bits) << bit_in_word;
    };

    const uint32_t first_word = lo / WORD_BITS;
    const uint32_t last_word = hi / WORD_BITS;

    if (first_word == last_word) {
        data[first_word] &= ~word_mask(lo % WORD_BITS, dst_bits);
    } else {
        data[first_word] &= ~word_mask(lo % WORD_BITS, WORD_BITS - (lo % WORD_BITS));
        for (uint32_t w = first_word + 1; w < last_word; ++w) {
            data[w] = 0;
        }
        data[last_word] &= ~word_mask(0, (hi % WORD_BITS) + 1);
    }

    if (copy_bits == 0) {
        return;
    }

    const uint32_t write_hi = lo + copy_bits - 1;
    const uint32_t write_first = lo / WORD_BITS;
    const uint32_t write_last = write_hi / WORD_BITS;

    for (uint32_t w = write_first; w <= write_last; ++w) {
        const uint32_t word_lo = w * WORD_BITS;
        const uint32_t seg_lo = (lo > word_lo) ? lo : word_lo;
        const uint32_t seg_hi = (write_hi < (word_lo + WORD_BITS - 1)) ? write_hi : (word_lo + WORD_BITS - 1);
        const uint32_t seg_bits = seg_hi - seg_lo + 1;

        const uint32_t dst_shift = seg_lo - word_lo;
        const uint32_t src_off = src_lo + (seg_lo - lo);
        const uint32_t src_word = src_off / WORD_BITS;
        const uint32_t src_shift = src_off % WORD_BITS;

        uint64_t chunk = value.data[src_word] >> src_shift;
        if (src_shift != 0 && src_word + 1 < Int<SrcBitWidth>::NUM_WORDS) {
            chunk |= value.data[src_word + 1] << (WORD_BITS - src_shift);
        }
        if (seg_bits < WORD_BITS) {
            chunk &= low_mask(seg_bits);
        }

        data[w] |= (chunk << dst_shift);
    }
}

template <uint32_t BitWidth>
template <typename T>
    requires std::is_integral_v<T>
constexpr void Int<BitWidth>::set_bits_from(uint32_t hi, uint32_t lo, T value) {
    Int<sizeof(T) * 8> src(value);
    set_bits(hi, lo, src);
}

template <uint32_t BitWidth>
constexpr bool Int<BitWidth>::get_bit(uint32_t bit) const {
    assert(bit < BitWidth);
    return (data[bit / WORD_BITS] >> (bit % WORD_BITS)) & 1;
}

template <uint32_t BitWidth>
constexpr void Int<BitWidth>::set_bit(uint32_t bit, bool value) {
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


template <uint32_t BitWidth>
class IntRangeProxy {
private:
    Int<BitWidth>& parent;
    uint32_t hi;
    uint32_t lo;
    template <uint32_t>
    friend class IntRangeProxy;
    template <uint32_t>
    friend class IntConstRangeProxy;

public:
    IntRangeProxy(Int<BitWidth>& p, uint32_t h, uint32_t l)
        : parent(p), hi(h), lo(l) {
        assert(h >= l);
        assert(h < BitWidth);
    }

    IntRangeProxy& operator=(const IntRangeProxy& value) {
        if (&parent == &value.parent) {
            const Int<BitWidth> snapshot(value.parent);
            parent.template set_bits_from_range<BitWidth>(hi, lo, snapshot, value.hi, value.lo);
        } else {
            parent.template set_bits_from_range<BitWidth>(hi, lo, value.parent, value.hi, value.lo);
        }
        return *this;
    }

    IntRangeProxy& operator=(IntRangeProxy&& value) {
        return (*this = static_cast<const IntRangeProxy&>(value));
    }

    template <uint32_t ResultBitWidth>
    operator Int<ResultBitWidth>() const {
        return parent.template get_bits<ResultBitWidth>(hi, lo);
    }

    template <typename T>
        requires std::is_integral_v<T>
    operator T() const {
        return parent.template get_bits_as<T>(hi, lo);
    }

    template <uint32_t SrcBitWidth>
    IntRangeProxy& operator=(const Int<SrcBitWidth>& value) {
        parent.set_bits(hi, lo, value);
        return *this;
    }

    template <typename T>
        requires std::is_integral_v<T>
    IntRangeProxy& operator=(T value) {
        parent.template set_bits_from<T>(hi, lo, value);
        return *this;
    }

    template <uint32_t SrcBitWidth>
    IntRangeProxy& operator=(const IntRangeProxy<SrcBitWidth>& value) {
        if constexpr (SrcBitWidth == BitWidth) {
            if (&parent == &value.parent) {
                const Int<SrcBitWidth> snapshot(value.parent);
                parent.template set_bits_from_range<SrcBitWidth>(hi, lo, snapshot, value.hi, value.lo);
                return *this;
            }
        }
        parent.template set_bits_from_range<SrcBitWidth>(hi, lo, value.parent, value.hi, value.lo);
        return *this;
    }

    template <uint32_t SrcBitWidth>
    IntRangeProxy& operator=(const IntConstRangeProxy<SrcBitWidth>& value) {
        if constexpr (SrcBitWidth == BitWidth) {
            if (&parent == &value.parent) {
                const Int<SrcBitWidth> snapshot(value.parent);
                parent.template set_bits_from_range<SrcBitWidth>(hi, lo, snapshot, value.hi, value.lo);
                return *this;
            }
        }
        parent.template set_bits_from_range<SrcBitWidth>(hi, lo, value.parent, value.hi, value.lo);
        return *this;
    }
};

template <uint32_t BitWidth>
class IntConstRangeProxy {
private:
    const Int<BitWidth>& parent;
    uint32_t hi;
    uint32_t lo;
    template <uint32_t>
    friend class IntRangeProxy;
    template <uint32_t>
    friend class IntConstRangeProxy;

public:
    IntConstRangeProxy(const Int<BitWidth>& p, uint32_t h, uint32_t l)
        : parent(p), hi(h), lo(l) {
        assert(h >= l);
        assert(h < BitWidth);
    }

    template <uint32_t ResultBitWidth>
    operator Int<ResultBitWidth>() const {
        return parent.template get_bits<ResultBitWidth>(hi, lo);
    }

    template <typename T>
        requires std::is_integral_v<T>
    operator T() const {
        return parent.template get_bits_as<T>(hi, lo);
    }
};

template <uint32_t BitWidth>
class IntBitProxy {
private:
    Int<BitWidth>& parent;
    uint32_t bit;

public:
    IntBitProxy(Int<BitWidth>& p, uint32_t b)
        : parent(p), bit(b) {
        assert(bit < BitWidth);
    }

    operator bool() const {
        return parent.get_bit(bit);
    }

    IntBitProxy& operator=(bool value) {
        parent.set_bit(bit, value);
        return *this;
    }
};

template <uint32_t BitWidth>
class IntConstBitProxy {
private:
    const Int<BitWidth>& parent;
    uint32_t bit;

public:
    IntConstBitProxy(const Int<BitWidth>& p, uint32_t b)
        : parent(p), bit(b) {
        assert(bit < BitWidth);
    }

    operator bool() const {
        return parent.get_bit(bit);
    }
};

template <uint32_t BitWidth>
constexpr IntRangeProxy<BitWidth> Int<BitWidth>::operator()(uint32_t hi, uint32_t lo) {
    return IntRangeProxy<BitWidth>(*this, hi, lo);
}

template <uint32_t BitWidth>
constexpr IntConstRangeProxy<BitWidth> Int<BitWidth>::operator()(uint32_t hi, uint32_t lo) const {
    return IntConstRangeProxy<BitWidth>(*this, hi, lo);
}

template <uint32_t BitWidth>
constexpr IntBitProxy<BitWidth> Int<BitWidth>::operator()(uint32_t bit) {
    return IntBitProxy<BitWidth>(*this, bit);
}

template <uint32_t BitWidth>
constexpr IntConstBitProxy<BitWidth> Int<BitWidth>::operator()(uint32_t bit) const {
    return IntConstBitProxy<BitWidth>(*this, bit);
}

template <uint32_t BitWidth>
template <uint32_t DstBitWidth>
constexpr IntRangeProxy<BitWidth> Int<BitWidth>::range_at(Int<IDX_WIDTH> idx) {
    static_assert(DstBitWidth > 0, "DstBitWidth must be greater than 0");
    const uint32_t lo = static_cast<uint32_t>(idx.data[0]);
    const uint32_t hi = lo + DstBitWidth - 1;
    return IntRangeProxy<BitWidth>(*this, hi, lo);
}

template <uint32_t BitWidth>
template <uint32_t DstBitWidth>
constexpr IntConstRangeProxy<BitWidth> Int<BitWidth>::range_at(Int<IDX_WIDTH> idx) const {
    static_assert(DstBitWidth > 0, "DstBitWidth must be greater than 0");
    const uint32_t lo = static_cast<uint32_t>(idx.data[0]);
    const uint32_t hi = lo + DstBitWidth - 1;
    return IntConstRangeProxy<BitWidth>(*this, hi, lo);
}

template <uint32_t BitWidth>
constexpr IntBitProxy<BitWidth> Int<BitWidth>::bit_at(Int<IDX_WIDTH> idx) {
    const uint32_t bit = static_cast<uint32_t>(idx.data[0]);
    return IntBitProxy<BitWidth>(*this, bit);
}

template <uint32_t BitWidth>
constexpr IntConstBitProxy<BitWidth> Int<BitWidth>::bit_at(Int<IDX_WIDTH> idx) const {
    const uint32_t bit = static_cast<uint32_t>(idx.data[0]);
    return IntConstBitProxy<BitWidth>(*this, bit);
}

template <uint32_t BitWidth>
constexpr IntSignedView<BitWidth> Int<BitWidth>::sint() const {
    return IntSignedView<BitWidth>(*this);
}

template <uint32_t BitWidth>
constexpr bool Int<BitWidth>::reduce_or() const {
    if constexpr (NUM_WORDS == 1) {
        return data[0] != 0;
    } else {
        for (uint32_t i = 0; i < NUM_WORDS; ++i) {
            if (data[i] != 0) {
                return true;
            }
        }
        return false;
    }
}

template <uint32_t BitWidth>
constexpr bool Int<BitWidth>::reduce_and() const {
    constexpr uint32_t LAST_BITS = BitWidth % WORD_BITS;
    if constexpr (NUM_WORDS == 1) {
        if constexpr (LAST_BITS == 0) {
            return data[0] == ~uint64_t(0);
        } else {
            return data[0] == ((uint64_t(1) << LAST_BITS) - 1);
        }
    } else {
        for (uint32_t i = 0; i + 1 < NUM_WORDS; ++i) {
            if (data[i] != ~uint64_t(0)) {
                return false;
            }
        }
        if constexpr (LAST_BITS == 0) {
            return data[NUM_WORDS - 1] == ~uint64_t(0);
        } else {
            return data[NUM_WORDS - 1] == ((uint64_t(1) << LAST_BITS) - 1);
        }
    }
}

template <uint32_t BitWidth>
constexpr bool Int<BitWidth>::reduce_xor() const {
    uint32_t parity = 0;
    if constexpr (NUM_WORDS == 1) {
        constexpr uint32_t LAST_BITS = BitWidth % WORD_BITS;
        if constexpr (LAST_BITS == 0) {
            parity = std::popcount(data[0]) & 1U;
        } else {
            const uint64_t mask = (uint64_t(1) << LAST_BITS) - 1;
            parity = std::popcount(data[0] & mask) & 1U;
        }
    } else {
        for (uint32_t i = 0; i + 1 < NUM_WORDS; ++i) {
            parity ^= (std::popcount(data[i]) & 1U);
        }
        constexpr uint32_t LAST_BITS = BitWidth % WORD_BITS;
        if constexpr (LAST_BITS == 0) {
            parity ^= (std::popcount(data[NUM_WORDS - 1]) & 1U);
        } else {
            const uint64_t mask = (uint64_t(1) << LAST_BITS) - 1;
            parity ^= (std::popcount(data[NUM_WORDS - 1] & mask) & 1U);
        }
    }
    return parity != 0;
}

template <uint32_t BitWidth>
template <uint32_t K>
constexpr Int<BitWidth * K> Int<BitWidth>::repeat() const {
    static_assert(K > 0, "K must be greater than 0");

    Int<BitWidth * K> out(0);
    constexpr uint32_t OUT_WORDS = Int<BitWidth * K>::NUM_WORDS;

    if constexpr (BitWidth % WORD_BITS == 0) {
        constexpr uint32_t STEP_WORDS = BitWidth / WORD_BITS;
        for (uint32_t r = 0; r < K; ++r) {
            const uint32_t base = r * STEP_WORDS;
            for (uint32_t i = 0; i < NUM_WORDS; ++i) {
                out.data[base + i] = data[i];
            }
        }
    } else {
        constexpr uint32_t STEP_WORDS = BitWidth / WORD_BITS;
        constexpr uint32_t STEP_SHIFT = BitWidth % WORD_BITS;

        uint32_t dst_word = 0;
        uint32_t dst_shift = 0;
        for (uint32_t r = 0; r < K; ++r) {
            if (dst_shift == 0) {
                for (uint32_t i = 0; i < NUM_WORDS; ++i) {
                    out.data[dst_word + i] |= data[i];
                }
            } else {
                const uint32_t inv_shift = WORD_BITS - dst_shift;
                for (uint32_t i = 0; i < NUM_WORDS; ++i) {
                    const uint32_t w = dst_word + i;
                    out.data[w] |= (data[i] << dst_shift);
                    if (w + 1 < OUT_WORDS) {
                        out.data[w + 1] |= (data[i] >> inv_shift);
                    }
                }
            }

            dst_word += STEP_WORDS;
            dst_shift += STEP_SHIFT;
            if (dst_shift >= WORD_BITS) {
                dst_shift -= WORD_BITS;
                ++dst_word;
            }
        }
    }

    out.mask_high_bits();
    return out;
}

template <uint32_t BitWidth>
template <uint32_t OtherBitWidth>
constexpr Int<BitWidth + OtherBitWidth> Int<BitWidth>::cat(const Int<OtherBitWidth>& other) const {
    Int<BitWidth + OtherBitWidth> out(0);
    constexpr uint32_t OUT_WORDS = Int<BitWidth + OtherBitWidth>::NUM_WORDS;
    constexpr uint32_t OTHER_WORDS = Int<OtherBitWidth>::NUM_WORDS;

    for (uint32_t i = 0; i < OTHER_WORDS; ++i) {
        out.data[i] = other.data[i];
    }

    constexpr uint32_t DST_WORD = OtherBitWidth / WORD_BITS;
    constexpr uint32_t DST_SHIFT = OtherBitWidth % WORD_BITS;

    if constexpr (DST_SHIFT == 0) {
        for (uint32_t i = 0; i < NUM_WORDS; ++i) {
            out.data[DST_WORD + i] |= data[i];
        }
    } else {
        constexpr uint32_t INV_SHIFT = WORD_BITS - DST_SHIFT;
        for (uint32_t i = 0; i < NUM_WORDS; ++i) {
            const uint32_t w = DST_WORD + i;
            out.data[w] |= (data[i] << DST_SHIFT);
            if (w + 1 < OUT_WORDS) {
                out.data[w + 1] |= (data[i] >> INV_SHIFT);
            }
        }
    }

    out.mask_high_bits();
    return out;
}

namespace uint_impl_detail {
template <uint32_t OutBitWidth, uint32_t BitOffset, uint32_t SrcBitWidth>
constexpr void cat_into(Int<OutBitWidth>& out, const Int<SrcBitWidth>& src) {
    constexpr uint32_t WORD_BITS = Int<OutBitWidth>::WORD_BITS;
    constexpr uint32_t OUT_WORDS = Int<OutBitWidth>::NUM_WORDS;
    constexpr uint32_t SRC_WORDS = Int<SrcBitWidth>::NUM_WORDS;
    constexpr uint32_t DST_WORD = BitOffset / WORD_BITS;
    constexpr uint32_t DST_SHIFT = BitOffset % WORD_BITS;

    if constexpr (DST_SHIFT == 0) {
        for (uint32_t i = 0; i < SRC_WORDS; ++i) {
            out.data[DST_WORD + i] |= src.data[i];
        }
    } else {
        constexpr uint32_t INV_SHIFT = WORD_BITS - DST_SHIFT;
        for (uint32_t i = 0; i < SRC_WORDS; ++i) {
            const uint32_t w = DST_WORD + i;
            out.data[w] |= (src.data[i] << DST_SHIFT);
            if (w + 1 < OUT_WORDS) {
                out.data[w + 1] |= (src.data[i] >> INV_SHIFT);
            }
        }
    }
}

template <uint32_t OutBitWidth, uint32_t NextBitOffset, uint32_t CurBitWidth>
constexpr void cat_build(Int<OutBitWidth>& out, const Int<CurBitWidth>& cur) {
    constexpr uint32_t CUR_OFFSET = NextBitOffset - CurBitWidth;
    cat_into<OutBitWidth, CUR_OFFSET>(out, cur);
}

template <uint32_t OutBitWidth, uint32_t NextBitOffset, uint32_t CurBitWidth, uint32_t NextBitWidth, uint32_t... RestBitWidths>
constexpr void cat_build(Int<OutBitWidth>& out,
                         const Int<CurBitWidth>& cur,
                         const Int<NextBitWidth>& next,
                         const Int<RestBitWidths>&... rest) {
    constexpr uint32_t CUR_OFFSET = NextBitOffset - CurBitWidth;
    cat_into<OutBitWidth, CUR_OFFSET>(out, cur);
    cat_build<OutBitWidth, CUR_OFFSET>(out, next, rest...);
}
} // namespace uint_impl_detail

template <uint32_t FirstBitWidth, uint32_t... RestBitWidths>
constexpr auto Cat(const Int<FirstBitWidth>& first, const Int<RestBitWidths>&... rest) {
    constexpr uint32_t TOTAL_BIT_WIDTH = FirstBitWidth + (RestBitWidths + ... + 0u);
    Int<TOTAL_BIT_WIDTH> out(0);
    uint_impl_detail::cat_build<TOTAL_BIT_WIDTH, TOTAL_BIT_WIDTH>(out, first, rest...);
    out.mask_high_bits();
    return out;
}


// Int operations

template <typename T>
struct IntOperandTraits {
    static constexpr bool VALID = false;
};

template <uint32_t BitWidth>
struct IntOperandTraits<Int<BitWidth>> {
    static constexpr bool VALID = true;
    static constexpr bool IS_SIGNED = false;
    static constexpr uint32_t BIT_WIDTH = BitWidth;
    static constexpr const Int<BitWidth>& get(const Int<BitWidth>& v) {
        return v;
    }
};

template <uint32_t BitWidth>
struct IntOperandTraits<IntSignedView<BitWidth>> {
    static constexpr bool VALID = true;
    static constexpr bool IS_SIGNED = true;
    static constexpr uint32_t BIT_WIDTH = BitWidth;
    static constexpr const Int<BitWidth>& get(const IntSignedView<BitWidth>& v) {
        return v.parent;
    }
};

template <uint32_t LhsBitWidth, uint32_t RhsBitWidth>
constexpr auto operator+(const Int<LhsBitWidth>& lhs, const Int<RhsBitWidth>& rhs) {
    constexpr uint32_t MAX_BIT_WIDTH = (LhsBitWidth > RhsBitWidth) ? LhsBitWidth : RhsBitWidth;
    constexpr uint32_t RESULT_BIT_WIDTH = MAX_BIT_WIDTH + 1;
    constexpr uint32_t LHS_WORDS = Int<LhsBitWidth>::NUM_WORDS;
    constexpr uint32_t RHS_WORDS = Int<RhsBitWidth>::NUM_WORDS;
    constexpr uint32_t MAX_WORDS = (LHS_WORDS > RHS_WORDS) ? LHS_WORDS : RHS_WORDS;

    Int<RESULT_BIT_WIDTH> out(0);

    if constexpr (LhsBitWidth <= 64 && RhsBitWidth <= 64) {
        const uint64_t l = lhs.data[0];
        const uint64_t r = rhs.data[0];
        const uint64_t sum = l + r;
        out.data[0] = sum;
        if constexpr (RESULT_BIT_WIDTH > 64) {
            out.data[1] = (sum < l) ? 1 : 0;
        }
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

        return out;
    }
}

template <uint32_t BitWidth, typename T>
    requires std::is_integral_v<std::remove_cvref_t<T>>
constexpr auto operator+(const Int<BitWidth>& lhs, T rhs) {
    using ValueT = std::remove_cvref_t<T>;
    constexpr uint32_t RESULT_BIT_WIDTH = BitWidth + 1;

    if constexpr (BitWidth < 64) {
        constexpr uint64_t MASK = (uint64_t(1) << BitWidth) - 1;
        const uint64_t l = lhs.data[0] & MASK;
        uint64_t r = 0;
        if constexpr (std::is_signed_v<ValueT>) {
            r = static_cast<uint64_t>(static_cast<int64_t>(rhs)) & MASK;
        } else {
            r = static_cast<uint64_t>(rhs) & MASK;
        }

        Int<RESULT_BIT_WIDTH> out(0);
        out.data[0] = l + r;
        return out;
    } else {
        const Int<BitWidth> rhs_u(rhs);
        return lhs + rhs_u;
    }
}

template <typename T, uint32_t BitWidth>
    requires std::is_integral_v<std::remove_cvref_t<T>>
constexpr auto operator+(T lhs, const Int<BitWidth>& rhs) {
    return rhs + lhs;
}


template <uint32_t LhsBitWidth, uint32_t RhsBitWidth>
constexpr auto operator-(const Int<LhsBitWidth>& lhs, const Int<RhsBitWidth>& rhs) {
    constexpr uint32_t RESULT_BIT_WIDTH = (LhsBitWidth > RhsBitWidth) ? LhsBitWidth : RhsBitWidth;
    constexpr uint32_t LHS_WORDS = Int<LhsBitWidth>::NUM_WORDS;
    constexpr uint32_t RHS_WORDS = Int<RhsBitWidth>::NUM_WORDS;

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

template <uint32_t BitWidth, typename T>
    requires std::is_integral_v<std::remove_cvref_t<T>>
constexpr auto operator-(const Int<BitWidth>& lhs, T rhs) {
    using ValueT = std::remove_cvref_t<T>;
    constexpr uint32_t RESULT_BIT_WIDTH = BitWidth;

    if constexpr (RESULT_BIT_WIDTH <= 64) {
        constexpr uint64_t MASK = (RESULT_BIT_WIDTH == 64) ? ~uint64_t(0)
                                                            : ((uint64_t(1) << RESULT_BIT_WIDTH) - 1);
        const uint64_t l = lhs.data[0] & MASK;
        uint64_t r = 0;
        if constexpr (std::is_signed_v<ValueT>) {
            r = static_cast<uint64_t>(static_cast<int64_t>(rhs)) & MASK;
        } else {
            r = static_cast<uint64_t>(rhs) & MASK;
        }

        Int<RESULT_BIT_WIDTH> out(0);
        out.data[0] = l - r;
        out.mask_high_bits();
        return out;
    } else {
        const Int<BitWidth> rhs_u(rhs);
        return lhs - rhs_u;
    }
}

template <typename T, uint32_t BitWidth>
    requires std::is_integral_v<std::remove_cvref_t<T>>
constexpr auto operator-(T lhs, const Int<BitWidth>& rhs) {
    using ValueT = std::remove_cvref_t<T>;
    constexpr uint32_t RESULT_BIT_WIDTH = BitWidth;

    if constexpr (RESULT_BIT_WIDTH <= 64) {
        constexpr uint64_t MASK = (RESULT_BIT_WIDTH == 64) ? ~uint64_t(0)
                                                            : ((uint64_t(1) << RESULT_BIT_WIDTH) - 1);
        uint64_t l = 0;
        if constexpr (std::is_signed_v<ValueT>) {
            l = static_cast<uint64_t>(static_cast<int64_t>(lhs)) & MASK;
        } else {
            l = static_cast<uint64_t>(lhs) & MASK;
        }
        const uint64_t r = rhs.data[0] & MASK;

        Int<RESULT_BIT_WIDTH> out(0);
        out.data[0] = l - r;
        out.mask_high_bits();
        return out;
    } else {
        const Int<BitWidth> lhs_u(lhs);
        return lhs_u - rhs;
    }
}

template <uint32_t BitWidth>
constexpr Int<BitWidth> operator-(const Int<BitWidth>& value) {
    Int<BitWidth> out(0);
    if constexpr (BitWidth <= 64) {
        out.data[0] = ~value.data[0] + 1;
    } else {
        uint64_t carry = 1;
        for (uint32_t i = 0; i < Int<BitWidth>::NUM_WORDS; ++i) {
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

    const auto& lhs = LhsTraits::get(lhs_operand);
    const auto& rhs = RhsTraits::get(rhs_operand);

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
            const uint64_t mask = (uint64_t(1) << bits) - 1;
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
            const uint128_t prod =
                static_cast<uint128_t>(lhs.data[0]) * static_cast<uint128_t>(rhs.data[0]);
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
                                const uint64_t low_mask = (uint64_t(1) << REM) - 1;
                                w |= ~low_mask;
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
                                const uint64_t low_mask = (uint64_t(1) << REM) - 1;
                                w |= ~low_mask;
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
                const uint128_t acc = static_cast<uint128_t>(lw) * rhs_ext_word(j)
                                              + static_cast<uint128_t>(out.data[k]) + carry;
                out.data[k] = static_cast<uint64_t>(acc);
                carry = acc >> 64;
            }
        }

        out.mask_high_bits();
        return out;
    }
}

template <uint32_t BitWidth>
constexpr Int<BitWidth> operator&(const Int<BitWidth>& lhs, const Int<BitWidth>& rhs) {
    Int<BitWidth> out(0);
    if constexpr (BitWidth <= 64) {
        out.data[0] = lhs.data[0] & rhs.data[0];
    } else {
        for (uint32_t i = 0; i < Int<BitWidth>::NUM_WORDS; ++i) {
            out.data[i] = lhs.data[i] & rhs.data[i];
        }
    }
    return out;
}

template <uint32_t BitWidth, uint32_t SrcBitWidth>
constexpr Int<BitWidth> operator&(const Int<BitWidth>& lhs, const IntRangeProxy<SrcBitWidth>& rhs) {
    return lhs & static_cast<Int<BitWidth>>(rhs);
}

template <uint32_t BitWidth, uint32_t SrcBitWidth>
constexpr Int<BitWidth> operator&(const Int<BitWidth>& lhs, const IntConstRangeProxy<SrcBitWidth>& rhs) {
    return lhs & static_cast<Int<BitWidth>>(rhs);
}

template <uint32_t SrcBitWidth, uint32_t BitWidth>
constexpr Int<BitWidth> operator&(const IntRangeProxy<SrcBitWidth>& lhs, const Int<BitWidth>& rhs) {
    return static_cast<Int<BitWidth>>(lhs) & rhs;
}

template <uint32_t SrcBitWidth, uint32_t BitWidth>
constexpr Int<BitWidth> operator&(const IntConstRangeProxy<SrcBitWidth>& lhs, const Int<BitWidth>& rhs) {
    return static_cast<Int<BitWidth>>(lhs) & rhs;
}

template <uint32_t BitWidth>
constexpr Int<BitWidth> operator|(const Int<BitWidth>& lhs, const Int<BitWidth>& rhs) {
    Int<BitWidth> out(0);
    if constexpr (BitWidth <= 64) {
        out.data[0] = lhs.data[0] | rhs.data[0];
    } else {
        for (uint32_t i = 0; i < Int<BitWidth>::NUM_WORDS; ++i) {
            out.data[i] = lhs.data[i] | rhs.data[i];
        }
    }
    return out;
}

template <uint32_t BitWidth, uint32_t SrcBitWidth>
constexpr Int<BitWidth> operator|(const Int<BitWidth>& lhs, const IntRangeProxy<SrcBitWidth>& rhs) {
    return lhs | static_cast<Int<BitWidth>>(rhs);
}

template <uint32_t BitWidth, uint32_t SrcBitWidth>
constexpr Int<BitWidth> operator|(const Int<BitWidth>& lhs, const IntConstRangeProxy<SrcBitWidth>& rhs) {
    return lhs | static_cast<Int<BitWidth>>(rhs);
}

template <uint32_t SrcBitWidth, uint32_t BitWidth>
constexpr Int<BitWidth> operator|(const IntRangeProxy<SrcBitWidth>& lhs, const Int<BitWidth>& rhs) {
    return static_cast<Int<BitWidth>>(lhs) | rhs;
}

template <uint32_t SrcBitWidth, uint32_t BitWidth>
constexpr Int<BitWidth> operator|(const IntConstRangeProxy<SrcBitWidth>& lhs, const Int<BitWidth>& rhs) {
    return static_cast<Int<BitWidth>>(lhs) | rhs;
}

template <uint32_t BitWidth>
constexpr Int<BitWidth> operator^(const Int<BitWidth>& lhs, const Int<BitWidth>& rhs) {
    Int<BitWidth> out(0);
    if constexpr (BitWidth <= 64) {
        out.data[0] = lhs.data[0] ^ rhs.data[0];
    } else {
        for (uint32_t i = 0; i < Int<BitWidth>::NUM_WORDS; ++i) {
            out.data[i] = lhs.data[i] ^ rhs.data[i];
        }
    }
    return out;
}

template <uint32_t BitWidth, uint32_t SrcBitWidth>
constexpr Int<BitWidth> operator^(const Int<BitWidth>& lhs, const IntRangeProxy<SrcBitWidth>& rhs) {
    return lhs ^ static_cast<Int<BitWidth>>(rhs);
}

template <uint32_t BitWidth, uint32_t SrcBitWidth>
constexpr Int<BitWidth> operator^(const Int<BitWidth>& lhs, const IntConstRangeProxy<SrcBitWidth>& rhs) {
    return lhs ^ static_cast<Int<BitWidth>>(rhs);
}

template <uint32_t SrcBitWidth, uint32_t BitWidth>
constexpr Int<BitWidth> operator^(const IntRangeProxy<SrcBitWidth>& lhs, const Int<BitWidth>& rhs) {
    return static_cast<Int<BitWidth>>(lhs) ^ rhs;
}

template <uint32_t SrcBitWidth, uint32_t BitWidth>
constexpr Int<BitWidth> operator^(const IntConstRangeProxy<SrcBitWidth>& lhs, const Int<BitWidth>& rhs) {
    return static_cast<Int<BitWidth>>(lhs) ^ rhs;
}

template <uint32_t BitWidth>
constexpr Int<BitWidth> operator~(const Int<BitWidth>& value) {
    Int<BitWidth> out(0);
    if constexpr (BitWidth <= 64) {
        out.data[0] = ~value.data[0];
    } else {
        for (uint32_t i = 0; i < Int<BitWidth>::NUM_WORDS; ++i) {
            out.data[i] = ~value.data[i];
        }
    }
    out.mask_high_bits();
    return out;
}

namespace uint_impl_detail {
constexpr uint64_t low_mask64(uint32_t bits) {
    if (bits == 0) {
        return 0;
    }
    if (bits >= 64) {
        return ~uint64_t(0);
    }
    return (uint64_t(1) << bits) - 1;
}

template <bool IsSigned, uint32_t BitWidth>
constexpr bool operand_negative(const Int<BitWidth>& v) {
    if constexpr (!IsSigned) {
        return false;
    } else {
        constexpr uint32_t SIGN_WORD = (BitWidth - 1) / 64;
        constexpr uint32_t SIGN_OFF = (BitWidth - 1) % 64;
        return ((v.data[SIGN_WORD] >> SIGN_OFF) & 1U) != 0;
    }
}

template <bool IsSigned, uint32_t BitWidth>
constexpr uint64_t extend_to_64(const Int<BitWidth>& v, uint32_t target_bits) {
    uint64_t out = v.data[0] & low_mask64(BitWidth);
    if constexpr (IsSigned) {
        if constexpr (BitWidth < 64) {
            const bool neg = ((out >> (BitWidth - 1)) & 1U) != 0;
            if (neg && target_bits > BitWidth) {
                out |= ~low_mask64(BitWidth);
            }
        }
    }
    return out & low_mask64(target_bits);
}

template <bool IsSigned, uint32_t BitWidth>
constexpr uint64_t extended_word(const Int<BitWidth>& v, uint32_t idx, bool neg) {
    constexpr uint32_t WORDS = Int<BitWidth>::NUM_WORDS;
    if (idx < WORDS) {
        uint64_t w = v.data[idx];
        if constexpr (IsSigned) {
            if (neg) {
                constexpr uint32_t LAST = WORDS - 1;
                constexpr uint32_t REM = BitWidth % 64;
                if constexpr (REM != 0) {
                    if (idx == LAST) {
                        w |= ~low_mask64(REM);
                    }
                }
            }
        }
        return w;
    }
    if constexpr (IsSigned) {
        return neg ? ~uint64_t(0) : uint64_t(0);
    } else {
        return uint64_t(0);
    }
}
}

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID)
constexpr bool operator==(const LhsOperand& lhs_operand, const RhsOperand& rhs_operand) {
    using LhsTraits = IntOperandTraits<std::remove_cvref_t<LhsOperand>>;
    using RhsTraits = IntOperandTraits<std::remove_cvref_t<RhsOperand>>;
    constexpr uint32_t LHS_BIT_WIDTH = LhsTraits::BIT_WIDTH;
    constexpr uint32_t RHS_BIT_WIDTH = RhsTraits::BIT_WIDTH;
    constexpr uint32_t MAX_BIT_WIDTH = (LHS_BIT_WIDTH > RHS_BIT_WIDTH) ? LHS_BIT_WIDTH : RHS_BIT_WIDTH;

    const auto& lhs = LhsTraits::get(lhs_operand);
    const auto& rhs = RhsTraits::get(rhs_operand);

    if constexpr (LHS_BIT_WIDTH == RHS_BIT_WIDTH) {
        if constexpr (LHS_BIT_WIDTH <= 64) {
            return lhs.data[0] == rhs.data[0];
        } else {
            for (uint32_t i = 0; i < Int<LHS_BIT_WIDTH>::NUM_WORDS; ++i) {
                if (lhs.data[i] != rhs.data[i]) {
                    return false;
                }
            }
            return true;
        }
    } else if constexpr (MAX_BIT_WIDTH <= 64) {
        const uint64_t l =
            uint_impl_detail::extend_to_64<LhsTraits::IS_SIGNED, LHS_BIT_WIDTH>(lhs, MAX_BIT_WIDTH);
        const uint64_t r =
            uint_impl_detail::extend_to_64<RhsTraits::IS_SIGNED, RHS_BIT_WIDTH>(rhs, MAX_BIT_WIDTH);
        return l == r;
    } else {
        constexpr uint32_t MAX_WORDS = Int<MAX_BIT_WIDTH>::NUM_WORDS;
        const bool lhs_neg = uint_impl_detail::operand_negative<LhsTraits::IS_SIGNED>(lhs);
        const bool rhs_neg = uint_impl_detail::operand_negative<RhsTraits::IS_SIGNED>(rhs);

        for (uint32_t i = 0; i < MAX_WORDS; ++i) {
            const uint64_t lw =
                uint_impl_detail::extended_word<LhsTraits::IS_SIGNED, LHS_BIT_WIDTH>(lhs, i, lhs_neg);
            const uint64_t rw =
                uint_impl_detail::extended_word<RhsTraits::IS_SIGNED, RHS_BIT_WIDTH>(rhs, i, rhs_neg);
            if (lw != rw) {
                return false;
            }
        }
        return true;
    }
}

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID)
constexpr bool operator!=(const LhsOperand& lhs_operand, const RhsOperand& rhs_operand) {
    return !(lhs_operand == rhs_operand);
}

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID)
constexpr bool operator<(const LhsOperand& lhs_operand, const RhsOperand& rhs_operand) {
    using LhsTraits = IntOperandTraits<std::remove_cvref_t<LhsOperand>>;
    using RhsTraits = IntOperandTraits<std::remove_cvref_t<RhsOperand>>;
    constexpr uint32_t LHS_BIT_WIDTH = LhsTraits::BIT_WIDTH;
    constexpr uint32_t RHS_BIT_WIDTH = RhsTraits::BIT_WIDTH;
    constexpr uint32_t MAX_BIT_WIDTH = (LHS_BIT_WIDTH > RHS_BIT_WIDTH) ? LHS_BIT_WIDTH : RHS_BIT_WIDTH;

    const auto& lhs = LhsTraits::get(lhs_operand);
    const auto& rhs = RhsTraits::get(rhs_operand);

    if constexpr (MAX_BIT_WIDTH <= 64) {
        const bool lhs_neg = uint_impl_detail::operand_negative<LhsTraits::IS_SIGNED>(lhs);
        const bool rhs_neg = uint_impl_detail::operand_negative<RhsTraits::IS_SIGNED>(rhs);
        if (lhs_neg != rhs_neg) {
            return lhs_neg;
        }

        const uint64_t l =
            uint_impl_detail::extend_to_64<LhsTraits::IS_SIGNED, LHS_BIT_WIDTH>(lhs, MAX_BIT_WIDTH);
        const uint64_t r =
            uint_impl_detail::extend_to_64<RhsTraits::IS_SIGNED, RHS_BIT_WIDTH>(rhs, MAX_BIT_WIDTH);
        return l < r;
    } else {
        constexpr uint32_t MAX_WORDS = Int<MAX_BIT_WIDTH>::NUM_WORDS;
        const bool lhs_neg = uint_impl_detail::operand_negative<LhsTraits::IS_SIGNED>(lhs);
        const bool rhs_neg = uint_impl_detail::operand_negative<RhsTraits::IS_SIGNED>(rhs);
        if (lhs_neg != rhs_neg) {
            return lhs_neg;
        }

        for (uint32_t i = MAX_WORDS; i-- > 0;) {
            const uint64_t lw =
                uint_impl_detail::extended_word<LhsTraits::IS_SIGNED, LHS_BIT_WIDTH>(lhs, i, lhs_neg);
            const uint64_t rw =
                uint_impl_detail::extended_word<RhsTraits::IS_SIGNED, RHS_BIT_WIDTH>(rhs, i, rhs_neg);
            if (lw != rw) {
                return lw < rw;
            }
        }
        return false;
    }
}

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID)
constexpr bool operator<=(const LhsOperand& lhs_operand, const RhsOperand& rhs_operand) {
    return !(rhs_operand < lhs_operand);
}

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID)
constexpr bool operator>(const LhsOperand& lhs_operand, const RhsOperand& rhs_operand) {
    return rhs_operand < lhs_operand;
}

template <typename LhsOperand, typename RhsOperand>
    requires(IntOperandTraits<std::remove_cvref_t<LhsOperand>>::VALID
             && IntOperandTraits<std::remove_cvref_t<RhsOperand>>::VALID)
constexpr bool operator>=(const LhsOperand& lhs_operand, const RhsOperand& rhs_operand) {
    return !(lhs_operand < rhs_operand);
}

template <typename IntOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr bool operator==(const IntOperand& lhs_operand, T rhs_value) {
    using Traits = IntOperandTraits<std::remove_cvref_t<IntOperand>>;
    using ValueT = std::remove_cvref_t<T>;
    constexpr uint32_t BIT_WIDTH = Traits::BIT_WIDTH;
    const Int<BIT_WIDTH> rhs(rhs_value);
    if constexpr (std::is_signed_v<ValueT>) {
        return lhs_operand == rhs.sint();
    } else {
        return lhs_operand == rhs;
    }
}

template <typename T, typename IntOperand>
    requires(std::is_integral_v<std::remove_cvref_t<T>>
             && IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID)
constexpr bool operator==(T lhs_value, const IntOperand& rhs_operand) {
    using Traits = IntOperandTraits<std::remove_cvref_t<IntOperand>>;
    using ValueT = std::remove_cvref_t<T>;
    constexpr uint32_t BIT_WIDTH = Traits::BIT_WIDTH;
    const Int<BIT_WIDTH> lhs(lhs_value);
    if constexpr (std::is_signed_v<ValueT>) {
        return lhs.sint() == rhs_operand;
    } else {
        return lhs == rhs_operand;
    }
}

template <typename IntOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr bool operator!=(const IntOperand& lhs_operand, T rhs_value) {
    return !(lhs_operand == rhs_value);
}

template <typename T, typename IntOperand>
    requires(std::is_integral_v<std::remove_cvref_t<T>>
             && IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID)
constexpr bool operator!=(T lhs_value, const IntOperand& rhs_operand) {
    return !(lhs_value == rhs_operand);
}

template <typename IntOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr bool operator<(const IntOperand& lhs_operand, T rhs_value) {
    using Traits = IntOperandTraits<std::remove_cvref_t<IntOperand>>;
    using ValueT = std::remove_cvref_t<T>;
    constexpr uint32_t BIT_WIDTH = Traits::BIT_WIDTH;
    const Int<BIT_WIDTH> rhs(rhs_value);
    if constexpr (std::is_signed_v<ValueT>) {
        return lhs_operand < rhs.sint();
    } else {
        return lhs_operand < rhs;
    }
}

template <typename T, typename IntOperand>
    requires(std::is_integral_v<std::remove_cvref_t<T>>
             && IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID)
constexpr bool operator<(T lhs_value, const IntOperand& rhs_operand) {
    using Traits = IntOperandTraits<std::remove_cvref_t<IntOperand>>;
    using ValueT = std::remove_cvref_t<T>;
    constexpr uint32_t BIT_WIDTH = Traits::BIT_WIDTH;
    const Int<BIT_WIDTH> lhs(lhs_value);
    if constexpr (std::is_signed_v<ValueT>) {
        return lhs.sint() < rhs_operand;
    } else {
        return lhs < rhs_operand;
    }
}

template <typename IntOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr bool operator<=(const IntOperand& lhs_operand, T rhs_value) {
    return !(rhs_value < lhs_operand);
}

template <typename T, typename IntOperand>
    requires(std::is_integral_v<std::remove_cvref_t<T>>
             && IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID)
constexpr bool operator<=(T lhs_value, const IntOperand& rhs_operand) {
    return !(rhs_operand < lhs_value);
}

template <typename IntOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr bool operator>(const IntOperand& lhs_operand, T rhs_value) {
    return rhs_value < lhs_operand;
}

template <typename T, typename IntOperand>
    requires(std::is_integral_v<std::remove_cvref_t<T>>
             && IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID)
constexpr bool operator>(T lhs_value, const IntOperand& rhs_operand) {
    return rhs_operand < lhs_value;
}

template <typename IntOperand, typename T>
    requires(IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID
             && std::is_integral_v<std::remove_cvref_t<T>>)
constexpr bool operator>=(const IntOperand& lhs_operand, T rhs_value) {
    return !(lhs_operand < rhs_value);
}

template <typename T, typename IntOperand>
    requires(std::is_integral_v<std::remove_cvref_t<T>>
             && IntOperandTraits<std::remove_cvref_t<IntOperand>>::VALID)
constexpr bool operator>=(T lhs_value, const IntOperand& rhs_operand) {
    return !(lhs_value < rhs_operand);
}

namespace uint_impl_detail {
template <uint32_t ShiftBitWidth>
constexpr bool parse_shift_amount(const Int<ShiftBitWidth>& shift_value, uint32_t limit,
                                  uint32_t& shift_out) {
    if constexpr (ShiftBitWidth <= 64) {
        const uint64_t v = shift_value.data[0];
        if (v >= limit) {
            return false;
        }
        shift_out = static_cast<uint32_t>(v);
        return true;
    } else {
        for (uint32_t i = 1; i < Int<ShiftBitWidth>::NUM_WORDS; ++i) {
            if (shift_value.data[i] != 0) {
                return false;
            }
        }
        const uint64_t v = shift_value.data[0];
        if (v >= limit) {
            return false;
        }
        shift_out = static_cast<uint32_t>(v);
        return true;
    }
}
}

template <uint32_t LhsBitWidth, uint32_t ShiftBitWidth>
constexpr Int<LhsBitWidth> operator<<(const Int<LhsBitWidth>& lhs,
                                       const Int<ShiftBitWidth>& rhs) {
    Int<LhsBitWidth> out(0);
    uint32_t shift = 0;
    if (!uint_impl_detail::parse_shift_amount(rhs, LhsBitWidth, shift)) {
        return out;
    }

    if constexpr (LhsBitWidth <= 64) {
        out.data[0] = lhs.data[0] << shift;
    } else {
        constexpr uint32_t WORDS = Int<LhsBitWidth>::NUM_WORDS;
        const uint32_t word_shift = shift / 64;
        const uint32_t bit_shift = shift % 64;
        for (uint32_t i = WORDS; i-- > 0;) {
            uint64_t v = 0;
            if (i >= word_shift) {
                const uint32_t src = i - word_shift;
                v = lhs.data[src] << bit_shift;
                if (bit_shift != 0 && src > 0) {
                    v |= lhs.data[src - 1] >> (64 - bit_shift);
                }
            }
            out.data[i] = v;
        }
    }

    out.mask_high_bits();
    return out;
}

template <uint32_t LhsBitWidth, uint32_t ShiftBitWidth>
constexpr Int<LhsBitWidth> operator>>(const Int<LhsBitWidth>& lhs,
                                       const Int<ShiftBitWidth>& rhs) {
    Int<LhsBitWidth> out(0);
    uint32_t shift = 0;
    if (!uint_impl_detail::parse_shift_amount(rhs, LhsBitWidth, shift)) {
        return out;
    }

    if constexpr (LhsBitWidth <= 64) {
        out.data[0] = lhs.data[0] >> shift;
    } else {
        constexpr uint32_t WORDS = Int<LhsBitWidth>::NUM_WORDS;
        const uint32_t word_shift = shift / 64;
        const uint32_t bit_shift = shift % 64;
        for (uint32_t i = 0; i < WORDS; ++i) {
            uint64_t v = 0;
            const uint32_t src = i + word_shift;
            if (src < WORDS) {
                v = lhs.data[src] >> bit_shift;
                if (bit_shift != 0 && src + 1 < WORDS) {
                    v |= lhs.data[src + 1] << (64 - bit_shift);
                }
            }
            out.data[i] = v;
        }
    }

    out.mask_high_bits();
    return out;
}

template <uint32_t LhsBitWidth, uint32_t ShiftBitWidth>
constexpr Int<LhsBitWidth> operator>>(const IntSignedView<LhsBitWidth>& lhs_signed,
                                       const Int<ShiftBitWidth>& rhs) {
    const auto& lhs = lhs_signed.parent;
    Int<LhsBitWidth> out(0);
    const bool neg =
        ((lhs.data[(LhsBitWidth - 1) / 64] >> ((LhsBitWidth - 1) % 64)) & uint64_t(1)) != 0;

    uint32_t shift = 0;
    if (!uint_impl_detail::parse_shift_amount(rhs, LhsBitWidth, shift)) {
        if (neg) {
            for (uint32_t i = 0; i < Int<LhsBitWidth>::NUM_WORDS; ++i) {
                out.data[i] = ~uint64_t(0);
            }
            out.mask_high_bits();
        }
        return out;
    }

    if constexpr (LhsBitWidth <= 64) {
        uint64_t v = lhs.data[0];
        if (neg) {
            if constexpr (LhsBitWidth < 64) {
                v |= ~uint_impl_detail::low_mask64(LhsBitWidth);
            }
            v = static_cast<uint64_t>(static_cast<int64_t>(v) >> shift);
        } else {
            v >>= shift;
        }
        out.data[0] = v;
    } else {
        constexpr uint32_t WORDS = Int<LhsBitWidth>::NUM_WORDS;
        const uint32_t word_shift = shift / 64;
        const uint32_t bit_shift = shift % 64;

        for (uint32_t i = 0; i < WORDS; ++i) {
            uint64_t v = 0;
            const uint32_t src = i + word_shift;
            if (src < WORDS) {
                v = lhs.data[src] >> bit_shift;
                if (bit_shift != 0 && src + 1 < WORDS) {
                    v |= lhs.data[src + 1] << (64 - bit_shift);
                }
            }
            out.data[i] = v;
        }

        if (neg && shift != 0) {
            const uint32_t fill_start = LhsBitWidth - shift;
            const uint32_t fill_word = fill_start / 64;
            const uint32_t fill_off = fill_start % 64;

            if (fill_word < WORDS) {
                if (fill_off != 0) {
                    out.data[fill_word] |= ~uint_impl_detail::low_mask64(fill_off);
                    for (uint32_t i = fill_word + 1; i < WORDS; ++i) {
                        out.data[i] = ~uint64_t(0);
                    }
                } else {
                    for (uint32_t i = fill_word; i < WORDS; ++i) {
                        out.data[i] = ~uint64_t(0);
                    }
                }
            }
        }
    }

    out.mask_high_bits();
    return out;
}

template <uint32_t LhsBitWidth, typename ShiftType>
    requires std::is_integral_v<std::remove_cvref_t<ShiftType>>
constexpr Int<LhsBitWidth> operator<<(const Int<LhsBitWidth>& lhs, ShiftType rhs) {
    if constexpr (std::is_signed_v<std::remove_cvref_t<ShiftType>>) {
        assert(rhs >= 0);
    }
    return lhs << Int<64>(static_cast<uint64_t>(rhs));
}

template <uint32_t LhsBitWidth, typename ShiftType>
    requires std::is_integral_v<std::remove_cvref_t<ShiftType>>
constexpr Int<LhsBitWidth> operator>>(const Int<LhsBitWidth>& lhs, ShiftType rhs) {
    if constexpr (std::is_signed_v<std::remove_cvref_t<ShiftType>>) {
        assert(rhs >= 0);
    }
    return lhs >> Int<64>(static_cast<uint64_t>(rhs));
}

template <uint32_t LhsBitWidth, typename ShiftType>
    requires std::is_integral_v<std::remove_cvref_t<ShiftType>>
constexpr Int<LhsBitWidth> operator>>(const IntSignedView<LhsBitWidth>& lhs_signed,
                                       ShiftType rhs) {
    if constexpr (std::is_signed_v<std::remove_cvref_t<ShiftType>>) {
        assert(rhs >= 0);
    }
    return lhs_signed >> Int<64>(static_cast<uint64_t>(rhs));
}
