
#include "common.h"
#include "global.h"
#include "bundles.h"

bool id_in_can_pop();
IDData & id_in_top();
void id_in_pop();

bool id_out_can_push();
void id_out_push(EXData &value);

void decode(uint32 rawinst, RV64DecInst * decinst);
void cdecode(uint32 rawinst, RV64DecInst * decinst);

void IDStage_tick() {
    if(!(id_in_can_pop() && id_out_can_push())) {
        return;
    }

    IDData fetch = id_in_top();
    EXData ex;
    memset(&ex, 0, sizeof(ex));
    ex.pc = fetch.pc;
    ex.rawinst = fetch.rawinst;

    if(fetch.exception) {
        ex.exception = fetch.exception;
        id_in_pop();
        id_out_push(ex);
        return;
    }

    RV64DecInst dec;
    if((fetch.rawinst & 3) == 3) {
        decode(fetch.rawinst, &dec);
    } else {
        cdecode(fetch.rawinst, &dec);
    }

    if(!dec.expt) {

        ex.opcode = dec.opcode;
        ex.rd = dec.rd;
        ex.rs1 = dec.rs1;
        ex.rs2 = dec.rs2;
        ex.funct3 = dec.funct3;
        ex.funct7 = dec.funct7;
        ex.imm = dec.imm;
        ex.instflag = dec.flag;

    } else {

        ex.exception = ((1<<30) | RVEXPT_ILLEGAL_INST);

    }

    id_in_pop();
    id_out_push(ex);
}

