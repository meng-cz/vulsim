

#pragma once

#include <defhelper.hpp>

#include "header.hpp"

STRUCT(UnpackedFP16) {
    bool sign;
    Int<8> exp;
    Int<11> frac;
};

STRUCT(UnpackedMulResult) {
    bool sign;
    Int<8> exp;
    Int<22> frac_unnorm;
};

REQUEST(output, ARG(Int<32>) sum);

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
SERVICE(input, ARG(Int<16>) a, ARG(Int<16>) b, ARG(Int<16>) c, ARG(bool) init, ARG(bool) last) {
    // Stage 1: Unpacking
    auto unpack = [](const Int<16>& input) -> UnpackedFP16 {
        UnpackedFP16 result;
        result.sign = input(15);
        result.exp = input(14, 10);
        if (result.exp == 0) {
            // Denormalized number
            result.frac = input(9, 0);
            result.exp = -24;
        } else {
            // Normalized number, add implicit leading 1
            result.frac = (Int<11>(1) << 10) | input(9, 0);
            result.exp = result.exp - 25;
        }
        return result;
    };

    A1.setnext(unpack(a));
    B1.setnext(unpack(b));
    C1.setnext(unpack(c));
    init1.setnext(init);
    last1.setnext(last);
}

REGISTER(P2, UnpackedMulResult) { P2.sign = false; P2.exp = 0; P2.frac_unnorm = 0; }
REGISTER(C2, UnpackedFP16) { C2.sign = false; C2.exp = 0; C2.frac = 0; }
REGISTER(init2, bool) { init2 = false; }
REGISTER(last2, bool) { last2 = false; }

TICK_IMPL() {
    // Stage 2: Multiplication
    UnpackedMulResult mul_result;
    mul_result.sign = A1.get().sign ^ B1.get().sign;
    mul_result.exp = A1.get().exp + B1.get().exp;
    mul_result.frac_unnorm = (A1.get().frac * B1.get().frac);

    P2.setnext(mul_result);
    C2.setnext(A1.get());
    init2.setnext(init1.get());
    last2.setnext(last1.get());
}

REGISTER(P3Fix, Int<96>) { P3Fix = 0; }
REGISTER(C3Fix, Int<96>) { C3Fix = 0; }
REGISTER(init3, bool) { init3 = false; }
REGISTER(last3, bool) { last3 = false; }

TICK_IMPL() {
    // Stage 3: transform to 96-bits 48-shifted fixed-point format
    uint8_t shiftP = (P2.get().exp + 48).to<uint8_t>();
    Int<96> unsignedP = Int<96>(P2.get().frac_unnorm) << shiftP;
    if (P2.get().sign) {
        P3Fix.setnext(~unsignedP + 1); // Two's complement for negative numbers
    } else {
        P3Fix.setnext(unsignedP);
    }

    uint8_t shiftC = (C2.get().exp + 48).to<uint8_t>();
    Int<96> unsignedC = Int<96>(C2.get().frac) << shiftC;
    if (C2.get().sign) {
        C3Fix.setnext(~unsignedC + 1); // Two's complement for negative numbers
    } else {
        C3Fix.setnext(unsignedC);
    }

    init3.setnext(init2.get());
    last3.setnext(last2.get());
}

REGISTER(accS, Int<96>) { accS = 0; }
REGISTER(accC, Int<96>) { accC = 0; }
REGISTER(last4, bool) { last4 = false; }

TICK_IMPL() {
    // Stage 4: Accumulation

    Int<96> X = (init3.get() ? C3Fix.get() : accS.get());
    Int<96> Y = (init3.get() ? 0 : accC.get());
    Int<96> Z = P3Fix.get();

    Int<96> sum_bits = X ^ Y ^ Z;
    Int<96> carry_bits = ((X & Y) | (X & Z) | (Y & Z)) << 1;

    accS.setnext(sum_bits);
    accC.setnext(carry_bits);
    last4.setnext(last3.get());
}

REGISTER(valid5, bool) { valid5 = false; }
REGISTER(sum5seg1, Int<32>) { sum5seg1 = 0; }
REGISTER(sum5seg2, Int<32>) { sum5seg2 = 0; }
REGISTER(sum5seg2C, Int<32>) { sum5seg2C = 0; }
REGISTER(sum5seg3, Int<32>) { sum5seg3 = 0; }
REGISTER(sum5seg3C, Int<32>) { sum5seg3C = 0; }
REGISTER(carry5seg1, bool) { carry5seg1 = false; }
REGISTER(carry5seg2, bool) { carry5seg2 = false; }
REGISTER(carry5seg2C, bool) { carry5seg2C = false; }
REGISTER(carry5seg3, bool) { carry5seg3 = false; }
REGISTER(carry5seg3C, bool) { carry5seg3C = false; }

TICK_IMPL() {
    // Stage 5: Carry-Select Adder for final sum: Stage 1
    valid5.setnext(last4.get());
    if (last4.get()) {
        Int<33> s1 = accS.get()(31, 0);
        Int<33> c1 = accC.get()(31, 0);
        Int<33> s2 = accS.get()(63, 32);
        Int<33> c2 = accC.get()(63, 32);
        Int<33> s3 = accS.get()(95, 64);
        Int<33> c3 = accC.get()(95, 64);

        Int<33> res1 = s1 + c1;
        Int<33> res2 = s2 + c2;
        Int<33> res2C = s2 + c2 + Int<33>(1);
        Int<33> res3 = s3 + c3;
        Int<33> res3C = s3 + c3 + Int<33>(1);

        sum5seg1.setnext(res1(31, 0));
        sum5seg2.setnext(res2(31, 0));
        sum5seg2C.setnext(res2C(31, 0));
        sum5seg3.setnext(res3(31, 0));
        sum5seg3C.setnext(res3C(31, 0));

        carry5seg1.setnext(res1(32));
        carry5seg2.setnext(res2(32));
        carry5seg2C.setnext(res2C(32));
        carry5seg3.setnext(res3(32));
        carry5seg3C.setnext(res3C(32));
    }
}

REGISTER(valid6, bool) { valid6 = false; }
REGISTER(absVal6, Int<96>) { absVal6 = 0; }
REGISTER(sign6, bool) { sign6 = false; }

TICK_IMPL() {
    // Stage 6: Carry-Select Adder for final sum: Stage 2
    valid6.setnext(valid5.get());
    if (valid5.get()) {
        Int<96> sum;
        bool seg1_carry = carry5seg1.get();
        bool seg2_carry = (seg1_carry ? carry5seg2C.get() : carry5seg2.get());
        sum(31, 0) = sum5seg1.get();
        sum(63, 32) = (seg1_carry ? sum5seg2C.get() : sum5seg2.get());
        sum(95, 64) = (seg2_carry ? sum5seg3C.get() : sum5seg3.get());
        if (sum(95)) {
            absVal6.setnext(~sum + 1);
            sign6.setnext(true);
        } else {
            absVal6.setnext(sum);
            sign6.setnext(false);
        }
    }
}

REGISTER(valid7, bool) { valid7 = false; }
REGISTER(sign7, bool) { sign7 = false; }
REGISTER(iszero7, bool) { iszero7 = false; }
REGISTER(mag7, Int<96>) { mag7 = 0; }
REGISTER(msbpos7, Int<7>) { msbpos7 = 0; }
REGISTER(expUnbias7, Int<8>) { expUnbias7 = 0; }
REGISTER(expField7, Int<8>) { expField7 = 0; }

TICK_IMPL() {
    // Stage 7: leading-one detection and exponent calculation
    valid7.setnext(valid6.get());
    if (valid6.get()) {
        sign7.setnext(sign6.get());
        iszero7.setnext(absVal6.get() == 0);
        mag7.setnext(absVal6.get());

        uint8_t msb_pos = 0;
        if (absVal6.get() != 0) {
            for (int i = 95; i >= 0; --i) {
                if (absVal6.get()(i)) {
                    msb_pos = i;
                    break;
                }
            }
        }

        msbpos7.setnext(msb_pos);
        expUnbias7.setnext(msb_pos - 48);
        expField7.setnext(msb_pos + 79);
    }
}

REGISTER(valid8, bool) { valid8 = false; }
REGISTER(sign8, bool) { sign8 = false; }
REGISTER(iszero8, bool) { iszero8 = false; }
REGISTER(expField8, Int<8>) { expField8 = 0; }
REGISTER(mant8, Int<24>) { mant8 = 0; }
REGISTER(guard8, bool) { guard8 = false; }
REGISTER(round8, bool) { round8 = false; }
REGISTER(sticky8, bool) { sticky8 = false; }

TICK_IMPL() {
    // Stage 8: Rounding
    valid8.setnext(valid7.get());
    if (valid7.get()) {
        sign8.setnext(sign7.get());
        iszero8.setnext(iszero7.get());
        expField8.setnext(expField7.get());
        
        if (iszero7.get()) {
            expField8.setnext(0);
            mant8.setnext(0);
            guard8.setnext(false);
            round8.setnext(false);
            sticky8.setnext(false);
        } else if (msbpos7.get() >= 23) {
            Int<7> shift = msbpos7.get() - Int<7>(23);
            Int<96> shifted_mag = mag7.get() >> shift.to<uint16_t>();
            mant8.setnext(shifted_mag(22, 0));
            if (shift == 0) {
                guard8.setnext(false);
                round8.setnext(false);
                sticky8.setnext(false);
            } else if (shift == 1) {
                guard8.setnext(shifted_mag(0));
                round8.setnext(false);
                sticky8.setnext(false);
            } else if (shift == 2) {
                guard8.setnext(shifted_mag(1));
                round8.setnext(shifted_mag(0));
                sticky8.setnext(false);
            } else {
                guard8.setnext(shifted_mag((shift - 1).to<uint16_t>()));
                round8.setnext(shifted_mag((shift - 2).to<uint16_t>()));
                sticky8.setnext(Int<96>(mag7.get()((shift - 3).to<uint16_t>(), 0)) != Int<96>(0));
            }
        } else {
            Int<7> shift = Int<7>(23) - msbpos7.get();
            Int<96> shifted_mag = mag7.get() << shift.to<uint16_t>();
            mant8.setnext(shifted_mag(22, 0));
            guard8.setnext(false);
            round8.setnext(false);
            sticky8.setnext(false);
        }

    }
}

TICK_IMPL() {
    // Stage 9: Output formatting
    if (valid8.get()) {
        if (iszero8.get()) {
            output(Int<32>(0));
        } else {
            bool round_up = guard8.get() && (round8.get() || sticky8.get() || (mant8.get()(0)));
            Int<25> mant25 = (Int<25>(mant8.get()) + (round_up ? 1 : 0));
            Int<8> exp = expField8.get();
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
            output((Int<32>(sign8.get()) << 31) | (Int<32>(exp) << 23) | Int<32>(mant25(22, 0)));
        }
    }
}



