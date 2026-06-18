#pragma once

#include <array>
#include <cstdint>
#include <defhelper.hpp>

CONFIG(ARCH_REGS, 8);
CONFIG(PHYS_REGS, 20);
CONFIG(ROB_SIZE, 16);
CONFIG(IQ_SIZE, 16);
CONFIG(INGRESS_WIDTH, 2);
CONFIG(DISPATCH_WIDTH, 2);
CONFIG(COMMIT_WIDTH, 2);
CONFIG(FRONTEND_QUEUE_DEPTH, 16);

CONFIG(OP_NOP, 0);
CONFIG(OP_ADD, 1);
CONFIG(OP_ADDI, 2);
CONFIG(OP_SUB, 3);
CONFIG(OP_AND, 4);
CONFIG(OP_OR, 5);
CONFIG(OP_XOR, 6);
CONFIG(OP_MUL, 7);
CONFIG(OP_DIV, 8);
CONFIG(OP_LOAD, 9);
CONFIG(OP_STORE, 10);
CONFIG(OP_HALT, 11);

STRUCT(BackendInstr) {
    bool valid;
    uint8_t opcode;
    uint8_t rd;
    uint8_t rs1;
    uint8_t rs2;
    int64_t imm;
};

STRUCT(IssueEntry) {
    bool valid;
    bool is_lsu;
    uint8_t opcode;
    uint8_t rob_idx;
    uint8_t dst_phys;
    uint8_t src1_tag;
    uint8_t src2_tag;
    bool src1_ready;
    bool src2_ready;
    uint64_t src1_value;
    uint64_t src2_value;
    int64_t imm;
    uint64_t seq;
};

STRUCT(RobEntry) {
    bool valid;
    bool ready;
    bool has_dest;
    uint8_t opcode;
    uint8_t arch_rd;
    uint8_t dst_phys;
    uint8_t old_phys;
    uint64_t value;
    uint64_t seq;
};

STRUCT(FuRequest) {
    bool valid;
    uint8_t opcode;
    uint8_t rob_idx;
    uint8_t dst_phys;
    uint64_t src1_value;
    uint64_t src2_value;
    int64_t imm;
    uint64_t seq;
};

STRUCT(WritebackEvent) {
    bool valid;
    bool has_dest;
    uint8_t rob_idx;
    uint8_t dst_phys;
    uint64_t value;
    uint64_t seq;
};

STRUCT(MemRequest) {
    bool valid;
    bool is_store;
    uint8_t rob_idx;
    uint8_t dst_phys;
    uint64_t addr;
    uint64_t data;
    uint64_t seq;
};

STRUCT(MemResponse) {
    bool valid;
    uint8_t rob_idx;
    uint8_t dst_phys;
    uint64_t data;
    uint64_t seq;
};

STRUCT(BackendStatus) {
    uint64_t cycle;
    uint64_t dispatched;
    uint64_t issued;
    uint64_t committed;
    uint32_t rob_count;
    uint32_t iq_count;
    uint32_t free_phys;
    bool halted;
};

STRUCT(ArchRegSnapshot) {
    uint64_t x1;
    uint64_t x2;
    uint64_t x3;
    uint64_t x4;
    uint64_t x5;
    uint64_t x6;
    uint64_t x7;
};

STRUCT(UnitStatus) {
    uint32_t inflight;
    uint32_t queued;
    bool can_accept;
    bool has_result;
};
