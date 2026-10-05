#pragma once
#include <cstdint>
#include <defhelper.hpp>
CONFIG(MEM_WORDS, 37);
CONFIG(S_IDLE, 0);
CONFIG(S_READ, 1);
CONFIG(S_WRITE, 2);
CONFIG(S_ADVANCE, 3);
CONFIG(S_DONE, 4);
CONFIG(S_WAIT_READ, 5);
CONFIG(S_WAIT_WRITE, 6);
STRUCT(DmaCommand) { uint32_t task; uint32_t src; uint32_t dst; uint32_t length; };
STRUCT(MemRequest) { uint32_t task; uint32_t element; bool write; uint32_t address; uint32_t data; };
STRUCT(OfferInfo) { uint32_t cycle; uint32_t channel; };
STRUCT(MemResponse) { uint32_t task; uint32_t element; bool write; uint32_t data; };
STRUCT(DmaCompletion) { uint32_t task; uint32_t elements; };
STRUCT(AuditInfo) { uint32_t cycle; bool reserved; uint32_t channel; MemRequest request; bool executed; };
STRUCT(BufferStatus) { uint32_t depth; uint32_t enqueued; uint32_t popped; bool can_push; bool can_pop; };
STRUCT(EngineStatus) {
    uint32_t width; uint32_t stride; uint32_t channel; uint32_t state;
    DmaCommand command; uint32_t element; uint32_t data;
    uint32_t accepted; uint32_t rejected; uint32_t completed; uint32_t responses;
    OfferInfo last_offer; MemResponse last_response; bool error;
};
STRUCT(ChannelStatus) { BufferStatus buffer; EngineStatus engine; uint32_t completed; };
STRUCT(DmaStatus) { ChannelStatus high; ChannelStatus low; uint32_t base_depth; };
ALIAS(MemoryWords, uint32_t, dims=[MEM_WORDS]);
STRUCT(MemoryStatus) { uint32_t words; uint32_t addr_bits; uint32_t seed; MemoryWords data; };
STRUCT(ArbiterStatus) { uint32_t cycle; uint32_t accesses; AuditInfo last; };
STRUCT(MemorySystemStatus) { MemoryStatus memory; ArbiterStatus arbiter; };
STRUCT(TopStatus) { DmaStatus dma; MemorySystemStatus memory; AuditInfo observed; uint32_t base_depth; uint32_t seed; };
