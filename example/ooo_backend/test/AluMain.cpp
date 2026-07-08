#include <cstdio>
#include <cstdlib>

#include <defhelper.hpp>
#include <run.hpp>

#include "../header.hpp"

TOP("../core/ALUPipeline.hpp");
PROJECT("..");

REQUEST_READY(issue, ARG(FuRequest) req);
REQUEST_READY(pop_result, RESP(WritebackEvent) wb);
QUERY(can_accept, bool);
QUERY(has_result, bool);
QUERY(status, UnitStatus);

SIMULATION() {
    auto fail = [&](const char *msg) {
        std::printf("alu unit failed: %s\n", msg);
        std::exit(1);
    };

    FuRequest add{};
    add.valid = true;
    add.opcode = OP_ADD;
    add.rob_idx = 1;
    add.dst_phys = 9;
    add.src1_value = 10;
    add.src2_value = 32;
    add.imm = 0;
    add.seq = 1;

    FuRequest mul = add;
    mul.opcode = OP_MUL;
    mul.rob_idx = 2;
    mul.dst_phys = 10;
    mul.src1_value = 7;
    mul.src2_value = 6;
    mul.seq = 2;

    if (!can_accept() || !issue(add)) fail("add issue rejected");
    sim_nextcycle();

    if (!issue(mul)) fail("mul issue rejected");
    sim_nextcycle();

    for (int i = 0; i < 2 && !has_result(); ++i) {
        sim_nextcycle();
    }
    if (!has_result()) fail("add result missing");
    WritebackEvent wb{};
    if (!pop_result(wb) || wb.value != 42 || wb.rob_idx != 1) fail("add result mismatch");
    sim_nextcycle();

    for (int i = 0; i < 4 && !has_result(); ++i) {
        sim_nextcycle();
    }

    if (!has_result()) fail("mul result missing");
    if (!pop_result(wb) || wb.value != 42 || wb.rob_idx != 2) fail("mul result mismatch");
    sim_nextcycle();

    UnitStatus st = status();
    if (st.inflight != 0U || st.has_result) fail("pipeline not drained");

    std::printf("alu unit passed\n");
}
