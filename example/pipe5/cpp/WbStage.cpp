
#include "common.h"
#include "global.h"
#include "bundles.h"

bool wb_in_can_pop();
MemData & wb_in_top();
void wb_in_pop();

array<uint64, 64> regs;
uint64 busybits;
uint64 busybits_next;
bool busybits_reset;
uint64 fcsr;
uint64 instret;

uint32 tick_exec_flag = 0;

void clear_pipeline();
void redirect(uint64 pc);
void se_exception(uint64 pc, uint32 cause, uint64 arg, array<uint64, 64> & regs, uint64 * nextpc, array<uint64, 64> * modregs);

void internal_tick();

void WbStage_internal_tick() {
    if(tick_exec_flag) return ;
    tick_exec_flag = 1;

    if(!wb_in_can_pop()) return;

    MemData inst = wb_in_top();
    wb_in_pop();

    instret++;

    #define REDIRECT(nextpc) do { redirect(nextpc); clear_pipeline(); busybits_reset = true;} while(0)
    #define WRITEREG(idx, value) do { regs[idx] = (value); busybits &= (~(1UL << (idx))); } while(0)

    if(inst.exception) {
        uint32 cause = (inst.exception & 0xffff);
        uint64 arg = 0, nextpc = 0;
        if(cause == RVEXPT_ILLEGAL_INST) {
            arg = inst.rawinst;
        } else if(cause == RVEXPT_INST_ACCESS_FAULT || cause == RVEXPT_INST_ADDR_MISALIGNED || cause == RVEXPT_INST_PAGE_FAULT) {
            arg = inst.pc;
        } else {
            arg = inst.vaddr;
        }
        se_exception(inst.pc, cause, arg, regs, &nextpc, &regs);
        REDIRECT(nextpc);
        return ;
    }

    if(inst.instflag & RVINSTFLAG_RDINT) {
        WRITEREG(inst.rd, RAW_DATA_AS(inst.arg0).i64);
    } else if(inst.instflag & RVINSTFLAG_RDFP) {
        WRITEREG(inst.rd + 32, RAW_DATA_AS(inst.arg0).i64);
    }

    if(inst.opcode == OP_JAL || inst.opcode == OP_JALR) {
        REDIRECT(inst.arg1);
        return ;
    } else if(inst.opcode == OP_BRANCH && inst.arg0) {
        REDIRECT(inst.arg1);
        return ;
    } else if(inst.opcode == OP_OPFP) {
        fcsr |= inst.arg1;
    } else if(inst.opcode == OP_SYSTEM) {
        if(inst.instflag & RVINSTFLAG_ECALL) {
            uint64 nextpc = 0;
            se_exception(inst.pc, RVEXPT_ECALL_U, 0, regs, &nextpc, &regs);
            REDIRECT(nextpc);
            return ;
        } else if(inst.instflag & RVINSTFLAG_EBREAK) {
            uint64 nextpc = 0;
            se_exception(inst.pc, RVEXPT_BREAK_POINT, 0, regs, &nextpc, &regs);
            REDIRECT(nextpc);
            return ;
        } else {
            // CSR
            if(inst.arg1 == 3) {
                // fcsr
                WRITEREG(inst.rd, fcsr);
                switch (inst.funct3 & 3)
                {
                case 1: fcsr = inst.arg0; break;
                case 2: fcsr |= inst.arg0; break;
                case 3: fcsr &= (~inst.arg0); break;
                }
            } else if(inst.arg1 == 2) {
                // frm
                uint64 part = ((fcsr >> 5) & 7);
                WRITEREG(inst.rd, part);
                switch (inst.funct3 & 3)
                {
                case 1: part = inst.arg0; break;
                case 2: part |= inst.arg0; break;
                case 3: part &= (~inst.arg0); break;
                }
                fcsr = (fcsr & (~(7UL << 5))) | ((part & 7) << 5);
            } else if(inst.arg1 == 1) {
                // fflags
                uint64 part = (fcsr & 31);
                WRITEREG(inst.rd, part);
                switch (inst.funct3 & 3)
                {
                case 1: part = inst.arg0; break;
                case 2: part |= inst.arg0; break;
                case 3: part &= (~inst.arg0); break;
                }
                fcsr = (fcsr & (~(31UL))) | (part & 31);
            } else if(inst.arg1 == 3072) {
                // cycle
                WRITEREG(inst.rd, get_current_tick());
            } else if(inst.arg1 == 3073) {
                // time
                WRITEREG(inst.rd, get_wall_time_tick());
            } else if(inst.arg1 == 3074) {
                // instret
                WRITEREG(inst.rd, instret);
            } 
        }
    }

    #undef REDIRECT
    #undef WRITEREG
}

void WbStage_regread(uint16 idx, uint64 * value, bool * valid) {
    internal_tick();

    if(idx > 0) {
        if(busybits & (1 << idx)) {
            *valid = false;
            return;
        } else {
            *value = regs[idx];
            *valid = true;
        }
    } else {
        *value = 0;
        *valid = true;
    }
}

void WbStage_reglock(uint16 idx) {
    internal_tick();

    if(idx > 0) {
        busybits_next |= (1 << idx);
    }
}

void WbStage_tick() {
    internal_tick();
}

void WbStage_applytick() {
    if(busybits_reset) busybits_next = 0;
    busybits_reset = false;
    busybits = busybits_next;
}

