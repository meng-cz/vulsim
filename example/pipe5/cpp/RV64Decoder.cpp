
#include "common.h"
#include "global.h"
#include "bundles.h"


void RV64Decoder_decode(uint32 rawinst, RV64DecInst * decinst) {
    #define EXTRACT_BITS(value, from, len) (((value) >> (from)) & ((1UL << (len))-1))

    decinst->opcode = EXTRACT_BITS(rawinst, 0, 7);
    decinst->rd = EXTRACT_BITS(rawinst, 7, 5);
    decinst->rs1 = EXTRACT_BITS(rawinst, 15, 5);
    decinst->rs2 = EXTRACT_BITS(rawinst, 20, 5);
    decinst->funct3 = EXTRACT_BITS(rawinst, 12, 3);
    decinst->funct7 = EXTRACT_BITS(rawinst, 25, 7);
    decinst->expt = 0;
    decinst->flag = 0;

    auto get_imm_i = [](uint32 inst) -> int32 {
        int32_t ret = (EXTRACT_BITS(inst, 20, 12));
        if(inst & (1<<31)) ret = ret | 0xfffff000UL;
        return ret;
    };
    auto get_imm_s = [](uint32 inst) -> int32 {
        int32_t ret = (EXTRACT_BITS(inst, 7, 5)) | (EXTRACT_BITS(inst, 25, 7) << 5);
        if(inst & (1<<31)) ret = ret | 0xfffff000UL;
        return ret;
    };
    auto get_imm_b = [](uint32 inst) -> int32 {
        int32_t ret = (EXTRACT_BITS(inst, 8, 4) << 1) | (EXTRACT_BITS(inst, 25, 6) << 5) | (EXTRACT_BITS(inst, 7, 1) << 11);
        if(inst & (1<<31)) ret |= 0xfffff000UL;
        return ret;
    };
    auto get_imm_u = [](uint32 inst) -> int32 {
        int32_t ret = (inst & 0xfffff000U);
        if(inst & (1<<31)) ret |= 0x00000000UL;
        return ret;
    };
    auto get_imm_j = [](uint32 inst) -> int32 {
        int32_t ret = (EXTRACT_BITS(inst, 21, 10) << 1) | (EXTRACT_BITS(inst, 20, 1) << 11) | (inst & 0xFF000);
        if(inst & (1<<31)) ret = ret | 0xfff00000UL;
        return ret;
    };

    uint8 fpop = (decinst->funct7 >> 2);
    const uint8 RVFOP5_ADD      = (0x00);
    const uint8  RVFOP5_SUB      = (0x01);
    const uint8  RVFOP5_MUL      = (0x02);
    const uint8  RVFOP5_DIV      = (0x03);
    const uint8  RVFOP5_SQRT     = (0x0b);
    const uint8  RVFOP5_SGNJ     = (0x04);
    const uint8  RVFOP5_MIN      = (0x05);
    const uint8  RVFOP5_MVF2F    = (0x08);
    const uint8  RVFOP5_CMP      = (0x14);
    const uint8  RVFOP5_CVTF2I   = (0x18);
    const uint8  RVFOP5_CVTI2F   = (0x1a);
    const uint8  RVFOP5_MVF2I    = (0x1c);
    const uint8  RVFOP5_MVI2F    = (0x1e);
    switch (decinst->opcode)
    {
    case OP_LUI:
    case OP_AUIPC:
        decinst->imm = get_imm_u(rawinst);
        decinst->flag |= RVINSTFLAG_RDINT;
        break;
    case OP_JAL:
        decinst->imm = get_imm_j(rawinst);
        decinst->flag |= RVINSTFLAG_RDINT;
        break;
    case OP_JALR:
    case OP_LOAD:
    case OP_OPIMM:
    case OP_OPIMM32:
        decinst->imm = get_imm_i(rawinst);
        decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT);
        break;
    case OP_LOADFP:
        decinst->imm = get_imm_i(rawinst);
        decinst->flag |= (RVINSTFLAG_RDFP | RVINSTFLAG_S1INT);
        break;
    case OP_BRANCH:
        decinst->imm = get_imm_b(rawinst);
        decinst->flag |= (RVINSTFLAG_S1INT | RVINSTFLAG_S2INT);
        break;
    case OP_STORE:
        decinst->imm = get_imm_s(rawinst);
        decinst->flag |= (RVINSTFLAG_S1INT | RVINSTFLAG_S2INT);
        break;
    case OP_STOREFP:
        decinst->imm = get_imm_s(rawinst);
        decinst->flag |= (RVINSTFLAG_S1INT | RVINSTFLAG_S2FP);
        break;
    case OP_OP:
    case OP_OP32:
    case OP_AMO:
        decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT | RVINSTFLAG_S2INT);
        break;
    case OP_OPFP:
        if((fpop == RVFOP5_CMP) || (fpop == RVFOP5_CVTF2I) || (fpop == RVFOP5_MVF2I)) {
            decinst->flag |= RVINSTFLAG_RDINT;
        } else {
            decinst->flag |= RVINSTFLAG_RDFP;
        }
        if((fpop == RVFOP5_CVTI2F) || (fpop == RVFOP5_MVI2F)) {
            decinst->flag |= RVINSTFLAG_S1INT;
        } else {
            decinst->flag |= RVINSTFLAG_S1FP;
        }
        if((fpop == RVFOP5_ADD) || (fpop == RVFOP5_SUB) || (fpop == RVFOP5_MUL) || (fpop == RVFOP5_DIV) ||
            (fpop == RVFOP5_SGNJ) || (fpop == RVFOP5_MIN) || (fpop == RVFOP5_CMP)) {
            decinst->flag |= RVINSTFLAG_S2FP;
        }
        break;
    case OP_MADD:
    case OP_MSUB:
    case OP_NMADD:
    case OP_NMSUB:
        decinst->flag |= (RVINSTFLAG_RDFP | RVINSTFLAG_S1FP | RVINSTFLAG_S2FP | RVINSTFLAG_S3FP);
        break;
    case OP_MISCMEM:
        decinst->imm = get_imm_i(rawinst);
        decinst->flag |= RVINSTFLAG_UNIQUE;
        if(decinst->funct3 == 0) {
            if(rawinst == 0x8330000) decinst->flag |= RVINSTFLAG_FENCETSO;
            else if(rawinst == 0x0100000f) decinst->flag |= RVINSTFLAG_PAUSE;
            else decinst->flag |= RVINSTFLAG_FENCE;
        }
        else if(decinst->funct3 == 1) {
            decinst->flag |= RVINSTFLAG_FENCEI;
        }
        else {
            decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT);
        }
        break;
    case OP_SYSTEM:
        decinst->imm = get_imm_i(rawinst);
        decinst->flag |= RVINSTFLAG_UNIQUE;
        if(rawinst == 0x00000073) decinst->flag |= RVINSTFLAG_ECALL;
        else if(rawinst == 0x00100073) decinst->flag |= RVINSTFLAG_EBREAK;
        else {
            if((decinst->funct3 & 3) == 0) decinst->expt = true;
            if(decinst->funct3 & 4) decinst->flag |= RVINSTFLAG_RDINT;
            else decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT);
        }
        break;
    default:
        decinst->expt = true;
        break;
    }

    #undef EXTRACT_BITS

}


void RV64Decoder_cdecode(uint32 rawinst, RV64DecInst * decinst) {
    #define EXTRACT_BITS(value, from, len) (((value) >> (from)) & ((1UL << (len))-1))

    memset(decinst, 0, sizeof(*decinst));
    decinst->flag |= (RVINSTFLAG_RVC);

    uint32 func = ((rawinst >> 13) & 7);
    if((rawinst & 3) == 0) {
        if(func == 0) { // C.ADDI4SPN
            decinst->opcode = OP_OPIMM;
            decinst->rd = EXTRACT_BITS(rawinst, 2, 3) + 8;
            decinst->rs1 = 2;
            uint32 uimm = 0;
            uimm |= (((rawinst >> 5) & 1) << 3);
            uimm |= (((rawinst >> 6) & 1) << 2);
            uimm |= (EXTRACT_BITS(rawinst, 7, 4) << 6);
            uimm |= (((rawinst >> 11) & 1) << 4);
            uimm |= (((rawinst >> 12) & 1) << 5);
            decinst->imm = uimm;
            decinst->funct3 = 0;
            decinst->funct7 = 0;
            decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT);
            return;
        }
        else if(func == 1) { // C.FLD
            decinst->opcode = OP_LOADFP;
            decinst->rd = EXTRACT_BITS(rawinst, 2, 3) + 8;
            decinst->rs1 = EXTRACT_BITS(rawinst, 7, 3) + 8;
            uint32 uimm = 0;
            uimm |= (EXTRACT_BITS(rawinst, 5, 2) << 6);
            uimm |= (EXTRACT_BITS(rawinst, 10, 3) << 3);
            decinst->imm = uimm;
            decinst->funct3 = 3;
            decinst->flag |= (RVINSTFLAG_RDFP | RVINSTFLAG_S1INT);
            return;
        }
        else if(func == 2) { // C.LW
            decinst->opcode = OP_LOAD;
            decinst->rd = EXTRACT_BITS(rawinst, 2, 3) + 8;
            decinst->rs1 = EXTRACT_BITS(rawinst, 7, 3) + 8;
            uint32 uimm = 0;
            uimm |= (EXTRACT_BITS(rawinst, 5, 1) << 6);
            uimm |= (EXTRACT_BITS(rawinst, 6, 1) << 2);
            uimm |= (EXTRACT_BITS(rawinst, 10, 3) << 3);
            decinst->imm = uimm;
            decinst->funct3 = 2;
            decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT);
            return;
        }
        else if(func == 3) { // C.LD
            decinst->opcode = OP_LOAD;
            decinst->rd = EXTRACT_BITS(rawinst, 2, 3) + 8;
            decinst->rs1 = EXTRACT_BITS(rawinst, 7, 3) + 8;
            uint32 uimm = 0;
            uimm |= (EXTRACT_BITS(rawinst, 5, 2) << 6);
            uimm |= (EXTRACT_BITS(rawinst, 10, 3) << 3);
            decinst->imm = uimm;
            decinst->funct3 = 3;
            decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT);
            return;
        }
        else if(func == 5) { // C.FSD
            decinst->opcode = OP_STOREFP;
            decinst->rs2 = EXTRACT_BITS(rawinst, 2, 3) + 8;
            decinst->rs1 = EXTRACT_BITS(rawinst, 7, 3) + 8;
            uint32 uimm = 0;
            uimm |= (EXTRACT_BITS(rawinst, 5, 2) << 6);
            uimm |= (EXTRACT_BITS(rawinst, 10, 3) << 3);
            decinst->imm = uimm;
            decinst->funct3 = 3;
            decinst->flag |= (RVINSTFLAG_S1INT | RVINSTFLAG_S2FP);
            return;
        }
        else if(func == 6) { // C.SW
            decinst->opcode = OP_STORE;
            decinst->rs2 = EXTRACT_BITS(rawinst, 2, 3) + 8;
            decinst->rs1 = EXTRACT_BITS(rawinst, 7, 3) + 8;
            uint32 uimm = 0;
            uimm |= (EXTRACT_BITS(rawinst, 5, 1) << 6);
            uimm |= (EXTRACT_BITS(rawinst, 6, 1) << 2);
            uimm |= (EXTRACT_BITS(rawinst, 10, 3) << 3);
            decinst->imm = uimm;
            decinst->funct3 = 2;
            decinst->flag |= (RVINSTFLAG_S1INT | RVINSTFLAG_S2INT);
            return;
        }
        else if(func == 7) { // C.SD
            decinst->opcode = OP_STORE;
            decinst->rs2 = EXTRACT_BITS(rawinst, 2, 3) + 8;
            decinst->rs1 = EXTRACT_BITS(rawinst, 7, 3) + 8;
            uint32 uimm = 0;
            uimm |= (EXTRACT_BITS(rawinst, 5, 2) << 6);
            uimm |= (EXTRACT_BITS(rawinst, 10, 3) << 3);
            decinst->imm = uimm;
            decinst->funct3 = 3;
            decinst->flag |= (RVINSTFLAG_S1INT | RVINSTFLAG_S2INT);
            return;
        }
    }
    else if((rawinst & 3) == 1) {
        if(func == 0) { // C.ADDI
            decinst->opcode = OP_OPIMM;
            decinst->funct3 = 0;
            decinst->funct7 = 0;
            decinst->rd = decinst->rs1 = EXTRACT_BITS(rawinst, 7, 5);
            uint32 uimm = EXTRACT_BITS(rawinst, 2, 5);
            if((rawinst >> 12) & 1) uimm |= 0xffffffe0UL;
            decinst->imm = uimm;
            decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT);
            return;
        }
        else if(func == 1) { // C.ADDIW
            decinst->opcode = OP_OPIMM32;
            decinst->funct3 = 0;
            decinst->funct7 = 0;
            decinst->rd = decinst->rs1 = EXTRACT_BITS(rawinst, 7, 5);
            uint32 uimm = EXTRACT_BITS(rawinst, 2, 5);
            if((rawinst >> 12) & 1) uimm |= 0xffffffe0UL;
            decinst->imm = uimm;
            decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT);
            return;
        }
        else if(func == 2 && EXTRACT_BITS(rawinst, 7, 5) != 0) { // C.LI
            decinst->opcode = OP_LUI;
            decinst->rd = EXTRACT_BITS(rawinst, 7, 5);
            uint32 uimm = EXTRACT_BITS(rawinst, 2, 5);
            if((rawinst >> 12) & 1) uimm |= 0xffffffe0UL;
            decinst->imm = uimm;
            decinst->flag |= (RVINSTFLAG_RDINT);
            return;
        }
        else if(func == 3 && EXTRACT_BITS(rawinst, 7, 5) == 2) { // C.ADDI16SP
            decinst->opcode = OP_OPIMM;
            decinst->funct3 = 0;
            decinst->funct7 = 0;
            decinst->rd = decinst->rs1 = 2;
            uint32 uimm = 0;
            uimm |= (EXTRACT_BITS(rawinst, 2, 1) << 5);
            uimm |= (EXTRACT_BITS(rawinst, 3, 2) << 7);
            uimm |= (EXTRACT_BITS(rawinst, 5, 1) << 6);
            uimm |= (EXTRACT_BITS(rawinst, 6, 1) << 4);
            if((rawinst >> 12) & 1) uimm |= 0xfffffe00UL;
            decinst->imm = uimm;
            decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT);
            return;
        }
        else if(func == 3 && EXTRACT_BITS(rawinst, 7, 5)) { // C.LUI
            decinst->opcode = OP_LUI;
            decinst->rd = EXTRACT_BITS(rawinst, 7, 5);
            uint32 uimm = (EXTRACT_BITS(rawinst, 2, 5) << 12);
            if((rawinst >> 12) & 1) uimm |= 0xfffe0000UL;
            decinst->imm = uimm;
            decinst->flag |= (RVINSTFLAG_RDINT);
            return;
        }
        else if(func == 4) {
            uint32_t f1 = EXTRACT_BITS(rawinst, 10, 2), f2 = EXTRACT_BITS(rawinst, 5, 2);
            if(f1 == 0) { // C.SRLI64
                decinst->opcode = OP_OPIMM;
                decinst->funct3 = 5;
                decinst->funct7 = 0;
                decinst->rd = decinst->rs1 = EXTRACT_BITS(rawinst, 7, 3) + 8;
                uint32 uimm = (EXTRACT_BITS(rawinst, 2, 5));
                uimm |= (((rawinst >> 12) & 1) << 5);
                decinst->imm = uimm;
                decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT);
                return;
            }
            else if(f1 == 1) { // C.SRAI64
                decinst->opcode = OP_OPIMM;
                decinst->funct3 = 5;
                decinst->funct7 = 0x20;
                decinst->rd = decinst->rs1 = EXTRACT_BITS(rawinst, 7, 3) + 8;
                uint32 uimm = (EXTRACT_BITS(rawinst, 2, 5));
                uimm |= (((rawinst >> 12) & 1) << 5);
                decinst->imm = uimm;
                decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT);
                return;
            }
            else if(f1 == 2) { // C.ANDI
                decinst->opcode = OP_OPIMM;
                decinst->funct3 = 7;
                decinst->funct7 = 0;
                decinst->rd = decinst->rs1 = EXTRACT_BITS(rawinst, 7, 3) + 8;
                uint32 uimm = (EXTRACT_BITS(rawinst, 2, 5));
                if((rawinst >> 12) & 1) uimm |= 0xffffffe0UL;
                decinst->imm = uimm;
                decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT);
                return;
            }
            else if((rawinst >> 12) & 1) { // C.SUBW  C.ADDW
                decinst->opcode = OP_OP32;
                decinst->rd = decinst->rs1 = EXTRACT_BITS(rawinst, 7, 3) + 8;
                decinst->rs2 = EXTRACT_BITS(rawinst, 2, 3) + 8;
                decinst->funct3 = 0;
                decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT | RVINSTFLAG_S2INT);
                if(f2 == 0) {
                    decinst->funct7 = 0x20;
                    return;
                }
                else if(f2 == 1) {
                    decinst->funct7 = 0;
                    return;
                }
            }
            else {
                decinst->opcode = OP_OP;
                decinst->rd = decinst->rs1 = EXTRACT_BITS(rawinst, 7, 3) + 8;
                decinst->rs2 = EXTRACT_BITS(rawinst, 2, 3) + 8;
                decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT | RVINSTFLAG_S2INT);
                if(f2 == 0) { // C.SUB
                    decinst->funct3 = 0;
                    decinst->funct7 = 0x20;
                    return;
                }
                else if(f2 == 1) { // C.XOR
                    decinst->funct3 = 4;
                    decinst->funct7 = 0;
                    return;
                }
                else if(f2 == 2) { // C.OR
                    decinst->funct3 = 6;
                    decinst->funct7 = 0;
                    return;
                }
                else if(f2 == 3) { // C.AND
                    decinst->funct3 = 7;
                    decinst->funct7 = 0;
                    return;
                }
            }
        }
        else if(func == 5) { // C.J
            decinst->opcode = OP_JAL;
            decinst->rd = 0;
            uint32 uimm = 0;
            uimm |= (EXTRACT_BITS(rawinst, 2, 1) << 5);
            uimm |= (EXTRACT_BITS(rawinst, 3, 3) << 1);
            uimm |= (EXTRACT_BITS(rawinst, 6, 1) << 7);
            uimm |= (EXTRACT_BITS(rawinst, 7, 1) << 6);
            uimm |= (EXTRACT_BITS(rawinst, 8, 1) << 10);
            uimm |= (EXTRACT_BITS(rawinst, 9, 2) << 8);
            uimm |= (EXTRACT_BITS(rawinst, 11, 1) << 4);
            if((rawinst >> 12) & 1) uimm |= 0xfffff800UL;
            decinst->imm = uimm;
            decinst->flag |= (RVINSTFLAG_RDINT);
            return;
        }
        else if(func == 6) { // C.BEQZ
            decinst->opcode = OP_BRANCH;
            decinst->funct3 = 0;
            decinst->rs1 = EXTRACT_BITS(rawinst, 7, 3) + 8;
            decinst->rs2 = 0;
            uint32 uimm = 0;
            uimm |= (EXTRACT_BITS(rawinst, 2, 1) << 5);
            uimm |= (EXTRACT_BITS(rawinst, 3, 2) << 1);
            uimm |= (EXTRACT_BITS(rawinst, 5, 2) << 6);
            uimm |= (EXTRACT_BITS(rawinst, 10, 2) << 3);
            if((rawinst >> 12) & 1) uimm |= 0xffffff00UL;
            decinst->imm = uimm;
            decinst->flag |= (RVINSTFLAG_S1INT | RVINSTFLAG_S2INT);
            return;
        }
        else if(func == 7) { // C.BNEZ
            decinst->opcode = OP_BRANCH;
            decinst->funct3 = 1;
            decinst->rs1 = EXTRACT_BITS(rawinst, 7, 3) + 8;
            decinst->rs2 = 0;
            uint32 uimm = 0;
            uimm |= (EXTRACT_BITS(rawinst, 2, 1) << 5);
            uimm |= (EXTRACT_BITS(rawinst, 3, 2) << 1);
            uimm |= (EXTRACT_BITS(rawinst, 5, 2) << 6);
            uimm |= (EXTRACT_BITS(rawinst, 10, 2) << 3);
            if((rawinst >> 12) & 1) uimm |= 0xffffff00UL;
            decinst->imm = uimm;
            decinst->flag |= (RVINSTFLAG_S1INT | RVINSTFLAG_S2INT);
            return;
        }
    }
    else if((rawinst & 3) == 2) { // C.SLLI64
        if(func == 0 && EXTRACT_BITS(rawinst, 7, 5) != 0) {
            decinst->opcode = OP_OPIMM;
            decinst->funct3 = 1;
            decinst->funct7 = 0;
            decinst->rd = decinst->rs1 = EXTRACT_BITS(rawinst, 7, 5);
            uint32 uimm = (EXTRACT_BITS(rawinst, 2, 5));
            uimm |= (((rawinst >> 12) & 1) << 5);
            decinst->imm = uimm;
            decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT);
            return;
        }
        else if(func == 1) { // C.FLDSP
            decinst->opcode = OP_LOADFP;
            decinst->funct3 = 3;
            decinst->rd = EXTRACT_BITS(rawinst, 7, 5);
            decinst->rs1 = 2;
            uint32 uimm = 0;
            uimm |= (EXTRACT_BITS(rawinst, 2, 3) << 6);
            uimm |= (EXTRACT_BITS(rawinst, 5, 2) << 3);
            uimm |= (EXTRACT_BITS(rawinst, 12, 1) << 5);
            decinst->imm = uimm;
            decinst->flag |= (RVINSTFLAG_RDFP | RVINSTFLAG_S1INT);
            return;
        }
        else if(func == 2) { // C.LWSP
            decinst->opcode = OP_LOAD;
            decinst->funct3 = 2;
            decinst->rd = EXTRACT_BITS(rawinst, 7, 5);
            decinst->rs1 = 2;
            uint32 uimm = 0;
            uimm |= (EXTRACT_BITS(rawinst, 2, 2) << 6);
            uimm |= (EXTRACT_BITS(rawinst, 4, 3) << 2);
            uimm |= (EXTRACT_BITS(rawinst, 12, 1) << 5);
            decinst->imm = uimm;
            decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT);
            return;
        }
        else if(func == 3) { // C.LDSP
            decinst->opcode = OP_LOAD;
            decinst->funct3 = 3;
            decinst->rd = EXTRACT_BITS(rawinst, 7, 5);
            decinst->rs1 = 2;
            uint32 uimm = 0;
            uimm |= (EXTRACT_BITS(rawinst, 2, 3) << 6);
            uimm |= (EXTRACT_BITS(rawinst, 5, 2) << 3);
            uimm |= (EXTRACT_BITS(rawinst, 12, 1) << 5);
            decinst->imm = uimm;
            decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT);
            return;
        }
        else if(func == 4) {
            if(((rawinst >> 12) & 1) == 0) {
                if(EXTRACT_BITS(rawinst, 2, 5)) { // C.MV
                    decinst->opcode = OP_OPIMM;
                    decinst->funct3 = 0;
                    decinst->funct7 = 0;
                    decinst->rd = EXTRACT_BITS(rawinst, 7, 5);
                    decinst->rs1 = EXTRACT_BITS(rawinst, 2, 5);
                    decinst->imm = 0;
                    decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT);
                    return;
                }
                else { // C.JR
                    decinst->opcode = OP_JALR;
                    decinst->rs1 = EXTRACT_BITS(rawinst, 7, 5);
                    decinst->rd = 0;
                    decinst->imm = 0;
                    decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT);
                    return;
                }
            }
            else {
                if(EXTRACT_BITS(rawinst, 2, 5) != 0) { // C.ADD
                    decinst->opcode = OP_OP;
                    decinst->funct3 = 0;
                    decinst->funct7 = 0;
                    decinst->rd = decinst->rs1 = EXTRACT_BITS(rawinst, 7, 5);
                    decinst->rs2 = EXTRACT_BITS(rawinst, 2, 5);
                    decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT | RVINSTFLAG_S2INT);
                    return;
                }
                else if(EXTRACT_BITS(rawinst, 7, 5)) { // C.JALR
                    decinst->opcode = OP_JALR;
                    decinst->rs1 = EXTRACT_BITS(rawinst, 7, 5);
                    decinst->rd = 1;
                    decinst->imm = 0;
                    decinst->flag |= (RVINSTFLAG_RDINT | RVINSTFLAG_S1INT);
                    return;
                }
                else { // C.EBREAK
                    decinst->opcode = OP_SYSTEM;
                    decinst->imm = 1;
                    decinst->rs1 = decinst->rs2 = decinst->funct3 = 0;
                    decinst->flag |= (RVINSTFLAG_EBREAK | RVINSTFLAG_UNIQUE);
                    return;
                }
            }
        }
        else if(func == 5) { // C.FSDSP
            decinst->opcode = OP_STOREFP;
            decinst->funct3 = 3;
            decinst->rs2 = EXTRACT_BITS(rawinst, 2, 5);
            decinst->rs1 = 2;
            uint32 uimm = 0;
            uimm |= (EXTRACT_BITS(rawinst, 7, 3) << 6);
            uimm |= (EXTRACT_BITS(rawinst, 10, 3) << 3);
            decinst->imm = uimm;
            decinst->flag |= (RVINSTFLAG_S1INT | RVINSTFLAG_S2FP);
            return;
        }
        else if(func == 6) { // C.SWSP
            decinst->opcode = OP_STORE;
            decinst->funct3 = 2;
            decinst->rs2 = EXTRACT_BITS(rawinst, 2, 5);
            decinst->rs1 = 2;
            uint32 uimm = 0;
            uimm |= (EXTRACT_BITS(rawinst, 7, 2) << 6);
            uimm |= (EXTRACT_BITS(rawinst, 9, 4) << 2);
            decinst->imm = uimm;
            decinst->flag |= (RVINSTFLAG_S1INT | RVINSTFLAG_S2INT);
            return;
        }
        else if(func == 7) { // C.SDSP
            decinst->opcode = OP_STORE;
            decinst->funct3 = 3;
            decinst->rs2 = EXTRACT_BITS(rawinst, 2, 5);
            decinst->rs1 = 2;
            uint32 uimm = 0;
            uimm |= (EXTRACT_BITS(rawinst, 7, 3) << 6);
            uimm |= (EXTRACT_BITS(rawinst, 10, 3) << 3);
            decinst->imm = uimm;
            decinst->flag |= (RVINSTFLAG_S1INT | RVINSTFLAG_S2INT);
            return;
        }
    }

    #undef EXTRACT_BITS

    decinst->expt = true;
}
