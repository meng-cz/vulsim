#pragma once
#include "header.hpp"
REQUEST(read, ARG(uint32_t) address, RESP(uint32_t) data);
REQUEST(write, ARG(uint32_t) address, ARG(uint32_t) data);
REQUEST(response0, ARG(MemResponse) response);
REQUEST(response1, ARG(MemResponse) response);
REGISTER(cycle, uint32_t);
REGISTER(accesses, uint32_t);
REGISTER(last, AuditInfo);
WIRE(reservation, AuditInfo) { reservation = AuditInfo{}; }
SERVICE(offer0, priority=20, ready=(cycle.get() % 5 != 2 && !reservation.reserved), ARG(MemRequest) request, RESP(OfferInfo) info) {
    reservation.reserved = true; reservation.channel = 0; reservation.request = request;
    info = OfferInfo{cycle.get(), 0};
}
SERVICE(offer1, priority=10, ready=(cycle.get() % 5 != 2 && !reservation.reserved), ARG(MemRequest) request, RESP(OfferInfo) info) {
    reservation.reserved = true; reservation.channel = 1; reservation.request = request;
    info = OfferInfo{cycle.get(), 1};
}
TICK_IMPL() {
    reservation.cycle = cycle.get();
    if (reservation.reserved) {
        MemRequest r = reservation.request;
        uint32_t data = 0;
        if (r.write) write(r.address, r.data);
        else read(r.address, data);
        reservation.executed = true;
        MemResponse response{};
        response.task = r.task; response.element = r.element; response.write = r.write; response.data = data;
        if (reservation.channel == 0) response0(response);
        else response1(response);
        accesses.setnext(accesses + 1);
    }
    last.setnext(reservation);
    cycle.setnext(cycle + 1);
}
SERVICE(audit, priority=-20, RESP(AuditInfo) info) { info = reservation; }
QUERY(status, ArbiterStatus) {
    ArbiterStatus s{};
    s.cycle = cycle; s.accesses = accesses; s.last = last.get();
    return s;
}
