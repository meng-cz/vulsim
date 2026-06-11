#include "fixint.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <type_traits>
#include <utility>

using namespace vulfixint;

namespace {

template <uint32_t W>
void expect_eq(const Int<W>& got, const Int<W>& expected) {
    assert(got == expected);
}

template <typename L, typename R>
concept HasFixintAdd = requires(const L& lhs, const R& rhs) {
    lhs + rhs;
};

template <typename L, typename R>
concept HasFixintSub = requires(const L& lhs, const R& rhs) {
    lhs - rhs;
};

template <typename T>
concept HasFixintNeg = requires(const T& value) {
    -value;
};

template <typename L, typename R>
concept HasFixintMul = requires(const L& lhs, const R& rhs) {
    lhs * rhs;
};

template <typename L, typename R>
concept HasFixintAnd = requires(const L& lhs, const R& rhs) {
    lhs & rhs;
};

template <typename L, typename R>
concept HasFixintOr = requires(const L& lhs, const R& rhs) {
    lhs | rhs;
};

template <typename L, typename R>
concept HasFixintXor = requires(const L& lhs, const R& rhs) {
    lhs ^ rhs;
};

template <typename T>
concept HasFixintNot = requires(const T& value) {
    ~value;
};

template <typename T>
concept HasFixintReduceAnd = requires(const T& value) {
    ReduceAnd(value);
};

template <typename T>
concept HasFixintReduceOr = requires(const T& value) {
    ReduceOr(value);
};

template <typename T>
concept HasFixintReduceXor = requires(const T& value) {
    ReduceXor(value);
};

template <uint32_t N, typename T>
concept HasFixintRepeat = requires(const T& value) {
    Repeat<N>(value);
};

template <typename... Ts>
concept HasFixintCat = requires(const Ts&... values) {
    Cat(values...);
};

template <typename L, typename R>
concept HasFixintEq = requires(const L& lhs, const R& rhs) {
    lhs == rhs;
};

template <typename L, typename R>
concept HasFixintNe = requires(const L& lhs, const R& rhs) {
    lhs != rhs;
};

template <typename L, typename R>
concept HasFixintLt = requires(const L& lhs, const R& rhs) {
    lhs < rhs;
};

template <typename L, typename R>
concept HasFixintLe = requires(const L& lhs, const R& rhs) {
    lhs <= rhs;
};

template <typename L, typename R>
concept HasFixintGt = requires(const L& lhs, const R& rhs) {
    lhs > rhs;
};

template <typename L, typename R>
concept HasFixintGe = requires(const L& lhs, const R& rhs) {
    lhs >= rhs;
};

void test_static_at() {
    Int<8> a = 0b11010110;

    expect_eq(Int<3>(a.at<4, 2>()), Int<3>(0b101));
    assert(static_cast<bool>(a.at<1>()));

    a.at<4, 2>() = Int<3>(0b001);
    expect_eq(a, Int<8>(0b11000110));

    a.at<1>() = false;
    expect_eq(a, Int<8>(0b11000100));
}

void test_dynamic_pick() {
    Int<8> a = 0b10110110;
    uint32_t i = 2;

    expect_eq(Int<2>(a.pick<2>(i)), Int<2>(0b01));
    assert(static_cast<bool>(a.pick(i)));

    a.pick<3>(3) = Int<3>(0b010);
    expect_eq(a, Int<8>(0b10010110));

    Int<Int<8>::IDX_WIDTH> idx = 7;
    a.pick(idx) = true;
    assert(static_cast<bool>(a.at<7>()));
}

void test_int_assignment_sources() {
    Int<8> a = int16_t(-2);
    expect_eq(a, Int<8>(0xFE));

    Int<8> b = uint16_t(0x1234);
    expect_eq(b, Int<8>(0x34));

    Int<12> c = Int<8>(0xAB);
    expect_eq(c, Int<12>(0x0AB));

    Int<12> d = a.at<7, 4>();
    expect_eq(d, Int<12>(0x00F));

    Int<8> e = false;
    e = c.at<0>();
    expect_eq(e, Int<8>(1));
}

void test_slice_assignment_sources() {
    Int<8> a = 0;
    Int<8> b = 0b10101100;

    a.at<5, 2>() = Int<4>(0b1101);
    expect_eq(a, Int<8>(0b00110100));

    a.at<5, 2>() = int8_t(-2);
    expect_eq(a, Int<8>(0b00111000));

    a.at<5, 2>() = b.at<7, 4>();
    expect_eq(a, Int<8>(0b00101000));

    a.pick<4>(0) = b.pick<4>(2);
    expect_eq(Int<4>(a.at<3, 0>()), Int<4>(0b1011));
}

void test_bit_assignment_sources() {
    Int<8> a = 0;
    Int<8> b = 0b00010000;

    a.at<0>() = Int<1>(1);
    assert(static_cast<bool>(a.at<0>()));

    a.at<1>() = 3;
    assert(static_cast<bool>(a.at<1>()));

    a.at<2>() = false;
    assert(!static_cast<bool>(a.at<2>()));

    a.at<3>() = b.at<4>();
    assert(static_cast<bool>(a.at<3>()));
}

void test_self_slice_and_bit_assignment() {
    Int<8> a = 0b11010101;

    a.at<7, 4>() = a.at<3, 0>();
    expect_eq(a, Int<8>(0b01010101));

    a = 0b11100101;
    a.at<5, 2>() = a.at<3, 0>();
    expect_eq(a, Int<8>(0b11010101));

    a = 0b11010101;
    a.pick<4>(0) = a.pick<4>(2);
    expect_eq(a, Int<8>(0b11010101));

    a = 0b10010000;
    a.at<0>() = a.at<7>();
    assert(static_cast<bool>(a.at<0>()));

    a.at<6>() = a.at<0>();
    assert(static_cast<bool>(a.at<6>()));

    a = 0b00000010;
    a.pick(0) = a.pick(1);
    assert(static_cast<bool>(a.at<0>()));
}

void test_cross_64bit_static_at() {
    Int<130> a = 0;

    a.at<63, 56>() = uint8_t(0xAB);
    a.at<71, 64>() = uint8_t(0xCD);
    a.at<95, 60>() = Int<36>(0x123456789ULL);
    a.at<129, 120>() = Int<10>(0x2A5);

    expect_eq(Int<8>(a.at<63, 56>()), Int<8>(0x9B));
    expect_eq(Int<8>(a.at<71, 64>()), Int<8>(0x78));
    expect_eq(Int<36>(a.at<95, 60>()), Int<36>(0x123456789ULL));
    expect_eq(Int<10>(a.at<129, 120>()), Int<10>(0x2A5));
    expect_eq(Int<16>(a.at<71, 56>()), Int<16>(0x789B));

    assert(static_cast<bool>(a.at<64>()) == false);
    assert(static_cast<bool>(a.at<95>()) == false);
    assert(static_cast<bool>(a.at<129>()) == true);
}

void test_cross_64bit_dynamic_pick() {
    Int<96> a = 0;

    a.pick<16>(60) = uint16_t(0xBEEF);
    a.pick<20>(63) = Int<20>(0xABCDE);
    a.pick<9>(55) = Int<9>(0x1A5);

    expect_eq(Int<16>(a.pick<16>(60)), Int<16>(0xE6FD));
    expect_eq(Int<20>(a.pick<20>(63)), Int<20>(0xABCDF));
    expect_eq(Int<9>(a.pick<9>(55)), Int<9>(0x1A5));

    Int<Int<96>::IDX_WIDTH> idx = 63;
    expect_eq(Int<20>(a.pick<20>(idx)), Int<20>(0xABCDF));

    idx = 64;
    a.pick(idx) = true;
    assert(static_cast<bool>(a.at<64>()));
}

void test_cross_64bit_self_assignment() {
    Int<130> a = 0;
    a.at<79, 48>() = Int<32>(0x89ABCDEFU);
    a.at<111, 80>() = Int<32>(0x01234567U);
    a.at<127, 112>() = uint16_t(0xBEEF);

    a.at<95, 64>() = a.at<79, 48>();
    expect_eq(Int<32>(a.at<95, 64>()), Int<32>(0x89ABCDEFU));

    a.at<87, 56>() = a.at<119, 88>();
    expect_eq(Int<32>(a.at<87, 56>()), Int<32>(0xEF012389U));

    a.pick<24>(61) = a.pick<24>(73);
    expect_eq(Int<24>(a.pick<24>(61)), Int<24>(0xC4F780));

    a.at<64>() = a.at<127>();
    assert(static_cast<bool>(a.at<64>()));

    a.pick(65) = a.pick(64);
    assert(static_cast<bool>(a.at<65>()));
}

void test_cross_64bit_assignment_sources() {
    Int<130> a = 0;
    Int<96> b = 0;

    b.at<31, 0>() = uint32_t(0x76543210U);
    b.at<63, 32>() = uint32_t(0xFEDCBA98U);
    b.at<95, 64>() = Int<32>(0x89ABCDEFU);

    a.at<100, 5>() = b.at<95, 0>();
    expect_eq(Int<96>(a.at<100, 5>()), Int<96>(b));

    a.pick<32>(60) = uint32_t(0x13579BDFU);
    expect_eq(Int<32>(a.pick<32>(60)), Int<32>(0x13579BDFU));

    Int<40> c = a.at<99, 60>();
    expect_eq(c, Int<40>(0x1313579BDFULL));

    Int<8> d = a.pick(64);
    expect_eq(d, Int<8>(1));
}

void test_assignability_contract() {
    using Slice4 = decltype(std::declval<Int<8>&>().template at<5, 2>());
    using Slice4Const = decltype(std::declval<const Int<8>&>().template at<5, 2>());
    using Slice3 = decltype(std::declval<Int<8>&>().template at<4, 2>());
    using Bit = decltype(std::declval<Int<8>&>().template at<1>());
    using ConstBit = decltype(std::declval<const Int<8>&>().template at<1>());

    static_assert(std::is_assignable_v<Slice4, Int<4>>);
    static_assert(std::is_assignable_v<Slice4, int>);
    static_assert(std::is_assignable_v<Slice4, Slice4>);
    static_assert(std::is_assignable_v<Slice4, Slice4Const>);
    static_assert(!std::is_assignable_v<Slice4, Slice3>);

    static_assert(std::is_assignable_v<Bit, Int<1>>);
    static_assert(std::is_assignable_v<Bit, int>);
    static_assert(std::is_assignable_v<Bit, bool>);
    static_assert(std::is_assignable_v<Bit, ConstBit>);
}

void test_signed_view_api_and_traits() {
    Int<8> a = 0xD6;
    const Int<8> ca = a;

    auto int_signed = a.sint();
    auto const_int_signed = ca.sint();
    auto slice_signed = a.at<7, 4>().sint();
    auto const_slice_signed = ca.at<5, 2>().sint();
    auto bit_signed = a.at<1>().sint();
    auto const_bit_signed = ca.at<2>().sint();

    static_assert(is_int_operand_v<Int<8>>);
    static_assert(is_int_operand_v<decltype(a.at<7, 4>())>);
    static_assert(is_int_operand_v<decltype(ca.at<5, 2>())>);
    static_assert(is_int_operand_v<decltype(a.at<1>())>);
    static_assert(is_int_operand_v<decltype(ca.at<2>())>);
    static_assert(is_int_operand_v<decltype(int_signed)>);
    static_assert(is_int_operand_v<decltype(slice_signed)>);
    static_assert(is_int_operand_v<decltype(bit_signed)>);
    static_assert(!is_int_operand_v<int>);

    static_assert(!is_int_operand_signed_v<Int<8>>);
    static_assert(!is_int_operand_signed_v<decltype(a.at<7, 4>())>);
    static_assert(!is_int_operand_signed_v<decltype(a.at<1>())>);
    static_assert(is_int_operand_signed_v<decltype(int_signed)>);
    static_assert(is_int_operand_signed_v<decltype(const_int_signed)>);
    static_assert(is_int_operand_signed_v<decltype(slice_signed)>);
    static_assert(is_int_operand_signed_v<decltype(const_slice_signed)>);
    static_assert(is_int_operand_signed_v<decltype(bit_signed)>);
    static_assert(is_int_operand_signed_v<decltype(const_bit_signed)>);

    static_assert(int_operand_bit_width_v<Int<8>> == 8);
    static_assert(int_operand_bit_width_v<decltype(a.at<7, 4>())> == 4);
    static_assert(int_operand_bit_width_v<decltype(a.at<1>())> == 1);
    static_assert(int_operand_bit_width_v<decltype(int_signed)> == 8);
    static_assert(int_operand_bit_width_v<decltype(slice_signed)> == 4);
    static_assert(int_operand_bit_width_v<decltype(bit_signed)> == 1);

    static_assert(std::is_same_v<int_operand_int_t<Int<8>>, Int<8>>);
    static_assert(std::is_same_v<int_operand_int_t<decltype(slice_signed)>, Int<4>>);
    static_assert(std::is_same_v<int_operand_int_t<decltype(bit_signed)>, Int<1>>);

    expect_eq(get_int_operand_value(a), Int<8>(0xD6));
    expect_eq(get_int_operand_value(int_signed), Int<8>(0xD6));
    expect_eq(get_int_operand_value(const_int_signed), Int<8>(0xD6));
    expect_eq(get_int_operand_value(a.at<7, 4>()), Int<4>(0xD));
    expect_eq(get_int_operand_value(slice_signed), Int<4>(0xD));
    expect_eq(get_int_operand_value(const_slice_signed), Int<4>(0x5));
    expect_eq(get_int_operand_value(a.at<1>()), Int<1>(1));
    expect_eq(get_int_operand_value(bit_signed), Int<1>(1));
    expect_eq(get_int_operand_value(const_bit_signed), Int<1>(1));

    Int<130> wide = 0;
    wide.at<71, 64>() = uint8_t(0xA5);
    wide.at<129>() = true;

    auto wide_slice_signed = wide.at<71, 64>().sint();
    auto wide_bit_signed = wide.at<129>().sint();

    static_assert(is_int_operand_signed_v<decltype(wide_slice_signed)>);
    static_assert(int_operand_bit_width_v<decltype(wide_slice_signed)> == 8);
    static_assert(int_operand_bit_width_v<decltype(wide_bit_signed)> == 1);

    expect_eq(get_int_operand_value(wide_slice_signed), Int<8>(0xA5));
    expect_eq(get_int_operand_value(wide_bit_signed), Int<1>(1));
}

void test_signed_view_assignment() {
    Int<8> neg8 = 0x80;
    Int<16> ext16 = neg8.sint();
    expect_eq(ext16, Int<16>(0xFF80));

    Int<16> neg16 = 0xFF80;
    Int<8> trunc8 = neg16.sint();
    expect_eq(trunc8, Int<8>(0x80));

    Int<8> pos8 = 0x7F;
    Int<16> ext_pos16 = pos8.sint();
    expect_eq(ext_pos16, Int<16>(0x007F));

    Int<8> a = 0xD6;
    Int<12> from_slice = a.at<7, 4>().sint();
    expect_eq(from_slice, Int<12>(0xFFD));

    Int<8> from_bit0 = a.at<0>().sint();
    Int<8> from_bit1 = a.at<1>().sint();
    expect_eq(from_bit0, Int<8>(0x00));
    expect_eq(from_bit1, Int<8>(0xFF));

    Int<8> dst = 0;
    dst = a.at<7, 4>().sint();
    expect_eq(dst, Int<8>(0xFD));

    Int<16> dst_wide = 0;
    dst_wide = a.at<7, 4>().sint();
    expect_eq(dst_wide, Int<16>(0xFFFD));

    Int<8> b = 0;
    b.at<7, 4>() = a.at<3, 0>().sint();
    expect_eq(b, Int<8>(0x60));

    b = 0;
    b.at<3, 0>() = a.at<7, 4>().sint();
    expect_eq(Int<4>(b.at<3, 0>()), Int<4>(0xD));

    b.at<0>() = a.at<1>().sint();
    assert(static_cast<bool>(b.at<0>()));
    b.at<1>() = a.at<0>().sint();
    assert(!static_cast<bool>(b.at<1>()));

    Int<130> wide = 0;
    wide.at<71, 64>() = uint8_t(0x80);
    Int<16> cross_ext = wide.at<71, 64>().sint();
    expect_eq(cross_ext, Int<16>(0xFF80));

    using Slice4 = decltype(std::declval<Int<8>&>().template at<7, 4>());
    using SignedSlice4 = decltype(std::declval<Int<8>&>().template at<7, 4>().sint());
    using SignedSlice3 = decltype(std::declval<Int<8>&>().template at<6, 4>().sint());
    using Bit = decltype(std::declval<Int<8>&>().template at<0>());
    using SignedBit = decltype(std::declval<Int<8>&>().template at<0>().sint());

    static_assert(std::is_assignable_v<Int<16>&, SignedSlice4>);
    static_assert(std::is_assignable_v<Slice4, SignedSlice4>);
    static_assert(!std::is_assignable_v<Slice4, SignedSlice3>);
    static_assert(std::is_assignable_v<Bit, SignedBit>);
}

void test_unsigned_addition() {
    Int<8> a = 200;
    Int<8> b = 100;
    auto sum = a + b;
    static_assert(std::is_same_v<decltype(sum), Int<9>>);
    expect_eq(sum, Int<9>(300));

    Int<4> c = 0xF;
    Int<4> d = 0x1;
    auto sum_small = c + d;
    static_assert(std::is_same_v<decltype(sum_small), Int<5>>);
    expect_eq(sum_small, Int<5>(0x10));

    Int<8> e = 0xD6;
    auto sum_slice = e.at<7, 4>() + e.at<3, 0>();
    static_assert(std::is_same_v<decltype(sum_slice), Int<5>>);
    expect_eq(sum_slice, Int<5>(0x13));

    const Int<8> ce = e;
    auto sum_const_slice = ce.at<7, 4>() + ce.at<5, 2>();
    static_assert(std::is_same_v<decltype(sum_const_slice), Int<5>>);
    expect_eq(sum_const_slice, Int<5>(0x12));

    Int<8> bits = 0b00000011;
    auto bit_sum = bits.at<0>() + bits.at<1>();
    static_assert(std::is_same_v<decltype(bit_sum), Int<2>>);
    expect_eq(bit_sum, Int<2>(2));

    auto mixed = bits.at<0>() + e.at<7, 4>();
    static_assert(std::is_same_v<decltype(mixed), Int<5>>);
    expect_eq(mixed, Int<5>(0x0E));

    auto int_plus_u = e + uint16_t(1);
    auto u_plus_int = uint16_t(1) + e;
    static_assert(std::is_same_v<decltype(int_plus_u), Int<9>>);
    static_assert(std::is_same_v<decltype(u_plus_int), Int<9>>);
    expect_eq(int_plus_u, Int<9>(0x0D7));
    expect_eq(u_plus_int, Int<9>(0x0D7));

    auto int_plus_s = e + int16_t(-1);
    auto s_plus_int = int16_t(-1) + e;
    expect_eq(int_plus_s, Int<9>(0x1D5));
    expect_eq(s_plus_int, Int<9>(0x1D5));

    auto slice_plus_u = e.at<7, 4>() + uint8_t(2);
    auto u_plus_slice = uint8_t(2) + e.at<7, 4>();
    static_assert(std::is_same_v<decltype(slice_plus_u), Int<5>>);
    static_assert(std::is_same_v<decltype(u_plus_slice), Int<5>>);
    expect_eq(slice_plus_u, Int<5>(0x0F));
    expect_eq(u_plus_slice, Int<5>(0x0F));

    auto slice_plus_s = e.at<3, 0>() + int8_t(-2);
    auto s_plus_slice = int8_t(-2) + e.at<3, 0>();
    expect_eq(slice_plus_s, Int<5>(0x14));
    expect_eq(s_plus_slice, Int<5>(0x14));

    auto bit_plus_u = bits.at<0>() + uint8_t(1);
    auto u_plus_bit = uint8_t(1) + bits.at<0>();
    static_assert(std::is_same_v<decltype(bit_plus_u), Int<2>>);
    static_assert(std::is_same_v<decltype(u_plus_bit), Int<2>>);
    expect_eq(bit_plus_u, Int<2>(2));
    expect_eq(u_plus_bit, Int<2>(2));

    Int<64> all_ones = ~uint64_t(0);
    Int<1> one = 1;
    auto carry64 = all_ones + one;
    static_assert(std::is_same_v<decltype(carry64), Int<65>>);
    expect_eq(Int<64>(carry64.at<63, 0>()), Int<64>(0));
    expect_eq(Int<1>(carry64.at<64>()), Int<1>(1));

    Int<130> wide_a = 0;
    Int<130> wide_b = 0;
    wide_a.at<129, 0>() = Int<130>(-1);
    wide_b.at<0>() = true;

    auto wide_sum = wide_a + wide_b;
    static_assert(std::is_same_v<decltype(wide_sum), Int<131>>);
    expect_eq(Int<130>(wide_sum.at<129, 0>()), Int<130>(0));
    expect_eq(Int<1>(wide_sum.at<130>()), Int<1>(1));

    Int<130> wide_c = 0;
    wide_c.at<129, 0>() = Int<130>(-1);
    auto wide_plus_s = wide_c + int8_t(-1);
    auto s_plus_wide = int8_t(-1) + wide_c;
    static_assert(std::is_same_v<decltype(wide_plus_s), Int<131>>);
    static_assert(std::is_same_v<decltype(s_plus_wide), Int<131>>);
    expect_eq(Int<130>(wide_plus_s.at<129, 0>()), Int<130>((Int<130>(wide_c.at<129, 0>()) + Int<130>(int8_t(-1))).at<129, 0>()));
    expect_eq(Int<1>(wide_plus_s.at<130>()), Int<1>(1));
    expect_eq(s_plus_wide, wide_plus_s);

    static_assert(HasFixintAdd<Int<8>, Int<4>>);
    static_assert(HasFixintAdd<Int<8>, uint16_t>);
    static_assert(HasFixintAdd<uint16_t, Int<8>>);
    static_assert(HasFixintAdd<decltype(std::declval<Int<8>&>().template at<7, 4>()), int>);
    static_assert(HasFixintAdd<int, decltype(std::declval<Int<8>&>().template at<7, 4>())>);
    static_assert(HasFixintAdd<decltype(std::declval<Int<8>&>().template at<7, 4>()),
                               decltype(std::declval<Int<8>&>().template at<3, 0>())>);
    static_assert(HasFixintAdd<decltype(std::declval<Int<8>&>().template at<0>()),
                               decltype(std::declval<Int<8>&>().template at<1>())>);
    static_assert(!HasFixintAdd<decltype(std::declval<Int<8>>().sint()), Int<8>>);
    static_assert(!HasFixintAdd<Int<8>, decltype(std::declval<Int<8>>().sint())>);
    static_assert(!HasFixintAdd<decltype(std::declval<Int<8>&>().template at<7, 4>().sint()),
                                decltype(std::declval<Int<8>&>().template at<3, 0>())>);
    static_assert(!HasFixintAdd<decltype(std::declval<Int<8>>().sint()), int>);
    static_assert(!HasFixintAdd<int, decltype(std::declval<Int<8>>().sint())>);
}

void test_unsigned_subtraction() {
    Int<8> a = 200;
    Int<8> b = 100;
    auto diff = a - b;
    static_assert(std::is_same_v<decltype(diff), Int<8>>);
    expect_eq(diff, Int<8>(100));

    Int<4> c = 0x1;
    Int<4> d = 0x2;
    auto diff_small = c - d;
    static_assert(std::is_same_v<decltype(diff_small), Int<4>>);
    expect_eq(diff_small, Int<4>(0xF));

    Int<8> e = 0xD6;
    auto diff_slice = e.at<7, 4>() - e.at<3, 0>();
    static_assert(std::is_same_v<decltype(diff_slice), Int<4>>);
    expect_eq(diff_slice, Int<4>(0x7));

    const Int<8> ce = e;
    auto diff_const_slice = ce.at<5, 2>() - ce.at<3, 0>();
    static_assert(std::is_same_v<decltype(diff_const_slice), Int<4>>);
    expect_eq(diff_const_slice, Int<4>(0xF));

    Int<8> bits = 0b00000001;
    auto bit_diff = bits.at<0>() - bits.at<0>();
    static_assert(std::is_same_v<decltype(bit_diff), Int<1>>);
    expect_eq(bit_diff, Int<1>(0));

    auto mixed = e.at<7, 4>() - bits.at<0>();
    static_assert(std::is_same_v<decltype(mixed), Int<4>>);
    expect_eq(mixed, Int<4>(0xC));

    auto int_minus_u = e - uint16_t(1);
    auto u_minus_int = uint16_t(1) - e;
    static_assert(std::is_same_v<decltype(int_minus_u), Int<8>>);
    static_assert(std::is_same_v<decltype(u_minus_int), Int<8>>);
    expect_eq(int_minus_u, Int<8>(0xD5));
    expect_eq(u_minus_int, Int<8>(0x2B));

    auto int_minus_s = e - int16_t(-1);
    auto s_minus_int = int16_t(-1) - e;
    expect_eq(int_minus_s, Int<8>(0xD7));
    expect_eq(s_minus_int, Int<8>(0x29));

    auto slice_minus_u = e.at<7, 4>() - uint8_t(2);
    auto u_minus_slice = uint8_t(2) - e.at<7, 4>();
    static_assert(std::is_same_v<decltype(slice_minus_u), Int<4>>);
    static_assert(std::is_same_v<decltype(u_minus_slice), Int<4>>);
    expect_eq(slice_minus_u, Int<4>(0xB));
    expect_eq(u_minus_slice, Int<4>(0x5));

    auto slice_minus_s = e.at<3, 0>() - int8_t(-2);
    auto s_minus_slice = int8_t(-2) - e.at<3, 0>();
    expect_eq(slice_minus_s, Int<4>(0x8));
    expect_eq(s_minus_slice, Int<4>(0x8));

    auto bit_minus_u = bits.at<0>() - uint8_t(1);
    auto u_minus_bit = uint8_t(0) - bits.at<0>();
    static_assert(std::is_same_v<decltype(bit_minus_u), Int<1>>);
    static_assert(std::is_same_v<decltype(u_minus_bit), Int<1>>);
    expect_eq(bit_minus_u, Int<1>(0));
    expect_eq(u_minus_bit, Int<1>(1));

    Int<64> zero64 = 0;
    Int<1> one = 1;
    auto borrow64 = zero64 - one;
    static_assert(std::is_same_v<decltype(borrow64), Int<64>>);
    expect_eq(borrow64, Int<64>(~uint64_t(0)));

    Int<130> wide_a = 0;
    Int<130> wide_b = 0;
    wide_a.at<129, 0>() = Int<130>(0);
    wide_b.at<0>() = true;

    auto wide_diff = wide_a - wide_b;
    static_assert(std::is_same_v<decltype(wide_diff), Int<130>>);
    expect_eq(Int<64>(wide_diff.at<63, 0>()), Int<64>(~uint64_t(0)));
    expect_eq(Int<64>(wide_diff.at<127, 64>()), Int<64>(~uint64_t(0)));
    expect_eq(Int<2>(wide_diff.at<129, 128>()), Int<2>(0x3));

    Int<130> wide_c = 0;
    wide_c.at<129, 0>() = Int<130>(0);
    auto wide_minus_s = wide_c - int8_t(-1);
    auto s_minus_wide = int8_t(-1) - wide_c;
    static_assert(std::is_same_v<decltype(wide_minus_s), Int<130>>);
    static_assert(std::is_same_v<decltype(s_minus_wide), Int<130>>);
    expect_eq(wide_minus_s, Int<130>(1));
    expect_eq(s_minus_wide, Int<130>(int8_t(-1)));

    static_assert(HasFixintSub<Int<8>, Int<4>>);
    static_assert(HasFixintSub<Int<8>, uint16_t>);
    static_assert(HasFixintSub<uint16_t, Int<8>>);
    static_assert(HasFixintSub<decltype(std::declval<Int<8>&>().template at<7, 4>()), int>);
    static_assert(HasFixintSub<int, decltype(std::declval<Int<8>&>().template at<7, 4>())>);
    static_assert(HasFixintSub<decltype(std::declval<Int<8>&>().template at<7, 4>()),
                               decltype(std::declval<Int<8>&>().template at<3, 0>())>);
    static_assert(HasFixintSub<decltype(std::declval<Int<8>&>().template at<0>()),
                               decltype(std::declval<Int<8>&>().template at<1>())>);
    static_assert(!HasFixintSub<decltype(std::declval<Int<8>>().sint()), Int<8>>);
    static_assert(!HasFixintSub<Int<8>, decltype(std::declval<Int<8>>().sint())>);
    static_assert(!HasFixintSub<decltype(std::declval<Int<8>&>().template at<7, 4>().sint()),
                                decltype(std::declval<Int<8>&>().template at<3, 0>())>);
    static_assert(!HasFixintSub<decltype(std::declval<Int<8>>().sint()), int>);
    static_assert(!HasFixintSub<int, decltype(std::declval<Int<8>>().sint())>);

    Int<8> neg_src = 0x01;
    auto neg_int = -neg_src;
    static_assert(std::is_same_v<decltype(neg_int), Int<8>>);
    expect_eq(neg_int, Int<8>(0xFF));

    Int<8> neg_zero_src = 0;
    expect_eq(-neg_zero_src, Int<8>(0));

    Int<8> neg_self_src = 0x80;
    expect_eq(-neg_self_src, Int<8>(0x80));

    Int<8> slice_src = 0xD6;
    auto neg_slice = -slice_src.at<7, 4>();
    auto neg_const_slice = -std::as_const(slice_src).at<3, 0>();
    static_assert(std::is_same_v<decltype(neg_slice), Int<4>>);
    static_assert(std::is_same_v<decltype(neg_const_slice), Int<4>>);
    expect_eq(neg_slice, Int<4>(0x3));
    expect_eq(neg_const_slice, Int<4>(0xA));

    Int<8> bit_src = 0b00000001;
    auto neg_bit1 = -bit_src.at<0>();
    auto neg_bit0 = -bit_src.at<1>();
    static_assert(std::is_same_v<decltype(neg_bit1), Int<1>>);
    static_assert(std::is_same_v<decltype(neg_bit0), Int<1>>);
    expect_eq(neg_bit1, Int<1>(1));
    expect_eq(neg_bit0, Int<1>(0));

    Int<8> signed_src = 0xFE;
    expect_eq(-signed_src.sint(), Int<8>(0x02));
    expect_eq(-signed_src.at<7, 4>().sint(), Int<4>(0x1));
    expect_eq(-bit_src.at<0>().sint(), Int<1>(1));

    Int<130> wide_neg_src = 0;
    wide_neg_src.at<0>() = true;
    auto wide_neg = -wide_neg_src;
    static_assert(std::is_same_v<decltype(wide_neg), Int<130>>);
    expect_eq(Int<64>(wide_neg.at<63, 0>()), Int<64>(~uint64_t(0)));
    expect_eq(Int<64>(wide_neg.at<127, 64>()), Int<64>(~uint64_t(0)));
    expect_eq(Int<2>(wide_neg.at<129, 128>()), Int<2>(0x3));

    Int<130> wide_signed_src = 0;
    wide_signed_src.at<129>() = true;
    expect_eq(-wide_signed_src.sint(), Int<130>(wide_signed_src));

    static_assert(HasFixintNeg<Int<8>>);
    static_assert(HasFixintNeg<decltype(std::declval<Int<8>&>().template at<7, 4>())>);
    static_assert(HasFixintNeg<decltype(std::declval<const Int<8>&>().template at<3, 0>())>);
    static_assert(HasFixintNeg<decltype(std::declval<Int<8>&>().template at<0>())>);
    static_assert(HasFixintNeg<decltype(std::declval<Int<8>>().sint())>);
    static_assert(HasFixintNeg<decltype(std::declval<Int<8>&>().template at<7, 4>().sint())>);
}

void test_multiplication() {
    Int<8> a = 200;
    Int<8> b = 100;
    auto prod = a * b;
    static_assert(std::is_same_v<decltype(prod), Int<16>>);
    expect_eq(prod, Int<16>(20000));

    Int<8> e = 0xD6;
    auto prod_slice = e.at<7, 4>() * e.at<3, 0>();
    static_assert(std::is_same_v<decltype(prod_slice), Int<8>>);
    expect_eq(prod_slice, Int<8>(0x4E));

    const Int<8> ce = e;
    auto prod_const_slice = ce.at<5, 2>() * ce.at<3, 0>();
    static_assert(std::is_same_v<decltype(prod_const_slice), Int<8>>);
    expect_eq(prod_const_slice, Int<8>(0x1E));

    Int<8> bits = 0b00000011;
    auto bit_prod = bits.at<0>() * bits.at<1>();
    static_assert(std::is_same_v<decltype(bit_prod), Int<2>>);
    expect_eq(bit_prod, Int<2>(1));

    auto mixed = bits.at<0>() * e.at<7, 4>();
    static_assert(std::is_same_v<decltype(mixed), Int<5>>);
    expect_eq(mixed, Int<5>(0x0D));

    Int<8> neg2 = 0xFE;
    Int<8> neg3 = 0xFD;
    Int<8> pos3 = 0x03;

    auto signed_unsigned = neg2.sint() * pos3;
    auto unsigned_signed = pos3 * neg2.sint();
    static_assert(std::is_same_v<decltype(signed_unsigned), Int<16>>);
    static_assert(std::is_same_v<decltype(unsigned_signed), Int<16>>);
    expect_eq(signed_unsigned, Int<16>(0xFFFA));
    expect_eq(unsigned_signed, Int<16>(0xFFFA));

    auto both_signed = neg2.sint() * neg3.sint();
    static_assert(std::is_same_v<decltype(both_signed), Int<16>>);
    expect_eq(both_signed, Int<16>(0x0006));

    auto signed_slice = e.at<7, 4>().sint() * e.at<3, 0>();
    auto signed_slice_rev = e.at<3, 0>() * e.at<7, 4>().sint();
    static_assert(std::is_same_v<decltype(signed_slice), Int<8>>);
    expect_eq(signed_slice, Int<8>(0xEE));
    expect_eq(signed_slice_rev, Int<8>(0xEE));

    auto signed_bit = bits.at<0>().sint() * Int<4>(3);
    static_assert(std::is_same_v<decltype(signed_bit), Int<5>>);
    expect_eq(signed_bit, Int<5>(0x1D));

    auto int_mul_u = e * uint16_t(2);
    auto u_mul_int = uint16_t(2) * e;
    static_assert(std::is_same_v<decltype(int_mul_u), Int<16>>);
    static_assert(std::is_same_v<decltype(u_mul_int), Int<16>>);
    expect_eq(int_mul_u, Int<16>(0x01AC));
    expect_eq(u_mul_int, Int<16>(0x01AC));

    auto int_mul_s = neg2.sint() * int8_t(3);
    auto s_mul_int = int8_t(3) * neg2.sint();
    expect_eq(int_mul_s, Int<16>(0xFFFA));
    expect_eq(s_mul_int, Int<16>(0xFFFA));

    auto slice_mul_u = e.at<7, 4>() * uint8_t(2);
    auto u_mul_slice = uint8_t(2) * e.at<7, 4>();
    static_assert(std::is_same_v<decltype(slice_mul_u), Int<8>>);
    static_assert(std::is_same_v<decltype(u_mul_slice), Int<8>>);
    expect_eq(slice_mul_u, Int<8>(0x1A));
    expect_eq(u_mul_slice, Int<8>(0x1A));

    auto slice_mul_s = e.at<7, 4>() * int8_t(-1);
    auto s_mul_slice = int8_t(-1) * e.at<7, 4>();
    expect_eq(slice_mul_s, Int<8>(0xF3));
    expect_eq(s_mul_slice, Int<8>(0xF3));

    auto bit_mul_s = bits.at<0>() * int8_t(-1);
    auto s_mul_bit = int8_t(-1) * bits.at<0>();
    static_assert(std::is_same_v<decltype(bit_mul_s), Int<2>>);
    static_assert(std::is_same_v<decltype(s_mul_bit), Int<2>>);
    expect_eq(bit_mul_s, Int<2>(0x3));
    expect_eq(s_mul_bit, Int<2>(0x3));

    Int<65> wide = 0;
    wide.at<64>() = true;
    wide.at<1, 0>() = Int<2>(0x3);
    auto wide_prod = wide * uint8_t(2);
    static_assert(std::is_same_v<decltype(wide_prod), Int<130>>);
    expect_eq(Int<64>(wide_prod.at<63, 0>()), Int<64>(0x6));
    expect_eq(Int<1>(wide_prod.at<65>()), Int<1>(1));

    Int<65> wide_neg = Int<65>(-1);
    auto wide_signed_prod = wide_neg.sint() * uint8_t(2);
    static_assert(std::is_same_v<decltype(wide_signed_prod), Int<130>>);
    expect_eq(wide_signed_prod, Int<130>(-2));

    static_assert(HasFixintMul<Int<8>, Int<4>>);
    static_assert(HasFixintMul<Int<8>, uint16_t>);
    static_assert(HasFixintMul<uint16_t, Int<8>>);
    static_assert(HasFixintMul<decltype(std::declval<Int<8>&>().template at<7, 4>()),
                               decltype(std::declval<Int<8>&>().template at<3, 0>())>);
    static_assert(HasFixintMul<decltype(std::declval<Int<8>&>().template at<7, 4>().sint()),
                               decltype(std::declval<Int<8>&>().template at<3, 0>())>);
    static_assert(HasFixintMul<decltype(std::declval<Int<8>>().sint()), Int<8>>);
    static_assert(HasFixintMul<Int<8>, decltype(std::declval<Int<8>>().sint())>);
    static_assert(HasFixintMul<decltype(std::declval<Int<8>>().sint()), int>);
    static_assert(HasFixintMul<int, decltype(std::declval<Int<8>>().sint())>);
}

void test_bitwise_ops() {
    Int<8> a = 0b11010110;
    Int<8> b = 0b10101100;

    auto and_int = a & b;
    auto or_int = a | b;
    auto xor_int = a ^ b;
    auto not_int = ~a;

    static_assert(std::is_same_v<decltype(and_int), Int<8>>);
    static_assert(std::is_same_v<decltype(or_int), Int<8>>);
    static_assert(std::is_same_v<decltype(xor_int), Int<8>>);
    static_assert(std::is_same_v<decltype(not_int), Int<8>>);

    expect_eq(and_int, Int<8>(0b10000100));
    expect_eq(or_int, Int<8>(0b11111110));
    expect_eq(xor_int, Int<8>(0b01111010));
    expect_eq(not_int, Int<8>(0b00101001));

    auto and_slice = a.at<7, 4>() & b.at<7, 4>();
    auto or_slice = a.at<7, 4>() | b.at<3, 0>();
    auto xor_const_slice = std::as_const(a).at<7, 4>() ^ std::as_const(b).at<3, 0>();
    auto not_slice = ~a.at<7, 4>();

    static_assert(std::is_same_v<decltype(and_slice), Int<4>>);
    static_assert(std::is_same_v<decltype(or_slice), Int<4>>);
    static_assert(std::is_same_v<decltype(xor_const_slice), Int<4>>);
    static_assert(std::is_same_v<decltype(not_slice), Int<4>>);

    expect_eq(and_slice, Int<4>(0x8));
    expect_eq(or_slice, Int<4>(0xD));
    expect_eq(xor_const_slice, Int<4>(0x1));
    expect_eq(not_slice, Int<4>(0x2));

    Int<8> bits = 0b00000001;
    auto and_bit = bits.at<0>() & a.at<0>();
    auto or_bit = bits.at<1>() | a.at<1>();
    auto xor_bit = bits.at<0>() ^ a.at<1>();
    auto not_bit = ~bits.at<0>();

    static_assert(std::is_same_v<decltype(and_bit), Int<1>>);
    static_assert(std::is_same_v<decltype(or_bit), Int<1>>);
    static_assert(std::is_same_v<decltype(xor_bit), Int<1>>);
    static_assert(std::is_same_v<decltype(not_bit), Int<1>>);

    expect_eq(and_bit, Int<1>(0));
    expect_eq(or_bit, Int<1>(1));
    expect_eq(xor_bit, Int<1>(0));
    expect_eq(not_bit, Int<1>(0));

    auto and_int_rhs = a & uint16_t(0x0F);
    auto and_int_lhs = uint16_t(0x0F) & a;
    auto or_int_rhs = a | uint16_t(0x01);
    auto or_int_lhs = uint16_t(0x01) | a;
    auto xor_int_rhs = a ^ uint16_t(0xFF);
    auto xor_int_lhs = uint16_t(0xFF) ^ a;

    static_assert(std::is_same_v<decltype(and_int_rhs), Int<8>>);
    static_assert(std::is_same_v<decltype(and_int_lhs), Int<8>>);
    static_assert(std::is_same_v<decltype(or_int_rhs), Int<8>>);
    static_assert(std::is_same_v<decltype(or_int_lhs), Int<8>>);
    static_assert(std::is_same_v<decltype(xor_int_rhs), Int<8>>);
    static_assert(std::is_same_v<decltype(xor_int_lhs), Int<8>>);

    expect_eq(and_int_rhs, Int<8>(0x06));
    expect_eq(and_int_lhs, Int<8>(0x06));
    expect_eq(or_int_rhs, Int<8>(0xD7));
    expect_eq(or_int_lhs, Int<8>(0xD7));
    expect_eq(xor_int_rhs, Int<8>(0x29));
    expect_eq(xor_int_lhs, Int<8>(0x29));

    auto slice_and_negative = a.at<7, 4>() & int8_t(-1);
    auto negative_or_slice = int8_t(-16) | a.at<3, 0>();
    auto slice_xor_negative = a.at<3, 0>() ^ int8_t(-1);

    expect_eq(slice_and_negative, Int<4>(0xD));
    expect_eq(negative_or_slice, Int<4>(0x6));
    expect_eq(slice_xor_negative, Int<4>(0x9));

    Int<130> wide_a = 0;
    Int<130> wide_b = 0;
    wide_a.at<63, 0>() = uint64_t(0xFFFF0000FFFF0000ULL);
    wide_a.at<127, 64>() = uint64_t(0x0123456789ABCDEFULL);
    wide_a.at<129, 128>() = Int<2>(0x2);
    wide_b.at<63, 0>() = uint64_t(0x00FF00FF00FF00FFULL);
    wide_b.at<127, 64>() = uint64_t(0xFEDCBA9876543210ULL);
    wide_b.at<129, 128>() = Int<2>(0x1);

    auto wide_and = wide_a & wide_b;
    auto wide_or = wide_a | wide_b;
    auto wide_xor = wide_a ^ wide_b;
    auto wide_not = ~wide_a;

    static_assert(std::is_same_v<decltype(wide_and), Int<130>>);
    static_assert(std::is_same_v<decltype(wide_or), Int<130>>);
    static_assert(std::is_same_v<decltype(wide_xor), Int<130>>);
    static_assert(std::is_same_v<decltype(wide_not), Int<130>>);

    expect_eq(Int<64>(wide_and.at<63, 0>()), Int<64>(0x00FF000000FF0000ULL));
    expect_eq(Int<64>(wide_and.at<127, 64>()), Int<64>(0x0000000000000000ULL));
    expect_eq(Int<2>(wide_and.at<129, 128>()), Int<2>(0x0));

    expect_eq(Int<64>(wide_or.at<63, 0>()), Int<64>(0xFFFF00FFFFFF00FFULL));
    expect_eq(Int<64>(wide_or.at<127, 64>()), Int<64>(0xFFFFFFFFFFFFFFFFULL));
    expect_eq(Int<2>(wide_or.at<129, 128>()), Int<2>(0x3));

    expect_eq(Int<64>(wide_xor.at<63, 0>()), Int<64>(0xFF0000FFFF0000FFULL));
    expect_eq(Int<64>(wide_xor.at<127, 64>()), Int<64>(0xFFFFFFFFFFFFFFFFULL));
    expect_eq(Int<2>(wide_xor.at<129, 128>()), Int<2>(0x3));

    expect_eq(Int<64>(wide_not.at<63, 0>()), Int<64>(0x0000FFFF0000FFFFULL));
    expect_eq(Int<64>(wide_not.at<127, 64>()), Int<64>(0xFEDCBA9876543210ULL));
    expect_eq(Int<2>(wide_not.at<129, 128>()), Int<2>(0x1));

    static_assert(HasFixintAnd<Int<8>, Int<8>>);
    static_assert(HasFixintOr<Int<8>, Int<8>>);
    static_assert(HasFixintXor<Int<8>, Int<8>>);
    static_assert(HasFixintNot<Int<8>>);

    static_assert(HasFixintAnd<Int<8>, uint16_t>);
    static_assert(HasFixintAnd<uint16_t, Int<8>>);
    static_assert(HasFixintOr<decltype(std::declval<Int<8>&>().template at<7, 4>()), int>);
    static_assert(HasFixintOr<int, decltype(std::declval<Int<8>&>().template at<7, 4>())>);
    static_assert(HasFixintXor<decltype(std::declval<Int<8>&>().template at<7, 4>()),
                               decltype(std::declval<Int<8>&>().template at<3, 0>())>);
    static_assert(HasFixintNot<decltype(std::declval<Int<8>&>().template at<7, 4>())>);
    static_assert(HasFixintNot<decltype(std::declval<Int<8>&>().template at<0>())>);

    static_assert(!HasFixintAnd<Int<8>, Int<4>>);
    static_assert(!HasFixintOr<Int<8>, Int<4>>);
    static_assert(!HasFixintXor<Int<8>, Int<4>>);
    static_assert(!HasFixintAnd<decltype(std::declval<Int<8>>().sint()), Int<8>>);
    static_assert(!HasFixintOr<Int<8>, decltype(std::declval<Int<8>>().sint())>);
    static_assert(!HasFixintXor<decltype(std::declval<Int<8>&>().template at<7, 4>().sint()),
                                decltype(std::declval<Int<8>&>().template at<3, 0>())>);
    static_assert(!HasFixintNot<decltype(std::declval<Int<8>>().sint())>);
    static_assert(!HasFixintAnd<decltype(std::declval<Int<8>>().sint()), int>);
    static_assert(!HasFixintOr<int, decltype(std::declval<Int<8>>().sint())>);
    static_assert(!HasFixintXor<decltype(std::declval<Int<8>>().sint()), int>);
}

void test_comparison_ops() {
    Int<8> a = 0x10;
    Int<8> b = 0x20;
    Int<8> c = 0x10;

    assert(a == c);
    assert(a != b);
    assert(a < b);
    assert(b > a);
    assert(a <= c);
    assert(a >= c);
    assert(!(a < c));
    assert(!(a > c));

    Int<8> s0 = 0xDD;
    Int<8> s1 = 0xCE;
    assert((s0.at<7, 4>() == s0.at<3, 0>()));
    assert((s0.at<7, 4>() != s1.at<7, 4>()));
    assert((s0.at<3, 0>() < s1.at<3, 0>()));
    assert((std::as_const(s1).at<7, 4>() < std::as_const(s0).at<7, 4>()));
    Int<8> bit_sample = 0b00000110;
    assert(bit_sample.at<1>() == bit_sample.at<2>());
    assert(bit_sample.at<0>() != bit_sample.at<1>());
    assert(bit_sample.at<1>() > bit_sample.at<0>());

    Int<8> neg2 = 0xFE;
    Int<8> neg1 = 0xFF;
    Int<8> pos3 = 0x03;
    assert(neg2.sint() < pos3);
    assert(pos3 > neg2.sint());
    assert(neg2.sint() < neg1.sint());
    assert(neg1.sint() <= neg1.sint());
    assert(neg1.sint() >= neg2.sint());
    assert(neg1 > neg2.sint());

    Int<4> neg3 = 0xD;
    assert(neg3.sint() < Int<4>(0));
    assert(Int<4>(7) > neg3.sint());
    assert((neg3.at<3, 0>().sint() < uint8_t(1)));

    assert(a == uint8_t(0x10));
    assert(uint8_t(0x10) == a);
    assert(a != uint8_t(0x11));
    assert(uint8_t(0x11) != a);
    assert(a < uint8_t(0x11));
    assert(uint8_t(0x11) > a);
    assert(a <= uint8_t(0x10));
    assert(uint8_t(0x10) >= a);
    assert(neg2.sint() < int8_t(1));
    assert(int8_t(-1) < pos3);
    assert(int8_t(-1) <= neg1.sint());
    assert(int8_t(4) >= pos3);

    Int<130> wide_a = 0;
    Int<130> wide_b = 0;
    wide_a.at<129>() = true;
    wide_a.at<63, 0>() = uint64_t(0);
    wide_b.at<128>() = true;
    wide_b.at<64>() = true;
    wide_b.at<0>() = true;
    assert(wide_b < wide_a);
    assert(wide_a > wide_b);
    assert(wide_a.sint() < wide_b);
    assert(wide_b > wide_a.sint());

    Int<130> wide_neg2 = Int<130>(-2);
    Int<130> wide_neg1 = Int<130>(-1);
    assert(wide_neg2.sint() < wide_neg1.sint());
    assert(wide_neg1.sint() > wide_neg2.sint());
    assert(wide_neg1 == Int<130>(-1));
    assert(wide_neg1 != wide_neg2);

    static_assert(HasFixintEq<Int<8>, Int<8>>);
    static_assert(HasFixintNe<Int<8>, Int<8>>);
    static_assert(HasFixintLt<Int<8>, Int<8>>);
    static_assert(HasFixintLe<Int<8>, Int<8>>);
    static_assert(HasFixintGt<Int<8>, Int<8>>);
    static_assert(HasFixintGe<Int<8>, Int<8>>);
    static_assert(HasFixintEq<decltype(std::declval<Int<8>&>().template at<7, 4>()),
                              decltype(std::declval<Int<8>&>().template at<3, 0>())>);
    static_assert(HasFixintEq<decltype(std::declval<Int<8>&>().template at<0>()),
                              decltype(std::declval<Int<8>&>().template at<1>())>);
    static_assert(HasFixintLt<decltype(std::declval<Int<8>>().sint()), Int<8>>);
    static_assert(HasFixintLt<Int<8>, decltype(std::declval<Int<8>>().sint())>);
    static_assert(HasFixintLt<decltype(std::declval<Int<8>&>().template at<7, 4>().sint()),
                              decltype(std::declval<Int<8>&>().template at<3, 0>())>);
    static_assert(HasFixintEq<Int<8>, uint8_t>);
    static_assert(HasFixintEq<uint8_t, Int<8>>);
    static_assert(HasFixintLt<Int<8>, int>);
    static_assert(HasFixintLt<int, Int<8>>);

    static_assert(!HasFixintEq<Int<8>, Int<4>>);
    static_assert(!HasFixintNe<Int<8>, Int<4>>);
    static_assert(!HasFixintLt<Int<8>, Int<4>>);
    static_assert(!HasFixintEq<decltype(std::declval<Int<8>>().sint()), Int<8>>);
    static_assert(!HasFixintNe<Int<8>, decltype(std::declval<Int<8>>().sint())>);
    static_assert(!HasFixintEq<decltype(std::declval<Int<8>&>().template at<7, 4>().sint()),
                               decltype(std::declval<Int<8>&>().template at<3, 0>())>);
    static_assert(!HasFixintEq<Int<8>, int>);
    static_assert(!HasFixintEq<int, Int<8>>);
    static_assert(!HasFixintNe<Int<8>, int>);
    static_assert(!HasFixintNe<int, Int<8>>);
}

void test_reduce_ops() {
    Int<8> zero = 0x00;
    Int<8> all = 0xFF;
    Int<8> one = 0x01;
    Int<8> two_bits = 0x81;

    assert(!ReduceAnd(zero));
    assert(!ReduceOr(zero));
    assert(!ReduceXor(zero));
    assert(ReduceAnd(all));
    assert(ReduceOr(all));
    assert(!ReduceXor(all));
    assert(!ReduceAnd(one));
    assert(ReduceOr(one));
    assert(ReduceXor(one));
    assert(ReduceOr(two_bits));
    assert(!ReduceXor(two_bits));

    Int<8> slices = 0b11110001;
    assert(ReduceAnd(slices.at<7, 4>()));
    assert(!ReduceAnd(slices.at<3, 0>()));
    assert(ReduceOr(slices.at<3, 0>()));
    assert(ReduceXor(slices.at<3, 0>()));
    assert(!ReduceXor(slices.at<7, 4>()));
    assert(ReduceOr(std::as_const(slices).at<7, 4>()));

    assert(ReduceAnd(slices.at<0>()));
    assert(ReduceOr(slices.at<0>()));
    assert(ReduceXor(slices.at<0>()));
    assert(!ReduceAnd(slices.at<1>()));
    assert(!ReduceOr(slices.at<1>()));
    assert(!ReduceXor(slices.at<1>()));

    Int<65> wide_all = Int<65>(-1);
    Int<65> wide_almost = wide_all;
    wide_almost.at<64>() = false;
    assert(ReduceAnd(wide_all));
    assert(!ReduceAnd(wide_almost));
    assert(ReduceOr(wide_almost));
    assert(ReduceXor(wide_all));
    assert(!ReduceXor(wide_almost));

    Int<130> wide = 0;
    wide.at<0>() = true;
    wide.at<64>() = true;
    wide.at<129>() = true;
    assert(!ReduceAnd(wide));
    assert(ReduceOr(wide));
    assert(ReduceXor(wide));
    wide.at<128>() = true;
    assert(!ReduceXor(wide));

    static_assert(ReduceAnd(Int<4>(0xF)));
    static_assert(!ReduceAnd(Int<4>(0x7)));
    static_assert(ReduceOr(Int<4>(0x8)));
    static_assert(!ReduceOr(Int<4>(0x0)));
    static_assert(ReduceXor(Int<4>(0x7)));
    static_assert(!ReduceXor(Int<4>(0xF)));

    static_assert(HasFixintReduceAnd<Int<8>>);
    static_assert(HasFixintReduceOr<Int<8>>);
    static_assert(HasFixintReduceXor<Int<8>>);
    static_assert(HasFixintReduceAnd<decltype(std::declval<Int<8>&>().template at<7, 4>())>);
    static_assert(HasFixintReduceOr<decltype(std::declval<Int<8>&>().template at<0>())>);
    static_assert(HasFixintReduceXor<decltype(std::declval<const Int<8>&>().template at<7, 4>())>);
    static_assert(!HasFixintReduceAnd<decltype(std::declval<Int<8>>().sint())>);
    static_assert(!HasFixintReduceOr<decltype(std::declval<Int<8>&>().template at<7, 4>().sint())>);
    static_assert(!HasFixintReduceXor<decltype(std::declval<Int<8>&>().template at<0>().sint())>);
    static_assert(!HasFixintReduceAnd<int>);
    static_assert(!HasFixintReduceOr<uint8_t>);
    static_assert(!HasFixintReduceXor<bool>);
}

void test_concat_ops() {
    Int<4> a = 0xA;
    auto repeat_a = Repeat<3>(a);
    static_assert(std::is_same_v<decltype(repeat_a), Int<12>>);
    expect_eq(repeat_a, Int<12>(0xAAA));

    auto repeat_one = Repeat<1>(a);
    static_assert(std::is_same_v<decltype(repeat_one), Int<4>>);
    expect_eq(repeat_one, Int<4>(0xA));

    Int<8> x = 0xD6;
    auto repeat_slice = Repeat<4>(x.at<3, 0>());
    static_assert(std::is_same_v<decltype(repeat_slice), Int<16>>);
    expect_eq(repeat_slice, Int<16>(0x6666));

    auto repeat_const_slice = Repeat<2>(std::as_const(x).at<7, 4>());
    static_assert(std::is_same_v<decltype(repeat_const_slice), Int<8>>);
    expect_eq(repeat_const_slice, Int<8>(0xDD));

    auto repeat_bit1 = Repeat<5>(x.at<1>());
    auto repeat_bit0 = Repeat<5>(x.at<0>());
    static_assert(std::is_same_v<decltype(repeat_bit1), Int<5>>);
    expect_eq(repeat_bit1, Int<5>(0x1F));
    expect_eq(repeat_bit0, Int<5>(0x00));

    Int<4> b = 0x5;
    auto cat_ab = Cat(a, b);
    static_assert(std::is_same_v<decltype(cat_ab), Int<8>>);
    expect_eq(cat_ab, Int<8>(0xA5));

    auto cat_many = Cat(Int<3>(0x5), Int<5>(0x12), Int<1>(1));
    static_assert(std::is_same_v<decltype(cat_many), Int<9>>);
    expect_eq(cat_many, Int<9>(0x165));

    auto cat_slices = Cat(x.at<7, 4>(), x.at<3, 0>());
    static_assert(std::is_same_v<decltype(cat_slices), Int<8>>);
    expect_eq(cat_slices, x);

    auto cat_mixed_refs = Cat(x.at<7>(), x.at<5, 2>(), std::as_const(x).at<1>());
    static_assert(std::is_same_v<decltype(cat_mixed_refs), Int<6>>);
    expect_eq(cat_mixed_refs, Int<6>(0b101011));

    Int<65> wide = 0;
    wide.at<64>() = true;
    wide.at<3, 0>() = Int<4>(0xB);
    auto repeat_wide = Repeat<2>(wide);
    static_assert(std::is_same_v<decltype(repeat_wide), Int<130>>);
    expect_eq(Int<65>(repeat_wide.at<64, 0>()), wide);
    expect_eq(Int<65>(repeat_wide.at<129, 65>()), wide);

    Int<64> hi = 0x0123456789ABCDEFULL;
    Int<2> mid = 0x2;
    Int<64> lo = 0xFEDCBA9876543210ULL;
    auto cat_wide = Cat(hi, mid, lo);
    static_assert(std::is_same_v<decltype(cat_wide), Int<130>>);
    expect_eq(Int<64>(cat_wide.at<129, 66>()), hi);
    expect_eq(Int<2>(cat_wide.at<65, 64>()), mid);
    expect_eq(Int<64>(cat_wide.at<63, 0>()), lo);

    static_assert(std::is_same_v<decltype(Repeat<2>(Int<7>(0x55))), Int<14>>);
    static_assert(Repeat<2>(Int<4>(0x9)) == Int<8>(0x99));
    static_assert(Cat(Int<3>(0x5), Int<2>(0x1)) == Int<5>(0x15));

    static_assert(HasFixintRepeat<2, Int<8>>);
    static_assert(HasFixintRepeat<3, decltype(std::declval<Int<8>&>().template at<7, 4>())>);
    static_assert(HasFixintRepeat<4, decltype(std::declval<const Int<8>&>().template at<0>())>);
    static_assert(HasFixintCat<Int<8>>);
    static_assert(HasFixintCat<Int<4>, Int<4>>);
    static_assert(HasFixintCat<decltype(std::declval<Int<8>&>().template at<7, 4>()),
                               decltype(std::declval<Int<8>&>().template at<0>())>);
    static_assert(!HasFixintRepeat<2, decltype(std::declval<Int<8>>().sint())>);
    static_assert(!HasFixintRepeat<2, int>);
    static_assert(!HasFixintCat<>);
    static_assert(!HasFixintCat<Int<8>, decltype(std::declval<Int<8>>().sint())>);
    static_assert(!HasFixintCat<int, Int<8>>);
}

} // namespace

int main() {
    test_static_at();
    test_dynamic_pick();
    test_int_assignment_sources();
    test_slice_assignment_sources();
    test_bit_assignment_sources();
    test_self_slice_and_bit_assignment();
    test_cross_64bit_static_at();
    test_cross_64bit_dynamic_pick();
    test_cross_64bit_self_assignment();
    test_cross_64bit_assignment_sources();
    test_assignability_contract();
    test_signed_view_api_and_traits();
    test_signed_view_assignment();
    test_unsigned_addition();
    test_unsigned_subtraction();
    test_multiplication();
    test_bitwise_ops();
    test_comparison_ops();
    test_reduce_ops();
    test_concat_ops();

    std::cout << "All fixint tests passed!" << std::endl;
    return 0;
}
