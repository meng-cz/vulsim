#include <cstdio>
#include <cstdlib>

#include <defhelper.hpp>
#include <run.hpp>

#include "../header.hpp"

TOP("../core/ExecuteUnit.hpp");
PROJECT("..");

REQUEST(exec, ARG(ExecRequest) req, RESP(ExecResult) out);

SIMULATION() {
    auto fail = [&](uint32_t case_id, uint64_t got, uint64_t expected) {
        std::printf("execute unit failed: case=%u got=%llu expected=%llu\n",
                    case_id,
                    static_cast<unsigned long long>(got),
                    static_cast<unsigned long long>(expected));
        std::exit(1);
    };

    auto check = [&](bool cond, uint32_t case_id, uint64_t got, uint64_t expected) {
        if (!cond) {
            fail(case_id, got, expected);
        }
    };

    auto make_req = [&]() {
        ExecRequest r;
        r.pc = 0;
        r.rs1_val = 0;
        r.rs2_val = 0;
        r.dec.valid = true;
        r.dec.writes_rd = false;
        r.dec.use_rs1 = false;
        r.dec.use_rs2 = false;
        r.dec.is_load = false;
        r.dec.is_store = false;
        r.dec.is_branch = false;
        r.dec.is_jal = false;
        r.dec.is_jalr = false;
        r.dec.is_lui = false;
        r.dec.is_auipc = false;
        r.dec.is_ebreak = false;
        r.dec.is_muldiv = false;
        r.dec.is_atomic = false;
        r.dec.is_lr = false;
        r.dec.is_sc = false;
        r.dec.rd = 0;
        r.dec.rs1 = 0;
        r.dec.rs2 = 0;
        r.dec.alu_op = static_cast<uint8_t>(ALU_ADD);
        r.dec.branch_op = static_cast<uint8_t>(BR_NONE);
        r.dec.mem_width = 8;
        r.dec.mem_unsigned = false;
        r.dec.muldiv_op = static_cast<uint8_t>(MDU_NONE);
        r.dec.atomic_op = static_cast<uint8_t>(AMO_NONE);
        r.dec.inst = 0;
        r.dec.imm = 0;
        return r;
    };

    ExecRequest req;
    ExecResult out;

    req = make_req();
    req.rs1_val = 11;
    req.dec.use_rs1 = true;
    req.dec.alu_op = static_cast<uint8_t>(ALU_ADD);
    req.dec.imm = 7;
    exec(req, out);
    check(out.result == 18ULL, 0, out.result, 18);
    sim_execute();
    sim_commit();

    req = make_req();
    req.rs1_val = 9;
    req.rs2_val = 4;
    req.dec.use_rs1 = true;
    req.dec.use_rs2 = true;
    req.dec.alu_op = static_cast<uint8_t>(ALU_SUB);
    exec(req, out);
    check(out.result == 5ULL, 1, out.result, 5);
    sim_execute();
    sim_commit();

    req = make_req();
    req.pc = 100;
    req.rs1_val = 5;
    req.rs2_val = 5;
    req.dec.use_rs1 = true;
    req.dec.use_rs2 = true;
    req.dec.is_branch = true;
    req.dec.branch_op = static_cast<uint8_t>(BR_EQ);
    req.dec.imm = 16;
    exec(req, out);
    check(out.branch_taken, 2, 0, 1);
    sim_execute();
    sim_commit();

    req = make_req();
    req.pc = 100;
    req.rs1_val = 5;
    req.rs2_val = 5;
    req.dec.use_rs1 = true;
    req.dec.use_rs2 = true;
    req.dec.is_branch = true;
    req.dec.branch_op = static_cast<uint8_t>(BR_EQ);
    req.dec.imm = 16;
    exec(req, out);
    check(out.branch_target == 116ULL, 3, out.branch_target, 116);
    sim_execute();
    sim_commit();

    req = make_req();
    req.rs1_val = 6;
    req.rs2_val = 7;
    req.dec.use_rs1 = true;
    req.dec.use_rs2 = true;
    req.dec.is_muldiv = true;
    req.dec.muldiv_op = static_cast<uint8_t>(MDU_MUL);
    exec(req, out);
    check(out.result == 42ULL, 4, out.result, 42);
    sim_execute();
    sim_commit();

    req = make_req();
    req.rs1_val = 0x100;
    req.rs2_val = 0x55;
    req.dec.use_rs1 = true;
    req.dec.use_rs2 = true;
    req.dec.is_atomic = true;
    req.dec.atomic_op = static_cast<uint8_t>(AMO_ADD);
    req.dec.mem_width = 8;
    exec(req, out);
    check(out.mem_addr == 0x100ULL, 5, out.mem_addr, 0x100);
    sim_execute();
    sim_commit();

    req = make_req();
    req.rs1_val = 0x100;
    req.rs2_val = 0x55;
    req.dec.use_rs1 = true;
    req.dec.use_rs2 = true;
    req.dec.is_atomic = true;
    req.dec.atomic_op = static_cast<uint8_t>(AMO_ADD);
    req.dec.mem_width = 8;
    exec(req, out);
    check(out.store_data == 0x55ULL, 6, out.store_data, 0x55);
    sim_execute();
    sim_commit();

    std::printf("execute unit passed 7 checks\n");
}
