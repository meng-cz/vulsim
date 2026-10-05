#pragma once
#include "header.hpp"
PARAMETER(BASE_DEPTH, 2);
PARAMETER(RESET_SEED, 0x1234);
SERVICE(submit0, handshake=1, ARG(DmaCommand) command);
SERVICE(submit1, handshake=1, ARG(DmaCommand) command);
REQUEST(done0, handshake=1, ARG(DmaCompletion) completion);
REQUEST(done1, handshake=1, ARG(DmaCompletion) completion);
CHILD_INSTANCE(MemorySubsystem, a_memory, PARAM(RESET_SEED)=RESET_SEED);
CHILD_INSTANCE(CycleObserver, b_observer);
CHILD_INSTANCE(DmaSubsystem, z_dma, PARAM(BASE_DEPTH)=BASE_DEPTH);
CONNECT_S_CS(submit0, z_dma, submit0);
CONNECT_S_CS(submit1, z_dma, submit1);
CONNECT_CR_R(z_dma, done0, done0);
CONNECT_CR_R(z_dma, done1, done1);
CONNECT_CR_CS(z_dma, offer0, a_memory, offer0);
CONNECT_CR_CS(z_dma, offer1, a_memory, offer1);
CONNECT_CR_CS(a_memory, response0, z_dma, response0);
CONNECT_CR_CS(a_memory, response1, z_dma, response1);
CONNECT_CR_CS(b_observer, audit, a_memory, audit);
USE_CHILD_QUERY(z_dma, status, dma_status, DmaStatus);
USE_CHILD_QUERY(a_memory, status, memory_status, MemorySystemStatus);
USE_CHILD_QUERY(b_observer, status, observer_status, AuditInfo);
QUERY(snapshot, TopStatus) {
    TopStatus s{};
    s.dma = dma_status(); s.memory = memory_status(); s.observed = observer_status();
    s.base_depth = BASE_DEPTH; s.seed = RESET_SEED;
    return s;
}
