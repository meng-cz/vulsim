
#include "common.h"
#include "global.h"
#include "bundles.h"

bool ex_in_can_pop();
EXData & ex_in_top();
void ex_in_pop();

bool ex_out_can_push();
void ex_out_push(MemData &value);

bool stalled = false;
void stall();

void read_reg(uint16 idx, uint64 * value, bool * valid);
void lock_reg(uint16 idx);

void alu64(uint32 funct73, uint64 s1, uint64 s2, uint64 * result, bool * invalidop);
void alu32(uint32 funct73, uint64 s1, uint64 s2, uint64 * result, bool * invalidop);
void branch(uint8 funct3, uint64 s1, uint64 s2, bool * result, bool * invalidop);
void fpu64(uint32 funct5_rs2_funct3, uint64 s1, uint64 s2, uint64 * result, bool * invalidop, uint64 * fcsr_update);
void fpu32(uint32 funct5_rs2_funct3, uint64 s1, uint64 s2, uint64 * result, bool * invalidop, uint64 * fcsr_update);

void EXStage_tick() {

    if(stalled) {
        return;
    }

    if(!(ex_in_can_pop() && ex_out_can_push())) {
        return;
    }

    EXData inst = ex_in_top();
    MemData mem;
    memset(&mem, 0, sizeof(mem));
    mem.pc = inst.pc;
    mem.rawinst = inst.rawinst;
    mem.instflag = inst.instflag;
    mem.rd = inst.rd;
    mem.opcode = inst.opcode;
    mem.funct3 = inst.funct3;
    mem.funct7 = inst.funct7;

    if(inst.exception) {
        mem.exception = inst.exception;
        ex_out_push(mem);
        ex_in_pop();
        return;
    }

    uint64 s1 = 0, s2 = 0, s3 = 0;
    bool regvalid = false, tmpvalid = false;
    if(inst.instflag & RVINSTFLAG_S1INT) {
        read_reg(inst.rs1, &s1, &tmpvalid);
        regvalid = regvalid || tmpvalid;
    } else if(inst.instflag & RVINSTFLAG_S1FP) {
        read_reg(inst.rs1 + 32, &s1, &tmpvalid);
        regvalid = regvalid || tmpvalid;
    }
    if(inst.instflag & RVINSTFLAG_S2INT) {
        read_reg(inst.rs2, &s2, &tmpvalid);
        regvalid = regvalid || tmpvalid;
    } else if(inst.instflag & RVINSTFLAG_S2FP) {
        read_reg(inst.rs2 + 32, &s2, &tmpvalid);
        regvalid = regvalid || tmpvalid;
    }
    if(inst.instflag & RVINSTFLAG_S3FP) {
        read_reg((inst.funct7 >> 2) + 32, &s3, &tmpvalid);
        regvalid = regvalid || tmpvalid;
    }

    if(!regvalid) {
        stall();
        return;
    }

    if(inst.instflag & RVINSTFLAG_RDINT) {
        lock_reg(inst.rd);
    } else if (inst.instflag & RVINSTFLAG_RDFP) {
        lock_reg(inst.rd + 32);
    }

    bool invalid_op = false;
    if(inst.opcode == OP_AUIPC) {
        RAW_DATA_AS(mem.arg0).i64 = inst.pc + inst.imm;
    }
    else if(inst.opcode == OP_LUI) {
        RAW_DATA_AS(mem.arg0).i64 = inst.imm;
    }
    else if(inst.opcode == OP_JAL) {
        RAW_DATA_AS(mem.arg1).i64 = inst.pc + inst.imm;
        RAW_DATA_AS(mem.arg0).i64 = inst.pc + ((inst.instflag & RVINSTFLAG_RVC)?2:4);
    }
    else if(inst.opcode == OP_JALR) {
        RAW_DATA_AS(mem.arg1).i64 = s1 + inst.imm;
        RAW_DATA_AS(mem.arg0).i64 = inst.pc + ((inst.instflag & RVINSTFLAG_RVC)?2:4);
    }
    else if(inst.opcode == OP_BRANCH) {
        RAW_DATA_AS(mem.arg1).i64 = inst.pc + inst.imm;
        bool do_br = false;
        branch(inst.funct3, s1, s2, &do_br, &invalid_op);
        mem.arg0 = (do_br?1:0);
    }
    else if(inst.opcode == OP_LOAD || inst.opcode == OP_LOADFP) {
        mem.vaddr = s1 + inst.imm;
    }
    else if(inst.opcode == OP_STORE || inst.opcode == OP_STOREFP || inst.opcode == OP_AMO) {
        mem.vaddr = s1 + inst.imm;
        mem.arg0 = s2;
    }
    else if(inst.opcode == OP_OPIMM) {
        alu64((inst.funct7 << 3) | inst.funct3, s1, inst.imm, &mem.arg0, &invalid_op);
    }
    else if(inst.opcode == OP_OPIMM32) {
        alu32((inst.funct7 << 3) | inst.funct3, s1, inst.imm, &mem.arg0, &invalid_op);
    }
    else if(inst.opcode == OP_OP) {
        alu64((inst.funct7 << 3) | inst.funct3, s1, s2, &mem.arg0, &invalid_op);
    }
    else if(inst.opcode == OP_OP32) {
        alu32((inst.funct7 << 3) | inst.funct3, s1, s2, &mem.arg0, &invalid_op);
    }
    else if(inst.opcode == OP_MADD) {
        switch (inst.funct7 & 3)
        {
        case 0: RAW_DATA_AS(mem.arg0).f32 = RAW_DATA_AS(s1).f32 * RAW_DATA_AS(s2).f32 + RAW_DATA_AS(s3).f32; break;
        case 1: RAW_DATA_AS(mem.arg0).f64 = RAW_DATA_AS(s1).f64 * RAW_DATA_AS(s2).f64 + RAW_DATA_AS(s3).f64; break;
        default: invalid_op = true;
        }
    }
    else if(inst.opcode == OP_MSUB) {
        switch (inst.funct7 & 3)
        {
        case 0: RAW_DATA_AS(mem.arg0).f32 = RAW_DATA_AS(s1).f32 * RAW_DATA_AS(s2).f32 - RAW_DATA_AS(s3).f32; break;
        case 1: RAW_DATA_AS(mem.arg0).f64 = RAW_DATA_AS(s1).f64 * RAW_DATA_AS(s2).f64 - RAW_DATA_AS(s3).f64; break;
        default: invalid_op = true;
        }
    }
    else if(inst.opcode == OP_NMSUB) {
        switch (inst.funct7 & 3)
        {
        case 0: RAW_DATA_AS(mem.arg0).f32 = -(RAW_DATA_AS(s1).f32 * RAW_DATA_AS(s2).f32) + RAW_DATA_AS(s3).f32; break;
        case 1: RAW_DATA_AS(mem.arg0).f64 = -(RAW_DATA_AS(s1).f64 * RAW_DATA_AS(s2).f64) + RAW_DATA_AS(s3).f64; break;
        default: invalid_op = true;
        }
    }
    else if(inst.opcode == OP_NMADD) {
        switch (inst.funct7 & 3)
        {
        case 0: RAW_DATA_AS(mem.arg0).f32 = -(RAW_DATA_AS(s1).f32 * RAW_DATA_AS(s2).f32) - RAW_DATA_AS(s3).f32; break;
        case 1: RAW_DATA_AS(mem.arg0).f64 = -(RAW_DATA_AS(s1).f64 * RAW_DATA_AS(s2).f64) - RAW_DATA_AS(s3).f64; break;
        default: invalid_op = true;
        }
    }
    else if(inst.opcode == OP_OPFP) {
        switch (inst.funct7 & 3)
        {
        case 0: fpu64(((inst.funct7 >> 2) << 8) | (inst.rs2 << 3) | inst.funct3, s1, s2, &mem.arg0, &invalid_op, &mem.arg1); break;
        case 1: fpu32(((inst.funct7 >> 2) << 8) | (inst.rs2 << 3) | inst.funct3, s1, s2, &mem.arg0, &invalid_op, &mem.arg1); break;
        default: invalid_op = true;
        }
    } else if(inst.opcode == OP_SYSTEM) {
        if(!(inst.instflag & RVINSTFLAG_S1INT)) {
            // For CSRXXI
            mem.arg0 = inst.rs1;
        }
        mem.arg1 = inst.imm;
    }

    if(invalid_op) {
        mem.exception = ((1 << 30) | RVEXPT_ILLEGAL_INST);
    }

    ex_out_push(mem);
    ex_in_pop();
}

