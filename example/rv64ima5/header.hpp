#pragma once

#include <cstdint>
#include <defhelper.hpp>

CONFIG(ALU_ADD, 0);
CONFIG(ALU_SUB, 1);
CONFIG(ALU_SLL, 2);
CONFIG(ALU_SLT, 3);
CONFIG(ALU_SLTU, 4);
CONFIG(ALU_XOR, 5);
CONFIG(ALU_SRL, 6);
CONFIG(ALU_SRA, 7);
CONFIG(ALU_OR, 8);
CONFIG(ALU_AND, 9);
CONFIG(ALU_COPY_RS1, 10);
CONFIG(ALU_COPY_RS2, 11);

CONFIG(BR_NONE, 0);
CONFIG(BR_EQ, 1);
CONFIG(BR_NE, 2);
CONFIG(BR_LT, 3);
CONFIG(BR_GE, 4);
CONFIG(BR_LTU, 5);
CONFIG(BR_GEU, 6);

CONFIG(MDU_NONE, 0);
CONFIG(MDU_MUL, 1);
CONFIG(MDU_MULH, 2);
CONFIG(MDU_MULHSU, 3);
CONFIG(MDU_MULHU, 4);
CONFIG(MDU_DIV, 5);
CONFIG(MDU_DIVU, 6);
CONFIG(MDU_REM, 7);
CONFIG(MDU_REMU, 8);

CONFIG(AMO_NONE, 0);
CONFIG(AMO_SWAP, 1);
CONFIG(AMO_ADD, 2);
CONFIG(AMO_XOR, 3);
CONFIG(AMO_AND, 4);
CONFIG(AMO_OR, 5);
CONFIG(AMO_MIN, 6);
CONFIG(AMO_MAX, 7);
CONFIG(AMO_MINU, 8);
CONFIG(AMO_MAXU, 9);

STRUCT(RegReadPair) {
    uint64_t rs1_val;
    uint64_t rs2_val;
};

STRUCT(DecodedInst) {
    bool valid;
    bool writes_rd;
    bool use_rs1;
    bool use_rs2;
    bool is_load;
    bool is_store;
    bool is_branch;
    bool is_jal;
    bool is_jalr;
    bool is_lui;
    bool is_auipc;
    bool is_ebreak;
    bool is_muldiv;
    bool is_atomic;
    bool is_lr;
    bool is_sc;
    uint8_t rd;
    uint8_t rs1;
    uint8_t rs2;
    uint8_t alu_op;
    uint8_t branch_op;
    uint8_t mem_width;
    bool mem_unsigned;
    uint8_t muldiv_op;
    uint8_t atomic_op;
    uint32_t inst;
    uint64_t imm;
};

STRUCT(ExecRequest) {
    uint64_t pc;
    uint64_t rs1_val;
    uint64_t rs2_val;
    DecodedInst dec;
};

STRUCT(ExecResult) {
    bool valid;
    bool branch_taken;
    uint64_t branch_target;
    uint64_t result;
    uint64_t mem_addr;
    uint64_t store_data;
};

STRUCT(MemRequest) {
    bool valid;
    bool is_write;
    bool is_atomic;
    bool is_lr;
    bool is_sc;
    bool mem_unsigned;
    uint8_t width;
    uint8_t atomic_op;
    uint64_t addr;
    uint64_t wdata;
};

STRUCT(MemResponse) {
    uint64_t rdata;
    bool success;
};

STRUCT(IfIdStage) {
    bool valid;
    uint64_t pc;
    uint32_t inst;
};

STRUCT(IdExStage) {
    bool valid;
    uint64_t pc;
    uint64_t rs1_val;
    uint64_t rs2_val;
    DecodedInst dec;
};

STRUCT(ExMemStage) {
    bool valid;
    uint64_t pc;
    ExecResult ex;
    DecodedInst dec;
};

STRUCT(MemWbStage) {
    bool valid;
    uint64_t pc;
    uint32_t inst;
    bool writes_rd;
    uint8_t rd;
    uint64_t wb_data;
    bool halt;
};

STRUCT(CoreStatus) {
    uint64_t cycle;
    uint64_t pc;
    bool halted;
    bool if_valid;
    bool id_valid;
    bool ex_valid;
    bool mem_valid;
    bool wb_valid;
};
