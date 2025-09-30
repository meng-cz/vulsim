#ifndef NULLSIM_GLOBAL_H
#define NULLSIM_GLOBAL_H

#include "common.h"


#define RVEXPT_INST_ADDR_MISALIGNED     (0)
#define RVEXPT_INST_ACCESS_FAULT        (1)
#define RVEXPT_ILLEGAL_INST             (2)
#define RVEXPT_BREAK_POINT              (3)
#define RVEXPT_LOAD_ADDR_MISALIGNED     (4)
#define RVEXPT_LOAD_ACCESS_FAULT        (5)
#define RVEXPT_STORE_ADDR_MISALIGNED    (6)
#define RVEXPT_STORE_ACCESS_FAULT       (7)
#define RVEXPT_ECALL_U                  (8)
#define RVEXPT_ECALL_S                  (9)
#define RVEXPT_ECALL_M                  (11)
#define RVEXPT_INST_PAGE_FAULT          (12)
#define RVEXPT_LOAD_PAGE_FAULT          (13)
#define RVEXPT_STORE_PAGE_FAULT         (15)

#define ERROR_SUCCESS                   (0)
#define ERROR_MISS                      (1)
#define ERROR_BUSY                      (2)
#define ERROR_MISSALIGN                 (8)
#define ERROR_ACCESS_FAULT              (9)
#define ERROR_PAGE_FAULT                (10)

#define OP_LOAD         (0x03) // 000 0011
#define OP_LOADFP       (0x07) // 000 0111
#define OP_MISCMEM      (0x0F) // 000 1111
#define OP_OPIMM        (0x13) // 001 0011
#define OP_AUIPC        (0x17) // 001 0111
#define OP_OPIMM32      (0x1B) // 001 1011

#define OP_STORE        (0x23) // 010 0011
#define OP_STOREFP      (0x27) // 010 0111
#define OP_AMO          (0x2F) // 010 1111
#define OP_OP           (0x33) // 011 0011
#define OP_LUI          (0x37) // 011 0111
#define OP_OP32         (0x3B) // 011 1011

#define OP_MADD         (0x43) // 100 0011
#define OP_MSUB         (0x47) // 100 0111
#define OP_NMSUB        (0x4B) // 100 1011
#define OP_NMADD        (0x4F) // 100 1111
#define OP_OPFP         (0x53) // 101 0011
#define OP_OPV          (0x57) // 101 0111

#define OP_BRANCH       (0x63) // 110 0011
#define OP_JALR         (0x67) // 110 0111
#define OP_JAL          (0x6F) // 110 1111
#define OP_SYSTEM       (0x73) // 111 0011

#define RVINSTFLAG_RVC      (1 << 0)
#define RVINSTFLAG_UNIQUE   (1 << 1)
#define RVINSTFLAG_FENCE    (1 << 2)
#define RVINSTFLAG_FENCEI   (1 << 3)
#define RVINSTFLAG_FENCETSO (1 << 4)
#define RVINSTFLAG_SFENCE   (1 << 5)
#define RVINSTFLAG_PAUSE    (1 << 8)
#define RVINSTFLAG_ECALL    (1 << 9)
#define RVINSTFLAG_EBREAK   (1 << 10)

#define RVINSTFLAG_S1INT    (1 << 16)
#define RVINSTFLAG_S1FP     (1 << 17)
#define RVINSTFLAG_S2INT    (1 << 18)
#define RVINSTFLAG_S2FP     (1 << 19)
#define RVINSTFLAG_S3FP     (1 << 21)
#define RVINSTFLAG_RDINT    (1 << 22)
#define RVINSTFLAG_RDFP     (1 << 23)


#define RV_FFLAGS_INVALID       (1UL<<4)
#define RV_FFLAGS_DIVBYZERO     (1UL<<3)
#define RV_FFLAGS_OVERFLOW      (1UL<<2)
#define RV_FFLAGS_UNDERFLOW     (1UL<<1)
#define RV_FFLAGS_INEXACT       (1UL<<0)



typedef union {
    uint64_t    u64;
    int64_t     i64;
    uint32_t    u32;
    int32_t     i32;
    uint16_t    u16;
    int16_t     i16;
    double      f64;
    float       f32;
} __internal_data_t;
#define RAW_DATA_AS(data) (*((__internal_data_t*)(&(data))))



#endif
