#include "uint.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace {

using u128 = unsigned __int128;

template <uint32_t BitWidth>
u128 width_mask() {
    if constexpr (BitWidth >= 128) {
        return ~u128(0);
    } else {
        return (u128(1) << BitWidth) - 1;
    }
}

template <uint32_t BitWidth>
u128 to_u128(const UInt<BitWidth>& value) {
    u128 result = 0;
    constexpr uint32_t words = UInt<BitWidth>::NUM_WORDS < 2 ? UInt<BitWidth>::NUM_WORDS : 2;
    for (uint32_t i = 0; i < words; ++i) {
        result |= u128(value.get_data()[i]) << (i * 64);
    }
    return result & width_mask<BitWidth>();
}

template <uint32_t BitWidth>
void expect_value(const UInt<BitWidth>& value, u128 expected) {
    assert(to_u128(value) == (expected & width_mask<BitWidth>()));
}

template <uint32_t BitWidth>
void expect_zero_extended(const UInt<BitWidth>& value, uint32_t from_width) {
    for (uint32_t bit = from_width; bit < BitWidth; ++bit) {
        assert(!static_cast<bool>(value(bit)));
    }
}

void test_default_and_integer_construction() {
    UInt<1> one_bit_default;
    expect_value(one_bit_default, 0);

    UInt<7> seven_bit = 0xff;
    expect_value(seven_bit, 0x7f);

    UInt<64> sixty_four_bit = 0xfedcba9876543210ULL;
    expect_value(sixty_four_bit, u128(0xfedcba9876543210ULL));

    UInt<65> sixty_five_bit = 0xffffffffffffffffULL;
    expect_value(sixty_five_bit, u128(0xffffffffffffffffULL));
    assert(sixty_five_bit.get_data()[1] == 0);
}

void test_cross_width_construction_and_assignment() {
    UInt<16> narrow = 0xabcd;
    UInt<32> wide = narrow;
    expect_value(wide, 0xabcd);
    expect_zero_extended(wide, 16);

    UInt<8> truncated = narrow;
    expect_value(truncated, 0xcd);

    UInt<65> wide_boundary = 0xffffffffffffffffULL;
    wide_boundary(64) = true;
    UInt<32> trunc_boundary = wide_boundary;
    expect_value(trunc_boundary, 0xffffffffULL);

    UInt<96> extend_boundary = wide_boundary;
    expect_value(extend_boundary, (u128(1) << 64) | u128(0xffffffffffffffffULL));
    expect_zero_extended(extend_boundary, 65);

    UInt<127> src127 = 0;
    src127(126) = true;
    src127(64) = true;
    src127(5) = true;

    UInt<128> dst128 = src127;
    assert(static_cast<bool>(dst128(126)));
    assert(static_cast<bool>(dst128(64)));
    assert(static_cast<bool>(dst128(5)));
    assert(!static_cast<bool>(dst128(127)));

    UInt<31> assign_target = 0;
    assign_target = UInt<65>(0x1ffffffffULL);
    expect_value(assign_target, 0x7fffffffULL);
}

template <uint32_t BitWidth>
void test_arithmetic_and_bitwise_for_width(u128 lhs, u128 rhs, uint32_t shl, uint32_t shr) {
    const u128 mask = width_mask<BitWidth>();
    lhs &= mask;
    rhs &= mask;
    UInt<BitWidth> a = static_cast<uint64_t>(lhs);
    UInt<BitWidth> b = static_cast<uint64_t>(rhs);

    if constexpr (BitWidth > 64) {
        for (uint32_t bit = 64; bit < BitWidth && bit < 128; ++bit) {
            if ((lhs >> bit) & 1) a(bit) = true;
            if ((rhs >> bit) & 1) b(bit) = true;
        }
    }

    expect_value(a + b, (lhs + rhs) & mask);
    expect_value(a - b, (lhs - rhs) & mask);
    expect_value(a * b, (lhs * rhs) & mask);

    expect_value(a & b, lhs & rhs);
    expect_value(a | b, lhs | rhs);
    expect_value(a ^ b, lhs ^ rhs);
    expect_value(~a, (~lhs) & mask);

    expect_value(a << shl, (lhs << shl) & mask);
    expect_value(a >> shr, (lhs >> shr) & mask);

    assert((a == b) == (lhs == rhs));
    assert((a != b) == (lhs != rhs));
    assert((a < b) == (lhs < rhs));
    assert((a <= b) == (lhs <= rhs));
    assert((a > b) == (lhs > rhs));
    assert((a >= b) == (lhs >= rhs));
}

void test_arithmetic_and_bitwise() {
    test_arithmetic_and_bitwise_for_width<8>(0xd3, 0x6e, 3, 2);
    test_arithmetic_and_bitwise_for_width<16>(12345, 67890, 5, 7);
    test_arithmetic_and_bitwise_for_width<31>(0x5abcdeffULL, 0x12345678ULL, 9, 11);
    test_arithmetic_and_bitwise_for_width<32>(0x89abcdefULL, 0x10203040ULL, 4, 12);
    test_arithmetic_and_bitwise_for_width<63>(u128(1) << 62, 0x123456789abcdefULL, 1, 17);
    test_arithmetic_and_bitwise_for_width<64>(0xfedcba9876543210ULL, 0x0f1e2d3c4b5a6978ULL, 8, 19);
    test_arithmetic_and_bitwise_for_width<65>((u128(1) << 64) | 0x123456789abcdef0ULL, 0xf0e1d2c3b4a59687ULL, 7, 13);
    test_arithmetic_and_bitwise_for_width<96>((u128(1) << 95) | (u128(0x89abcdef01234567ULL) << 16) | 0x4321ULL,
                                               (u128(0x1234567890abcdefULL) << 8) | 0x5aULL, 15, 20);
    test_arithmetic_and_bitwise_for_width<127>((u128(1) << 126) | (u128(0x0123456789abcdefULL) << 32) | 0x76543210ULL,
                                                (u128(0x0fedcba987654321ULL) << 48) | 0x13579bdfULL, 9, 27);
    test_arithmetic_and_bitwise_for_width<128>((u128(0xfedcba9876543210ULL) << 64) | 0x0123456789abcdefULL,
                                                (u128(0x0f1e2d3c4b5a6978ULL) << 64) | 0x8899aabbccddeeffULL, 16, 29);
}

void test_slice_and_bit_access() {
    UInt<32> example = 0x12345678;
    UInt<16> high = example(31, 16);
    UInt<16> low = example(15, 0);
    expect_value(high, 0x1234);
    expect_value(low, 0x5678);

    example(31, 16) = 0xabcd;
    expect_value(example, 0xabcd5678ULL);

    assert(static_cast<bool>(example(3)));
    example(3) = false;
    assert(!static_cast<bool>(example(3)));
    example(3) = true;
    assert(static_cast<bool>(example(3)));

    UInt<96> wide = 0;
    wide(80, 48) = 0x1ffffffffULL;
    UInt<33> middle = wide(80, 48);
    expect_value(middle, 0x1ffffffffULL);

    wide(63, 60) = 0xa;
    UInt<4> nibble = wide(63, 60);
    expect_value(nibble, 0xa);

    wide(95) = true;
    wide(47) = true;
    assert(static_cast<bool>(wide(95)));
    assert(static_cast<bool>(wide(47)));

    const UInt<96>& const_wide = wide;
    UInt<33> const_middle = const_wide(80, 48);
    expect_value(const_middle, 0x1ffffafffULL);
    UInt<4> const_nibble = const_wide(63, 60);
    expect_value(const_nibble, 0xa);
    assert(static_cast<bool>(const_wide(95)));
    assert(static_cast<bool>(const_wide(47)));
}

void test_shift_boundaries() {
    UInt<65> value = 0;
    value(64) = true;
    value(0) = true;
    expect_value(value << 64, u128(1) << 64);
    expect_value(value >> 64, 1);

    UInt<128> full = 0;
    full(127) = true;
    full(64) = true;
    full(1) = true;
    expect_value(full << 1, ((u128(1) << 127) | (u128(1) << 64) | 2) << 1);
    expect_value(full >> 64, (u128(1) << 63) | 1);
}

} // namespace

int main() {
    test_default_and_integer_construction();
    test_cross_width_construction_and_assignment();
    test_arithmetic_and_bitwise();
    test_slice_and_bit_access();
    test_shift_boundaries();

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
