#pragma once

#include "../header.hpp"
#include "Decoder.hpp"
#include "ExecuteUnit.hpp"
#include "RegisterFile.hpp"

HELPER() {
inline IfIdStage make_ifid_invalid() {
    IfIdStage s;
    s.valid = false;
    s.pc = 0;
    s.inst = 0;
    return s;
}

inline DecodedInst make_decoded_invalid() {
    DecodedInst d;
    d.valid = false;
    d.writes_rd = false;
    d.use_rs1 = false;
    d.use_rs2 = false;
    d.is_load = false;
    d.is_store = false;
    d.is_branch = false;
    d.is_jal = false;
    d.is_jalr = false;
    d.is_lui = false;
    d.is_auipc = false;
    d.is_ebreak = false;
    d.is_muldiv = false;
    d.is_atomic = false;
    d.is_lr = false;
    d.is_sc = false;
    d.rd = 0;
    d.rs1 = 0;
    d.rs2 = 0;
    d.alu_op = static_cast<uint8_t>(ALU_ADD);
    d.branch_op = static_cast<uint8_t>(BR_NONE);
    d.mem_width = 8;
    d.mem_unsigned = false;
    d.muldiv_op = static_cast<uint8_t>(MDU_NONE);
    d.atomic_op = static_cast<uint8_t>(AMO_NONE);
    d.inst = 0;
    d.imm = 0;
    return d;
}

inline IdExStage make_idex_invalid() {
    IdExStage s;
    s.valid = false;
    s.pc = 0;
    s.rs1_val = 0;
    s.rs2_val = 0;
    s.dec = make_decoded_invalid();
    return s;
}

inline ExecResult make_exec_zero() {
    ExecResult e;
    e.valid = false;
    e.branch_taken = false;
    e.branch_target = 0;
    e.result = 0;
    e.mem_addr = 0;
    e.store_data = 0;
    return e;
}

inline ExMemStage make_exmem_invalid() {
    ExMemStage s;
    s.valid = false;
    s.pc = 0;
    s.ex = make_exec_zero();
    s.dec = make_decoded_invalid();
    return s;
}

inline MemWbStage make_memwb_invalid() {
    MemWbStage s;
    s.valid = false;
    s.pc = 0;
    s.inst = 0;
    s.writes_rd = false;
    s.rd = 0;
    s.wb_data = 0;
    s.halt = false;
    return s;
}

inline uint64_t load_extend_data(uint64_t value, uint8_t width, bool is_unsigned) {
    if (width == 1) {
        if (is_unsigned) return value & 0xffULL;
        return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(value & 0xffULL)));
    }
    if (width == 2) {
        if (is_unsigned) return value & 0xffffULL;
        return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(value & 0xffffULL)));
    }
    if (width == 4) {
        if (is_unsigned) return value & 0xffffffffULL;
        return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(value & 0xffffffffULL)));
    }
    return value;
}
}

REGISTER(cycle, uint64_t) {
    cycle = 0;
}

REGISTER(pc, uint64_t) {
    pc = 0;
}

REGISTER(halted, bool) {
    halted = false;
}

REGISTER(if_id, IfIdStage) {
    if_id = make_ifid_invalid();
}

REGISTER(id_ex, IdExStage) {
    id_ex = make_idex_invalid();
}

REGISTER(ex_mem, ExMemStage) {
    ex_mem = make_exmem_invalid();
}

REGISTER(mem_wb, MemWbStage) {
    mem_wb = make_memwb_invalid();
}

REGISTER(fetch_waiting, bool) {
    fetch_waiting = false;
}

REGISTER(fetch_drop_resp, bool) {
    fetch_drop_resp = false;
}

REGISTER(fetch_req_pc, uint64_t) {
    fetch_req_pc = 0;
}

REGISTER(mem_waiting, bool) {
    mem_waiting = false;
}

REQUEST(icache_req, ARG(uint64_t) pc);
REQUEST(icache_resp, RESP(bool) hit, RESP(uint32_t) inst);
REQUEST(dcache_req, ARG(MemRequest) req);
REQUEST(dcache_resp, RESP(bool) hit, RESP(MemResponse) resp);
REQUEST(trace_wb, ARG(uint32_t) rd, ARG(uint64_t) data, ARG(bool) wen);
REQUEST(trace_halt, ARG(uint64_t) pc);

CHILD_INSTANCE(Decoder, dec);
CHILD_INSTANCE(ExecuteUnit, exu);
CHILD_INSTANCE(RegisterFile, rf);

USE_CHILD_SERVICE_PORT(dec, decode, decode_inst, ARG(uint32_t) inst, RESP(DecodedInst) out);
USE_CHILD_SERVICE_PORT(exu, exec, exec_inst, ARG(ExecRequest) req, RESP(ExecResult) out);
USE_CHILD_SERVICE_PORT(rf, read2, rf_read2, ARG(uint32_t) rs1, ARG(uint32_t) rs2, RESP(RegReadPair) out);
USE_CHILD_SERVICE_PORT(rf, write, rf_write, ARG(uint32_t) rd, ARG(uint64_t) data, ARG(bool) wen);
USE_CHILD_QUERY(rf, zero_and_ra, rf_zero_and_ra, RegReadPair);

QUERY(status, CoreStatus) {
    CoreStatus s;
    s.cycle = cycle;
    s.pc = pc;
    s.halted = halted;
    s.if_valid = if_id.get().valid;
    s.id_valid = id_ex.get().valid;
    s.ex_valid = ex_mem.get().valid;
    s.mem_valid = mem_wb.get().valid;
    s.wb_valid = mem_wb.get().valid;
    return s;
}

QUERY(peek_links, RegReadPair) {
    return rf_zero_and_ra();
}

TICK_IMPL() {
    uint64_t cur_pc = pc;
    bool cur_halted = halted;
    IfIdStage cur_if = if_id.get();
    IdExStage cur_id = id_ex.get();
    ExMemStage cur_ex = ex_mem.get();
    MemWbStage cur_wb = mem_wb.get();
    bool cur_fetch_waiting = fetch_waiting;
    bool cur_fetch_drop = fetch_drop_resp;
    uint64_t cur_fetch_pc = fetch_req_pc;
    bool cur_mem_waiting = mem_waiting;

    uint64_t next_pc = cur_pc;
    bool next_halted = cur_halted;
    IfIdStage next_if = cur_if;
    IdExStage next_id = cur_id;
    ExMemStage next_ex = cur_ex;
    MemWbStage next_wb = make_memwb_invalid();
    bool next_fetch_waiting = cur_fetch_waiting;
    bool next_fetch_drop = cur_fetch_drop;
    uint64_t next_fetch_pc = cur_fetch_pc;
    bool next_mem_waiting = cur_mem_waiting;

    bool wb_write_en = cur_wb.valid && cur_wb.writes_rd && (cur_wb.rd != 0U);
    if (wb_write_en) {
        rf_write(cur_wb.rd, cur_wb.wb_data, true);
    }
    trace_wb(cur_wb.rd, cur_wb.wb_data, wb_write_en);

    if (cur_wb.valid && cur_wb.halt) {
        next_halted = true;
        trace_halt(cur_wb.pc);
    }

    bool stall_frontend = false;
    bool branch_taken = false;
    uint64_t branch_target = cur_pc;

    if (cur_ex.valid) {
        if (cur_mem_waiting) {
            stall_frontend = true;
            bool dcache_hit = false;
            MemResponse mem_resp;
            mem_resp.rdata = 0;
            mem_resp.success = false;
            dcache_resp(dcache_hit, mem_resp);
            if (dcache_hit) {
                next_wb.valid = true;
                next_wb.pc = cur_ex.pc;
                next_wb.inst = cur_ex.dec.inst;
                next_wb.writes_rd = cur_ex.dec.writes_rd;
                next_wb.rd = cur_ex.dec.rd;
                next_wb.wb_data = cur_ex.ex.result;
                next_wb.halt = cur_ex.dec.is_ebreak;

                if (cur_ex.dec.is_load) {
                    next_wb.wb_data = load_extend_data(mem_resp.rdata, cur_ex.dec.mem_width, cur_ex.dec.mem_unsigned);
                } else if (cur_ex.dec.is_store) {
                    next_wb.writes_rd = false;
                } else if (cur_ex.dec.is_sc) {
                    next_wb.wb_data = mem_resp.success ? 0ULL : 1ULL;
                } else if (cur_ex.dec.is_atomic) {
                    next_wb.wb_data = load_extend_data(mem_resp.rdata, cur_ex.dec.mem_width, false);
                }
                next_ex = make_exmem_invalid();
                next_mem_waiting = false;
                stall_frontend = false;
            }
        } else if (cur_ex.dec.is_load || cur_ex.dec.is_store || cur_ex.dec.is_atomic) {
            MemRequest mem_req;
            mem_req.valid = true;
            mem_req.is_write = cur_ex.dec.is_store || cur_ex.dec.is_sc;
            mem_req.is_atomic = cur_ex.dec.is_atomic;
            mem_req.is_lr = cur_ex.dec.is_lr;
            mem_req.is_sc = cur_ex.dec.is_sc;
            mem_req.mem_unsigned = cur_ex.dec.mem_unsigned;
            mem_req.width = cur_ex.dec.mem_width;
            mem_req.atomic_op = cur_ex.dec.atomic_op;
            mem_req.addr = cur_ex.ex.mem_addr;
            mem_req.wdata = cur_ex.ex.store_data;
            dcache_req(mem_req);
            next_mem_waiting = true;
            stall_frontend = true;
        } else {
            next_wb.valid = true;
            next_wb.pc = cur_ex.pc;
            next_wb.inst = cur_ex.dec.inst;
            next_wb.writes_rd = cur_ex.dec.writes_rd;
            next_wb.rd = cur_ex.dec.rd;
            next_wb.wb_data = cur_ex.ex.result;
            next_wb.halt = cur_ex.dec.is_ebreak;
            next_ex = make_exmem_invalid();
        }
    }

    bool ex_slot_free = !next_ex.valid;
    if (!stall_frontend && cur_id.valid && ex_slot_free) {
        ExecRequest exec_req;
        exec_req.pc = cur_id.pc;
        exec_req.rs1_val = cur_id.rs1_val;
        exec_req.rs2_val = cur_id.rs2_val;
        exec_req.dec = cur_id.dec;

        ExecResult ex_res;
        exec_inst(exec_req, ex_res);
        next_ex.valid = true;
        next_ex.pc = cur_id.pc;
        next_ex.dec = cur_id.dec;
        next_ex.ex = ex_res;
        branch_taken = ex_res.branch_taken;
        branch_target = ex_res.branch_target;
        next_id = make_idex_invalid();
    }

    if (branch_taken) {
        next_if = make_ifid_invalid();
        next_id = make_idex_invalid();
        next_pc = branch_target;
    }

    bool can_decode = !stall_frontend && !branch_taken && !cur_halted && cur_if.valid && !next_id.valid;
    if (can_decode) {
        DecodedInst dec_out;
        decode_inst(cur_if.inst, dec_out);

        bool hazard = false;
        if (dec_out.valid) {
            if (dec_out.use_rs1 && dec_out.rs1 != 0U) {
                bool p0 = cur_id.valid && cur_id.dec.writes_rd && (cur_id.dec.rd == dec_out.rs1);
                bool p1 = cur_ex.valid && cur_ex.dec.writes_rd && (cur_ex.dec.rd == dec_out.rs1);
                bool p2 = cur_wb.valid && cur_wb.writes_rd && (cur_wb.rd == dec_out.rs1);
                hazard = hazard || p0 || p1 || p2;
            }
            if (dec_out.use_rs2 && dec_out.rs2 != 0U) {
                bool p0 = cur_id.valid && cur_id.dec.writes_rd && (cur_id.dec.rd == dec_out.rs2);
                bool p1 = cur_ex.valid && cur_ex.dec.writes_rd && (cur_ex.dec.rd == dec_out.rs2);
                bool p2 = cur_wb.valid && cur_wb.writes_rd && (cur_wb.rd == dec_out.rs2);
                hazard = hazard || p0 || p1 || p2;
            }
        }

        if (!hazard && dec_out.valid) {
            RegReadPair reads;
            rf_read2(dec_out.rs1, dec_out.rs2, reads);
            next_id.valid = true;
            next_id.pc = cur_if.pc;
            next_id.rs1_val = reads.rs1_val;
            next_id.rs2_val = reads.rs2_val;
            next_id.dec = dec_out;
            next_if = make_ifid_invalid();
        }
    }

    if (!cur_halted && !next_halted && !stall_frontend) {
        bool allow_fetch_resp = !next_if.valid;
        bool drop_resp = cur_fetch_drop || branch_taken;
        if (cur_fetch_waiting && allow_fetch_resp) {
            bool icache_hit = false;
            uint32_t fetched_inst = 0;
            icache_resp(icache_hit, fetched_inst);
            if (icache_hit) {
                if (!drop_resp) {
                    next_if.valid = true;
                    next_if.pc = cur_fetch_pc;
                    next_if.inst = fetched_inst;
                }
                next_fetch_waiting = false;
                next_fetch_drop = false;
            } else {
                next_fetch_waiting = true;
                next_fetch_drop = drop_resp;
            }
        } else if (branch_taken && cur_fetch_waiting) {
            next_fetch_drop = true;
        }

        if (!next_fetch_waiting) {
            icache_req(next_pc);
            next_fetch_pc = next_pc;
            next_fetch_waiting = true;
            next_fetch_drop = false;
            next_pc = next_pc + 4;
        }
    }

    cycle.setnext(cycle + 1);
    pc.setnext(next_pc);
    halted.setnext(next_halted);
    if_id.setnext(next_if);
    id_ex.setnext(next_id);
    ex_mem.setnext(next_ex);
    mem_wb.setnext(next_wb);
    fetch_waiting.setnext(next_fetch_waiting);
    fetch_drop_resp.setnext(next_fetch_drop);
    fetch_req_pc.setnext(next_fetch_pc);
    mem_waiting.setnext(next_mem_waiting);
}
