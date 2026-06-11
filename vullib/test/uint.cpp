#include "uint.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <type_traits>

namespace {

template <uint32_t W>
void expect_eq(const Int<W>& got, const Int<W>& expected) {
    assert(got == expected);
}

void test_type_constants() {
    static_assert(Int<1>::WORD_BITS == 64);
    static_assert(Int<64>::NUM_WORDS == 1);
    static_assert(Int<65>::NUM_WORDS == 2);
    static_assert(Int<130>::NUM_WORDS == 3);
    static_assert(Int<64>::IDX_WIDTH == 6);
    static_assert(Int<130>::IDX_WIDTH == 8);
}

void test_constructors_and_to() {
    Int<8> a;
    Int<8> b = true;
    Int<8> c = 0x1234;
    Int<16> d = Int<8>(0xAB);
    Int<16> e = Int<8>(-1).sint();
    Int<4> f = -1;

    expect_eq(a, Int<8>(0));
    expect_eq(b, Int<8>(1));
    expect_eq(c, Int<8>(0x34));
    expect_eq(d, Int<16>(0x00AB));
    expect_eq(e, Int<16>(0xFFFF));
    expect_eq(f, Int<4>(0xF));

    Int<128> g = int64_t(-1);
    assert(g.data[0] == ~uint64_t(0));
    assert(g.data[1] == ~uint64_t(0));

    Int<8> x = 0xFF;
    assert(x.to<uint32_t>() == 255U);
    assert(x.to<int32_t>() == -1);

    Int<72> h = 0;
    h(63, 0) = uint64_t(0x0123456789ABCDEFULL);
    h(71, 64) = uint8_t(0xAB);
    assert(h.to<uint64_t>() == uint64_t(0x0123456789ABCDEFULL));
}

void test_signed_view_conversion() {
    Int<8> neg8 = 0x80;
    Int<16> ext16 = neg8.sint();
    expect_eq(ext16, Int<16>(0xFF80));

    Int<16> neg16 = 0xFF80;
    Int<8> trunc8_neg = neg16.sint();
    expect_eq(trunc8_neg, Int<8>(0x80));

    Int<16> pos16 = 0x007F;
    Int<8> trunc8_pos = pos16.sint();
    expect_eq(trunc8_pos, Int<8>(0x7F));
}

void test_slice_and_bit_proxy() {
    Int<32> a = 0;
    a(31, 16) = 0x1234;
    a(15, 0) = 0x5678;

    Int<16> hi = a(31, 16);
    uint8_t low8 = a(7, 0);
    bool b3 = a(3);

    expect_eq(hi, Int<16>(0x1234));
    assert(low8 == 0x78);
    assert(b3);

    a(31, 16) = 0xABCD;
    expect_eq(a, Int<32>(0xABCD5678));

    a(7, 0) = 0x3C;
    a(7, 4) = a(3, 0);
    assert(static_cast<uint8_t>(a(7, 0)) == 0xCC);

    a(0) = false;
    assert(!static_cast<bool>(a(0)));
    a(0) = true;
    assert(static_cast<bool>(a(0)));

    const Int<32> c = a;
    const uint8_t byte0 = c(7, 0);
    const bool bit0 = c(0);
    assert(byte0 == static_cast<uint8_t>(a(7, 0)));
    assert(bit0);
}

void test_range_to_range_overlap_assignment() {
    Int<8> a = 0xD5; // 11010101
    a(7, 1) = a(6, 0);
    expect_eq(a, Int<8>(0xAB)); // 10101011

    a = 0xD5;
    a(6, 0) = a(7, 1);
    expect_eq(a, Int<8>(0xEA)); // 11101010

    a = 0xE5; // 11100101
    a(5, 2) = a(3, 0);
    expect_eq(a, Int<8>(0xD5)); // 11010101

    Int<32> src = 0x12345678;
    Int<16> dst = 0;
    dst(15, 0) = src(19, 4);
    expect_eq(dst, Int<16>(0x4567));
}

void test_range_at_and_bit_at() {
    Int<130> wide = 0;
    wide(10, 3) = 0xA5;

    Int<Int<130>::IDX_WIDTH> idx = 3;
    Int<8> mid = wide.range_at<8>(idx);
    bool dyn_bit = wide.bit_at(idx);
    expect_eq(mid, Int<8>(0xA5));
    assert(dyn_bit);

    Int<Int<130>::IDX_WIDTH> idx0 = 0;
    wide.range_at<4>(idx0) = 0xF;
    assert(static_cast<uint8_t>(wide(3, 0)) == 0xF);
    wide.bit_at(idx0) = false;
    assert(!static_cast<bool>(wide(0)));
}

void test_cross_word_slice_and_dynamic_access() {
    Int<130> wide = 0;
    wide(71, 56) = 0x3210;
    wide(103, 72) = 0x76543210;
    wide(129, 114) = 0xBEEF;

    expect_eq(Int<16>(wide(71, 56)), Int<16>(0x3210));
    expect_eq(Int<32>(wide(103, 72)), Int<32>(0x76543210));
    expect_eq(Int<16>(wide(129, 114)), Int<16>(0xBEEF));

    Int<Int<130>::IDX_WIDTH> idx = 60;
    wide.range_at<12>(idx) = 0xABC;
    expect_eq(Int<12>(wide(71, 60)), Int<12>(0xABC));

    Int<Int<130>::IDX_WIDTH> bit_idx = 64;
    wide.bit_at(bit_idx) = true;
    assert(static_cast<bool>(wide(64)));
    bit_idx = 129;
    assert(static_cast<bool>(wide.bit_at(bit_idx)));

    const Int<130> frozen = wide;
    idx = 68;
    expect_eq(Int<20>(frozen.range_at<20>(idx)), Int<20>(wide(87, 68)));
}

void test_range_proxy_cross_width_assignment() {
    Int<96> a = 0;
    Int<130> b = 0;
    b(100, 5) = Cat(Int<32>(0x89ABCDEF), Int<32>(0x01234567), Int<32>(0x76543210));

    a(95, 0) = b(100, 5);
    expect_eq(Int<32>(a(95, 64)), Int<32>(0x89ABCDEF));
    expect_eq(Int<32>(a(63, 32)), Int<32>(0x01234567));
    expect_eq(Int<32>(a(31, 0)), Int<32>(0x76543210));

    Int<130> c = 0;
    c(120, 25) = a(95, 0);
    expect_eq(Int<96>(c(120, 25)), a);

    const Int<130> bconst = b;
    Int<96> d = 0;
    d(95, 0) = bconst(100, 5);
    expect_eq(d, a);
}

void test_reduce_repeat_cat() {
    Int<4> a = 0b1010;
    assert(a.reduce_or());
    assert(!a.reduce_and());
    assert(!a.reduce_xor());

    Int<4> b = 0b1011;
    assert(b.reduce_xor());

    Int<70> all_ones = ~Int<70>(0);
    assert(all_ones.reduce_and());
    Int<70> not_all_ones = all_ones;
    not_all_ones(69) = false;
    assert(!not_all_ones.reduce_and());
    assert(Int<70>(0).reduce_or() == false);

    Int<4> n = 0b1100;
    auto r = n.repeat<3>();
    static_assert(std::is_same_v<decltype(r), Int<12>>);
    expect_eq(r, Int<12>(0xCCC));

    Int<8> h = 0xAB;
    Int<8> l = 0xCD;
    Int<16> x = h.cat(l);
    expect_eq(x, Int<16>(0xABCD));

    auto y = Cat(Int<8>(0x12), Int<8>(0x34), Int<8>(0x56));
    static_assert(std::is_same_v<decltype(y), Int<24>>);
    expect_eq(y, Int<24>(0x123456));
}

void test_repeat_cat_cross_word() {
    Int<65> pattern = 0;
    pattern(64) = true;
    pattern(3, 0) = 0b1011;

    auto repeated = pattern.repeat<2>();
    static_assert(std::is_same_v<decltype(repeated), Int<130>>);
    expect_eq(Int<65>(repeated(64, 0)), pattern);
    expect_eq(Int<65>(repeated(129, 65)), pattern);

    Int<70> hi = 0;
    hi(69, 64) = 0x2A;
    hi(63, 0) = uint64_t(0x0123456789ABCDEFULL);
    Int<67> lo = 0;
    lo(66) = true;
    lo(2, 0) = 0b101;

    auto combined = hi.cat(lo);
    static_assert(std::is_same_v<decltype(combined), Int<137>>);
    expect_eq(Int<67>(combined(66, 0)), lo);
    expect_eq(Int<70>(combined(136, 67)), hi);

    auto multi = Cat(Int<5>(0x1B), Int<70>(hi), Int<3>(0x5), Int<67>(lo));
    static_assert(std::is_same_v<decltype(multi), Int<145>>);
    expect_eq(Int<67>(multi(66, 0)), lo);
    expect_eq(Int<3>(multi(69, 67)), Int<3>(0x5));
    expect_eq(Int<70>(multi(139, 70)), hi);
    expect_eq(Int<5>(multi(144, 140)), Int<5>(0x1B));
}

void test_arithmetic_ops() {
    Int<8> a = 200;
    Int<8> b = 100;

    auto s = a + b;
    static_assert(std::is_same_v<decltype(s), Int<9>>);
    assert(s.to<uint16_t>() == 300U);

    auto d = a - b;
    static_assert(std::is_same_v<decltype(d), Int<8>>);
    expect_eq(d, Int<8>(100));

    auto n = -Int<8>(1);
    expect_eq(n, Int<8>(0xFF));

    auto s2 = a + 1;
    auto s3 = 1 + a;
    static_assert(std::is_same_v<decltype(s2), Int<9>>);
    static_assert(std::is_same_v<decltype(s3), Int<9>>);
    assert(s2.to<uint16_t>() == 201U);
    assert(s3.to<uint16_t>() == 201U);

    auto d2 = a - 50;
    auto d3 = 250 - a;
    expect_eq(d2, Int<8>(150));
    expect_eq(d3, Int<8>(50));

    Int<130> big = 0;
    big(63, 0) = ~uint64_t(0);
    auto big_add = big + Int<130>(1);
    static_assert(std::is_same_v<decltype(big_add), Int<131>>);
    assert(!Int<64>(big_add(63, 0)).reduce_or());
    assert(static_cast<bool>(big_add(64)));

    auto big_sub = Int<130>(0) - Int<130>(1);
    assert(big_sub.reduce_and());

    auto p0 = Int<8>(0xF0) * Int<8>(2);
    static_assert(std::is_same_v<decltype(p0), Int<16>>);
    assert(p0.to<uint16_t>() == 0x01E0U);

    auto p1 = Int<8>(0xF0).sint() * Int<8>(2);
    static_assert(std::is_same_v<decltype(p1), Int<16>>);
    assert(p1.to<uint16_t>() == 0xFFE0U);
}

void test_arithmetic_cross_word_and_signed_mix() {
    Int<65> one = 1;
    auto sum = Int<65>(~uint64_t(0)).cat(Int<1>(1)) + one;
    static_assert(std::is_same_v<decltype(sum), Int<67>>);
    assert(!Int<65>(sum(64, 0)).reduce_or());
    assert(static_cast<bool>(sum(65)));
    assert(!static_cast<bool>(sum(66)));

    Int<130> lhs = 0;
    lhs(129) = true;
    lhs(64) = true;
    Int<130> rhs = 0;
    rhs(64) = true;
    rhs(0) = true;
    auto diff = lhs - rhs;
    static_assert(std::is_same_v<decltype(diff), Int<130>>);
    assert(!static_cast<bool>(diff(129)));
    assert(static_cast<bool>(diff(128)));
    assert(static_cast<bool>(diff(64)));
    assert(static_cast<bool>(diff(0)));

    auto mul_ss = Int<8>(0xFE).sint() * Int<8>(0xFD).sint();
    static_assert(std::is_same_v<decltype(mul_ss), Int<16>>);
    assert(mul_ss.to<int16_t>() == 6);

    auto mul_su = Int<8>(0xFE).sint() * Int<8>(3);
    assert(mul_su.to<int16_t>() == -6);
}

void test_bitwise_ops() {
    Int<8> a = 0xA5;
    Int<8> b = 0x3C;
    expect_eq(a & b, Int<8>(0x24));
    expect_eq(a | b, Int<8>(0xBD));
    expect_eq(a ^ b, Int<8>(0x99));
    expect_eq(~a, Int<8>(0x5A));
}

void test_bitwise_cross_word() {
    Int<130> a = 0;
    Int<130> b = 0;
    a(63, 0) = uint64_t(0xFFFF0000FFFF0000ULL);
    a(129, 64) = Cat(Int<2>(0x3), Int<64>(0x0F0F0F0F0F0F0F0FULL));
    b(63, 0) = uint64_t(0x00FFFF0000FFFF00ULL);
    b(129, 64) = Cat(Int<2>(0x1), Int<64>(0x3333333333333333ULL));

    auto andv = a & b;
    auto orv = a | b;
    auto xorv = a ^ b;
    expect_eq(Int<64>(andv(63, 0)), Int<64>(0x00FF000000FF0000ULL));
    expect_eq(Int<64>(orv(63, 0)), Int<64>(0xFFFFFF00FFFFFF00ULL));
    expect_eq(Int<64>(xorv(63, 0)), Int<64>(0xFF00FF00FF00FF00ULL));
    assert(static_cast<bool>(andv(128)));
    assert(static_cast<bool>(orv(129)));
}

void test_bitwise_with_range_proxy_ops() {
    const Int<16> input = 0x02AA; // low 10 bits = 0b10_1010_1010

    // Must support expression style like:
    // result.frac = (Int<10>(1) << 10) | input(9, 0);
    Int<10> frac10 = (Int<10>(1) << 10) | input(9, 0);
    expect_eq(frac10, Int<10>(0x2AA)); // left part overflows width-10 and becomes 0

    // Practical hidden-1 pattern with sufficient destination width.
    Int<11> frac11 = (Int<11>(1) << 10) | input(9, 0);
    expect_eq(frac11, Int<11>(0x6AA));

    Int<10> andv = Int<10>(0x3FF) & input(9, 0);
    expect_eq(andv, Int<10>(0x2AA));

    Int<10> xorv = input(9, 0) ^ Int<10>(0x155);
    expect_eq(xorv, Int<10>(0x3FF));
}

void test_shift_ops() {
    Int<8> a = 0b11110000;
    expect_eq(a << 2, Int<8>(0b11000000));
    expect_eq(a >> 2, Int<8>(0b00111100));
    expect_eq(a.sint() >> 2, Int<8>(0b11111100));

    expect_eq(a << 99, Int<8>(0));
    expect_eq(a >> 99, Int<8>(0));
    expect_eq(a >> Int<8>(99), Int<8>(0));

    Int<8> neg = 0x80;
    Int<8> pos = 0x7F;
    expect_eq(neg.sint() >> 99, Int<8>(0xFF));
    expect_eq(pos.sint() >> 99, Int<8>(0x00));

    Int<130> wide = 0;
    wide(0) = true;
    wide(65) = true;
    Int<130> sl = wide << 1;
    Int<130> sr = wide >> 1;
    assert(static_cast<bool>(sl(1)));
    assert(static_cast<bool>(sl(66)));
    assert(static_cast<bool>(sr(64)));
    assert(!static_cast<bool>(sr(0)));
}

void test_shift_cross_word_and_dynamic_amount() {
    Int<130> wide = 0;
    wide(0) = true;
    wide(63) = true;
    wide(64) = true;
    wide(129) = true;

    Int<8> sh1 = 64;
    Int<130> left64 = wide << sh1;
    assert(static_cast<bool>(left64(64)));
    assert(static_cast<bool>(left64(127)));
    assert(static_cast<bool>(left64(128)));
    assert(!static_cast<bool>(left64(0)));

    Int<130> right64 = wide >> sh1;
    assert(static_cast<bool>(right64(0)));
    assert(static_cast<bool>(right64(65)));
    assert(!static_cast<bool>(right64(66)));

    Int<130> neg = 0;
    neg(129) = true;
    neg(80) = true;
    auto asr = neg.sint() >> Int<8>(65);
    assert(static_cast<bool>(asr(129)));
    assert(static_cast<bool>(asr(64)));
    assert(static_cast<bool>(asr(15)));
}

void test_comparison_ops() {
    Int<8> u = 0xFF;
    assert(u == 255);
    assert(u != 254);
    assert(!(u < 0));
    assert(u > 0);
    assert(u >= 255U);
    assert(u <= 255U);

    assert(u.sint() < 0);
    assert(u.sint() <= -1);
    assert(u.sint() > -2);
    assert(Int<4>(0xF).sint() == Int<8>(-1).sint());

    assert(Int<4>(0xF) == Int<8>(0x0F));
    assert(Int<4>(0xF) < Int<8>(0x10));
    assert(Int<4>(0x8).sint() < Int<8>(0).sint());

    assert(-1 == Int<8>(0xFF).sint());
    assert(0 < Int<8>(1));
    assert(!(0 < Int<8>(0).sint()));
    assert(0 <= Int<8>(0));
    assert(2 >= Int<8>(1));
}

void test_comparison_cross_width_and_signed_mix() {
    Int<130> neg = 0;
    neg(129) = true;
    Int<130> pos = 0;
    pos(128) = true;
    pos(5) = true;

    assert(neg.sint() < pos.sint());
    assert(!(pos.sint() < neg.sint()));
    assert(neg > pos);
    assert(neg >= pos);
    assert(neg.sint() <= -1);

    Int<4> minus_one4 = 0xF;
    Int<130> minus_one130 = Int<130>(minus_one4.sint());
    assert(minus_one130.sint() == minus_one4.sint());
    assert(Int<65>(0).sint() < Int<66>(1).sint());
}

void test_compound_operation_chains() {
    Int<17> a = 0x12345;
    Int<9> b = 0x155;
    auto packed = Cat(Int<8>(a(15, 8)) ^ Int<8>(b(8, 1)),
                      Int<8>(a(7, 0)) + Int<8>(3),
                      Int<1>(a(16)));
    static_assert(std::is_same_v<decltype(packed), Int<18>>);

    Int<8> top = a(15, 8);
    Int<8> mix = top ^ Int<8>(b(8, 1));
    Int<9> low = Int<8>(a(7, 0)) + Int<8>(3);
    expect_eq(Int<8>(packed(17, 10)), mix);
    expect_eq(Int<9>(packed(9, 1)), low);
    expect_eq(Int<1>(packed(0)), Int<1>(a(16)));

    Int<72> wide = 0;
    wide.range_at<9>(Int<72>::IDX_WIDTH == 7 ? Int<7>(5) : Int<7>(5)) = low;
    expect_eq(Int<9>(wide(13, 5)), low);
}

} // namespace

int main() {
    test_type_constants();
    test_constructors_and_to();
    test_signed_view_conversion();
    test_slice_and_bit_proxy();
    test_range_to_range_overlap_assignment();
    test_range_at_and_bit_at();
    test_cross_word_slice_and_dynamic_access();
    test_range_proxy_cross_width_assignment();
    test_reduce_repeat_cat();
    test_repeat_cat_cross_word();
    test_arithmetic_ops();
    test_arithmetic_cross_word_and_signed_mix();
    test_bitwise_ops();
    test_bitwise_cross_word();
    test_bitwise_with_range_proxy_ops();
    test_shift_ops();
    test_shift_cross_word_and_dynamic_amount();
    test_comparison_ops();
    test_comparison_cross_width_and_signed_mix();
    test_compound_operation_chains();

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
