#pragma once

#include "../header.hpp"
#include "ALUPipeline.hpp"
#include "LSUPipeline.hpp"

PARAMETER(ALU_LANES, 2);
PARAMETER(LSU_LANES, 1);

HELPER() {
inline bool op_writes_dest(uint8_t opcode, uint8_t rd) {
    return rd != 0U && opcode != OP_STORE && opcode != OP_HALT && opcode != OP_NOP;
}

inline bool op_is_lsu(uint8_t opcode) {
    return opcode == OP_LOAD || opcode == OP_STORE;
}
}

REGISTER(cycle, uint64_t) {
    cycle = 0;
}

REGISTER(dispatched, uint64_t) {
    dispatched = 0;
}

REGISTER(issued, uint64_t) {
    issued = 0;
}

REGISTER(committed, uint64_t) {
    committed = 0;
}

REGISTER(seq_alloc, uint64_t) {
    seq_alloc = 1;
}

REGISTER(rob_head, uint32_t) {
    rob_head = 0;
}

REGISTER(rob_tail, uint32_t) {
    rob_tail = 0;
}

REGISTER(rob_count, uint32_t) {
    rob_count = 0;
}

REGISTER(iq_count, uint32_t) {
    iq_count = 0;
}

REGISTER(ingress_head, uint32_t) {
    ingress_head = 0;
}

REGISTER(ingress_tail, uint32_t) {
    ingress_tail = 0;
}

REGISTER(ingress_count, uint32_t) {
    ingress_count = 0;
}

REGISTER(free_head, uint32_t) {
    free_head = 0;
}

REGISTER(free_tail, uint32_t) {
    free_tail = PHYS_REGS - ARCH_REGS;
}

REGISTER(free_count, uint32_t) {
    free_count = PHYS_REGS - ARCH_REGS;
}

REGISTER(halted, bool) {
    halted = false;
}

REGISTER_ARRAY1(map_table, uint8_t, ARCH_REGS, 1) {
    for (int i = 0; i < ARCH_REGS; ++i) {
        map_table[i] = static_cast<uint8_t>(i);
    }
}

REGISTER_ARRAY1(phys_ready, bool, PHYS_REGS, 1) {
    for (int i = 0; i < PHYS_REGS; ++i) {
        phys_ready[i] = i < ARCH_REGS;
    }
}

REGISTER_ARRAY1(phys_value, uint64_t, PHYS_REGS, 1) {
    for (int i = 0; i < PHYS_REGS; ++i) {
        phys_value[i] = 0;
    }
}

REGISTER_ARRAY1(free_tags, uint8_t, PHYS_REGS, 1) {
    for (int i = 0; i < PHYS_REGS; ++i) {
        free_tags[i] = 0;
    }
    for (int i = 0; i < PHYS_REGS - ARCH_REGS; ++i) {
        free_tags[i] = static_cast<uint8_t>(ARCH_REGS + i);
    }
}

REGISTER_ARRAY1(rob, RobEntry, ROB_SIZE, 1) {
    for (int i = 0; i < ROB_SIZE; ++i) {
        rob[i].valid = false;
        rob[i].ready = false;
        rob[i].has_dest = false;
        rob[i].opcode = OP_NOP;
        rob[i].arch_rd = 0;
        rob[i].dst_phys = 0;
        rob[i].old_phys = 0;
        rob[i].value = 0;
        rob[i].seq = 0;
    }
}

REGISTER_ARRAY1(iq, IssueEntry, IQ_SIZE, 1) {
    for (int i = 0; i < IQ_SIZE; ++i) {
        iq[i].valid = false;
        iq[i].is_lsu = false;
        iq[i].opcode = OP_NOP;
        iq[i].rob_idx = 0;
        iq[i].dst_phys = 0;
        iq[i].src1_tag = 0;
        iq[i].src2_tag = 0;
        iq[i].src1_ready = true;
        iq[i].src2_ready = true;
        iq[i].src1_value = 0;
        iq[i].src2_value = 0;
        iq[i].imm = 0;
        iq[i].seq = 0;
    }
}

REGISTER_ARRAY1(push_valid, bool, INGRESS_WIDTH, 1) {
    for (int i = 0; i < INGRESS_WIDTH; ++i) {
        push_valid[i] = false;
    }
}

REGISTER_ARRAY1(push_data, BackendInstr, INGRESS_WIDTH, 1) {
    for (int i = 0; i < INGRESS_WIDTH; ++i) {
        push_data[i].valid = false;
        push_data[i].opcode = OP_NOP;
        push_data[i].rd = 0;
        push_data[i].rs1 = 0;
        push_data[i].rs2 = 0;
        push_data[i].imm = 0;
    }
}

REGISTER_ARRAY1(ingress_buf, BackendInstr, FRONTEND_QUEUE_DEPTH, 1) {
    for (int i = 0; i < FRONTEND_QUEUE_DEPTH; ++i) {
        ingress_buf[i].valid = false;
        ingress_buf[i].opcode = OP_NOP;
        ingress_buf[i].rd = 0;
        ingress_buf[i].rs1 = 0;
        ingress_buf[i].rs2 = 0;
        ingress_buf[i].imm = 0;
    }
}

SERVICE_READY(push_inst, !push_valid[IDX], ARRAY(INGRESS_WIDTH), ARG(BackendInstr) inst) {
    BackendInstr copy = inst;
    copy.valid = true;
    push_data.setnext<0>(IDX, copy);
    push_valid.setnext<0>(IDX, true);
}

REQUEST(mem_req0, ARG(MemRequest) req);
REQUEST_READY(mem_resp0, RESP(MemResponse) resp);
REQUEST(mem_req1, ARG(MemRequest) req);
REQUEST_READY(mem_resp1, RESP(MemResponse) resp);

CHILD_INSTANCE(ALUPipeline, alu0);
CHILD_INSTANCE(ALUPipeline, alu1);
CHILD_INSTANCE(LSUPipeline, lsu0);
CHILD_INSTANCE(LSUPipeline, lsu1);

USE_CHILD_SERVICE_PORT(alu0, issue, alu0_issue, ARG(FuRequest) req);
USE_CHILD_SERVICE_PORT(alu0, pop_result, alu0_pop_result, RESP(WritebackEvent) wb);
USE_CHILD_QUERY(alu0, can_accept, alu0_can_accept, bool);
USE_CHILD_QUERY(alu0, has_result, alu0_has_result, bool);

USE_CHILD_SERVICE_PORT(alu1, issue, alu1_issue, ARG(FuRequest) req);
USE_CHILD_SERVICE_PORT(alu1, pop_result, alu1_pop_result, RESP(WritebackEvent) wb);
USE_CHILD_QUERY(alu1, can_accept, alu1_can_accept, bool);
USE_CHILD_QUERY(alu1, has_result, alu1_has_result, bool);

USE_CHILD_SERVICE_PORT(lsu0, issue, lsu0_issue, ARG(FuRequest) req);
USE_CHILD_SERVICE_PORT(lsu0, pop_result, lsu0_pop_result, RESP(WritebackEvent) wb);
USE_CHILD_QUERY(lsu0, can_accept, lsu0_can_accept, bool);
USE_CHILD_QUERY(lsu0, has_result, lsu0_has_result, bool);

USE_CHILD_SERVICE_PORT(lsu1, issue, lsu1_issue, ARG(FuRequest) req);
USE_CHILD_SERVICE_PORT(lsu1, pop_result, lsu1_pop_result, RESP(WritebackEvent) wb);
USE_CHILD_QUERY(lsu1, can_accept, lsu1_can_accept, bool);
USE_CHILD_QUERY(lsu1, has_result, lsu1_has_result, bool);

CONNECT_CR_R(lsu0, mem_req, mem_req0);
CONNECT_CR_R(lsu0, mem_resp, mem_resp0);
CONNECT_CR_R(lsu1, mem_req, mem_req1);
CONNECT_CR_R(lsu1, mem_resp, mem_resp1);

QUERY(status, BackendStatus) {
    BackendStatus s;
    s.cycle = cycle;
    s.dispatched = dispatched;
    s.issued = issued;
    s.committed = committed;
    s.rob_count = rob_count;
    s.iq_count = iq_count;
    s.free_phys = free_count;
    s.halted = halted;
    return s;
}

QUERY(regs, ArchRegSnapshot) {
    ArchRegSnapshot s;
    s.x1 = phys_value[map_table[1]];
    s.x2 = phys_value[map_table[2]];
    s.x3 = phys_value[map_table[3]];
    s.x4 = phys_value[map_table[4]];
    s.x5 = phys_value[map_table[5]];
    s.x6 = phys_value[map_table[6]];
    s.x7 = phys_value[map_table[7]];
    return s;
}

TICK_IMPL() {
    if (halted) {
        cycle.setnext(cycle + 1);
        return;
    }

    uint8_t next_map[ARCH_REGS];
    bool next_phys_ready[PHYS_REGS];
    uint64_t next_phys_value[PHYS_REGS];
    uint8_t next_free_tags[PHYS_REGS];
    RobEntry next_rob[ROB_SIZE];
    IssueEntry next_iq[IQ_SIZE];

    for (int i = 0; i < ARCH_REGS; ++i) next_map[i] = map_table[i];
    for (int i = 0; i < PHYS_REGS; ++i) {
        next_phys_ready[i] = phys_ready[i];
        next_phys_value[i] = phys_value[i];
        next_free_tags[i] = free_tags[i];
    }
    for (int i = 0; i < ROB_SIZE; ++i) next_rob[i] = rob[i];
    for (int i = 0; i < IQ_SIZE; ++i) next_iq[i] = iq[i];

    BackendInstr next_ingress_buf[FRONTEND_QUEUE_DEPTH];
    for (int i = 0; i < FRONTEND_QUEUE_DEPTH; ++i) next_ingress_buf[i] = ingress_buf[i];

    uint64_t next_dispatched = dispatched;
    uint64_t next_issued = issued;
    uint64_t next_committed = committed;
    uint64_t next_seq_alloc = seq_alloc;
    uint32_t next_rob_head = rob_head;
    uint32_t next_rob_tail = rob_tail;
    uint32_t next_rob_count = rob_count;
    uint32_t next_iq_count = iq_count;
    uint32_t next_ingress_head = ingress_head;
    uint32_t next_ingress_tail = ingress_tail;
    uint32_t next_ingress_count = ingress_count;
    uint32_t next_free_head = free_head;
    uint32_t next_free_tail = free_tail;
    uint32_t next_free_count = free_count;
    bool next_halted = halted;

    uint32_t ingress_room = static_cast<uint32_t>(FRONTEND_QUEUE_DEPTH) - next_ingress_count;
    if (ingress_room > 0U) {
        for (int i = 0; i < INGRESS_WIDTH && ingress_room > 0U; ++i) {
            if (push_valid[i]) {
                next_ingress_buf[next_ingress_tail] = push_data[i];
                next_ingress_tail = (next_ingress_tail + 1U) % FRONTEND_QUEUE_DEPTH;
                next_ingress_count++;
                ingress_room--;
                push_valid.setnext<0>(i, false);
            }
        }
    }

    WritebackEvent wb;
    if (alu0_has_result() && alu0_pop_result(wb)) {
        if (wb.has_dest && wb.dst_phys != 0U) {
            next_phys_ready[wb.dst_phys] = true;
            next_phys_value[wb.dst_phys] = wb.value;
        }
        next_rob[wb.rob_idx].ready = true;
        next_rob[wb.rob_idx].value = wb.value;
        for (int i = 0; i < IQ_SIZE; ++i) {
            if (!next_iq[i].valid) continue;
            if (!next_iq[i].src1_ready && next_iq[i].src1_tag == wb.dst_phys) {
                next_iq[i].src1_ready = true;
                next_iq[i].src1_value = wb.value;
            }
            if (!next_iq[i].src2_ready && next_iq[i].src2_tag == wb.dst_phys) {
                next_iq[i].src2_ready = true;
                next_iq[i].src2_value = wb.value;
            }
        }
    }
    if (ALU_LANES > 1 && alu1_has_result() && alu1_pop_result(wb)) {
        if (wb.has_dest && wb.dst_phys != 0U) {
            next_phys_ready[wb.dst_phys] = true;
            next_phys_value[wb.dst_phys] = wb.value;
        }
        next_rob[wb.rob_idx].ready = true;
        next_rob[wb.rob_idx].value = wb.value;
        for (int i = 0; i < IQ_SIZE; ++i) {
            if (!next_iq[i].valid) continue;
            if (!next_iq[i].src1_ready && next_iq[i].src1_tag == wb.dst_phys) {
                next_iq[i].src1_ready = true;
                next_iq[i].src1_value = wb.value;
            }
            if (!next_iq[i].src2_ready && next_iq[i].src2_tag == wb.dst_phys) {
                next_iq[i].src2_ready = true;
                next_iq[i].src2_value = wb.value;
            }
        }
    }
    if (lsu0_has_result() && lsu0_pop_result(wb)) {
        if (wb.has_dest && wb.dst_phys != 0U) {
            next_phys_ready[wb.dst_phys] = true;
            next_phys_value[wb.dst_phys] = wb.value;
        }
        next_rob[wb.rob_idx].ready = true;
        next_rob[wb.rob_idx].value = wb.value;
        for (int i = 0; i < IQ_SIZE; ++i) {
            if (!next_iq[i].valid) continue;
            if (!next_iq[i].src1_ready && next_iq[i].src1_tag == wb.dst_phys) {
                next_iq[i].src1_ready = true;
                next_iq[i].src1_value = wb.value;
            }
            if (!next_iq[i].src2_ready && next_iq[i].src2_tag == wb.dst_phys) {
                next_iq[i].src2_ready = true;
                next_iq[i].src2_value = wb.value;
            }
        }
    }
    if (LSU_LANES > 1 && lsu1_has_result() && lsu1_pop_result(wb)) {
        if (wb.has_dest && wb.dst_phys != 0U) {
            next_phys_ready[wb.dst_phys] = true;
            next_phys_value[wb.dst_phys] = wb.value;
        }
        next_rob[wb.rob_idx].ready = true;
        next_rob[wb.rob_idx].value = wb.value;
        for (int i = 0; i < IQ_SIZE; ++i) {
            if (!next_iq[i].valid) continue;
            if (!next_iq[i].src1_ready && next_iq[i].src1_tag == wb.dst_phys) {
                next_iq[i].src1_ready = true;
                next_iq[i].src1_value = wb.value;
            }
            if (!next_iq[i].src2_ready && next_iq[i].src2_tag == wb.dst_phys) {
                next_iq[i].src2_ready = true;
                next_iq[i].src2_value = wb.value;
            }
        }
    }

    for (int retire = 0; retire < COMMIT_WIDTH; ++retire) {
        if (next_rob_count == 0U) {
            break;
        }
        RobEntry head_entry = next_rob[next_rob_head];
        if (!head_entry.valid || !head_entry.ready) {
            break;
        }
        next_committed++;
        if (head_entry.has_dest && head_entry.old_phys != 0U) {
            next_free_tags[next_free_tail] = head_entry.old_phys;
            next_free_tail = (next_free_tail + 1U) % PHYS_REGS;
            next_free_count++;
        }
        if (head_entry.opcode == OP_HALT) {
            next_halted = true;
        }
        head_entry.valid = false;
        head_entry.ready = false;
        head_entry.has_dest = false;
        head_entry.opcode = OP_NOP;
        head_entry.arch_rd = 0;
        head_entry.dst_phys = 0;
        head_entry.old_phys = 0;
        head_entry.value = 0;
        head_entry.seq = 0;
        next_rob[next_rob_head] = head_entry;
        next_rob_head = (next_rob_head + 1U) % ROB_SIZE;
        next_rob_count--;
    }

    uint32_t iq_free = static_cast<uint32_t>(IQ_SIZE) - next_iq_count;
    uint32_t rob_free = static_cast<uint32_t>(ROB_SIZE) - next_rob_count;
    uint32_t dispatch_limit = next_ingress_count;
    if (dispatch_limit > DISPATCH_WIDTH) dispatch_limit = DISPATCH_WIDTH;
    if (dispatch_limit > iq_free) dispatch_limit = iq_free;
    if (dispatch_limit > rob_free) dispatch_limit = rob_free;
    if (dispatch_limit > next_free_count) dispatch_limit = next_free_count;

    if (dispatch_limit > 0U) {
        uint32_t probe_head = next_ingress_head;
        for (uint32_t k = 0; k < dispatch_limit; ++k) {
            BackendInstr inst = next_ingress_buf[probe_head];
            probe_head = (probe_head + 1U) % FRONTEND_QUEUE_DEPTH;
            if (!inst.valid) continue;
            bool need_dest = op_writes_dest(inst.opcode, inst.rd);

            int iq_idx = -1;
            for (int i = 0; i < IQ_SIZE; ++i) {
                if (!next_iq[i].valid) {
                    iq_idx = i;
                    break;
                }
            }
            if (iq_idx < 0) {
                break;
            }

            uint8_t src1_tag = inst.rs1 == 0U ? 0U : next_map[inst.rs1];
            uint8_t src2_tag = inst.rs2 == 0U ? 0U : next_map[inst.rs2];
            bool src1_ready = inst.rs1 == 0U ? true : next_phys_ready[src1_tag];
            bool src2_ready = inst.rs2 == 0U ? true : next_phys_ready[src2_tag];
            uint64_t src1_value = inst.rs1 == 0U ? 0ULL : next_phys_value[src1_tag];
            uint64_t src2_value = inst.rs2 == 0U ? 0ULL : next_phys_value[src2_tag];

            uint8_t dst_phys = 0;
            uint8_t old_phys = inst.rd == 0U ? 0U : next_map[inst.rd];
            if (need_dest) {
                dst_phys = next_free_tags[next_free_head];
                next_free_head = (next_free_head + 1U) % PHYS_REGS;
                next_free_count--;
                next_map[inst.rd] = dst_phys;
                next_phys_ready[dst_phys] = false;
                next_phys_value[dst_phys] = 0;
            }

            RobEntry re;
            re.valid = true;
            re.ready = false;
            re.has_dest = need_dest;
            re.opcode = inst.opcode;
            re.arch_rd = inst.rd;
            re.dst_phys = dst_phys;
            re.old_phys = old_phys;
            re.value = 0;
            re.seq = next_seq_alloc;
            next_rob[next_rob_tail] = re;

            IssueEntry iqe;
            iqe.valid = true;
            iqe.is_lsu = op_is_lsu(inst.opcode);
            iqe.opcode = inst.opcode;
            iqe.rob_idx = static_cast<uint8_t>(next_rob_tail);
            iqe.dst_phys = dst_phys;
            iqe.src1_tag = src1_tag;
            iqe.src2_tag = src2_tag;
            iqe.src1_ready = src1_ready;
            iqe.src2_ready = src2_ready;
            iqe.src1_value = src1_value;
            iqe.src2_value = src2_value;
            iqe.imm = inst.imm;
            iqe.seq = next_seq_alloc;
            next_iq[iq_idx] = iqe;

            next_rob_tail = (next_rob_tail + 1U) % ROB_SIZE;
            next_rob_count++;
            next_iq_count++;
            next_dispatched++;
            next_seq_alloc++;
            next_ingress_buf[next_ingress_head].valid = false;
            next_ingress_head = (next_ingress_head + 1U) % FRONTEND_QUEUE_DEPTH;
            next_ingress_count--;
        }
    }

    int best_alu0 = -1;
    int best_alu1 = -1;
    int best_lsu0 = -1;
    int best_lsu1 = -1;
    uint64_t alu0_seq = 0;
    uint64_t alu1_seq = 0;
    uint64_t lsu0_seq = 0;
    uint64_t lsu1_seq = 0;

    bool can_alu0 = alu0_can_accept();
    bool can_alu1 = ALU_LANES > 1 && alu1_can_accept();
    bool can_lsu0 = lsu0_can_accept();
    bool can_lsu1 = LSU_LANES > 1 && lsu1_can_accept();

    for (int i = 0; i < IQ_SIZE; ++i) {
        if (!next_iq[i].valid || !next_iq[i].src1_ready || !next_iq[i].src2_ready) {
            continue;
        }
        if (next_iq[i].is_lsu) {
            if (can_lsu0 && (best_lsu0 < 0 || next_iq[i].seq < lsu0_seq)) {
                if (i != best_lsu1) {
                    best_lsu0 = i;
                    lsu0_seq = next_iq[i].seq;
                }
            } else if (can_lsu1 && (best_lsu1 < 0 || next_iq[i].seq < lsu1_seq) && i != best_lsu0) {
                best_lsu1 = i;
                lsu1_seq = next_iq[i].seq;
            }
        } else {
            if (can_alu0 && (best_alu0 < 0 || next_iq[i].seq < alu0_seq)) {
                if (i != best_alu1) {
                    best_alu0 = i;
                    alu0_seq = next_iq[i].seq;
                }
            } else if (can_alu1 && (best_alu1 < 0 || next_iq[i].seq < alu1_seq) && i != best_alu0) {
                best_alu1 = i;
                alu1_seq = next_iq[i].seq;
            }
        }
    }

    auto issue_to_lane = [&](int idx, bool use_alu0, bool use_alu1, bool use_lsu0, bool use_lsu1) {
        if (idx < 0) {
            return;
        }
        FuRequest req;
        req.valid = true;
        req.opcode = next_iq[idx].opcode;
        req.rob_idx = next_iq[idx].rob_idx;
        req.dst_phys = next_iq[idx].dst_phys;
        req.src1_value = next_iq[idx].src1_value;
        req.src2_value = next_iq[idx].src2_value;
        req.imm = next_iq[idx].imm;
        req.seq = next_iq[idx].seq;

        bool accepted = false;
        if (use_alu0) accepted = alu0_issue(req);
        if (use_alu1) accepted = alu1_issue(req);
        if (use_lsu0) accepted = lsu0_issue(req);
        if (use_lsu1) accepted = lsu1_issue(req);
        if (accepted) {
            next_iq[idx].valid = false;
            next_iq_count--;
            next_issued++;
        }
    };

    issue_to_lane(best_alu0, true, false, false, false);
    issue_to_lane(best_alu1, false, true, false, false);
    issue_to_lane(best_lsu0, false, false, true, false);
    issue_to_lane(best_lsu1, false, false, false, true);

    for (int i = 0; i < ARCH_REGS; ++i) {
        map_table.setnext<0>(i, next_map[i]);
    }
    for (int i = 0; i < PHYS_REGS; ++i) {
        phys_ready.setnext<0>(i, next_phys_ready[i]);
        phys_value.setnext<0>(i, next_phys_value[i]);
        free_tags.setnext<0>(i, next_free_tags[i]);
    }
    for (int i = 0; i < ROB_SIZE; ++i) {
        rob.setnext<0>(i, next_rob[i]);
    }
    for (int i = 0; i < IQ_SIZE; ++i) {
        iq.setnext<0>(i, next_iq[i]);
    }
    for (int i = 0; i < FRONTEND_QUEUE_DEPTH; ++i) {
        ingress_buf.setnext<0>(i, next_ingress_buf[i]);
    }

    dispatched.setnext(next_dispatched);
    issued.setnext(next_issued);
    committed.setnext(next_committed);
    seq_alloc.setnext(next_seq_alloc);
    rob_head.setnext(next_rob_head);
    rob_tail.setnext(next_rob_tail);
    rob_count.setnext(next_rob_count);
    iq_count.setnext(next_iq_count);
    ingress_head.setnext(next_ingress_head);
    ingress_tail.setnext(next_ingress_tail);
    ingress_count.setnext(next_ingress_count);
    free_head.setnext(next_free_head);
    free_tail.setnext(next_free_tail);
    free_count.setnext(next_free_count);
    halted.setnext(next_halted);
    cycle.setnext(cycle + 1);
}
