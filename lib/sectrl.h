#pragma once

#include "common.h"

class VulSimSEController {

public:

    const int64 __internal_config_memory_size_mb = 1024;
    const int64 __internal_config_base_addr = 0x80000000UL;
    const int64 __internal_config_dev_base_addr = 0xc00000000000UL;

    class ConstructorArguments {
    public:
        void (*request_redirect)(uint32 cpuid);
        void (*request_dma_read)(uint64 addr, array<uint8,64> * data);
        void (*request_dma_write)(uint64 addr, array<uint8,64> & data);
        int32 __dummy = 0;
    };

    VulSimSEController(ConstructorArguments & arg) {
        this->_request_redirect = arg.request_redirect;
        this->_request_dma_read = arg.request_dma_read;
        this->_request_dma_write = arg.request_dma_write;
    }

    ~VulSimSEController() {
    }

    void all_current_tick();
    void all_current_applytick();

    void translate(uint32 cpuid, uint64 vaddr, uint64 * paddr, uint32 * error);
    void se_exception(uint32 cpuid, uint64 pc, uint32 cause, uint64 arg, array<uint64, 64> & regs, uint64 * nextpc, array<uint64, 64> * modregs);
    void devmem_read(uint64 paddr, uint8 len, uint64 * data);
    void devmem_write(uint64 paddr, uint8 len, uint64 data);

    void (*_request_redirect)(uint32 cpuid);
    void redirect(uint32 cpuid) { _request_redirect(cpuid); };
    void (*_request_dma_read)(uint64 addr, array<uint8,64> * data);
    void dma_read(uint64 addr, array<uint8,64> * data) { _request_dma_read(addr, data); };
    void (*_request_dma_write)(uint64 addr, array<uint8,64> & data);
    void dma_write(uint64 addr, array<uint8,64> & data) { _request_dma_write(addr, data); };


};

