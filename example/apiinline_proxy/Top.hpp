#pragma once

#include "header.hpp"

QUEUE(simple_q, ProxyPayload, 4);
QUEUE_MP(mp_q, ProxyPayload, 8, 2, 2);

BRAM_1RW(rw_mem, ProxyPayload, PROXY_MEM_SIZE);
BRAM(bank_mem, ProxyPayload, PROXY_MEM_SIZE, 2, 2);
ROM(coeff_rom, 16, PROXY_MEM_SIZE, 2, rom_init.hex);

REGISTER(addr_reg, uint32_t) {
    addr_reg = 0;
}

REQUEST(done, ARG(uint32_t) code);
REQUEST_READY(emit_payload, ARG(ProxyPayload) payload, RESP(ProxyPayload) echoed);

TICK_IMPL() {
    uint32_t addr = addr_reg & (PROXY_MEM_SIZE - 1);

    ProxyPayload payload;
    payload.data = addr_reg + coeff_rom.readdata<0>().template to<uint32_t>();
    payload.meta.tag = static_cast<uint8_t>(addr);
    payload.meta.valid = true;

    coeff_rom.readreq<0>(addr);
    coeff_rom.readreq<1>((addr + 1) & (PROXY_MEM_SIZE - 1));

    if (simple_q.enqready()) {
        simple_q.enqnext(payload);
    }
    if (simple_q.deqvalid()) {
        ProxyPayload head = simple_q.front();
        bank_mem.write<0>(addr, head);
        simple_q.deqnext();
    }
    if (addr == 0) {
        simple_q.clrnext();
    }

    PayloadPair pair;
    pair[0] = payload;
    pair[1] = payload;
    pair[1].data = bank_mem.readdata<1>().data + coeff_rom.readdata<1>().template to<uint32_t>();
    if (mp_q.enqreqdy() >= 2) {
        mp_q.enqnext(pair, 2);
    }
    if (mp_q.deqvalid() > 0) {
        PayloadPair popped = mp_q.front(2);
        rw_mem.req(addr, popped[0], true);
        mp_q.deqnext(1);
    }
    if (addr == 3) {
        mp_q.clrnext();
    }

    bank_mem.readreq<0>(addr);
    bank_mem.readreq<1>((addr + 1) & (PROXY_MEM_SIZE - 1));

    ProxyPayload echoed;
    if (emit_payload(payload, echoed)) {
        rw_mem.req((addr + 1) & (PROXY_MEM_SIZE - 1), echoed, true);
    }

    ProxyPayload rw_read = rw_mem.readdata();
    done(rw_read.data + coeff_rom.readdata<0>().template to<uint32_t>());
    addr_reg.setnext(addr_reg + 1);
}
