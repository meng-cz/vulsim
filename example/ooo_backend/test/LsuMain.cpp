#include <cstdio>
#include <cstdlib>

#include <defhelper.hpp>
#include <run.hpp>

#include "../header.hpp"

TOP("../core/LSUPipeline.hpp");
PROJECT("..");

GLOBAL() {
    uint64_t mem_slot = 0;
    bool have_resp = false;
    MemResponse pending_resp{};
}

REQUEST_READY(issue, ARG(FuRequest) req);
REQUEST_READY(pop_result, RESP(WritebackEvent) wb);
QUERY(status, UnitStatus);

SERVICE(mem_req, ARG(MemRequest) req) {
    if (req.is_store) {
        mem_slot = req.data;
        pending_resp.valid = true;
        pending_resp.rob_idx = req.rob_idx;
        pending_resp.dst_phys = req.dst_phys;
        pending_resp.data = 0;
        pending_resp.seq = req.seq;
    } else {
        pending_resp.valid = true;
        pending_resp.rob_idx = req.rob_idx;
        pending_resp.dst_phys = req.dst_phys;
        pending_resp.data = mem_slot;
        pending_resp.seq = req.seq;
    }
    have_resp = true;
}

SERVICE_READY(mem_resp, have_resp, RESP(MemResponse) resp) {
    resp = pending_resp;
    have_resp = false;
}

SIMULATION() {
    auto fail = [&](const char *msg) {
        std::printf("lsu unit failed: %s\n", msg);
        std::exit(1);
    };

    FuRequest store{};
    store.valid = true;
    store.opcode = OP_STORE;
    store.rob_idx = 1;
    store.dst_phys = 0;
    store.src1_value = 0x100;
    store.src2_value = 55;
    store.imm = 0;
    store.seq = 1;

    FuRequest load = store;
    load.opcode = OP_LOAD;
    load.rob_idx = 2;
    load.dst_phys = 11;
    load.src2_value = 0;
    load.seq = 2;

    if (!issue(store)) fail("store rejected");
    sim_execute();
    sim_commit();

    for (int i = 0; i < 5 && !status().has_result; ++i) {
        sim_execute();
        sim_commit();
    }

    WritebackEvent wb{};
    if (!pop_result(wb) || wb.rob_idx != 1 || wb.has_dest) fail("store completion mismatch");
    sim_execute();
    sim_commit();

    if (!issue(load)) fail("load rejected");
    sim_execute();
    sim_commit();

    for (int i = 0; i < 5 && !status().has_result; ++i) {
        sim_execute();
        sim_commit();
    }

    if (!pop_result(wb) || wb.rob_idx != 2 || wb.value != 55 || !wb.has_dest) fail("load completion mismatch");
    sim_execute();
    sim_commit();

    UnitStatus st = status();
    if (st.inflight != 0U) fail("lsu not drained");

    std::printf("lsu unit passed\n");
}
