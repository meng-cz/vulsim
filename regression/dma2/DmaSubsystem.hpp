#pragma once
#include "header.hpp"
PARAMETER(BASE_DEPTH, 2);
SERVICE(submit0, handshake=1, ARG(DmaCommand) command);
SERVICE(submit1, handshake=1, ARG(DmaCommand) command);
SERVICE(response0, ARG(MemResponse) response);
SERVICE(response1, ARG(MemResponse) response);
REQUEST(offer0, handshake=1, ARG(MemRequest) request, RESP(OfferInfo) info);
REQUEST(offer1, handshake=1, ARG(MemRequest) request, RESP(OfferInfo) info);
REQUEST(done0, handshake=1, ARG(DmaCompletion) completion);
REQUEST(done1, handshake=1, ARG(DmaCompletion) completion);
CHILD_INSTANCE(DmaChannel, a_low, PARAM(CHANNEL)=1, PARAM(CMD_DEPTH)=BASE_DEPTH+2, PARAM(DATA_WIDTH)=29, PARAM(STRIDE)=BASE_DEPTH-1);
CHILD_INSTANCE(DmaChannel, z_high);
CONNECT_S_CS(submit0, z_high, submit);
CONNECT_S_CS(submit1, a_low, submit);
CONNECT_S_CS(response0, z_high, response);
CONNECT_S_CS(response1, a_low, response);
CONNECT_CR_R(z_high, offer, offer0);
CONNECT_CR_R(a_low, offer, offer1);
CONNECT_CR_R(z_high, done, done0);
CONNECT_CR_R(a_low, done, done1);
USE_CHILD_QUERY(z_high, status, high_status, ChannelStatus);
USE_CHILD_QUERY(a_low, status, low_status, ChannelStatus);
QUERY(status, DmaStatus) {
    DmaStatus s{};
    s.high = high_status(); s.low = low_status(); s.base_depth = BASE_DEPTH;
    return s;
}
