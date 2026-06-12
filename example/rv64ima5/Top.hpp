#pragma once

#include "core/Core.hpp"
#include "header.hpp"

REQUEST(icache_req, ARG(uint64_t) pc);
REQUEST(icache_resp, RESP(bool) hit, RESP(uint32_t) inst);
REQUEST(dcache_req, ARG(MemRequest) req);
REQUEST(dcache_resp, RESP(bool) hit, RESP(MemResponse) resp);
REQUEST(trace_wb, ARG(uint32_t) rd, ARG(uint64_t) data, ARG(bool) wen);
REQUEST(trace_halt, ARG(uint64_t) pc);

CHILD_INSTANCE(Core, core);

USE_CHILD_QUERY(core, status, core_status, CoreStatus);

CONNECT_CR_R(core, icache_req, icache_req);
CONNECT_CR_R(core, icache_resp, icache_resp);
CONNECT_CR_R(core, dcache_req, dcache_req);
CONNECT_CR_R(core, dcache_resp, dcache_resp);
CONNECT_CR_R(core, trace_wb, trace_wb);
CONNECT_CR_R(core, trace_halt, trace_halt);

QUERY(status, CoreStatus) {
    return core_status();
}
