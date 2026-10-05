#pragma once
#include "header.hpp"
REQUEST(audit, RESP(AuditInfo) info);
REGISTER(last, AuditInfo);
TICK_IMPL() {
    AuditInfo info{};
    audit(info);
    last.setnext(info);
}
QUERY(status, AuditInfo) { return last.get(); }
