// Copyright (c) 2025 Meng Chengzhen, in Shandong University
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "module_handlers.hpp"

#include "common.hpp"
#include "../stringop.hpp"

using namespace cppparse;
using namespace stringop;

VCPPModuleHandlerRegistry& VCPPModuleHandlerRegistry::instance() {
    static VCPPModuleHandlerRegistry registry;
    return registry;
}

bool VCPPModuleHandlerRegistry::register_handler(std::unique_ptr<VCPPModuleHandler> handler) {
    std::string handler_name = handler->name();
    if (handlers_.count(handler_name) > 0) {
        return false;
    }
    handlers_[handler_name] = std::move(handler);
    return true;
}

void VCPPModuleHandlerRegistry::run(VCPPModuleContext &context, const MacroEntry &entry) {
    std::string handler_name = entry.name;
    if (handlers_.count(handler_name) == 0) {
        throw VulException("No handler registered for macro: " + entry.name + " at " + context.getOriginalPosition(entry.pos));
    }
    handlers_[handler_name]->run(context, entry);
}

class VCPPModuleCONFIG : public VCPPModuleHandler {
public:
    virtual string name() const { return "CONFIG"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 2) {
            throw VulException("CONFIG requires exactly 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempConfig config;
        config.name = entry.args[0];
        config.value = entry.args[1];
        context.declareName(config.name, "CONFIG", entry);
        context.temp.configs.push_back(std::move(config));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleCONFIG> _auto_register_CONFIG_handler;

class VCPPModulePARAMETER : public VCPPModuleHandler {
public:
    virtual string name() const { return "PARAMETER"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 2) {
            throw VulException("PARAMETER requires exactly 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempConfig param;
        param.name = entry.args[0];
        param.value = entry.args[1];
        context.declareName(param.name, "PARAMETER", entry);
        context.temp.params.push_back(std::move(param));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModulePARAMETER> _auto_register_PARAMETER_handler;

class VCPPModuleALIAS : public VCPPModuleHandler {
public:
    virtual string name() const { return "ALIAS"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 2) {
            throw VulException("ALIAS requires at least 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempBundle bundle;
        bundle.name = entry.args[0];
        bundle.is_alias = true;
        context.declareName(bundle.name, "ALIAS", entry);
        VulTempBundleMember alias_member;
        alias_member.name = "target";
        alias_member.type = entry.args[1];
        for (size_t i = 2; i < entry.args.size(); ++i) {
            auto attr = parseKeyValueArg(entry.args[i]);
            if (isAttrKey(attr, "dims")) {
                appendDimList(alias_member.dims, attr->second);
            } else if (isAttrKey(attr, "dim")) {
                appendDimList(alias_member.dims, attr->second);
            } else if (attr) {
                throw VulException("ALIAS has unknown attribute '" + attr->first + "' at " + context.getOriginalPosition(entry.pos));
            } else {
                alias_member.dims.push_back(entry.args[i]);
            }
        }
        bundle.members.push_back(std::move(alias_member));
        context.temp.bundles.push_back(std::move(bundle));
    }
};

class VCPPModuleALIAS_ARRAY1 : public VCPPModuleHandler {
public:
    virtual string name() const { return "ALIAS_ARRAY1"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 3) {
            throw VulException("ALIAS_ARRAY1 requires exactly 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempBundle bundle;
        bundle.name = entry.args[0];
        bundle.is_alias = true;
        context.declareName(bundle.name, "ALIAS_ARRAY1", entry);
        VulTempBundleMember alias_member;
        alias_member.name = "target";
        alias_member.type = entry.args[1];
        for (size_t i = 2; i < entry.args.size(); ++i) {
            alias_member.dims.push_back(entry.args[i]);
        }
        bundle.members.push_back(std::move(alias_member));
        context.temp.bundles.push_back(std::move(bundle));
    }
};

class VCPPModuleALIAS_ARRAY2 : public VCPPModuleHandler {
public:
    virtual string name() const { return "ALIAS_ARRAY2"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 4) {
            throw VulException("ALIAS_ARRAY2 requires exactly 4 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempBundle bundle;
        bundle.name = entry.args[0];
        bundle.is_alias = true;
        context.declareName(bundle.name, "ALIAS_ARRAY2", entry);
        VulTempBundleMember alias_member;
        alias_member.name = "target";
        alias_member.type = entry.args[1];
        for (size_t i = 2; i < entry.args.size(); ++i) {
            alias_member.dims.push_back(entry.args[i]);
        }
        bundle.members.push_back(std::move(alias_member));
        context.temp.bundles.push_back(std::move(bundle));
    }
};

static VCPPModuleAutoRegisterHandler<VCPPModuleALIAS> _auto_register_ALIAS_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleALIAS_ARRAY1> _auto_register_ALIAS_ARRAY1_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleALIAS_ARRAY2> _auto_register_ALIAS_ARRAY2_handler;

class VCPPModuleSTRUCT : public VCPPModuleHandler {
public:
    virtual string name() const { return "STRUCT"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 1) {
            throw VulException("STRUCT requires exactly 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        VulErrorContextGuard _err{"Processing STRUCT '" + entry.args[0] + "' at " + context.getOriginalPosition(entry.pos)};
        VulTempBundle bundle;
        bundle.name = entry.args[0];
        bundle.is_alias = false;
        context.declareName(bundle.name, "STRUCT", entry);
        string body;
        for (const auto line : entry.body) {
            body += trim(line);
        }
        vector<string> member_strs = split(body, ';');
        for (const auto &member_str : member_strs) {
            if (member_str.empty()) continue;
            VulTempBundleMember member = parseMemberDeclaration(member_str);
            bundle.members.push_back(std::move(member));
        }
        context.temp.bundles.push_back(std::move(bundle));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleSTRUCT> _auto_register_STRUCT_handler;

class VCPPModuleENUM : public VCPPModuleHandler {
public:
    virtual string name() const { return "ENUM"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 1) {
            throw VulException("ENUM requires exactly 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        VulErrorContextGuard _err{"Processing ENUM '" + entry.args[0] + "' at " + context.getOriginalPosition(entry.pos)};
        VulTempBundle bundle;
        bundle.name = entry.args[0];
        bundle.is_alias = false;
        context.declareName(bundle.name, "ENUM", entry);

        string body;
        for (const auto &line : entry.body) {
            body += trim(line);
        }
        vector<string> member_strs = split(body, ',');
        for (const auto &member_str_raw : member_strs) {
            string member_str = trim(member_str_raw);
            if (member_str.empty()) continue;
            VulTempEnumMember member;
            size_t eq_pos = member_str.find('=');
            if (eq_pos == string::npos) {
                member.name = trim(member_str);
                member.value = "";
            } else {
                member.name = trim(member_str.substr(0, eq_pos));
                member.value = trim(member_str.substr(eq_pos + 1));
            }
            if (member.name.empty()) {
                throw VulException("ENUM member name cannot be empty");
            }
            bundle.enum_members.push_back(std::move(member));
        }
        context.temp.bundles.push_back(std::move(bundle));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleENUM> _auto_register_ENUM_handler;

class VCPPModuleREGISTER : public VCPPModuleHandler {
public:
    virtual string name() const { return "REGISTER"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 2) {
            throw VulException("REGISTER requires at least 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempRegister reg;
        reg.name = entry.args[0];
        reg.type = entry.args[1];
        context.declareName(reg.name, "REGISTER", entry);
        bool saw_positional_port = false;
        if (entry.args.size() >= 3) {
            for (size_t i = 2; i < entry.args.size(); ++i) {
                auto attr = parseKeyValueArg(entry.args[i]);
                if (isAttrKey(attr, "ports") || isAttrKey(attr, "portnum")) {
                    if (!reg.portnum.empty()) {
                        throw VulException("REGISTER port count is specified more than once at " + context.getOriginalPosition(entry.pos));
                    }
                    ensureAttrValue("REGISTER", attr->first, attr->second);
                    reg.portnum = trim(attr->second);
                } else if (isAttrKey(attr, "dims") || isAttrKey(attr, "dim")) {
                    appendDimList(reg.dims, attr->second);
                } else if (attr) {
                    throw VulException("REGISTER has unknown attribute '" + attr->first + "' at " + context.getOriginalPosition(entry.pos));
                } else if (!saw_positional_port && reg.portnum.empty()) {
                    reg.portnum = entry.args[i];
                    saw_positional_port = true;
                } else {
                    reg.dims.push_back(entry.args[i]);
                }
            }
        }
        reg.reset_codelines = entry.body;
        reg.reset_codelines_debug = context.bodyDebugLocs(entry);
        context.temp.registers.push_back(std::move(reg));
    }
};
class VCPPModuleREGISTER_MUL : public VCPPModuleHandler {
public:
    virtual string name() const { return "REGISTER_MUL"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 3) {
            throw VulException("REGISTER_MUL requires at least 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempRegister reg;
        reg.name = entry.args[0];
        reg.type = entry.args[1];
        context.declareName(reg.name, "REGISTER_MUL", entry);
        bool saw_port = false;
        for (size_t i = 3; i < entry.args.size(); ++i) {
            auto attr = parseKeyValueArg(entry.args[i]);
            if (isAttrKey(attr, "dims") || isAttrKey(attr, "dim")) {
                appendDimList(reg.dims, attr->second);
            } else if (attr) {
                throw VulException("REGISTER_MUL has unknown attribute '" + attr->first + "' at " + context.getOriginalPosition(entry.pos));
            } else {
                reg.dims.push_back(entry.args[i]);
            }
        }
        auto port_attr = parseKeyValueArg(entry.args[2]);
        if (isAttrKey(port_attr, "ports") || isAttrKey(port_attr, "portnum")) {
            ensureAttrValue("REGISTER_MUL", port_attr->first, port_attr->second);
            reg.portnum = trim(port_attr->second);
            saw_port = true;
        } else if (port_attr) {
            throw VulException("REGISTER_MUL has unknown attribute '" + port_attr->first + "' at " + context.getOriginalPosition(entry.pos));
        }
        if (!saw_port) {
            reg.portnum = entry.args[2];
        }
        reg.reset_codelines = entry.body;
        reg.reset_codelines_debug = context.bodyDebugLocs(entry);
        context.temp.registers.push_back(std::move(reg));
    }
};
class VCPPModuleREGISTER_ARRAY1 : public VCPPModuleHandler {
public:
    virtual string name() const { return "REGISTER_ARRAY1"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 4) {
            throw VulException("REGISTER_ARRAY1 requires exactly 4 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempRegister reg;
        reg.name = entry.args[0];
        reg.type = entry.args[1];
        context.declareName(reg.name, "REGISTER_ARRAY1", entry);
        reg.portnum = entry.args[3];
        reg.dims.push_back(entry.args[2]);
        reg.reset_codelines = entry.body;
        reg.reset_codelines_debug = context.bodyDebugLocs(entry);
        context.temp.registers.push_back(std::move(reg));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleREGISTER> _auto_register_REGISTER_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleREGISTER_MUL> _auto_register_REGISTER_MUL_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleREGISTER_ARRAY1> _auto_register_REGISTER_ARRAY1_handler;

class VCPPModuleWIRE : public VCPPModuleHandler {
public:
    virtual string name() const { return "WIRE"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 2) {
            throw VulException("WIRE requires exactly 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempWire wire;
        wire.name = entry.args[0];
        wire.type = entry.args[1];
        context.declareName(wire.name, "WIRE", entry);
        wire.reset_codelines = entry.body;
        wire.reset_codelines_debug = context.bodyDebugLocs(entry);
        context.temp.wires.push_back(std::move(wire));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleWIRE> _auto_register_WIRE_handler;

class VCPPModuleREQUEST : public VCPPModuleHandler {
public:
    virtual string name() const { return "REQUEST"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.empty()) {
            throw VulException("REQUEST requires at least 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        VulTempReq req;
        req.name = entry.args[0];
        req.has_handshake = false;
        context.declareName(req.name, "REQUEST", entry);
        VulErrorContextGuard _err{"Processing REQUEST '" + req.name + "' at " + context.getOriginalPosition(entry.pos)};
        ReqServParseOptions options;
        options.allow_handshake = true;
        options.allow_array = true;
        parseReqArgsAndRets(entry.args, 1, req, &options, "REQUEST");
        if (options.handshake.has_value()) {
            req.has_handshake = *options.handshake;
        }
        context.temp.requests.push_back(std::move(req));
    }
};

class VCPPModuleREQUEST_READY : public VCPPModuleHandler {
public:
    virtual string name() const { return "REQUEST_READY"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.empty()) {
            throw VulException("REQUEST_READY requires at least 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        VulTempReq req;
        req.name = entry.args[0];
        req.has_handshake = true;
        context.declareName(req.name, "REQUEST_READY", entry);
        VulErrorContextGuard _err{"Processing REQUEST_READY '" + req.name + "' at " + context.getOriginalPosition(entry.pos)};
        ReqServParseOptions options;
        options.allow_array = true;
        parseReqArgsAndRets(entry.args, 1, req, &options, "REQUEST_READY");
        context.temp.requests.push_back(std::move(req));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleREQUEST> _auto_register_REQUEST_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleREQUEST_READY> _auto_register_REQUEST_READY_handler;

class VCPPModuleSERVICE : public VCPPModuleHandler {
public:
    virtual string name() const { return "SERVICE"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.empty()) {
            throw VulException("SERVICE requires at least 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        VulTempServ serv;
        serv.name = entry.args[0];
        serv.is_declaration = !entry.has_body;
        serv.has_handshake = false;
        context.declareServiceName(serv.name, serv.is_declaration, entry);
        serv.cond = "";
        serv.priority = "";
        serv.codelines = entry.body;
        serv.codelines_debug = context.bodyDebugLocs(entry);
        VulErrorContextGuard _err{
            string("Processing SERVICE ") + (serv.is_declaration ? "declaration" : "implementation") +
            " '" + serv.name + "' at " + context.getOriginalPosition(entry.pos)
        };
        ReqServParseOptions options;
        options.allow_handshake = true;
        options.allow_ready = true;
        options.allow_priority = true;
        options.allow_array = true;
        parseReqArgsAndRets(entry.args, 1, serv, &options, "SERVICE");
        if (options.handshake.has_value()) {
            serv.has_handshake = *options.handshake;
        }
        if (options.ready.has_value()) {
            if (serv.is_declaration) {
                throw VulException("SERVICE declaration cannot specify ready=<condition> at " + context.getOriginalPosition(entry.pos));
            }
            if (options.handshake.has_value() && !*options.handshake) {
                throw VulException("SERVICE ready=<condition> conflicts with handshake=0 at " + context.getOriginalPosition(entry.pos));
            }
            serv.cond = *options.ready;
            serv.cond_debug = context.toDebugLoc(entry.pos);
            serv.has_handshake = true;
        }
        if (!serv.is_declaration && serv.has_handshake && serv.cond.empty()) {
            throw VulException("SERVICE with handshake=1 requires ready=<condition> at " + context.getOriginalPosition(entry.pos));
        }
        if (options.priority.has_value()) {
            serv.priority = *options.priority;
        }
        context.temp.services.push_back(std::move(serv));
    }
};

class VCPPModuleSERVICE_READY : public VCPPModuleHandler {
public:
    virtual string name() const { return "SERVICE_READY"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 2) {
            throw VulException("SERVICE_READY requires at least 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempServ serv;
        serv.name = entry.args[0];
        serv.has_handshake = true;
        context.declareName(serv.name, "SERVICE_READY", entry);
        serv.cond = entry.args[1];
        serv.cond_debug = context.toDebugLoc(entry.pos);
        serv.priority = "";
        serv.codelines = entry.body;
        serv.codelines_debug = context.bodyDebugLocs(entry);
        VulErrorContextGuard _err{"Processing SERVICE_READY '" + serv.name + "' at " + context.getOriginalPosition(entry.pos)};
        ReqServParseOptions options;
        options.allow_array = true;
        parseReqArgsAndRets(entry.args, 2, serv, &options, "SERVICE_READY");
        context.temp.services.push_back(std::move(serv));
    }
};

class VCPPModuleSERVICE_PRIO : public VCPPModuleHandler {
public:
    virtual string name() const { return "SERVICE_PRIO"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 2) {
            throw VulException("SERVICE_PRIO requires at least 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempServ serv;
        serv.name = entry.args[0];
        serv.has_handshake = false;
        context.declareName(serv.name, "SERVICE_PRIO", entry);
        serv.cond = "";
        serv.priority = entry.args[1];
        serv.codelines = entry.body;
        serv.codelines_debug = context.bodyDebugLocs(entry);
        VulErrorContextGuard _err{"Processing SERVICE_PRIO '" + serv.name + "' at " + context.getOriginalPosition(entry.pos)};
        ReqServParseOptions options;
        options.allow_array = true;
        parseReqArgsAndRets(entry.args, 2, serv, &options, "SERVICE_PRIO");
        context.temp.services.push_back(std::move(serv));
    }
};

class VCPPModuleSERVICE_PRIO_READY : public VCPPModuleHandler {
public:
    virtual string name() const { return "SERVICE_PRIO_READY"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 3) {
            throw VulException("SERVICE_PRIO_READY requires at least 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempServ serv;
        serv.name = entry.args[0];
        serv.has_handshake = true;
        context.declareName(serv.name, "SERVICE_PRIO_READY", entry);
        serv.priority = entry.args[1];
        serv.cond = entry.args[2];
        serv.cond_debug = context.toDebugLoc(entry.pos);
        serv.codelines = entry.body;
        serv.codelines_debug = context.bodyDebugLocs(entry);
        VulErrorContextGuard _err{"Processing SERVICE_PRIO_READY '" + serv.name + "' at " + context.getOriginalPosition(entry.pos)};
        ReqServParseOptions options;
        options.allow_array = true;
        parseReqArgsAndRets(entry.args, 3, serv, &options, "SERVICE_PRIO_READY");
        context.temp.services.push_back(std::move(serv));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleSERVICE> _auto_register_SERVICE_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleSERVICE_READY> _auto_register_SERVICE_READY_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleSERVICE_PRIO> _auto_register_SERVICE_PRIO_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleSERVICE_PRIO_READY> _auto_register_SERVICE_PRIO_READY_handler;

class VCPPModuleQUERY : public VCPPModuleHandler {
public:
    virtual string name() const { return "QUERY"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 2) {
            throw VulException("QUERY requires exactly 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempQuery query;
        query.name = entry.args[0];
        query.ret_type = entry.args[1];
        context.declareName(query.name, "QUERY", entry);
        query.codelines = entry.body;
        query.codelines_debug = context.bodyDebugLocs(entry);
        VulErrorContextGuard _err{"Processing QUERY '" + query.name + "' at " + context.getOriginalPosition(entry.pos)};
        context.temp.queries.push_back(std::move(query));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleQUERY> _auto_register_QUERY_handler;

class VCPPModuleTICK_IMPL : public VCPPModuleHandler {
public:
    virtual string name() const { return "TICK_IMPL"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 0) {
            throw VulException("TICK_IMPL does not take any arguments at " + context.getOriginalPosition(entry.pos));
        }
        context.temp.tick_blocks.push_back(entry.body);
        context.temp.tick_blocks_debug.push_back(context.bodyDebugLocs(entry));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleTICK_IMPL> _auto_register_TICK_IMPL_handler;

class VCPPModuleCHILD_INSTANCE : public VCPPModuleHandler {
public:
    virtual string name() const { return "CHILD_INSTANCE"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 2) {
            throw VulException("CHILD_INSTANCE requires at least 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempInstance inst;
        inst.name = entry.args[1];
        inst.module_name = entry.args[0];
        context.declareName(inst.name, "CHILD_INSTANCE", entry);
        for (size_t i = 2; i < entry.args.size(); ++i) {
            const string &param_override_raw = entry.args[i];
            auto attr = parseKeyValueArg(param_override_raw);
            if (!attr) {
                throw VulException("invalid parameter override '" + param_override_raw + "' at " + context.getOriginalPosition(entry.pos));
            }
            if (attr->first == "dims" || attr->first == "dim") {
                appendDimList(inst.array_dims, attr->second);
                continue;
            }
            inst.parameter_overrides.push_back(parseParameterOverrideArg(param_override_raw, "CHILD_INSTANCE"));
        }
        context.temp.instances.push_back(std::move(inst));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleCHILD_INSTANCE> _auto_register_CHILD_INSTANCE_handler;

class VCPPModuleCHILD_INSTANCE_ARRAY1 : public VCPPModuleHandler {
public:
    virtual string name() const { return "CHILD_INSTANCE_ARRAY1"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 3) {
            throw VulException("CHILD_INSTANCE_ARRAY1 requires at least 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempInstance inst;
        inst.name = entry.args[1];
        inst.module_name = entry.args[0];
        context.declareName(inst.name, "CHILD_INSTANCE_ARRAY1", entry);
        inst.array_dims.push_back(entry.args[2]);
        for (size_t i = 3; i < entry.args.size(); ++i) {
            const string &param_override_raw = entry.args[i];
            const size_t split_pos = param_override_raw.find('=');
            if (split_pos == string::npos) {
                throw VulException("invalid parameter override '" + param_override_raw + "' at " + context.getOriginalPosition(entry.pos));
            }
            inst.parameter_overrides.push_back(parseParameterOverrideArg(param_override_raw, "CHILD_INSTANCE_ARRAY1"));
        }
        context.temp.instances.push_back(std::move(inst));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleCHILD_INSTANCE_ARRAY1> _auto_register_CHILD_INSTANCE_ARRAY1_handler;

class VCPPModuleCHILD_INSTANCE_ARRAY2 : public VCPPModuleHandler {
public:
    virtual string name() const { return "CHILD_INSTANCE_ARRAY2"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 4) {
            throw VulException("CHILD_INSTANCE_ARRAY2 requires at least 4 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempInstance inst;
        inst.name = entry.args[1];
        inst.module_name = entry.args[0];
        context.declareName(inst.name, "CHILD_INSTANCE_ARRAY2", entry);
        inst.array_dims.push_back(entry.args[2]);
        inst.array_dims.push_back(entry.args[3]);
        for (size_t i = 4; i < entry.args.size(); ++i) {
            const string &param_override_raw = entry.args[i];
            const size_t split_pos = param_override_raw.find('=');
            if (split_pos == string::npos) {
                throw VulException("invalid parameter override '" + param_override_raw + "' at " + context.getOriginalPosition(entry.pos));
            }
            inst.parameter_overrides.push_back(parseParameterOverrideArg(param_override_raw, "CHILD_INSTANCE_ARRAY2"));
        }
        context.temp.instances.push_back(std::move(inst));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleCHILD_INSTANCE_ARRAY2> _auto_register_CHILD_INSTANCE_ARRAY2_handler;

class VCPPModuleUSE_CHILD_SERVICE_PORT : public VCPPModuleHandler {
public:
    virtual string name() const { return "USE_CHILD_SERVICE_PORT"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 3) {
            throw VulException("USE_CHILD_SERVICE_PORT requires at least 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempChildServiceUse use;
        use.instance_expr = entry.args[0];
        use.service_name = entry.args[1];
        use.alias_name = entry.args[2];
        context.declareName(use.alias_name, "USE_CHILD_SERVICE_PORT", entry);
        context.temp.child_service_uses.push_back(std::move(use));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleUSE_CHILD_SERVICE_PORT> _auto_register_USE_CHILD_SERVICE_PORT_handler;

class VCPPModuleUSE_CHILD_SERVICE : public VCPPModuleHandler {
public:
    virtual string name() const { return "USE_CHILD_SERVICE"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 3) {
            throw VulException("USE_CHILD_SERVICE requires at least 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        string array_size;
        for (size_t i = 3; i < entry.args.size(); ++i) {
            auto attr = parseKeyValueArg(entry.args[i]);
            if (!attr) {
                continue;
            }
            if (attr->first != "array") {
                throw VulException("USE_CHILD_SERVICE has unknown attribute '" + attr->first + "' at " + context.getOriginalPosition(entry.pos));
            }
            ensureAttrValue("USE_CHILD_SERVICE", attr->first, attr->second);
            if (!array_size.empty()) {
                throw VulException("USE_CHILD_SERVICE array size is specified more than once at " + context.getOriginalPosition(entry.pos));
            }
            array_size = trim(attr->second);
        }
        VulTempChildServiceUse use;
        use.instance_expr = entry.args[0];
        use.service_name = entry.args[1];
        use.alias_name = entry.args[2];
        use.array_size = std::move(array_size);
        context.declareName(use.alias_name, "USE_CHILD_SERVICE", entry);
        context.temp.child_service_uses.push_back(std::move(use));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleUSE_CHILD_SERVICE> _auto_register_USE_CHILD_SERVICE_handler;

class VCPPModuleUSE_CHILD_QUERY : public VCPPModuleHandler {
public:
    virtual string name() const { return "USE_CHILD_QUERY"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 4) {
            throw VulException("USE_CHILD_QUERY requires at least 4 arguments at " + context.getOriginalPosition(entry.pos));
        }
        string array_size;
        for (size_t i = 4; i < entry.args.size(); ++i) {
            auto attr = parseKeyValueArg(entry.args[i]);
            if (!attr || attr->first != "array") {
                throw VulException("USE_CHILD_QUERY only supports optional array=<N> after rettype at " + context.getOriginalPosition(entry.pos));
            }
            ensureAttrValue("USE_CHILD_QUERY", attr->first, attr->second);
            if (!array_size.empty()) {
                throw VulException("USE_CHILD_QUERY array size is specified more than once at " + context.getOriginalPosition(entry.pos));
            }
            array_size = trim(attr->second);
        }
        VulTempChildQueryUse use;
        use.instance_expr = entry.args[0];
        use.query_name = entry.args[1];
        use.alias_name = entry.args[2];
        use.ret_type = entry.args[3];
        use.array_size = std::move(array_size);
        context.declareName(use.alias_name, "USE_CHILD_QUERY", entry);
        context.temp.child_query_uses.push_back(std::move(use));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleUSE_CHILD_QUERY> _auto_register_USE_CHILD_QUERY_handler;

class VCPPModuleCONNECT_CR_CS : public VCPPModuleHandler {
public:
    virtual string name() const { return "CONNECT_CR_CS"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 4) {
            throw VulException("CONNECT_CR_CS requires exactly 4 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulReqServConnection conn;
        conn.req_instance = entry.args[0];
        conn.req_name = entry.args[1];
        conn.serv_instance = entry.args[2];
        conn.serv_name = entry.args[3];
        context.temp.req_connections.push_back(std::move(conn));
    }
};

class VCPPModuleCONNECT_CR_S : public VCPPModuleHandler {
public:
    virtual string name() const { return "CONNECT_CR_S"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 3) {
            throw VulException("CONNECT_CR_S requires exactly 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulReqServConnection conn;
        conn.req_instance = entry.args[0];
        conn.req_name = entry.args[1];
        conn.serv_instance = "";
        conn.serv_name = entry.args[2];
        context.temp.req_connections.push_back(std::move(conn));
    }
};

class VCPPModuleCONNECT_CR_R : public VCPPModuleHandler {
public:
    virtual string name() const { return "CONNECT_CR_R"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 3) {
            throw VulException("CONNECT_CR_R requires exactly 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulReqServConnection conn;
        conn.req_instance = entry.args[0];
        conn.req_name = entry.args[1];
        conn.serv_instance = "";
        conn.serv_name = entry.args[2];
        context.temp.req_connections.push_back(std::move(conn));
    }
};

class VCPPModuleCONNECT_S_CS : public VCPPModuleHandler {
public:
    virtual string name() const { return "CONNECT_S_CS"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 3) {
            throw VulException("CONNECT_S_CS requires exactly 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulReqServConnection conn;
        conn.req_instance = "";
        conn.req_name = entry.args[0];
        conn.serv_instance = entry.args[1];
        conn.serv_name = entry.args[2];
        context.temp.req_connections.push_back(std::move(conn));
    }
};

static VCPPModuleAutoRegisterHandler<VCPPModuleCONNECT_CR_CS> _auto_register_CONNECT_CR_CS_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleCONNECT_CR_S> _auto_register_CONNECT_CR_S_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleCONNECT_CR_R> _auto_register_CONNECT_CR_R_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleCONNECT_S_CS> _auto_register_CONNECT_S_CS_handler;

class VCPPModuleBRAM : public VCPPModuleHandler {
public:
    virtual string name() const { return "BRAM"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 3) {
            throw VulException("BRAM requires at least 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempBRAM bram;
        bram.name = entry.args[0];
        bram.data_type = entry.args[1];
        context.declareName(bram.name, "BRAM", entry);
        bram.addr_size = entry.args[2];
        bram.read_ports = "1";
        bram.write_ports = "1";
        bool mode_1rw = false;
        size_t positional_idx = 0;
        for (size_t i = 3; i < entry.args.size(); ++i) {
            auto attr = parseKeyValueArg(entry.args[i]);
            if (isAttrKey(attr, "read_ports") || isAttrKey(attr, "readports")) {
                ensureAttrValue("BRAM", attr->first, attr->second);
                bram.read_ports = trim(attr->second);
            } else if (isAttrKey(attr, "write_ports") || isAttrKey(attr, "writeports")) {
                ensureAttrValue("BRAM", attr->first, attr->second);
                bram.write_ports = trim(attr->second);
            } else if (isAttrKey(attr, "mode")) {
                ensureAttrValue("BRAM", attr->first, attr->second);
                const string mode = trim(attr->second);
                if (mode == "1rw") {
                    mode_1rw = true;
                } else if (mode == "generic") {
                    mode_1rw = false;
                } else {
                    throw VulException("BRAM mode must be 'generic' or '1rw' at " + context.getOriginalPosition(entry.pos));
                }
            } else if (attr) {
                throw VulException("BRAM has unknown attribute '" + attr->first + "' at " + context.getOriginalPosition(entry.pos));
            } else if (positional_idx == 0) {
                bram.read_ports = entry.args[i];
                ++positional_idx;
            } else if (positional_idx == 1) {
                bram.write_ports = entry.args[i];
                ++positional_idx;
            } else {
                throw VulException("BRAM has too many positional arguments at " + context.getOriginalPosition(entry.pos));
            }
        }
        if (mode_1rw) {
            bram.read_ports = "";
            bram.write_ports = "";
        }
        context.temp.brams.push_back(std::move(bram));
    }
};

class VCPPModuleBRAM_1RW : public VCPPModuleHandler {
public:
    virtual string name() const { return "BRAM_1RW"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 3) {
            throw VulException("BRAM_1RW requires exactly 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempBRAM bram;
        bram.name = entry.args[0];
        bram.data_type = entry.args[1];
        context.declareName(bram.name, "BRAM_1RW", entry);
        bram.addr_size = entry.args[2];
        bram.read_ports = "";
        bram.write_ports = "";
        context.temp.brams.push_back(std::move(bram));
    }
};

class VCPPModuleROM : public VCPPModuleHandler {
public:
    virtual string name() const { return "ROM"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 3) {
            throw VulException("ROM requires at least 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempDigitalROM rom;
        rom.name = entry.args[0];
        rom.data_width = entry.args[1];
        context.declareName(rom.name, "ROM", entry);
        rom.addr_size = entry.args[2];
        rom.read_ports = "1";
        size_t positional_idx = 0;
        for (size_t i = 3; i < entry.args.size(); ++i) {
            auto attr = parseKeyValueArg(entry.args[i]);
            if (isAttrKey(attr, "read_ports") || isAttrKey(attr, "readports")) {
                ensureAttrValue("ROM", attr->first, attr->second);
                rom.read_ports = trim(attr->second);
            } else if (isAttrKey(attr, "init") || isAttrKey(attr, "init_path")) {
                ensureAttrValue("ROM", attr->first, attr->second);
                rom.init_path = trim(attr->second);
            } else if (attr) {
                throw VulException("ROM has unknown attribute '" + attr->first + "' at " + context.getOriginalPosition(entry.pos));
            } else if (positional_idx == 0) {
                rom.read_ports = entry.args[i];
                ++positional_idx;
            } else if (positional_idx == 1) {
                rom.init_path = entry.args[i];
                ++positional_idx;
            } else {
                throw VulException("ROM has too many positional arguments at " + context.getOriginalPosition(entry.pos));
            }
        }
        if (rom.init_path.empty()) {
            throw VulException("ROM requires init=<path> or a positional init_path at " + context.getOriginalPosition(entry.pos));
        }
        context.temp.roms.push_back(std::move(rom));
    }
};

class VCPPModuleQUEUE : public VCPPModuleHandler {
public:
    virtual string name() const { return "QUEUE"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 3) {
            throw VulException("QUEUE requires at least 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempQueue queue;
        queue.name = entry.args[0];
        queue.type = entry.args[1];
        context.declareName(queue.name, "QUEUE", entry);
        queue.depth = entry.args[2];
        queue.enq_width = "1";
        queue.deq_width = "1";
        size_t positional_idx = 0;
        for (size_t i = 3; i < entry.args.size(); ++i) {
            auto attr = parseKeyValueArg(entry.args[i]);
            if (isAttrKey(attr, "enq_width") || isAttrKey(attr, "enqwidth")) {
                ensureAttrValue("QUEUE", attr->first, attr->second);
                queue.enq_width = trim(attr->second);
            } else if (isAttrKey(attr, "deq_width") || isAttrKey(attr, "deqwidth")) {
                ensureAttrValue("QUEUE", attr->first, attr->second);
                queue.deq_width = trim(attr->second);
            } else if (attr) {
                throw VulException("QUEUE has unknown attribute '" + attr->first + "' at " + context.getOriginalPosition(entry.pos));
            } else if (positional_idx == 0) {
                queue.enq_width = entry.args[i];
                ++positional_idx;
            } else if (positional_idx == 1) {
                queue.deq_width = entry.args[i];
                ++positional_idx;
            } else {
                throw VulException("QUEUE has too many positional arguments at " + context.getOriginalPosition(entry.pos));
            }
        }
        context.temp.queues.push_back(std::move(queue));
    }
};

class VCPPModuleQUEUE_MP : public VCPPModuleHandler {
public:
    virtual string name() const { return "QUEUE_MP"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 5) {
            throw VulException("QUEUE_MP requires exactly 5 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempQueue queue;
        queue.name = entry.args[0];
        queue.type = entry.args[1];
        context.declareName(queue.name, "QUEUE_MP", entry);
        queue.depth = entry.args[2];
        queue.enq_width = entry.args[3];
        queue.deq_width = entry.args[4];
        context.temp.queues.push_back(std::move(queue));
    }
};

static VCPPModuleAutoRegisterHandler<VCPPModuleBRAM> _auto_register_BRAM_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleBRAM_1RW> _auto_register_BRAM_1RW_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleROM> _auto_register_ROM_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleQUEUE> _auto_register_QUEUE_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleQUEUE_MP> _auto_register_QUEUE_MP_handler;

class VCPPModuleHELPER : public VCPPModuleHandler {
public:
    virtual string name() const { return "HELPER"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 0) {
            throw VulException("HELPER does not take any arguments at " + context.getOriginalPosition(entry.pos));
        }
        context.temp.helper_codes = entry.body;
        context.temp.helper_codes_debug = context.bodyDebugLocs(entry);
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleHELPER> _auto_register_HELPER_handler;
