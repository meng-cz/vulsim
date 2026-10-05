#pragma once

#include "header.hpp"
#include <defhelper.hpp>

INTERFACE() {
    REQUEST(s3output, ARG(uint64_t) y);
    SERVICE(s0input, ARG(uint32_t) a, ARG(uint32_t) b);
}

STRUCT(S0S1RegData) {
    uint32_t a;
    uint32_t b;
};

// All registers default to zero after reset unless a reset block overrides it.
REGISTER_MUL(s1valid, bool, 2);
REGISTER(s1reg, S0S1RegData);

SERVICE(s0input, ARG(uint32_t) a, ARG(uint32_t) b) {
    s1reg.setnext(S0S1RegData{a, b});
    s1valid.setnext<0>(true);
}

TICK_IMPL() {
    // Clear the valid bit unless this cycle accepts a new input.
    s1valid.setnext<1>(false);
};

STRUCT(S1S2RegData) {
    Int<32> p0; // a_lo * b_lo
    Int<32> p1; // a_lo * b_hi
    Int<32> p2; // a_hi * b_lo
    Int<32> p3; // a_hi * b_hi
};

REGISTER(s2valid, bool);
REGISTER(s2reg, S1S2RegData);

TICK_IMPL() {
    // S1: calculate 16 x 16 bit partial products.
    s2valid.setnext(s1valid);
    if (s1valid) {
        S0S1RegData input = s1reg;
        Int<32> a = input.a;
        Int<32> b = input.b;
        Int<16> a_lo = a.at<15, 0>();
        Int<16> b_lo = b.at<15, 0>();
        Int<16> a_hi = a.at<31, 16>();
        Int<16> b_hi = b.at<31, 16>();

        S1S2RegData next;
        next.p0 = a_lo * b_lo;
        next.p1 = a_lo * b_hi;
        next.p2 = a_hi * b_lo;
        next.p3 = a_hi * b_hi;
        s2reg.setnext(next);
    };
}

STRUCT(S2S3RegData) {
    Int<16> low16;
    Int<34> mid;
    Int<32> high_base;
};

REGISTER(s3valid, bool);
REGISTER(s3reg, S2S3RegData);

TICK_IMPL() {
    // S2: keep only the carry widths needed to combine the cross products.
    s3valid.setnext(s2valid);
    if (s2valid) {
        S1S2RegData partial = s2reg;
        S2S3RegData next;
        next.low16 = partial.p0.at<15, 0>();

        Int<32> p0_high = partial.p0.at<31, 16>();
        auto mid_low = AddCarry(p0_high, partial.p1); // Int<33>
        auto mid = AddCarry(mid_low, Int<33>(partial.p2)); // Int<34>
        next.mid = mid;
        next.high_base = partial.p3;

        s3reg.setnext(next);
    };
}

TICK_IMPL() {
    // S3: combine the high partial product and emit the 64-bit product.
    if (s3valid) {
        S2S3RegData partial = s3reg;
        Int<32> mid_high = partial.mid.at<33, 16>();
        auto high = AddCarry(partial.high_base, mid_high); // Int<33>
        Int<64> y = Cat(high.at<31, 0>(), partial.mid.at<15, 0>(), partial.low16);
        s3output(y.to<uint64_t>());
    };
}
