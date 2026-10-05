#pragma once
#include "header.hpp"
PARAMETER(RESET_SEED, 0x1234);
SERVICE(offer0, handshake=1, ARG(MemRequest) request, RESP(OfferInfo) info);
SERVICE(offer1, handshake=1, ARG(MemRequest) request, RESP(OfferInfo) info);
SERVICE(audit, RESP(AuditInfo) info);
REQUEST(response0, ARG(MemResponse) response);
REQUEST(response1, ARG(MemResponse) response);
CHILD_INSTANCE(MemoryArbiter, arbiter);
CHILD_INSTANCE(Scratchpad, scratchpad, PARAM(WORDS)=MEM_WORDS, PARAM(RESET_SEED)=RESET_SEED);
CONNECT_S_CS(offer0, arbiter, offer0);
CONNECT_S_CS(offer1, arbiter, offer1);
CONNECT_S_CS(audit, arbiter, audit);
CONNECT_CR_R(arbiter, response0, response0);
CONNECT_CR_R(arbiter, response1, response1);
CONNECT_CR_S(arbiter, read, read_memory);
CONNECT_CR_S(arbiter, write, write_memory);
USE_CHILD_SERVICE(scratchpad, read, memory_read, ARG(uint32_t) address, RESP(uint32_t) data);
USE_CHILD_SERVICE(scratchpad, write, memory_write, ARG(uint32_t) address, ARG(uint32_t) data);
SERVICE(read_memory, ARG(uint32_t) address, RESP(uint32_t) data) { memory_read(address, data); }
SERVICE(write_memory, ARG(uint32_t) address, ARG(uint32_t) data) { memory_write(address, data); }
USE_CHILD_QUERY(scratchpad, status, memory_status, MemoryStatus);
USE_CHILD_QUERY(arbiter, status, arbiter_status, ArbiterStatus);
QUERY(status, MemorySystemStatus) {
    MemorySystemStatus s{};
    s.memory = memory_status(); s.arbiter = arbiter_status();
    return s;
}
