
#include "common.h"
#include "global.h"
#include "bundles.h"

void RV64ALUFPU_alu64(uint32 funct73, uint64 s1, uint64 s2, uint64 * result, bool * invalidop) {

    uint64 ret = 0;
    *invalidop = false;
    const uint32    ADD     = 0x000;
    const uint32    SUB     = 0x100;
    const uint32    SLL     = 0x001;
    const uint32    SLT     = 0x002;
    const uint32    SLTU    = 0x003;
    const uint32    XOR     = 0x004;
    const uint32    SRL     = 0x005;
    const uint32    SRA     = 0x105;
    const uint32    OR      = 0x006;
    const uint32    AND     = 0x007;
    const uint32    MUL     = 0x008;
    const uint32    MULH    = 0x009;
    const uint32    MULHSU  = 0x00a;
    const uint32    MULHU   = 0x00b;
    const uint32    DIV     = 0x00c;
    const uint32    DIVU    = 0x00d;
    const uint32    REM     = 0x00e;
    const uint32    REMU    = 0x00f;

    if((funct73 == DIV || funct73 == DIVU || funct73 == REM || funct73 == REMU) && (s2 == 0)) {
        *invalidop = true;
        return;
    }

    switch (funct73)
    {
    case ADD : RAW_DATA_AS(ret).i64 = (RAW_DATA_AS(s1).i64) + (RAW_DATA_AS(s2).i64); break;
    case SUB : RAW_DATA_AS(ret).i64 = (RAW_DATA_AS(s1).i64) - (RAW_DATA_AS(s2).i64); break;
    case SLL : RAW_DATA_AS(ret).i64 = (RAW_DATA_AS(s1).i64) << (RAW_DATA_AS(s2).u64 & 0x3f); break;
    case SLT : RAW_DATA_AS(ret).i64 = (((RAW_DATA_AS(s1).i64) < (RAW_DATA_AS(s2).i64))?1:0); break;
    case SLTU: RAW_DATA_AS(ret).i64 = (((RAW_DATA_AS(s1).u64) < (RAW_DATA_AS(s2).u64))?1:0); break;
    case XOR : RAW_DATA_AS(ret).i64 = (RAW_DATA_AS(s1).i64) ^ (RAW_DATA_AS(s2).i64); break;
    case SRL : RAW_DATA_AS(ret).u64 = (RAW_DATA_AS(s1).u64) >> (RAW_DATA_AS(s2).u64 & 0x3f); break;
    case SRA : RAW_DATA_AS(ret).i64 = (RAW_DATA_AS(s1).i64) >> (RAW_DATA_AS(s2).u64 & 0x3f); break;
    case OR  : RAW_DATA_AS(ret).i64 = (RAW_DATA_AS(s1).i64) | (RAW_DATA_AS(s2).i64); break;
    case AND : RAW_DATA_AS(ret).i64 = (RAW_DATA_AS(s1).i64) & (RAW_DATA_AS(s2).i64); break;
    case MUL : RAW_DATA_AS(ret).i64 = (RAW_DATA_AS(s1).i64) * (RAW_DATA_AS(s2).i64); break;
    case MULH: RAW_DATA_AS(ret).i64 = ((((int128)(RAW_DATA_AS(s1).i64)) * (int128)(RAW_DATA_AS(s2).i64)) >> 64); break;
    case MULHSU: RAW_DATA_AS(ret).i64 = ((((int128)(RAW_DATA_AS(s1).i64)) * (uint128)(RAW_DATA_AS(s2).u64)) >> 64); break;
    case MULHU: RAW_DATA_AS(ret).i64 = ((((uint128)(RAW_DATA_AS(s1).u64)) * (uint128)(RAW_DATA_AS(s2).u64)) >> 64); break;
    case DIV : RAW_DATA_AS(ret).i64 = (RAW_DATA_AS(s1).i64) / (RAW_DATA_AS(s2).i64); break;
    case DIVU: RAW_DATA_AS(ret).u64 = (RAW_DATA_AS(s1).u64) / (RAW_DATA_AS(s2).u64); break;
    case REM : RAW_DATA_AS(ret).i64 = (RAW_DATA_AS(s1).i64) % (RAW_DATA_AS(s2).i64); break;
    case REMU: RAW_DATA_AS(ret).u64 = (RAW_DATA_AS(s1).u64) % (RAW_DATA_AS(s2).u64); break;
    default: *invalidop = true;
    }
    *result = ret;
}

void RV64ALUFPU_alu32(uint32 funct73, uint64 s1, uint64 s2, uint64 * result, bool * invalidop) {

    uint64 ret = 0;
    *invalidop = false;
    const uint32    ADD     = 0x000;
    const uint32    SUB     = 0x100;
    const uint32    SLL     = 0x001;
    const uint32    SRL     = 0x005;
    const uint32    SRA     = 0x105;
    const uint32    MUL     = 0x008;
    const uint32    DIV     = 0x00c;
    const uint32    DIVU    = 0x00d;
    const uint32    REM     = 0x00e;
    const uint32    REMU    = 0x00f;

    if((funct73 == DIV || funct73 == DIVU || funct73 == REM || funct73 == REMU) && (s2 == 0)) {
        *invalidop = true;
        return;
    }

    switch (funct73)
    {
    case ADD : RAW_DATA_AS(ret).i64 = (RAW_DATA_AS(s1).i32) + (RAW_DATA_AS(s2).i32); break;
    case SUB : RAW_DATA_AS(ret).i64 = (RAW_DATA_AS(s1).i32) - (RAW_DATA_AS(s2).i32); break;
    case SLL : RAW_DATA_AS(ret).i64 = (RAW_DATA_AS(s1).i32) << (RAW_DATA_AS(s2).u32 & 0x1f); break;
    case SRL : RAW_DATA_AS(ret).u64 = (RAW_DATA_AS(s1).u32) >> (RAW_DATA_AS(s2).u32 & 0x1f); if(RAW_DATA_AS(ret).u64 >> 31) RAW_DATA_AS(ret).u64 |= 0xffffffff00000000UL; break;
    case SRA : RAW_DATA_AS(ret).i64 = (RAW_DATA_AS(s1).i32) >> (RAW_DATA_AS(s2).u32 & 0x1f); break;
    case MUL : RAW_DATA_AS(ret).i64 = (RAW_DATA_AS(s1).i32) * (RAW_DATA_AS(s2).i32); break;
    case DIV : RAW_DATA_AS(ret).i64 = (RAW_DATA_AS(s1).i32) / (RAW_DATA_AS(s2).i32); break;
    case DIVU: RAW_DATA_AS(ret).u64 = (RAW_DATA_AS(s1).u32) / (RAW_DATA_AS(s2).u32); break;
    case REM : RAW_DATA_AS(ret).i64 = (RAW_DATA_AS(s1).i32) % (RAW_DATA_AS(s2).i32); break;
    case REMU: RAW_DATA_AS(ret).u64 = (RAW_DATA_AS(s1).u32) % (RAW_DATA_AS(s2).u32); break;
    default: *invalidop = true;
    }
    *result = ret;
}

void RV64ALUFPU_branch(uint8 funct3, uint64 s1, uint64 s2, bool * result, bool * invalidop) {
    
    bool res = false;
    *invalidop = false;

    const uint32    BEQ     = 0;
    const uint32    BNE     = 1;
    const uint32    BLT     = 4;
    const uint32    BGE     = 5;
    const uint32    BLTU    = 6;
    const uint32    BGEU    = 7;

    switch (funct3)
    {
    case BEQ : res = (RAW_DATA_AS(s1).u64 == RAW_DATA_AS(s2).u64); break;
    case BNE : res = (RAW_DATA_AS(s1).u64 != RAW_DATA_AS(s2).u64); break;
    case BLT : res = (RAW_DATA_AS(s1).i64 < RAW_DATA_AS(s2).i64); break;
    case BGE : res = (RAW_DATA_AS(s1).i64 >= RAW_DATA_AS(s2).i64); break;
    case BLTU: res = (RAW_DATA_AS(s1).u64 < RAW_DATA_AS(s2).u64); break;
    case BGEU: res = (RAW_DATA_AS(s1).u64 >= RAW_DATA_AS(s2).u64); break;
    default: *invalidop = true;
    }

    *result = res;
}

void RV64ALUFPU_fpu64(uint32 funct5_rs2_funct3, uint64 s1, uint64 s2, uint64 * result, bool * invalidop, uint64 * fcsr_update) {
    uint64 ret = 0;
    *invalidop = false;
    *fcsr_update = 0;

    feclearexcept(FE_ALL_EXCEPT);

    uint32 op = (funct5_rs2_funct3 >> 8);
    uint32 rs2 = ((funct5_rs2_funct3 >> 3) & 31);
    uint32 funct3 = (funct5_rs2_funct3  & 7);

    const uint32 ADD     = 0x00;
    const uint32 SUB     = 0x01;
    const uint32 MUL     = 0x02;
    const uint32 DIV     = 0x03;
    const uint32 SQRT    = 0x0b;
    const uint32 SGNJ    = 0x04;
    const uint32 MIN     = 0x05;
    const uint32 MVF2F   = 0x08;
    const uint32 CMP     = 0x14;
    const uint32 CVTF2I  = 0x18;
    const uint32 CVTI2F  = 0x1a;
    const uint32 MVF2I   = 0x1c;
    const uint32 MVI2F   = 0x1e;

    auto _check_canonical_nan_64 = [](uint64 *out) {
        double f = RAW_DATA_AS(*out).f64;
        if(isnan(f)) {
            RAW_DATA_AS(*out).u64 = 0x7ff8000000000000UL;
        }
    };

    if(op == ADD) {
        RAW_DATA_AS(ret).f64 = RAW_DATA_AS(s1).f64 + RAW_DATA_AS(s2).f64;
        _check_canonical_nan_64(&ret);
    } else if(op == SUB) {
        RAW_DATA_AS(ret).f64 = RAW_DATA_AS(s1).f64 - RAW_DATA_AS(s2).f64;
        _check_canonical_nan_64(&ret);
    } else if(op == MUL) {
        RAW_DATA_AS(ret).f64 = RAW_DATA_AS(s1).f64 * RAW_DATA_AS(s2).f64;
        _check_canonical_nan_64(&ret);
    } else if(op == DIV) {
        RAW_DATA_AS(ret).f64 = RAW_DATA_AS(s1).f64 / RAW_DATA_AS(s2).f64;
        _check_canonical_nan_64(&ret);
    } else if(op == SQRT) {
        if(RAW_DATA_AS(s1).f64 < 0) {
            RAW_DATA_AS(ret).u64 = 0x7ff8000000000000UL;
            *fcsr_update |= RV_FFLAGS_INVALID;
        }
        else {
            RAW_DATA_AS(ret).f64 = sqrt(RAW_DATA_AS(s1).f64);
        }
        _check_canonical_nan_64(&ret);
    } else if(op == SGNJ) {
        uint64_t sbit1 = (s1 >> 63U);
        uint64_t sbit2 = (s2 >> 63U);
        switch (funct3)
        {
        case 0: RAW_DATA_AS(ret).f64 = ((sbit1 == sbit2)?(RAW_DATA_AS(s1).f64):(-RAW_DATA_AS(s1).f64)); break;
        case 1: RAW_DATA_AS(ret).f64 = ((sbit1 != sbit2)?(RAW_DATA_AS(s1).f64):(-RAW_DATA_AS(s1).f64)); break;
        case 2: RAW_DATA_AS(ret).f64 = ((sbit1 == sbit2)?(std::abs(RAW_DATA_AS(s1).f64)):(-std::abs(RAW_DATA_AS(s1).f64))); break;
        default: *invalidop = true;
        }
        _check_canonical_nan_64(&ret);
    } else if(op == MIN) {
        if(isnan(RAW_DATA_AS(s1).f64)) {
            ret = s2;
        } else if(isnan(RAW_DATA_AS(s2).f64)) {
            ret = s1;
        } else {
            switch (funct3)
            {
            case 0: RAW_DATA_AS(ret).f64 = std::min<double>(RAW_DATA_AS(s1).f64, RAW_DATA_AS(s2).f64); break;
            case 1: RAW_DATA_AS(ret).f64 = std::max<double>(RAW_DATA_AS(s1).f64, RAW_DATA_AS(s2).f64); break;
            default: *invalidop = true;
            }
        }
        _check_canonical_nan_64(&ret);
    } else if(op == MVF2F) {
        if(rs2 == 0) {
            RAW_DATA_AS(ret).f64 = RAW_DATA_AS(s1).f32; 
        } else if(rs2 == 1) {
            ret = s1;
        } else {
            *invalidop = true;
        }
    } else if(op == CVTF2I) {
        if(((s1 & 0x7ff0000000000000UL) == 0x7ff0000000000000UL) && (s1 & 0x000fffffffffffffUL)) {
            // NaN, -NaN
            switch (rs2)
            {
            case 0: ret = 0x000000007fffffffUL; break;
            case 1: ret = 0xffffffffffffffffUL; break;
            case 2: ret = 0x7fffffffffffffffUL; break;
            case 3: ret = 0xffffffffffffffffUL; break;
            default: *invalidop = true;
            }
            *fcsr_update |= RV_FFLAGS_INVALID;
        }
        else if(((s1 & 0xfff0000000000000UL) == 0xfff0000000000000UL) && !(s1 & 0x000fffffffffffffUL)) {
            // -inf
            switch (rs2)
            {
            case 0: ret = 0xffffffff80000000UL; break;
            case 1: ret = 0UL; break;
            case 2: ret = 0x8000000000000000UL; break;
            case 3: ret = 0UL; break;
            default: *invalidop = true;
            }
            *fcsr_update |= RV_FFLAGS_INVALID;
        }
        else if(((s1 & 0xfff0000000000000UL) == 0x7ff0000000000000UL) && !(s1 & 0x000fffffffffffffUL)) {
            // inf
            switch (rs2)
            {
            case 0: ret = 0x000000007fffffffUL; break;
            case 1: ret = 0xffffffffffffffffUL; break;
            case 2: ret = 0x7fffffffffffffffUL; break;
            case 3: ret = 0xffffffffffffffffUL; break;
            default: *invalidop = true;
            }
            *fcsr_update |= RV_FFLAGS_INVALID;
        } else {
            double v = RAW_DATA_AS(s1).f64;
            switch (funct3)
            {
            case 0: v += 0.5; break; // RNE
            case 1: v = ((v>0)?floor(v):ceil(v)); break; // RTZ
            case 2: v = floor(v); break; // RDN
            case 3: v = ceil(v); break; // RUP
            default: *invalidop = true;
            }
            if(rs2 == 0) {
                if(v < -pow(2., 31.)) {
                    RAW_DATA_AS(ret).u64 = 0xffffffff80000000UL;
                    *fcsr_update |= RV_FFLAGS_INVALID;
                }
                else if(v > pow(2., 31.)-1.) {
                    RAW_DATA_AS(ret).u64 = 0x000000007fffffffUL;
                    *fcsr_update |= RV_FFLAGS_INVALID;
                }
                else {
                    RAW_DATA_AS(ret).i64 = (int32_t)v;
                }
            } else if(rs2 == 1) {
                if(v < 0) {
                    RAW_DATA_AS(ret).u64 = 0x0000000000000000UL;
                    *fcsr_update |= RV_FFLAGS_INVALID;
                }
                else if(v > pow(2., 32.)-1.) {
                    RAW_DATA_AS(ret).u64 = 0x00000000ffffffffUL;
                    *fcsr_update |= RV_FFLAGS_INVALID;
                }
                else {
                    RAW_DATA_AS(ret).u64 = (uint32_t)v;
                }
            } else if(rs2 == 2) {
                if(v < -pow(2., 63.)) {
                    RAW_DATA_AS(ret).u64 = 0x8000000000000000UL;
                    *fcsr_update |= RV_FFLAGS_INVALID;
                }
                else if(v > pow(2., 63.)-1.) {
                    RAW_DATA_AS(ret).u64 = 0x7fffffffffffffffUL;
                    *fcsr_update |= RV_FFLAGS_INVALID;
                }
                else {
                    RAW_DATA_AS(ret).i64 = (int64_t)v;
                }
            } else if(rs2 == 3) {
                if(v < 0) {
                    RAW_DATA_AS(ret).u64 = 0x0000000000000000UL;
                    *fcsr_update |= RV_FFLAGS_INVALID;
                }
                else if(v > pow(2., 64.)-1.) {
                    RAW_DATA_AS(ret).u64 = 0xffffffffffffffffUL;
                    *fcsr_update |= RV_FFLAGS_INVALID;
                }
                else {
                    RAW_DATA_AS(ret).u64 = (uint64_t)v;
                }
            } else {
                *invalidop = true;
            }
        }
    } else if(op == CVTI2F) {
        switch (rs2)
        {
        case 0: RAW_DATA_AS(ret).f64 = RAW_DATA_AS(s1).i32; break;
        case 1: RAW_DATA_AS(ret).f64 = RAW_DATA_AS(s1).u32; break;
        case 2: RAW_DATA_AS(ret).f64 = RAW_DATA_AS(s1).i64; break;
        case 3: RAW_DATA_AS(ret).f64 = RAW_DATA_AS(s1).u64; break;
        default: *invalidop = true;
        }
        _check_canonical_nan_64(&ret);
    } else if(op == MVF2I) {
        if(funct3 == 0) {
            // RAW
            RAW_DATA_AS(ret).u64 = RAW_DATA_AS(s1).u64;
        } else if(funct3 == 1) {
            // FCLASS
            if(s1 == 0xfff0000000000000UL) {
                ret |= (1<<0);
            }
            else if(s1 == 0x8000000000000000UL) {
                ret |= (1<<3);
            }
            else if(s1 == 0UL) {
                ret |= (1<<4);
            }
            else if(s1 == 0x7ff0000000000000UL) {
                ret |= (1<<7);
            }
            else if((s1 & 0x7ff8000000000000UL) == 0x7ff8000000000000UL) {
                ret |= (1<<9);
            }
            else if((s1 & 0x7ff8000000000000UL) == 0x7ff0000000000000UL) {
                ret |= (1<<8);
            }
            else if(isnormal(RAW_DATA_AS(s1).f64)) {
                if(RAW_DATA_AS(s1).f64 > 0) ret |= (1<<6);
                else ret |= (1<<1);
            }
            else {
                if(RAW_DATA_AS(s1).f64 > 0) ret |= (1<<5);
                else ret |= (1<<2);
            }
        } else {
            *invalidop = true;
        }
    } else if(op == MVI2F) {
        RAW_DATA_AS(ret).u64 = RAW_DATA_AS(s1).u64;
        _check_canonical_nan_64(&ret);
    } else if(op == CMP) {
        switch (funct3)
        {
        case 0: ret = ((RAW_DATA_AS(s1).f64 <= RAW_DATA_AS(s2).f64)?1:0); break; // FLE
        case 1: ret = ((RAW_DATA_AS(s1).f64 < RAW_DATA_AS(s2).f64)?1:0); break; // FLT
        case 2: ret = ((RAW_DATA_AS(s1).f64 == RAW_DATA_AS(s2).f64)?1:0); break; // FEQ
        default: *invalidop = true;
        }
    } else {
        *invalidop = true;
    }
    uint32_t flg = fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT);
    if(flg & FE_INEXACT) *fcsr_update |= RV_FFLAGS_INEXACT;
    if(flg & FE_UNDERFLOW) *fcsr_update |= RV_FFLAGS_UNDERFLOW;
    if(flg & FE_OVERFLOW) *fcsr_update |= RV_FFLAGS_OVERFLOW;
    if(flg & FE_DIVBYZERO) *fcsr_update |= RV_FFLAGS_DIVBYZERO;
    if(flg & FE_INVALID) *fcsr_update |= RV_FFLAGS_INVALID;
}


void RV64ALUFPU_fpu32(uint32 funct5_rs2_funct3, uint64 s1, uint64 s2, uint64 * result, bool * invalidop, uint64 * fcsr_update) {
    uint64 ret = 0;
    *invalidop = false;
    *fcsr_update = 0;

    feclearexcept(FE_ALL_EXCEPT);

    uint32 op = (funct5_rs2_funct3 >> 8);
    uint32 rs2 = ((funct5_rs2_funct3 >> 3) & 31);
    uint32 funct3 = (funct5_rs2_funct3  & 7);

    const uint32 ADD     = 0x00;
    const uint32 SUB     = 0x01;
    const uint32 MUL     = 0x02;
    const uint32 DIV     = 0x03;
    const uint32 SQRT    = 0x0b;
    const uint32 SGNJ    = 0x04;
    const uint32 MIN     = 0x05;
    const uint32 MVF2F   = 0x08;
    const uint32 CMP     = 0x14;
    const uint32 CVTF2I  = 0x18;
    const uint32 CVTI2F  = 0x1a;
    const uint32 MVF2I   = 0x1c;
    const uint32 MVI2F   = 0x1e;

    auto _check_canonical_nan_32 = [](uint64 *out) {
        double f = RAW_DATA_AS(*out).f32;
        if(isnan(f)) {
            RAW_DATA_AS(*out).u32 = 0x7fc00000UL;
        }
    };

    if(op == ADD) {
        RAW_DATA_AS(ret).f32 = RAW_DATA_AS(s1).f32 + RAW_DATA_AS(s2).f32;
        _check_canonical_nan_32(&ret);
    } else if(op == SUB) {
        RAW_DATA_AS(ret).f32 = RAW_DATA_AS(s1).f32 - RAW_DATA_AS(s2).f32;
        _check_canonical_nan_32(&ret);
    } else if(op == MUL) {
        RAW_DATA_AS(ret).f32 = RAW_DATA_AS(s1).f32 * RAW_DATA_AS(s2).f32;
        _check_canonical_nan_32(&ret);
    } else if(op == DIV) {
        RAW_DATA_AS(ret).f32 = RAW_DATA_AS(s1).f32 / RAW_DATA_AS(s2).f32;
        _check_canonical_nan_32(&ret);
    } else if(op == SQRT) {
        if(RAW_DATA_AS(s1).f32 < 0) {
            RAW_DATA_AS(ret).u64 = 0x7FC00000UL;
            *fcsr_update |= RV_FFLAGS_INVALID;
        }
        else {
            RAW_DATA_AS(ret).f32 = sqrt(RAW_DATA_AS(s1).f32);
        }
        _check_canonical_nan_32(&ret);
    } else if(op == SGNJ) {
        uint64_t sbit1 = (s1 >> 31U);
        uint64_t sbit2 = (s2 >> 31U);
        switch (funct3)
        {
        case 0: RAW_DATA_AS(ret).f32 = ((sbit1 == sbit2)?(RAW_DATA_AS(s1).f32):(-RAW_DATA_AS(s1).f32)); break;
        case 1: RAW_DATA_AS(ret).f32 = ((sbit1 != sbit2)?(RAW_DATA_AS(s1).f32):(-RAW_DATA_AS(s1).f32)); break;
        case 2: RAW_DATA_AS(ret).f32 = ((sbit1 == sbit2)?(std::abs(RAW_DATA_AS(s1).f32)):(-std::abs(RAW_DATA_AS(s1).f32))); break;
        default: *invalidop = true;
        }
        _check_canonical_nan_32(&ret);
    } else if(op == MIN) {
        if(isnan(RAW_DATA_AS(s1).f32)) {
            ret = s2;
        } else if(isnan(RAW_DATA_AS(s2).f32)) {
            ret = s1;
        } else {
            switch (funct3)
            {
            case 0: RAW_DATA_AS(ret).f32 = std::min<float>(RAW_DATA_AS(s1).f32, RAW_DATA_AS(s2).f32); break;
            case 1: RAW_DATA_AS(ret).f32 = std::max<float>(RAW_DATA_AS(s1).f32, RAW_DATA_AS(s2).f32); break;
            default: *invalidop = true;
            }
        }
        _check_canonical_nan_32(&ret);
    } else if(op == MVF2F) {
        if(rs2 == 0) {
            ret = s1;
        } else if(rs2 == 1) {
            RAW_DATA_AS(ret).f32 = RAW_DATA_AS(s1).f64; 
        } else {
            *invalidop = true;
        }
    } else if(op == CVTF2I) {
        if(((s1 & 0x7f800000UL) == 0x7f800000UL) && (s1 & 0x007fffffUL)) {
            // NaN, -NaN
            switch (rs2)
            {
            case 0: ret = 0x000000007fffffffUL; break;
            case 1: ret = 0xffffffffffffffffUL; break;
            case 2: ret = 0x7fffffffffffffffUL; break;
            case 3: ret = 0xffffffffffffffffUL; break;
            default: *invalidop = true;
            }
            *fcsr_update |= RV_FFLAGS_INVALID;
        }
        else if(((s1 & 0xff800000UL) == 0xff800000UL) && !(s1 & 0x007fffffUL)) {
            // -inf
            switch (rs2)
            {
            case 0: ret = 0xffffffff80000000UL; break;
            case 1: ret = 0UL; break;
            case 2: ret = 0x8000000000000000UL; break;
            case 3: ret = 0UL; break;
            default: *invalidop = true;
            }
            *fcsr_update |= RV_FFLAGS_INVALID;
        }
        else if(((s1 & 0xff800000UL) == 0x7f800000UL) && !(s1 & 0x007fffffUL)) {
            // inf
            switch (rs2)
            {
            case 0: ret = 0x000000007fffffffUL; break;
            case 1: ret = 0xffffffffffffffffUL; break;
            case 2: ret = 0x7fffffffffffffffUL; break;
            case 3: ret = 0xffffffffffffffffUL; break;
            default: *invalidop = true;
            }
            *fcsr_update |= RV_FFLAGS_INVALID;
        } else {
            float v = RAW_DATA_AS(s1).f32;
            switch (funct3)
            {
            case 0: v += 0.5; break; // RNE
            case 1: v = ((v>0)?floor(v):ceil(v)); break; // RTZ
            case 2: v = floor(v); break; // RDN
            case 3: v = ceil(v); break; // RUP
            default: *invalidop = true;
            }
            if(rs2 == 0) {
                if(v < -pow(2., 31.)) {
                    RAW_DATA_AS(ret).u64 = 0xffffffff80000000UL;
                    *fcsr_update |= RV_FFLAGS_INVALID;
                }
                else if(v > pow(2., 31.)-1.) {
                    RAW_DATA_AS(ret).u64 = 0x000000007fffffffUL;
                    *fcsr_update |= RV_FFLAGS_INVALID;
                }
                else {
                    RAW_DATA_AS(ret).i64 = (int32_t)v;
                }
            } else if(rs2 == 1) {
                if(v < 0) {
                    RAW_DATA_AS(ret).u64 = 0x0000000000000000UL;
                    *fcsr_update |= RV_FFLAGS_INVALID;
                }
                else if(v > pow(2., 32.)-1.) {
                    RAW_DATA_AS(ret).u64 = 0x00000000ffffffffUL;
                    *fcsr_update |= RV_FFLAGS_INVALID;
                }
                else {
                    RAW_DATA_AS(ret).u64 = (uint32_t)v;
                }
            } else if(rs2 == 2) {
                if(v < -pow(2., 63.)) {
                    RAW_DATA_AS(ret).u64 = 0x8000000000000000UL;
                    *fcsr_update |= RV_FFLAGS_INVALID;
                }
                else if(v > pow(2., 63.)-1.) {
                    RAW_DATA_AS(ret).u64 = 0x7fffffffffffffffUL;
                    *fcsr_update |= RV_FFLAGS_INVALID;
                }
                else {
                    RAW_DATA_AS(ret).i64 = (int64_t)v;
                }
            } else if(rs2 == 3) {
                if(v < 0) {
                    RAW_DATA_AS(ret).u64 = 0x0000000000000000UL;
                    *fcsr_update |= RV_FFLAGS_INVALID;
                }
                else if(v > pow(2., 64.)-1.) {
                    RAW_DATA_AS(ret).u64 = 0xffffffffffffffffUL;
                    *fcsr_update |= RV_FFLAGS_INVALID;
                }
                else {
                    RAW_DATA_AS(ret).u64 = (uint64_t)v;
                }
            } else {
                *invalidop = true;
            }
        }
    } else if(op == CVTI2F) {
        switch (rs2)
        {
        case 0: RAW_DATA_AS(ret).f32 = RAW_DATA_AS(s1).i32; break;
        case 1: RAW_DATA_AS(ret).f32 = RAW_DATA_AS(s1).u32; break;
        case 2: RAW_DATA_AS(ret).f32 = RAW_DATA_AS(s1).i64; break;
        case 3: RAW_DATA_AS(ret).f32 = RAW_DATA_AS(s1).u64; break;
        default: *invalidop = true;
        }
        _check_canonical_nan_32(&ret);
    } else if(op == MVF2I) {
        if(funct3 == 0) {
            // RAW
            RAW_DATA_AS(ret).u64 = RAW_DATA_AS(s1).u64;
        } else if(funct3 == 1) {
            // FCLASS
            if(s1 == 0xff800000U) {
                ret |= (1<<0);
            }
            else if(s1 == 0x80000000U) {
                ret |= (1<<3);
            }
            else if(s1 == 0UL) {
                ret |= (1<<4);
            }
            else if(s1 == 0x7f800000U) {
                ret |= (1<<7);
            }
            else if((s1 & 0x7fc00000U) == 0x7fc00000) {
                ret |= (1<<9);
            }
            else if((s1 & 0x7fc00000U) == 0x7f800000) {
                ret |= (1<<8);
            }
            else if(isnormal(RAW_DATA_AS(s1).f32)) {
                if(RAW_DATA_AS(s1).f32 > 0) ret |= (1<<6);
                else ret |= (1<<1);
            }
            else {
                if(RAW_DATA_AS(s1).f32 > 0) ret |= (1<<5);
                else ret |= (1<<2);
            }
        } else {
            *invalidop = true;
        }
    } else if(op == MVI2F) {
        RAW_DATA_AS(ret).u64 = RAW_DATA_AS(s1).u64;
        _check_canonical_nan_32(&ret);
    } else if(op == CMP) {
        switch (funct3)
        {
        case 0: ret = ((RAW_DATA_AS(s1).f32 <= RAW_DATA_AS(s2).f32)?1:0); break; // FLE
        case 1: ret = ((RAW_DATA_AS(s1).f32 < RAW_DATA_AS(s2).f32)?1:0); break; // FLT
        case 2: ret = ((RAW_DATA_AS(s1).f32 == RAW_DATA_AS(s2).f32)?1:0); break; // FEQ
        default: *invalidop = true;
        }
    } else {
        *invalidop = true;
    }
    uint32_t flg = fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT);
    if(flg & FE_INEXACT) *fcsr_update |= RV_FFLAGS_INEXACT;
    if(flg & FE_UNDERFLOW) *fcsr_update |= RV_FFLAGS_UNDERFLOW;
    if(flg & FE_OVERFLOW) *fcsr_update |= RV_FFLAGS_OVERFLOW;
    if(flg & FE_DIVBYZERO) *fcsr_update |= RV_FFLAGS_DIVBYZERO;
    if(flg & FE_INVALID) *fcsr_update |= RV_FFLAGS_INVALID;
}




