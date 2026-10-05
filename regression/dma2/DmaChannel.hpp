#pragma once
#include "header.hpp"
PARAMETER(CMD_DEPTH, 1);
PARAMETER(DATA_WIDTH, 17);
PARAMETER(STRIDE, 1);
PARAMETER(CHANNEL, 0);
SERVICE(submit, handshake=1, ARG(DmaCommand) command);
SERVICE(response, ARG(MemResponse) response);
REQUEST(offer, handshake=1, ARG(MemRequest) request, RESP(OfferInfo) info);
REQUEST(done, handshake=1, ARG(DmaCompletion) completion);
CHILD_INSTANCE(CommandBuffer, buffer, PARAM(DEPTH)=CMD_DEPTH);
CHILD_INSTANCE(TransferEngine, engine, PARAM(DATA_WIDTH)=DATA_WIDTH, PARAM(STRIDE)=STRIDE, PARAM(CHANNEL)=CHANNEL);
USE_CHILD_SERVICE(buffer, pop, buffer_pop, RESP(DmaCommand) command);
USE_CHILD_QUERY(buffer, status, buffer_status, BufferStatus);
USE_CHILD_QUERY(engine, status, engine_status, EngineStatus);
CONNECT_S_CS(submit, buffer, submit);
CONNECT_S_CS(response, engine, response);
CONNECT_CR_R(engine, offer, offer);
CONNECT_CR_R(engine, done, done);
CONNECT_CR_S(engine, pop, pop_command);
CONNECT_CR_S(engine, record_done, record_done);
REGISTER(completed, uint32_t);
SERVICE(pop_command, ready=buffer_status().can_pop, RESP(DmaCommand) command) {
    buffer_pop(command);
}
SERVICE(record_done) { completed.setnext(completed + 1); }
QUERY(status, ChannelStatus) {
    ChannelStatus s{};
    s.buffer = buffer_status(); s.engine = engine_status(); s.completed = completed;
    return s;
}
