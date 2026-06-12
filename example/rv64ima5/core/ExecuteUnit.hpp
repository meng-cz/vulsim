#pragma once

#include "../header.hpp"

HELPER() {
inline uint64_t sign_extend_width(uint64_t value, uint8_t width) {
    if (width == 1) {
        return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(value & 0xffU)));
    }
    if (width == 2) {
        return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(value & 0xffffU)));
    }
    if (width == 4) {
        return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(value & 0xffffffffU)));
    }
    return value;
}

inline uint64_t do_div_signed(uint64_t lhs_u, uint64_t rhs_u) {
    int64_t lhs = static_cast<int64_t>(lhs_u);
    int64_t rhs = static_cast<int64_t>(rhs_u);
    if (rhs == 0) {
        return ~0ULL;
    }
    if (lhs == static_cast<int64_t>(0x8000000000000000ULL) && rhs == -1) {
        return lhs_u;
    }
    return static_cast<uint64_t>(lhs / rhs);
}

inline uint64_t do_rem_signed(uint64_t lhs_u, uint64_t rhs_u) {
    int64_t lhs = static_cast<int64_t>(lhs_u);
    int64_t rhs = static_cast<int64_t>(rhs_u);
    if (rhs == 0) {
        return lhs_u;
    }
    if (lhs == static_cast<int64_t>(0x8000000000000000ULL) && rhs == -1) {
        return 0;
    }
    return static_cast<uint64_t>(lhs % rhs);
}
}

SERVICE(exec, ARG(ExecRequest) req, RESP(ExecResult) out) {
    const DecodedInst &dec = req.dec;
    uint64_t lhs = req.rs1_val;
    uint64_t rhs = dec.use_rs2 ? req.rs2_val : dec.imm;
    uint32_t shamt = static_cast<uint32_t>(rhs & 63ULL);

    out.valid = dec.valid;
    out.branch_taken = false;
    out.branch_target = req.pc + 4;
    out.result = 0;
    out.mem_addr = lhs + dec.imm;
    out.store_data = req.rs2_val;

    if (!dec.valid) {
        return;
    }

    if (dec.is_lui) {
        out.result = dec.imm;
    } else if (dec.is_auipc) {
        out.result = req.pc + dec.imm;
    } else if (dec.is_jal) {
        out.result = req.pc + 4;
        out.branch_taken = true;
        out.branch_target = req.pc + dec.imm;
    } else if (dec.is_jalr) {
        out.result = req.pc + 4;
        out.branch_taken = true;
        out.branch_target = (lhs + dec.imm) & ~1ULL;
    } else if (dec.is_branch) {
        bool take = false;
        if (dec.branch_op == BR_EQ) take = (lhs == req.rs2_val);
        else if (dec.branch_op == BR_NE) take = (lhs != req.rs2_val);
        else if (dec.branch_op == BR_LT) take = (static_cast<int64_t>(lhs) < static_cast<int64_t>(req.rs2_val));
        else if (dec.branch_op == BR_GE) take = (static_cast<int64_t>(lhs) >= static_cast<int64_t>(req.rs2_val));
        else if (dec.branch_op == BR_LTU) take = (lhs < req.rs2_val);
        else if (dec.branch_op == BR_GEU) take = (lhs >= req.rs2_val);
        out.branch_taken = take;
        out.branch_target = req.pc + dec.imm;
    } else if (dec.is_muldiv) {
        if (dec.muldiv_op == MDU_MUL) {
            out.result = static_cast<uint64_t>(static_cast<__int128>(lhs) * static_cast<__int128>(req.rs2_val));
        } else if (dec.muldiv_op == MDU_MULH) {
            __int128 prod = static_cast<__int128>(static_cast<int64_t>(lhs)) *
                            static_cast<__int128>(static_cast<int64_t>(req.rs2_val));
            out.result = static_cast<uint64_t>(static_cast<unsigned __int128>(prod) >> 64);
        } else if (dec.muldiv_op == MDU_MULHSU) {
            __int128 prod = static_cast<__int128>(static_cast<int64_t>(lhs)) *
                            static_cast<unsigned __int128>(req.rs2_val);
            out.result = static_cast<uint64_t>(static_cast<unsigned __int128>(prod) >> 64);
        } else if (dec.muldiv_op == MDU_MULHU) {
            unsigned __int128 prod = static_cast<unsigned __int128>(lhs) *
                                     static_cast<unsigned __int128>(req.rs2_val);
            out.result = static_cast<uint64_t>(prod >> 64);
        } else if (dec.muldiv_op == MDU_DIV) {
            out.result = do_div_signed(lhs, req.rs2_val);
        } else if (dec.muldiv_op == MDU_DIVU) {
            out.result = (req.rs2_val == 0) ? ~0ULL : (lhs / req.rs2_val);
        } else if (dec.muldiv_op == MDU_REM) {
            out.result = do_rem_signed(lhs, req.rs2_val);
        } else if (dec.muldiv_op == MDU_REMU) {
            out.result = (req.rs2_val == 0) ? lhs : (lhs % req.rs2_val);
        }
    } else if (dec.is_load || dec.is_store || dec.is_atomic) {
        out.mem_addr = lhs + dec.imm;
        out.store_data = req.rs2_val;
    } else if (dec.is_ebreak) {
        out.result = 0;
    } else if (dec.alu_op == ALU_ADD) {
        out.result = lhs + rhs;
    } else if (dec.alu_op == ALU_SUB) {
        out.result = lhs - rhs;
    } else if (dec.alu_op == ALU_SLL) {
        out.result = lhs << shamt;
    } else if (dec.alu_op == ALU_SLT) {
        out.result = (static_cast<int64_t>(lhs) < static_cast<int64_t>(rhs)) ? 1ULL : 0ULL;
    } else if (dec.alu_op == ALU_SLTU) {
        out.result = (lhs < rhs) ? 1ULL : 0ULL;
    } else if (dec.alu_op == ALU_XOR) {
        out.result = lhs ^ rhs;
    } else if (dec.alu_op == ALU_SRL) {
        out.result = lhs >> shamt;
    } else if (dec.alu_op == ALU_SRA) {
        out.result = static_cast<uint64_t>(static_cast<int64_t>(lhs) >> shamt);
    } else if (dec.alu_op == ALU_OR) {
        out.result = lhs | rhs;
    } else if (dec.alu_op == ALU_AND) {
        out.result = lhs & rhs;
    } else if (dec.alu_op == ALU_COPY_RS1) {
        out.result = lhs;
    } else if (dec.alu_op == ALU_COPY_RS2) {
        out.result = rhs;
    }
}
