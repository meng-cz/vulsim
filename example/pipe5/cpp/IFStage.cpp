
#include "common.h"
#include "global.h"
#include "bundles.h"


bool if_out_can_push();
void if_out_push(IDData &value);

uint64 pc_get();
void pc_setnext(uint64 &value, uint8 priority);

void icache_read(uint64 paddr, uint8 len, uint64 * data, uint32 * error);
void translate(uint64 vaddr, uint64 * paddr, uint32 * error);

void IFStage_redirect(uint64 pc) {
    pc_setnext(pc, 0);
}

void IFStage_tick() {
    if(!if_out_can_push()) {
        return;
    }

    uint64 curpc = pc_get();
    uint64 inst = 0;

    if(curpc == 0) return;

    IDData fetch;
    memset(&fetch, 0, sizeof(fetch));
    fetch.pc = curpc;

    #define FETCH_16BITS(vpc, inst, err) do { \ 
        uint64 ppc = 0; translate(vpc, &ppc, &err); if(err == ERROR_SUCCESS) icache_read(ppc, 2, &inst, &err); \ 
    } while(0)

    uint32 err = ERROR_SUCCESS;
    FETCH_16BITS(curpc, inst, err);
    if(err == ERROR_SUCCESS && ((inst & 3) == 3)) {
        uint64 inst2 = 0;
        FETCH_16BITS(curpc + 2, inst2, err);
        inst |= (inst2 << 16);
    }

    if(err == ERROR_MISSALIGN) {
        fetch.exception = ((1<<30) | RVEXPT_INST_ADDR_MISALIGNED);
    } else if(err == ERROR_ACCESS_FAULT) {
        fetch.exception = ((1<<30) | RVEXPT_INST_ACCESS_FAULT);
    } else if(err == ERROR_PAGE_FAULT) {
        fetch.exception = ((1<<30) | RVEXPT_INST_PAGE_FAULT);
    } else if(err == ERROR_MISS || err == ERROR_BUSY) {
        return;
    }
    assertf(err == ERROR_SUCCESS, "Invalid Error Value in IFStage: %d", err);

    #undef FETCH_16BITS

    #define GET_BITS_AT(n, offset, len) (((n) >> (offset)) & ((1<<(len)) - 1))

    uint64 nextpc = (((inst & 3) == 3)?(curpc+4):(curpc+2));
    if((inst & 3) == 1 && ((inst>>13) & 7) == 5) {
        // C.J
        int32 imm = 0;
        imm |= (GET_BITS_AT(inst, 2, 1) << 5);
        imm |= (GET_BITS_AT(inst, 3, 3) << 1);
        imm |= (GET_BITS_AT(inst, 6, 1) << 7);
        imm |= (GET_BITS_AT(inst, 7, 1) << 6);
        imm |= (GET_BITS_AT(inst, 8, 1) << 10);
        imm |= (GET_BITS_AT(inst, 9, 2) << 8);
        imm |= (GET_BITS_AT(inst, 11, 1) << 4);
        if((inst >> 12) & 1) imm |= 0xfffff800U;
        nextpc = curpc + imm;
    } else if((inst & 0x7F) == OP_JAL) {
        // JAL
        int32 imm = (GET_BITS_AT(inst, 21, 10) << 1) | (GET_BITS_AT(inst, 20, 1) << 11) | (inst & 0xFF000);
        if(inst & (1<<31)) imm = imm | 0xfff00000U;
        nextpc = curpc + imm;
    }
    pc_setnext(nextpc, 1);
    
    #undef GET_BITS_AT

    fetch.rawinst = inst;
    if_out_push(fetch);

}
