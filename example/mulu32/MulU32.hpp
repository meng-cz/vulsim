#pragma once

#include "header.hpp"
#include "defhelper.hpp"

REQUEST(s3output, ARG(uint64_t) y);

STRUCT(S0S1RegData) {
    uint32_t a;
    uint32_t b;
};
REGISTER_MUL(s1valid, bool, 2) {
    s1valid = false;
};
REGISTER(s1reg, S0S1RegData) {
    s1reg.a = 0;
    s1reg.b = 0;
};

SERVICE(s0input, ARG(uint32_t) a, ARG(uint32_t) b) {
    s1reg.setnext(S0S1RegData{a, b});
    s1valid.setnext<0>(true);
};

TICK_IMPL() {
    // S0: default clear s1valid when no input
    s1valid.setnext<1>(false);
};


STRUCT(S1S2RegData) {
    Int<32> p0; // a_lo * b_lo
    Int<32> p1; // a_lo * b_hi
    Int<32> p2; // a_hi * b_lo
    Int<32> p3; // a_hi * b_hi
};
REGISTER(s2valid, bool) {
    s2valid = false;
};
REGISTER(s2reg, S1S2RegData) {
    s2reg.p0 = 0;
    s2reg.p1 = 0;
    s2reg.p2 = 0;
    s2reg.p3 = 0;
};

TICK_IMPL() {
    // S1: calculate partial products
    s2valid.setnext(s1valid);
    if (s1valid) {
        S0S1RegData s1 = s1reg;
        Int<32> a = s1.a;
        Int<32> b = s1.b;
        Int<16> a_lo = a.at<15, 0>();
        Int<16> b_lo = b.at<15, 0>();
        Int<16> a_hi = a.at<31, 16>();
        Int<16> b_hi = b.at<31, 16>();

        S1S2RegData s2;
        s2.p0 = a_lo * b_lo;
        s2.p1 = a_lo * b_hi;
        s2.p2 = a_hi * b_lo;
        s2.p3 = a_hi * b_hi;
        s2reg.setnext(s2);
    };
}

STRUCT(S2S3RegData) {
    Int<16> low16;
    Int<34> mid;
    Int<32> high_base;
};
REGISTER(s3valid, bool) {
    s3valid = false;
};
REGISTER(s3reg, S2S3RegData) {
    s3reg.low16 = 0;
    s3reg.mid = 0;
    s3reg.high_base = 0;
};

TICK_IMPL() {
    // S2: compose middle parts
    s3valid.setnext(s2valid);
    if (s2valid) {
        S1S2RegData s2 = s2reg;
        S2S3RegData s3;

        s3.low16 = s2.p0.at<15, 0>();
        s3.mid = s2.p0.at<31, 16>() + s2.p1 + s2.p2; // 16wid + 32wid + 32wid = 34wid
        s3.high_base = s2.p3;

        s3reg.setnext(s3);
    };
}

TICK_IMPL() {
    // S3: final result
    if (s3valid) {
        S2S3RegData s3 = s3reg;
        
        Int<33> high = s3.high_base + s3.mid.at<33, 16>();
        Int<64> y = Cat(high.at<31, 0>(), s3.mid.at<15, 0>(), s3.low16);
        s3output(y.to<uint64_t>());
    };
}

