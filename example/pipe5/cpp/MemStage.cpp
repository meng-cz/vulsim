
#include "common.h"
#include "global.h"
#include "bundles.h"

bool mem_in_can_pop();
MemData & mem_in_top();
void mem_in_pop();

bool mem_out_can_push();
void mem_out_push(MemData &value);

bool stalled = false;
void stall();


void dcache_read(uint64 paddr, uint8 len, uint64 * data, uint32 * error);
void dcache_write(uint64 paddr, uint8 len, uint64 data, uint32 * error);
void dcache_amo(uint64 paddr, uint8 len, uint8 funct7, uint64 arg, uint64 * ret, uint32 * error);
void translate(uint64 vaddr, uint64 * paddr, uint32 * error);


void MemStage_tick() {

    if(stalled) {
        return;
    }

    if(!(mem_in_can_pop() && mem_out_can_push())) {
        return;
    }

    MemData inst = mem_in_top();

    if(inst.exception) {
        mem_out_push(inst);
        mem_in_pop();
        return;
    }

    uint8 len = 0;
    switch (inst.funct3)
    {
    case 0: case 4: len = 1; break;
    case 1: case 5: len = 2; break;
    case 2: case 6: len = 4; break;
    case 3: len = 8; break;
    }

    uint32 err = ERROR_SUCCESS;
    bool format_arg0_as_signed = false;
    if(inst.opcode == OP_AMO) {
        if(len == 4 || len == 8) {
            uint64 vaddr = inst.vaddr;
            uint64 paddr = vaddr;
            uint64 value = 0;
            translate(vaddr, &paddr, &err);
            if(err == ERROR_SUCCESS) {
                dcache_amo(paddr, len, inst.funct7, inst.arg0, &value, &err);
                if(err == ERROR_SUCCESS) {
                    inst.arg0 = value;
                    format_arg0_as_signed = true;
                }
            }
        } else {
            inst.exception = ((1 << 30) | RVEXPT_ILLEGAL_INST);
        }
    } else if(inst.opcode == OP_LOAD || inst.opcode == OP_LOADFP) {
        if(len != 0) {
            uint64 vaddr = inst.vaddr;
            uint64 paddr = vaddr;
            translate(vaddr, &paddr, &err);
            if(err == ERROR_SUCCESS) {
                dcache_read(paddr, len, &inst.arg0, &err);
                format_arg0_as_signed = true;
            }
        } else {
            inst.exception = ((1 << 30) | RVEXPT_ILLEGAL_INST);
        }
    } else if(inst.opcode == OP_STORE || inst.opcode == OP_STOREFP) {
        if(len != 0) {
            uint64 vaddr = inst.vaddr;
            uint64 paddr = vaddr;
            translate(vaddr, &paddr, &err);
            if(err == ERROR_SUCCESS) {
                dcache_write(paddr, len, inst.arg0, &err);
            }
        } else {
            inst.exception = ((1 << 30) | RVEXPT_ILLEGAL_INST);
        }
    }

    if(err == ERROR_MISS || err == ERROR_BUSY) {
        stall();
        return ;
    } else if(err == ERROR_MISSALIGN) {
        if(inst.opcode == OP_LOAD || inst.opcode == OP_LOADFP) {
            inst.exception = ((1 << 30) | RVEXPT_LOAD_ADDR_MISALIGNED);
        } else {
            inst.exception = ((1 << 30) | RVEXPT_STORE_ADDR_MISALIGNED);
        }
    } else if(err == ERROR_ACCESS_FAULT) {
        if(inst.opcode == OP_LOAD || inst.opcode == OP_LOADFP) {
            inst.exception = ((1 << 30) | RVEXPT_LOAD_ACCESS_FAULT);
        } else {
            inst.exception = ((1 << 30) | RVEXPT_STORE_ACCESS_FAULT);
        }
    } else if(err == ERROR_PAGE_FAULT) {
        if(inst.opcode == OP_LOAD || inst.opcode == OP_LOADFP) {
            inst.exception = ((1 << 30) | RVEXPT_LOAD_PAGE_FAULT);
        } else {
            inst.exception = ((1 << 30) | RVEXPT_STORE_PAGE_FAULT);
        }
    }

    if(format_arg0_as_signed) {
        if(inst.funct3 == 0) {
            if(inst.arg0 >> 7) inst.arg0 |= 0xffffffffffffff00UL;
        } else if(inst.funct3 == 1) {
            if(inst.arg0 >> 15) inst.arg0 |= 0xffffffffffff0000UL;
        } else if(inst.funct3 == 2) {
            if(inst.arg0 >> 31) inst.arg0 |= 0xffffffff00000000UL;
        }
    }

    mem_out_push(inst);
    mem_in_pop();
    return;
}

