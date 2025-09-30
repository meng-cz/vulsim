#ifndef NULLSIM_BUNDLES_H
#define NULLSIM_BUNDLES_H

#include "common.h"

typedef struct {
    uint8   opcode;
    bool    expt;
    uint8   rd;
    uint8   rs1;
    uint8   rs2;
    uint8   funct3;
    uint8   funct7;
    int32   imm;
    uint32  flag;
} RV64DecInst;

typedef struct {
    uint64      pc;
    uint32      rawinst;
    uint32      exception;
} IDData;

typedef struct {
    uint64      pc;
    uint8       opcode;
    uint8       rd;
    uint8       rs1;
    uint8       rs2;
    uint8       funct3;
    uint8       funct7;
    int32       imm;
    uint32      instflag;
    uint32      rawinst;
    uint32      exception;
} EXData;

typedef struct {
    uint64      pc;
    uint64      arg0;
    uint64      arg1;
    uint64      vaddr;
    uint8       opcode;
    uint8       rd;
    uint8       funct3;
    uint8       funct7;
    uint32      instflag;
    uint32      rawinst;
    uint32      exception;
} MemData;


#endif
