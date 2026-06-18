#pragma once

#include "../header.hpp"

PARAMETER(PIPE_SLOTS, 4);
PARAMETER(INPUT_DEPTH, 4);
PARAMETER(RESULT_DEPTH, 4);
PARAMETER(ADDR_LATENCY, 2);

STRUCT(LSUSlot) {
    bool valid;
    bool waiting_mem;
    bool sent_req;
    uint32_t remain;
    FuRequest req;
};

QUEUE(issueq, FuRequest, INPUT_DEPTH);
QUEUE(resultq, WritebackEvent, RESULT_DEPTH);

REQUEST(mem_req, ARG(MemRequest) req);
REQUEST_READY(mem_resp, RESP(MemResponse) resp);

REGISTER_ARRAY1(slot_state, LSUSlot, PIPE_SLOTS, 1) {
    for (int i = 0; i < PIPE_SLOTS; ++i) {
        slot_state[i].valid = false;
        slot_state[i].waiting_mem = false;
        slot_state[i].sent_req = false;
        slot_state[i].remain = 0;
        slot_state[i].req.valid = false;
        slot_state[i].req.opcode = OP_NOP;
        slot_state[i].req.rob_idx = 0;
        slot_state[i].req.dst_phys = 0;
        slot_state[i].req.src1_value = 0;
        slot_state[i].req.src2_value = 0;
        slot_state[i].req.imm = 0;
        slot_state[i].req.seq = 0;
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
        if (slot_state[i].valid) {
            s.inflight++;
        }
    }
    s.can_accept = issueq.enqready();
    s.has_result = resultq.deqvalid();
    return s;
}

TICK_IMPL() {
    LSUSlot next_state[PIPE_SLOTS];
    for (int i = 0; i < PIPE_SLOTS; ++i) {
        next_state[i] = slot_state[i];
    }

    int free_slot = -1;

    for (int i = 0; i < PIPE_SLOTS; ++i) {
        LSUSlot st = next_state[i];
        if (!st.valid && free_slot < 0) {
            free_slot = i;
            continue;
        }
        if (!st.valid) {
            continue;
        }

        if (!st.waiting_mem) {
            if (st.remain > 0U) {
                st.remain--;
                next_state[i] = st;
            } else {
                st.waiting_mem = true;
                st.sent_req = false;
                next_state[i] = st;
            }
            continue;
        }

        if (!st.sent_req) {
            MemRequest req;
            req.valid = true;
            req.is_store = st.req.opcode == OP_STORE;
            req.rob_idx = st.req.rob_idx;
            req.dst_phys = st.req.dst_phys;
            req.addr = st.req.src1_value + static_cast<uint64_t>(st.req.imm);
            req.data = st.req.src2_value;
            req.seq = st.req.seq;
            mem_req(req);
            st.sent_req = true;
            next_state[i] = st;
            continue;
        }

        MemResponse resp;
        if (mem_resp(resp)) {
            WritebackEvent wb;
            wb.valid = true;
            wb.has_dest = st.req.opcode == OP_LOAD && st.req.dst_phys != 0U;
            wb.rob_idx = st.req.rob_idx;
            wb.dst_phys = st.req.dst_phys;
            wb.value = resp.data;
            wb.seq = st.req.seq;
            if (resultq.enqready()) {
                resultq.enqnext(wb);
                LSUSlot zero = st;
                zero.valid = false;
                zero.waiting_mem = false;
                zero.sent_req = false;
                zero.remain = 0;
                zero.req.valid = false;
                zero.req.opcode = OP_NOP;
                zero.req.rob_idx = 0;
                zero.req.dst_phys = 0;
                zero.req.src1_value = 0;
                zero.req.src2_value = 0;
                zero.req.imm = 0;
                zero.req.seq = 0;
                next_state[i] = zero;
                if (free_slot < 0) {
                    free_slot = i;
                }
            }
        }
    }

    if (free_slot >= 0 && issueq.deqvalid()) {
        FuRequest req = issueq.front();
        issueq.deqnext();
        LSUSlot st;
        st.valid = true;
        st.waiting_mem = false;
        st.sent_req = false;
        st.remain = static_cast<uint32_t>(ADDR_LATENCY - 1);
        st.req = req;
        next_state[free_slot] = st;
    }

    for (int i = 0; i < PIPE_SLOTS; ++i) {
        slot_state.setnext<0>(i, next_state[i]);
    }
}
