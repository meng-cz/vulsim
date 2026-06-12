#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <defhelper.hpp>
#include <run.hpp>

#include "../header.hpp"

TOP("../Top.hpp");
PROJECT("..");

GLOBAL() {
static constexpr uint64_t kMemSize = 4096;
std::array<uint8_t, 4096> memory{};
std::array<uint64_t, 32> arch_regs{};
bool halted_seen = false;
uint64_t halt_pc = 0;
uint64_t sim_cycle = 0;
bool icache_pending = false;
uint64_t icache_pending_pc = 0;
uint64_t icache_issue_cycle = 0;
bool dcache_pending = false;
MemRequest dcache_pending_req;
uint64_t dcache_issue_cycle = 0;
bool reservation_valid = false;
uint64_t reservation_addr = 0;
uint8_t reservation_width = 0;

uint64_t load_le(uint64_t addr, uint8_t width) {
    uint64_t value = 0;
    for (uint8_t i = 0; i < width; ++i) {
        value |= static_cast<uint64_t>(memory[addr + i]) << (8U * i);
    }
    return value;
}

void store_le(uint64_t addr, uint64_t value, uint8_t width) {
    for (uint8_t i = 0; i < width; ++i) {
        memory[addr + i] = static_cast<uint8_t>((value >> (8U * i)) & 0xffU);
    }
}

uint32_t load32(uint64_t addr) {
    return static_cast<uint32_t>(load_le(addr, 4));
}

void store32(uint64_t addr, uint32_t value) {
    store_le(addr, value, 4);
}

uint64_t mask_by_width(uint64_t value, uint8_t width) {
    if (width == 1) return value & 0xffULL;
    if (width == 2) return value & 0xffffULL;
    if (width == 4) return value & 0xffffffffULL;
    return value;
}

uint64_t amo_compute(uint8_t op, uint64_t old_val, uint64_t arg_val, uint8_t width) {
    uint64_t old_masked = mask_by_width(old_val, width);
    uint64_t arg_masked = mask_by_width(arg_val, width);
    if (width == 4) {
        uint32_t a = static_cast<uint32_t>(old_masked);
        uint32_t b = static_cast<uint32_t>(arg_masked);
        uint32_t r = a;
        if (op == AMO_SWAP) r = b;
        else if (op == AMO_ADD) r = static_cast<uint32_t>(a + b);
        else if (op == AMO_XOR) r = a ^ b;
        else if (op == AMO_AND) r = a & b;
        else if (op == AMO_OR) r = a | b;
        else if (op == AMO_MIN) r = (static_cast<int32_t>(a) < static_cast<int32_t>(b)) ? a : b;
        else if (op == AMO_MAX) r = (static_cast<int32_t>(a) > static_cast<int32_t>(b)) ? a : b;
        else if (op == AMO_MINU) r = (a < b) ? a : b;
        else if (op == AMO_MAXU) r = (a > b) ? a : b;
        return static_cast<uint64_t>(r);
    }

    uint64_t r = old_masked;
    if (op == AMO_SWAP) r = arg_masked;
    else if (op == AMO_ADD) r = old_masked + arg_masked;
    else if (op == AMO_XOR) r = old_masked ^ arg_masked;
    else if (op == AMO_AND) r = old_masked & arg_masked;
    else if (op == AMO_OR) r = old_masked | arg_masked;
    else if (op == AMO_MIN) r = (static_cast<int64_t>(old_masked) < static_cast<int64_t>(arg_masked)) ? old_masked : arg_masked;
    else if (op == AMO_MAX) r = (static_cast<int64_t>(old_masked) > static_cast<int64_t>(arg_masked)) ? old_masked : arg_masked;
    else if (op == AMO_MINU) r = (old_masked < arg_masked) ? old_masked : arg_masked;
    else if (op == AMO_MAXU) r = (old_masked > arg_masked) ? old_masked : arg_masked;
    return r;
}

uint32_t enc_r(uint32_t funct7, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
    return (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

uint32_t enc_i(int32_t imm, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
    return ((static_cast<uint32_t>(imm) & 0xfffU) << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

uint32_t enc_s(int32_t imm, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t opcode) {
    uint32_t uimm = static_cast<uint32_t>(imm) & 0xfffU;
    return ((uimm >> 5) << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | ((uimm & 0x1fU) << 7) | opcode;
}

uint32_t enc_b(int32_t imm, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t opcode) {
    uint32_t uimm = static_cast<uint32_t>(imm) & 0x1fffU;
    return (((uimm >> 12) & 0x1U) << 31) |
           (((uimm >> 5) & 0x3fU) << 25) |
           (rs2 << 20) |
           (rs1 << 15) |
           (funct3 << 12) |
           (((uimm >> 1) & 0xfU) << 8) |
           (((uimm >> 11) & 0x1U) << 7) |
           opcode;
}

uint32_t enc_u(int32_t imm, uint32_t rd, uint32_t opcode) {
    return (static_cast<uint32_t>(imm) & 0xfffff000U) | (rd << 7) | opcode;
}

uint32_t enc_j(int32_t imm, uint32_t rd, uint32_t opcode) {
    uint32_t uimm = static_cast<uint32_t>(imm) & 0x1fffffU;
    return (((uimm >> 20) & 0x1U) << 31) |
           (((uimm >> 1) & 0x3ffU) << 21) |
           (((uimm >> 11) & 0x1U) << 20) |
           (((uimm >> 12) & 0xffU) << 12) |
           (rd << 7) |
           opcode;
}

uint32_t enc_amo(uint32_t funct5, uint32_t aqrl, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t rd) {
    return (funct5 << 27) | (aqrl << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | 0x2fU;
}
}

SERVICE(icache_req, ARG(uint64_t) pc) {
    if (icache_pending) {
        std::printf("icache_req while previous request still pending at cycle=%llu\n",
                    static_cast<unsigned long long>(sim_cycle));
        std::exit(1);
    }
    icache_pending = true;
    icache_pending_pc = pc;
    icache_issue_cycle = sim_cycle;
}

SERVICE(icache_resp, RESP(bool) hit, RESP(uint32_t) inst) {
    hit = false;
    inst = 0;
    if (!icache_pending || sim_cycle <= icache_issue_cycle) {
        return;
    }
    if (icache_pending_pc + 4 > kMemSize) {
        std::printf("icache out of range pc=0x%llx\n", static_cast<unsigned long long>(icache_pending_pc));
        std::exit(1);
    }
    hit = true;
    inst = load32(icache_pending_pc);
    icache_pending = false;
}

SERVICE(dcache_req, ARG(MemRequest) req) {
    if (dcache_pending) {
        std::printf("dcache_req while previous request still pending at cycle=%llu\n",
                    static_cast<unsigned long long>(sim_cycle));
        std::exit(1);
    }
    dcache_pending = true;
    dcache_pending_req = req;
    dcache_issue_cycle = sim_cycle;
}

SERVICE(dcache_resp, RESP(bool) hit, RESP(MemResponse) resp) {
    hit = false;
    resp.rdata = 0;
    resp.success = false;

    if (!dcache_pending || sim_cycle <= dcache_issue_cycle) {
        return;
    }
    if (!dcache_pending_req.valid) {
        hit = true;
        resp.success = true;
        dcache_pending = false;
        return;
    }
    if (dcache_pending_req.addr + dcache_pending_req.width > kMemSize) {
        std::printf("dcache out of range addr=0x%llx width=%u\n",
                    static_cast<unsigned long long>(dcache_pending_req.addr),
                    static_cast<unsigned>(dcache_pending_req.width));
        std::exit(1);
    }

    hit = true;
    resp.success = true;

    if (dcache_pending_req.is_atomic) {
        if (dcache_pending_req.is_lr) {
            resp.rdata = load_le(dcache_pending_req.addr, dcache_pending_req.width);
            reservation_valid = true;
            reservation_addr = dcache_pending_req.addr;
            reservation_width = dcache_pending_req.width;
            dcache_pending = false;
            return;
        }
        if (dcache_pending_req.is_sc) {
            bool ok = reservation_valid &&
                      reservation_addr == dcache_pending_req.addr &&
                      reservation_width == dcache_pending_req.width;
            resp.success = ok;
            if (ok) {
                store_le(dcache_pending_req.addr, dcache_pending_req.wdata, dcache_pending_req.width);
            }
            reservation_valid = false;
            dcache_pending = false;
            return;
        }

        uint64_t old_val = load_le(dcache_pending_req.addr, dcache_pending_req.width);
        uint64_t new_val = amo_compute(dcache_pending_req.atomic_op, old_val, dcache_pending_req.wdata, dcache_pending_req.width);
        store_le(dcache_pending_req.addr, new_val, dcache_pending_req.width);
        resp.rdata = old_val;
        reservation_valid = false;
        dcache_pending = false;
        return;
    }

    if (dcache_pending_req.is_write) {
        store_le(dcache_pending_req.addr, dcache_pending_req.wdata, dcache_pending_req.width);
        reservation_valid = false;
    } else {
        resp.rdata = load_le(dcache_pending_req.addr, dcache_pending_req.width);
    }
    dcache_pending = false;
}

SERVICE(trace_wb, ARG(uint32_t) rd, ARG(uint64_t) data, ARG(bool) wen) {
    if (wen && rd < 32U) {
        arch_regs[rd] = data;
    }
}

SERVICE(trace_halt, ARG(uint64_t) pc) {
    halted_seen = true;
    halt_pc = pc;
}

SIMULATION() {
    std::memset(memory.data(), 0, memory.size());
    for (auto &x : arch_regs) {
        x = 0;
    }
    halted_seen = false;
    halt_pc = 0;
    sim_cycle = 0;
    icache_pending = false;
    icache_pending_pc = 0;
    icache_issue_cycle = 0;
    dcache_pending = false;
    dcache_issue_cycle = 0;
    reservation_valid = false;
    reservation_addr = 0;
    reservation_width = 0;

    uint32_t pc = 0;
    auto emit = [&](uint32_t inst) {
        store32(pc, inst);
        pc += 4;
    };

    emit(enc_i(5, 0, 0, 1, 0x13));
    emit(enc_i(7, 0, 0, 2, 0x13));
    emit(enc_r(0x00, 2, 1, 0x0, 3, 0x33));
    emit(enc_r(0x01, 2, 3, 0x0, 4, 0x33));
    emit(enc_r(0x01, 1, 4, 0x4, 5, 0x33));
    emit(enc_r(0x01, 1, 4, 0x6, 6, 0x33));
    emit(enc_i(0x100, 0, 0, 10, 0x13));
    emit(enc_s(0, 4, 10, 0x3, 0x23));
    emit(enc_i(0, 10, 0x3, 7, 0x03));
    emit(enc_i(1, 0, 0, 8, 0x13));
    emit(enc_i(1, 8, 0, 8, 0x13));
    emit(enc_b(-4, 2, 8, 0x4, 0x63));
    emit(enc_i(0x108, 0, 0, 10, 0x13));
    emit(enc_amo(0x00, 0, 2, 10, 0x3, 11));
    emit(enc_amo(0x02, 0, 0, 10, 0x3, 12));
    emit(enc_i(5, 12, 0, 13, 0x13));
    emit(enc_amo(0x03, 0, 13, 10, 0x3, 14));
    emit(enc_amo(0x02, 0, 0, 10, 0x3, 15));
    emit(enc_i(1, 0, 0, 20, 0x13));
    emit(enc_j(20, 21, 0x6f));
    emit(enc_i(1, 20, 0, 20, 0x13));
    emit(enc_u(0x1000, 23, 0x37));
    emit(enc_u(0, 22, 0x17));
    emit(0x00100073U);
    emit(enc_i(9, 20, 0, 20, 0x13));
    emit(enc_i(0, 21, 0, 0, 0x67));

    store_le(0x108, 100, 8);

    dcache_pending_req.valid = false;
    dcache_pending_req.is_write = false;
    dcache_pending_req.is_atomic = false;
    dcache_pending_req.is_lr = false;
    dcache_pending_req.is_sc = false;
    dcache_pending_req.mem_unsigned = false;
    dcache_pending_req.width = 0;
    dcache_pending_req.atomic_op = 0;
    dcache_pending_req.addr = 0;
    dcache_pending_req.wdata = 0;

    const uint64_t max_cycles = 512;
    for (uint64_t tick = 0; tick < max_cycles; ++tick) {
        sim_cycle = tick;
        sim_execute();
        sim_commit();
        if (halted_seen) {
            break;
        }
    }

    if (!halted_seen) {
        std::printf("core integration timed out\n");
        std::exit(1);
    }

    auto expect_reg = [&](uint32_t idx, uint64_t expected) {
        if (arch_regs[idx] != expected) {
            std::printf("x%u mismatch: got=%llu expected=%llu\n",
                        idx,
                        static_cast<unsigned long long>(arch_regs[idx]),
                        static_cast<unsigned long long>(expected));
            std::exit(1);
        }
    };

    expect_reg(1, 5);
    expect_reg(2, 7);
    expect_reg(3, 12);
    expect_reg(4, 84);
    expect_reg(5, 16);
    expect_reg(6, 4);
    expect_reg(7, 84);
    expect_reg(8, 7);
    expect_reg(10, 0x108);
    expect_reg(11, 100);
    expect_reg(12, 107);
    expect_reg(13, 112);
    expect_reg(14, 0);
    expect_reg(15, 112);
    expect_reg(20, 11);
    expect_reg(21, 80);
    expect_reg(22, 88);
    expect_reg(23, 0x1000);

    uint64_t mem100 = load_le(0x100, 8);
    uint64_t mem108 = load_le(0x108, 8);
    if (mem100 != 84ULL || mem108 != 112ULL || halt_pc != 92ULL) {
        std::printf("memory/halt mismatch mem100=%llu mem108=%llu halt_pc=%llu\n",
                    static_cast<unsigned long long>(mem100),
                    static_cast<unsigned long long>(mem108),
                    static_cast<unsigned long long>(halt_pc));
        std::exit(1);
    }

    std::printf("rv64ima5 integration passed: x4=%llu x11=%llu x15=%llu x20=%llu\n",
                static_cast<unsigned long long>(arch_regs[4]),
                static_cast<unsigned long long>(arch_regs[11]),
                static_cast<unsigned long long>(arch_regs[15]),
                static_cast<unsigned long long>(arch_regs[20]));
}
