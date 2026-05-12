

#pragma once

#include <defhelper.hpp>

#include "header.hpp"

STRUCT(UnpackedFP16) {
    bool sign;
    UInt<8> exp;
    UInt<10> frac;
};

STRUCT(UnpackedMulResult) {
    bool sign;
    UInt<8> exp;
    UInt<22> frac_unnorm;
};

REQUEST(output, ARG(UInt<32>) sum);

REGISTER(A1, UnpackedFP16) { A1.sign = false; A1.exp = 0; A1.frac = 0; }
REGISTER(B1, UnpackedFP16) { B1.sign = false; B1.exp = 0; B1.frac = 0; }
REGISTER(C1, UnpackedFP16) { C1.sign = false; C1.exp = 0; C1.frac = 0; }
REGISTER(init1, bool) { init1 = false; }
REGISTER(last1, bool) { last1 = false; }

/**
 * a,b: 16-bit FP16 inputs in IEEE 754 binary16 format for multiplication
 * c: 16-bit FP16 input in IEEE 754 binary16 format for accumulation, valid only when init is true
 * init: indicates whether this is the first multiplication-accumulation in the sequence
 * last: indicates whether this is the last multiplication-accumulation in the sequence
 */
SERVICE(input, ARG(UInt<16>) a, ARG(UInt<16>) b, ARG(UInt<16>) c, ARG(bool) init, ARG(bool) last) {
    // Stage 1: Unpacking
    auto unpack = [](const UInt<16>& input) -> UnpackedFP16 {
        UnpackedFP16 result;
        result.sign = input(15);
        result.exp = input(14, 10);
        if (result.exp == 0) {
            // Denormalized number
            result.frac = input(9, 0);
            result.exp = -24;
        } else {
            // Normalized number, add implicit leading 1
            result.frac = (UInt<10>(1) << 10) | input(9, 0);
            result.exp = result.exp - 25;
        }
        return result;
    };

    A1_setnext(unpack(a));
    B1_setnext(unpack(b));
    C1_setnext(unpack(c));
    init1_setnext(init);
    last1_setnext(last);
}

REGISTER(P2, UnpackedMulResult) { P2.sign = false; P2.exp = 0; P2.frac_unnorm = 0; }
REGISTER(C2, UnpackedFP16) { C2.sign = false; C2.exp = 0; C2.frac = 0; }
REGISTER(init2, bool) { init2 = false; }
REGISTER(last2, bool) { last2 = false; }

TICK_IMPL() {
    // Stage 2: Multiplication
    UnpackedMulResult mul_result;
    mul_result.sign = A1_get().sign ^ B1_get().sign;
    mul_result.exp = A1_get().exp + B1_get().exp;
    mul_result.frac_unnorm = (A1_get().frac * B1_get().frac);

    P2_setnext(mul_result);
    C2_setnext(A1_get());
    init2_setnext(init1_get());
    last2_setnext(last1_get());
}

REGISTER(P3Fix, UInt<96>) { P3Fix = 0; }
REGISTER(C3Fix, UInt<96>) { C3Fix = 0; }
REGISTER(init3, bool) { init3 = false; }
REGISTER(last3, bool) { last3 = false; }

TICK_IMPL() {
    // Stage 3: transform to 96-bits 48-shifted fixed-point format
    uint8_t shiftP = (P2_get().exp + 48).get_u8();
    UInt<96> unsignedP = UInt<96>(P2_get().frac_unnorm) << shiftP;
    if (P2_get().sign) {
        P3Fix_setnext(~unsignedP + 1); // Two's complement for negative numbers
    } else {
        P3Fix_setnext(unsignedP);
    }

    uint8_t shiftC = (C2_get().exp + 48).get_u8();
    UInt<96> unsignedC = UInt<96>(C2_get().frac) << shiftC;
    if (C2_get().sign) {
        C3Fix_setnext(~unsignedC + 1); // Two's complement for negative numbers
    } else {
        C3Fix_setnext(unsignedC);
    }

    init3_setnext(init2_get());
    last3_setnext(last2_get());
}

REGISTER(accS, UInt<96>) { accS = 0; }
REGISTER(accC, UInt<96>) { accC = 0; }
REGISTER(last4, bool) { last4 = false; }

TICK_IMPL() {
    // Stage 4: Accumulation

    UInt<96> X = (init3_get() ? C3Fix_get() : accS_get());
    UInt<96> Y = (init3_get() ? 0 : accC_get());
    UInt<96> Z = P3Fix_get();

    UInt<96> sum_bits = X ^ Y ^ Z;
    UInt<96> carry_bits = ((X & Y) | (X & Z) | (Y & Z)) << 1;

    accS_setnext(sum_bits);
    accC_setnext(carry_bits);
    last4_setnext(last3_get());
}

REGISTER(valid5, bool) { valid5 = false; }
REGISTER(sum5seg1, UInt<32>) { sum5seg1 = 0; }
REGISTER(sum5seg2, UInt<32>) { sum5seg2 = 0; }
REGISTER(sum5seg2C, UInt<32>) { sum5seg2C = 0; }
REGISTER(sum5seg3, UInt<32>) { sum5seg3 = 0; }
REGISTER(sum5seg3C, UInt<32>) { sum5seg3C = 0; }
REGISTER(carry5seg1, bool) { carry5seg1 = false; }
REGISTER(carry5seg2, bool) { carry5seg2 = false; }
REGISTER(carry5seg2C, bool) { carry5seg2C = false; }
REGISTER(carry5seg3, bool) { carry5seg3 = false; }
REGISTER(carry5seg3C, bool) { carry5seg3C = false; }

TICK_IMPL() {
    // Stage 5: Carry-Select Adder for final sum: Stage 1
    valid5_setnext(last4_get());
    if (last4_get()) {
        UInt<33> s1 = accS_get()(31, 0);
        UInt<33> c1 = accC_get()(31, 0);
        UInt<33> s2 = accS_get()(63, 32);
        UInt<33> c2 = accC_get()(63, 32);
        UInt<33> s3 = accS_get()(95, 64);
        UInt<33> c3 = accC_get()(95, 64);

        UInt<33> res1 = s1 + c1;
        UInt<33> res2 = s2 + c2;
        UInt<33> res2C = s2 + c2 + UInt<33>(1);
        UInt<33> res3 = s3 + c3;
        UInt<33> res3C = s3 + c3 + UInt<33>(1);

        sum5seg1_setnext(res1(31, 0));
        sum5seg2_setnext(res2(31, 0));
        sum5seg2C_setnext(res2C(31, 0));
        sum5seg3_setnext(res3(31, 0));
        sum5seg3C_setnext(res3C(31, 0));

        carry5seg1_setnext(res1(32));
        carry5seg2_setnext(res2(32));
        carry5seg2C_setnext(res2C(32));
        carry5seg3_setnext(res3(32));
        carry5seg3C_setnext(res3C(32));
    }
}

REGISTER(valid6, bool) { valid6 = false; }
REGISTER(absVal6, UInt<96>) { absVal6 = 0; }
REGISTER(sign6, bool) { sign6 = false; }

TICK_IMPL() {
    // Stage 6: Carry-Select Adder for final sum: Stage 2
    valid6_setnext(valid5_get());
    if (valid5_get()) {
        UInt<96> sum;
        bool seg1_carry = carry5seg1_get();
        bool seg2_carry = (seg1_carry ? carry5seg2C_get() : carry5seg2_get());
        sum(31, 0) = sum5seg1_get();
        sum(63, 32) = (seg1_carry ? sum5seg2C_get() : sum5seg2_get());
        sum(95, 64) = (seg2_carry ? sum5seg3C_get() : sum5seg3_get());
        if (sum(95)) {
            absVal6_setnext(~sum + 1);
            sign6_setnext(true);
        } else {
            absVal6_setnext(sum);
            sign6_setnext(false);
        }
    }
}

REGISTER(valid7, bool) { valid7 = false; }
REGISTER(sign7, bool) { sign7 = false; }
REGISTER(iszero7, bool) { iszero7 = false; }
REGISTER(mag7, UInt<96>) { mag7 = 0; }
REGISTER(msbpos7, UInt<7>) { msbpos7 = 0; }
REGISTER(expUnbias7, UInt<8>) { expUnbias7 = 0; }
REGISTER(expField7, UInt<8>) { expField7 = 0; }

TICK_IMPL() {
    // Stage 7: leading-one detection and exponent calculation
    valid7_setnext(valid6_get());
    if (valid6_get()) {
        sign7_setnext(sign6_get());
        iszero7_setnext(absVal6_get() == 0);
        mag7_setnext(absVal6_get());

        uint8_t msb_pos = 0;
        if (absVal6_get() != 0) {
            for (int i = 95; i >= 0; --i) {
                if (absVal6_get()(i)) {
                    msb_pos = i;
                    break;
                }
            }
        }

        msbpos7_setnext(msb_pos);
        expUnbias7_setnext(msb_pos - 48);
        expField7_setnext(msb_pos + 79);
    }
}

REGISTER(valid8, bool) { valid8 = false; }
REGISTER(sign8, bool) { sign8 = false; }
REGISTER(iszero8, bool) { iszero8 = false; }
REGISTER(expField8, UInt<8>) { expField8 = 0; }
REGISTER(mant8, UInt<24>) { mant8 = 0; }
REGISTER(guard8, bool) { guard8 = false; }
REGISTER(round8, bool) { round8 = false; }
REGISTER(sticky8, bool) { sticky8 = false; }

TICK_IMPL() {
    // Stage 8: Rounding
    valid8_setnext(valid7_get());
    if (valid7_get()) {
        sign8_setnext(sign7_get());
        iszero8_setnext(iszero7_get());
        expField8_setnext(expField7_get());
        
        if (iszero7_get()) {
            expField8_setnext(0);
            mant8_setnext(0);
            guard8_setnext(false);
            round8_setnext(false);
            sticky8_setnext(false);
        } else if (msbpos7_get() >= 23) {
            UInt<7> shift = msbpos7_get() - UInt<7>(23);
            UInt<96> shifted_mag = mag7_get() >> shift.get_u16();
            mant8_setnext(shifted_mag(22, 0));
            if (shift == 0) {
                guard8_setnext(false);
                round8_setnext(false);
                sticky8_setnext(false);
            } else if (shift == 1) {
                guard8_setnext(shifted_mag(0));
                round8_setnext(false);
                sticky8_setnext(false);
            } else if (shift == 2) {
                guard8_setnext(shifted_mag(1));
                round8_setnext(shifted_mag(0));
                sticky8_setnext(false);
            } else {
                guard8_setnext(shifted_mag((shift - 1).get_u16()));
                round8_setnext(shifted_mag((shift - 2).get_u16()));
                sticky8_setnext(mag7_get()((shift - 3).get_u16(), 0) != UInt<96>(0));
            }
        } else {
            UInt<7> shift = UInt<7>(23) - msbpos7_get();
            UInt<96> shifted_mag = mag7_get() << shift.get_u16();
            mant8_setnext(shifted_mag(22, 0));
            guard8_setnext(false);
            round8_setnext(false);
            sticky8_setnext(false);
        }

    }
}

TICK_IMPL() {
    // Stage 9: Output formatting
    if (valid8_get()) {
        if (iszero8_get()) {
            output(UInt<32>(0));
        } else {
            bool round_up = guard8_get() && (round8_get() || sticky8_get() || (mant8_get()(0)));
            UInt<25> mant25 = (UInt<25>(mant8_get()) + (round_up ? 1 : 0));
            UInt<8> exp = expField8_get();
            if (mant25(24)) {
                // Rounding caused overflow in mantissa
                mant25 = mant25 >> 1;
                if (exp == 255) {
                    exp = 255;
                    mant25 = 0;
                } else {
                    exp = exp + 1;
                }
            }
            output((UInt<32>(sign8_get()) << 31) | (UInt<32>(exp) << 23) | UInt<32>(mant25(22, 0)));
        }
    }
}




