#pragma once
#include "header.hpp"
PARAMETER(DATA_WIDTH, 17);
PARAMETER(STRIDE, 1);
PARAMETER(CHANNEL, 0);
ALIAS(LocalData, Int<DATA_WIDTH>);
REQUEST(pop, handshake=1, RESP(DmaCommand) command);
REQUEST(offer, handshake=1, ARG(MemRequest) request, RESP(OfferInfo) info);
REQUEST(done, handshake=1, ARG(DmaCompletion) completion);
REQUEST(record_done);
REGISTER(state, uint32_t, ports=2);
REGISTER(command_reg, DmaCommand);
REGISTER(element, uint32_t);
REGISTER(data_reg, LocalData);
REGISTER(accepted, uint32_t);
REGISTER(rejected, uint32_t);
REGISTER(completed, uint32_t);
REGISTER(responses, uint32_t);
REGISTER(last_offer, OfferInfo);
REGISTER(last_response, MemResponse);
REGISTER(error, bool);
WIRE(issued, bool) { issued = false; }
SERVICE(response, priority=-10, ARG(MemResponse) response) {
    DmaCommand c = command_reg.get();
    if (!issued || response.task != c.task || response.element != element.get() ||
        response.write != (state.get() == S_WRITE)) {
        error.setnext(true);
    }
    last_response.setnext(response);
    responses.setnext(responses + 1);
    if (response.write) {
        state.setnext<0>(S_ADVANCE);
    } else {
        data_reg.setnext(LocalData(response.data));
        state.setnext<0>(S_WRITE);
    }
}
TICK_IMPL() {
    uint32_t s = state.get();
    DmaCommand c = command_reg.get();
    if (s == S_IDLE) {
        DmaCommand next{};
        if (pop(next)) {
            command_reg.setnext(next);
            element.setnext(0);
            state.setnext<1>(next.length == 0 ? S_DONE : S_READ);
        }
    } else if (s == S_READ || s == S_WRITE) {
        MemRequest request{};
        request.task = c.task; request.element = element.get(); request.write = s == S_WRITE;
        request.address = (request.write ? c.dst : c.src) + element.get() * STRIDE;
        request.data = request.write ? data_reg.get().to<uint32_t>() : 0;
        OfferInfo info{};
        if (offer(request, info)) {
            issued = true;
            last_offer.setnext(info);
            accepted.setnext(accepted + 1);
            state.setnext<1>(request.write ? S_WAIT_WRITE : S_WAIT_READ);
        } else {
            rejected.setnext(rejected + 1);
        }
    } else if (s == S_ADVANCE) {
        uint32_t next = element.get() + 1;
        element.setnext(next);
        state.setnext<1>(next == c.length ? S_DONE : S_READ);
    } else if (s == S_DONE) {
        DmaCompletion completion{};
        completion.task = c.task; completion.elements = c.length;
        if (done(completion)) {
            completed.setnext(completed + 1);
            record_done();
            state.setnext<1>(S_IDLE);
        }
    }
}
QUERY(status, EngineStatus) {
    EngineStatus s{};
    s.width = DATA_WIDTH; s.stride = STRIDE; s.channel = CHANNEL; s.state = state;
    s.command = command_reg.get(); s.element = element; s.data = data_reg.get().to<uint32_t>();
    s.accepted = accepted; s.rejected = rejected; s.completed = completed; s.responses = responses;
    s.last_offer = last_offer.get(); s.last_response = last_response.get(); s.error = error;
    return s;
}
