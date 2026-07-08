#include <cstdio>
#include <cstdlib>

#include <defhelper.hpp>
#include <run.hpp>

#include "../header.hpp"

TOP("../core/RegisterFile.hpp");
PROJECT("..");

REQUEST(read2, ARG(uint32_t) rs1, ARG(uint32_t) rs2, RESP(RegReadPair) out);
REQUEST(write, ARG(uint32_t) rd, ARG(uint64_t) data, ARG(bool) wen);
QUERY(zero_and_ra, RegReadPair);

SIMULATION() {
    auto fail = [&](uint32_t case_id, uint64_t got, uint64_t expected) {
        std::printf("regfile unit failed: case=%u got=%llu expected=%llu\n",
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

    RegReadPair out;

    write(1, 11, true);
    sim_nextcycle();

    write(0, 99, true);
    sim_nextcycle();

    read2(1, 0, out);
    check(out.rs1_val == 11ULL, 0, out.rs1_val, 11);
    sim_nextcycle();

    read2(1, 0, out);
    check(out.rs2_val == 0ULL, 1, out.rs2_val, 0);
    sim_nextcycle();

    write(2, 22, true);
    sim_nextcycle();

    read2(1, 2, out);
    check(out.rs1_val == 11ULL, 2, out.rs1_val, 11);
    sim_nextcycle();

    read2(1, 2, out);
    check(out.rs2_val == 22ULL, 3, out.rs2_val, 22);
    sim_nextcycle();

    write(1, 33, true);
    sim_nextcycle();

    read2(1, 2, out);
    check(out.rs1_val == 33ULL, 4, out.rs1_val, 33);
    sim_nextcycle();

    read2(1, 2, out);
    check(out.rs2_val == 22ULL, 5, out.rs2_val, 22);
    sim_nextcycle();

    out = zero_and_ra();
    check(out.rs1_val == 0ULL, 6, out.rs1_val, 0);
    sim_nextcycle();

    out = zero_and_ra();
    check(out.rs2_val == 33ULL, 7, out.rs2_val, 33);
    sim_nextcycle();

    std::printf("regfile unit passed 8 checks\n");
}
