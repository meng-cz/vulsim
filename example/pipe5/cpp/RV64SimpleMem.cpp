
#include "common.h"
#include "bundles.h"
#include "global.h"

const int64_t SizeMB = 1024;
const int64_t BaseAddr = 0x80000000UL;
const int64_t DevBaseAddr = 0xc00000000000UL;

vector<uint64> memory;
uint64 reserved_addr = 0;

void devmem_read(uint64 addr, uint8 len, uint64 * data);
void devmem_write(uint64 addr, uint8 len, uint64 data);

void RV64SimpleMem_read(uint64 paddr, uint8 len, uint64 * data, uint32 * error) {
    if(paddr >= DevBaseAddr) {
        devmem_read(paddr, len, data);
        *error = ERROR_SUCCESS;
        return;
    }
    
    if(paddr < BaseAddr || paddr + len > BaseAddr + (SizeMB * 1024UL * 1024UL)) {
        *error = ERROR_ACCESS_FAULT;
        return;
    }
    if(len > 8 || (len & (len - 1)) || (paddr & (len - 1))) {
        *error = ERROR_MISSALIGN;
        return;
    }

    memcpy(data, ((uint8_t*)(memory.data())) + (paddr - BaseAddr), len);
    *error = ERROR_SUCCESS;
    return ;
}

void RV64SimpleMem_write(uint64 paddr, uint8 len, uint64 data, uint32 * error) {
    if(paddr >= DevBaseAddr) {
        devmem_write(paddr, len, data);
        *error = ERROR_SUCCESS;
        return;
    }
    
    if(paddr < BaseAddr || paddr + len > BaseAddr + (SizeMB * 1024UL * 1024UL)) {
        *error = ERROR_ACCESS_FAULT;
        return;
    }
    if(len > 8 || (len & (len - 1)) || (paddr & (len - 1))) {
        *error = ERROR_MISSALIGN;
        return;
    }

    memcpy(((uint8_t*)(memory.data())) + (paddr - BaseAddr), &data, len);
    *error = ERROR_SUCCESS;
    return ;
}

void RV64SimpleMem_amo(uint64 paddr, uint8 len, uint8 funct7, uint64 arg, uint64 * ret, uint32 * error) {
    if(paddr < BaseAddr || paddr + len > BaseAddr + (SizeMB * 1024UL * 1024UL)) {
        *error = ERROR_ACCESS_FAULT;
        return;
    }
    if(len != 8 && len != 4) {
        *error = ERROR_MISSALIGN;
        return;
    }
    if((paddr & (len - 1))) {
        *error = ERROR_MISSALIGN;
        return;
    }

    const uint8 ADD     = 0x00;
    const uint8 SWAP    = 0x01;
    const uint8 LR      = 0x02;
    const uint8 SC      = 0x03;
    const uint8 XOR     = 0x04;
    const uint8 AND     = 0x0c;
    const uint8 OR      = 0x08;
    const uint8 MIN     = 0x10;
    const uint8 MAX     = 0x14;
    const uint8 MINU    = 0x18;
    const uint8 MAXU    = 0x1c;

    *error = ERROR_SUCCESS;
    uint8 funct5 = (funct7 >> 2);
    if(funct5 == LR) {
        reserved_addr = paddr;
        memcpy(ret, ((uint8_t*)(memory.data())) + (paddr - BaseAddr), len);
    } else if(funct5 == SC) {
        if(paddr == reserved_addr) {
            memcpy(((uint8_t*)(memory.data())) + (paddr - BaseAddr), &arg, len);
            *ret = 0;
        } else {
            *ret = 1;
        }
        reserved_addr = 0;
    } else {
        uint64 previous = 0, stdata = arg;
        memcpy(&previous, ((uint8_t*)(memory.data())) + (paddr - BaseAddr), len);
        if(len == 4) {
            switch (funct5)
            {
            case ADD : RAW_DATA_AS(stdata).i64 = RAW_DATA_AS(previous).i32 + RAW_DATA_AS(arg).i32; break;
            case AND : RAW_DATA_AS(stdata).i64 = RAW_DATA_AS(previous).i32 & RAW_DATA_AS(arg).i32; break;
            case OR  : RAW_DATA_AS(stdata).i64 = RAW_DATA_AS(previous).i32 | RAW_DATA_AS(arg).i32; break;
            case MAX : RAW_DATA_AS(stdata).i64 = std::max(RAW_DATA_AS(previous).i32, RAW_DATA_AS(arg).i32); break;
            case MIN : RAW_DATA_AS(stdata).i64 = std::min(RAW_DATA_AS(previous).i32, RAW_DATA_AS(arg).i32); break;
            case MAXU: RAW_DATA_AS(stdata).i64 = std::max(RAW_DATA_AS(previous).u32, RAW_DATA_AS(arg).u32); break;
            case MINU: RAW_DATA_AS(stdata).i64 = std::min(RAW_DATA_AS(previous).u32, RAW_DATA_AS(arg).u32); break;
            case SWAP: break;
            }
            if(previous >> 31) previous |= 0xffffffff00000000UL;
        } else {
            switch (funct5)
            {
            case ADD : RAW_DATA_AS(stdata).i64 = RAW_DATA_AS(previous).i64 + RAW_DATA_AS(arg).i64; break;
            case AND : RAW_DATA_AS(stdata).i64 = RAW_DATA_AS(previous).i64 & RAW_DATA_AS(arg).i64; break;
            case OR  : RAW_DATA_AS(stdata).i64 = RAW_DATA_AS(previous).i64 | RAW_DATA_AS(arg).i64; break;
            case MAX : RAW_DATA_AS(stdata).i64 = std::max(RAW_DATA_AS(previous).i64, RAW_DATA_AS(arg).i64); break;
            case MIN : RAW_DATA_AS(stdata).i64 = std::min(RAW_DATA_AS(previous).i64, RAW_DATA_AS(arg).i64); break;
            case MAXU: RAW_DATA_AS(stdata).i64 = std::max(RAW_DATA_AS(previous).u64, RAW_DATA_AS(arg).u64); break;
            case MINU: RAW_DATA_AS(stdata).i64 = std::min(RAW_DATA_AS(previous).u64, RAW_DATA_AS(arg).u64); break;
            case SWAP: break;
            }
        }
        *ret = previous;
        memcpy(((uint8_t*)(memory.data())) + (paddr - BaseAddr), &stdata, len);
    }
}

