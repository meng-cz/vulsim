#include <cstdio>
#include <cstdlib>

#include <defhelper.hpp>
#include <run.hpp>

#include "../header.hpp"

TOP("../core/Decoder.hpp");
PROJECT("..");

REQUEST(decode, ARG(uint32_t) inst, RESP(DecodedInst) out);

SIMULATION() {
    auto fail = [&](uint32_t case_id, uint64_t got, uint64_t expected) {
        std::printf("decoder unit failed: case=%u got=%llu expected=%llu\n",
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

    DecodedInst out;

    decode(0x00500093U, out);
    check(out.valid, 0, 0, 1);
    sim_execute();
    sim_commit();

    decode(0x00500093U, out);
    check(out.rd == 1U, 1, out.rd, 1);
    sim_execute();
    sim_commit();

    decode(0x00500093U, out);
    check(out.rs1 == 0U, 2, out.rs1, 0);
    sim_execute();
    sim_commit();

    decode(0x00500093U, out);
    check(out.imm == 5ULL, 3, out.imm, 5);
    sim_execute();
    sim_commit();

    decode(0x00500093U, out);
    check(out.alu_op == ALU_ADD, 4, out.alu_op, ALU_ADD);
    sim_execute();
    sim_commit();

    decode(0x0020c1b3U, out);
    check(out.valid, 5, 0, 1);
    sim_execute();
    sim_commit();

    decode(0x0020c1b3U, out);
    check(out.rd == 3U, 6, out.rd, 3);
    sim_execute();
    sim_commit();

    decode(0x0020c1b3U, out);
    check(out.rs1 == 1U, 7, out.rs1, 1);
    sim_execute();
    sim_commit();

    decode(0x0020c1b3U, out);
    check(out.rs2 == 2U, 8, out.rs2, 2);
    sim_execute();
    sim_commit();

    decode(0x0020c1b3U, out);
    check(out.alu_op == ALU_XOR, 9, out.alu_op, ALU_XOR);
    sim_execute();
    sim_commit();

    decode(0x02208233U, out);
    check(out.is_muldiv, 10, 0, 1);
    sim_execute();
    sim_commit();

    decode(0x02208233U, out);
    check(out.muldiv_op == MDU_MUL, 11, out.muldiv_op, MDU_MUL);
    sim_execute();
    sim_commit();

    decode(0x0020b023U, out);
    check(out.is_store, 12, 0, 1);
    sim_execute();
    sim_commit();

    decode(0x0020b023U, out);
    check(out.mem_width == 8U, 13, out.mem_width, 8);
    sim_execute();
    sim_commit();

    decode(0x0020b023U, out);
    check(out.imm == 0ULL, 14, out.imm, 0);
    sim_execute();
    sim_commit();

    decode(0x002535afU, out);
    check(out.is_atomic, 15, 0, 1);
    sim_execute();
    sim_commit();

    decode(0x002535afU, out);
    check(out.atomic_op == AMO_ADD, 16, out.atomic_op, AMO_ADD);
    sim_execute();
    sim_commit();

    decode(0x002535afU, out);
    check(out.mem_width == 8U, 17, out.mem_width, 8);
    sim_execute();
    sim_commit();

    decode(0x00100073U, out);
    check(out.is_ebreak, 18, 0, 1);
    sim_execute();
    sim_commit();

    std::printf("decoder unit passed 19 checks\n");
}
