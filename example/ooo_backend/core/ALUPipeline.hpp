#pragma once

#include "../header.hpp"

PARAMETER(PIPE_SLOTS, 8);
PARAMETER(INPUT_DEPTH, 8);
PARAMETER(RESULT_DEPTH, 8);
PARAMETER(MUL_LATENCY, 3);
PARAMETER(DIV_LATENCY, 8);

HELPER() {
inline uint32_t alu_latency(const FuRequest &req) {
    if (req.opcode == OP_MUL) return static_cast<uint32_t>(MUL_LATENCY);
    if (req.opcode == OP_DIV) return static_cast<uint32_t>(DIV_LATENCY);
    return 1U;
}

inline uint64_t alu_execute(const FuRequest &req) {
    switch (req.opcode) {
        case OP_ADD:
            return req.src1_value + req.src2_value;
        case OP_ADDI:
            return req.src1_value + static_cast<uint64_t>(req.imm);
        case OP_SUB:
            return req.src1_value - req.src2_value;
        case OP_AND:
            return req.src1_value & req.src2_value;
        case OP_OR:
            return req.src1_value | req.src2_value;
        case OP_XOR:
            return req.src1_value ^ req.src2_value;
        case OP_MUL:
            return req.src1_value * req.src2_value;
        case OP_DIV:
            if (req.src2_value == 0) {
                return ~0ULL;
            }
            return static_cast<uint64_t>(static_cast<int64_t>(req.src1_value) / static_cast<int64_t>(req.src2_value));
        case OP_HALT:
            return 0;
        default:
            return 0;
    }
}
}

QUEUE(issueq, FuRequest, INPUT_DEPTH);
QUEUE(resultq, WritebackEvent, RESULT_DEPTH);

REGISTER_ARRAY1(slot_valid, bool, PIPE_SLOTS, 1) {
    for (int i = 0; i < PIPE_SLOTS; ++i) {
        slot_valid[i] = false;
    }
}

REGISTER_ARRAY1(slot_remain, uint32_t, PIPE_SLOTS, 1) {
    for (int i = 0; i < PIPE_SLOTS; ++i) {
        slot_remain[i] = 0;
    }
}

REGISTER_ARRAY1(slot_req, FuRequest, PIPE_SLOTS, 1) {
    for (int i = 0; i < PIPE_SLOTS; ++i) {
        slot_req[i].valid = false;
        slot_req[i].opcode = OP_NOP;
        slot_req[i].rob_idx = 0;
        slot_req[i].dst_phys = 0;
        slot_req[i].src1_value = 0;
        slot_req[i].src2_value = 0;
        slot_req[i].imm = 0;
        slot_req[i].seq = 0;
    }
}

SERVICE_READY(issue, issueq.enqready(), ARG(FuRequest) req) {
    issueq.enqnext(req);
}

SERVICE_READY(pop_result, resultq.deqvalid(), RESP(WritebackEvent) wb) {
    wb = resultq.front();
    resultq.deqnext();
}

QUERY(can_accept, bool) {
    return issueq.enqready();
}

QUERY(has_result, bool) {
    return resultq.deqvalid();
}

QUERY(status, UnitStatus) {
    UnitStatus s;
    s.inflight = 0;
    s.queued = issueq.deqvalid() ? 1U : 0U;
    for (int i = 0; i < PIPE_SLOTS; ++i) {
        if (slot_valid[i]) {
            s.inflight++;
        }
    }
    s.can_accept = issueq.enqready();
    s.has_result = resultq.deqvalid();
    return s;
}

TICK_IMPL() {
    bool next_valid[PIPE_SLOTS];
    uint32_t next_remain[PIPE_SLOTS];
    FuRequest next_req[PIPE_SLOTS];
    for (int i = 0; i < PIPE_SLOTS; ++i) {
        next_valid[i] = slot_valid[i];
        next_remain[i] = slot_remain[i];
        next_req[i] = slot_req[i];
    }

    int free_slot = -1;
    int ready_slot = -1;
    uint64_t ready_seq = 0;

    for (int i = 0; i < PIPE_SLOTS; ++i) {
        if (!next_valid[i] && free_slot < 0) {
            free_slot = i;
        }
        if (next_valid[i] && next_remain[i] > 0U) {
            next_remain[i]--;
        } else if (next_valid[i] && next_remain[i] == 0U) {
            if (ready_slot < 0 || next_req[i].seq < ready_seq) {
                ready_slot = i;
                ready_seq = next_req[i].seq;
            }
        }
    }

    if (ready_slot >= 0 && resultq.enqready()) {
        WritebackEvent wb;
        wb.valid = true;
        wb.has_dest = next_req[ready_slot].dst_phys != 0U;
        wb.rob_idx = next_req[ready_slot].rob_idx;
        wb.dst_phys = next_req[ready_slot].dst_phys;
        wb.value = alu_execute(next_req[ready_slot]);
        wb.seq = next_req[ready_slot].seq;
        resultq.enqnext(wb);
        next_valid[ready_slot] = false;
        next_remain[ready_slot] = 0U;
        FuRequest zero = next_req[ready_slot];
        zero.valid = false;
        zero.opcode = OP_NOP;
        zero.rob_idx = 0;
        zero.dst_phys = 0;
        zero.src1_value = 0;
        zero.src2_value = 0;
        zero.imm = 0;
        zero.seq = 0;
        next_req[ready_slot] = zero;
        if (free_slot < 0) {
            free_slot = ready_slot;
        }
    }

    if (free_slot >= 0 && issueq.deqvalid()) {
        FuRequest req = issueq.front();
        issueq.deqnext();
        next_valid[free_slot] = true;
        next_remain[free_slot] = alu_latency(req) - 1U;
        next_req[free_slot] = req;
    }

    for (int i = 0; i < PIPE_SLOTS; ++i) {
        slot_valid.setnext<0>(i, next_valid[i]);
        slot_remain.setnext<0>(i, next_remain[i]);
        slot_req.setnext<0>(i, next_req[i]);
    }
}
