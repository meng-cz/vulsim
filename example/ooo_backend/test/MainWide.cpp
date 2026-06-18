#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <defhelper.hpp>
#include <run.hpp>

#include "../header.hpp"

TOP("../core/BackendCore.hpp");
PROJECT("..");

PARAMETER(ALU_LANES, 2);
PARAMETER(LSU_LANES, 2);

GLOBAL() {
    std::vector<BackendInstr> program;
    uint64_t mem[64]{};
    bool resp0_valid = false;
    bool resp1_valid = false;
    MemResponse resp0{};
    MemResponse resp1{};
}

REQUEST_READY(push_inst, ARRAY(INGRESS_WIDTH), ARG(BackendInstr) inst);
QUERY(status, BackendStatus);
QUERY(regs, ArchRegSnapshot);

SERVICE(mem_req0, ARG(MemRequest) req) {
    uint64_t idx = (req.addr >> 3) & 63ULL;
    if (req.is_store) {
        mem[idx] = req.data;
        resp0.data = 0;
    } else {
        resp0.data = mem[idx];
    }
    resp0.valid = true;
    resp0.rob_idx = req.rob_idx;
    resp0.dst_phys = req.dst_phys;
    resp0.seq = req.seq;
    resp0_valid = true;
}

SERVICE_READY(mem_resp0, resp0_valid, RESP(MemResponse) resp) {
    resp = resp0;
    resp0_valid = false;
}

SERVICE(mem_req1, ARG(MemRequest) req) {
    uint64_t idx = (req.addr >> 3) & 63ULL;
    if (req.is_store) {
        mem[idx] = req.data;
        resp1.data = 0;
    } else {
        resp1.data = mem[idx];
    }
    resp1.valid = true;
    resp1.rob_idx = req.rob_idx;
    resp1.dst_phys = req.dst_phys;
    resp1.seq = req.seq;
    resp1_valid = true;
}

SERVICE_READY(mem_resp1, resp1_valid, RESP(MemResponse) resp) {
    resp = resp1;
    resp1_valid = false;
}

SIMULATION() {
    auto emit = [&](uint8_t op, uint8_t rd, uint8_t rs1, uint8_t rs2, int64_t imm) {
        BackendInstr inst{};
        inst.valid = true;
        inst.opcode = op;
        inst.rd = rd;
        inst.rs1 = rs1;
        inst.rs2 = rs2;
        inst.imm = imm;
        program.push_back(inst);
    };

    for (uint64_t i = 0; i < 64; ++i) {
        mem[i] = i * 3ULL;
    }

    emit(OP_ADDI, 1, 0, 0, 10);
    emit(OP_ADDI, 2, 0, 0, 3);
    emit(OP_ADDI, 3, 0, 0, 4);
    for (int rep = 0; rep < 24; ++rep) {
        emit(OP_MUL, 4, 1, 2, 0);
        emit(OP_ADD, 5, 4, 3, 0);
        emit(OP_LOAD, 6, 0, 0, (rep & 7) * 8);
        emit(OP_ADD, 7, 6, 5, 0);
        emit(OP_STORE, 0, 0, 7, (16 + rep) * 8);
        emit(OP_ADDI, 1, 1, 0, 1);
    }
    emit(OP_HALT, 0, 0, 0, 0);

    std::size_t pc = 0;
    uint64_t max_cycles = 4000;
    for (uint64_t cyc = 0; cyc < max_cycles; ++cyc) {
        if (pc < program.size()) {
            if (push_inst<0>(program[pc])) {
                pc++;
            }
            if (pc < program.size() && push_inst<1>(program[pc])) {
                pc++;
            }
        }
        sim_execute();
        sim_commit();
        BackendStatus st = status();
        if (st.halted) {
            ArchRegSnapshot snap = regs();
            if (snap.x1 != 34ULL) {
                std::printf("wide integration failed: x1=%llu expected=34\n", static_cast<unsigned long long>(snap.x1));
                std::exit(1);
            }
            if (mem[16] != 34ULL) {
                std::printf("wide integration failed: mem[16]=%llu expected=34\n", static_cast<unsigned long long>(mem[16]));
                std::exit(1);
            }
            std::printf("ooo wide passed: cycles=%llu committed=%llu x1=%llu mem16=%llu\n",
                        static_cast<unsigned long long>(st.cycle),
                        static_cast<unsigned long long>(st.committed),
                        static_cast<unsigned long long>(snap.x1),
                        static_cast<unsigned long long>(mem[16]));
            return;
        }
    }

    std::printf("wide integration failed: timeout\n");
    std::exit(1);
}
