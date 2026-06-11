// MIT License

// Copyright (c) 2025 Meng Chengzhen, in Shandong University

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


#include "vcpp.hpp"
#include "project.h"
#include "configexpr.hpp"
#include "cppparse.hpp"
#include "stringop.hpp"
#include "vullib.hpp"

using namespace cppparse;
using namespace stringop;

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <assert.h>
#include <optional>
#include <algorithm>
#include <deque>

struct VCPPModuleContext {
    VulTempModule temp;
    std::vector<uint32_t> line_mapping; // new line number -> original line number (1-based)
    inline string getOriginalPosition(const LinePosition &pos) const {
        if (pos.line < 0 || pos.line >= line_mapping.size()) {
            return "Line " + std::to_string(pos.line + 1) + ":" + std::to_string(pos.column + 1);
        }
        return "Line " + std::to_string(line_mapping[pos.line]) + ":" + std::to_string(pos.column + 1);
    }
};

class VCPPModuleHandler {
public:
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) = 0;
    virtual string name() const = 0;
};

class VCPPModuleHandlerRegistry {
public:
    static VCPPModuleHandlerRegistry& instance() {
        static VCPPModuleHandlerRegistry registry;
        return registry;
    }

    bool register_handler(std::unique_ptr<VCPPModuleHandler> handler) {
        // For simplicity, we assume each handler handles a unique macro name, which is determined by the handler's dynamic type name.
        std::string handler_name = handler->name();
        if (handlers_.count(handler_name) > 0) {
            return false; // handler for this macro already exists
        }
        handlers_[handler_name] = std::move(handler);
        return true;
    }

    void run(VCPPModuleContext &context, const MacroEntry &entry) {
        std::string handler_name = entry.name; // assume macro name is the same as handler's dynamic type name for simplicity
        if (handlers_.count(handler_name) == 0) {
            throw VulException("No handler registered for macro: " + entry.name + " at " + context.getOriginalPosition(entry.pos));
        }
        handlers_[handler_name]->run(context, entry);
    }

private:
    VCPPModuleHandlerRegistry() = default;
    std::unordered_map<std::string, std::unique_ptr<VCPPModuleHandler>> handlers_;
};

template <typename T>
class VCPPModuleAutoRegisterHandler {
public:
    explicit VCPPModuleAutoRegisterHandler() {
        VCPPModuleHandlerRegistry::instance().register_handler(std::make_unique<T>());
    }
};


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
        if (entry.args.size() >= 3) {
            reg.portnum = entry.args[2];
        }
        for (size_t i = 3; i < entry.args.size(); ++i) {
            reg.dims.push_back(entry.args[i]);
        }
        reg.reset_codelines = entry.body;
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
        if (entry.args.size() >= 3) {
            reg.portnum = entry.args[2];
        }
        for (size_t i = 3; i < entry.args.size(); ++i) {
            reg.dims.push_back(entry.args[i]);
        }
        reg.reset_codelines = entry.body;
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
        reg.portnum = entry.args[3];
        reg.dims.push_back(entry.args[2]);
        reg.reset_codelines = entry.body;
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
        wire.reset_codelines = entry.body;
        context.temp.wires.push_back(std::move(wire));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleWIRE> _auto_register_WIRE_handler;

static inline void parseReqArgsAndRets(
    const vector<string> &macro_args,
    size_t begin_idx,
    VulTempReqServBase &reqserv
) {
    size_t i = begin_idx;
    if (i < macro_args.size()) {
        const string first_tag = trim(macro_args[i]);
        if (first_tag.size() > 7 && first_tag.rfind("ARRAY(", 0) == 0 && first_tag.back() == ')') {
            reqserv.array_size = trim(first_tag.substr(6, first_tag.size() - 7));
            if (reqserv.array_size.empty()) {
                throw VulException("empty ARRAY() expression");
            }
            ++i;
        }
    }

    for (; i < macro_args.size(); ++i) {
        const string &arg_decl_raw = macro_args[i];
        const string arg_decl = trim(arg_decl_raw);
        const size_t split_pos = arg_decl.find_last_of(" \t\r\n");
        if (split_pos == string::npos) {
            throw VulException("invalid argument declaration '" + arg_decl_raw + "'");
        }

        const string type_tag = trim(arg_decl.substr(0, split_pos));
        const string arg_name = trim(arg_decl.substr(split_pos + 1));
        if (type_tag.empty() || arg_name.empty()) {
            throw VulException("invalid argument declaration '" + arg_decl_raw + "'");
        }

        if (type_tag.size() > 5 && type_tag.rfind("ARG(", 0) == 0 && type_tag.back() == ')') {
            reqserv.args.emplace_back(type_tag.substr(4, type_tag.size() - 5), arg_name);
        } else if (type_tag.size() > 6 && type_tag.rfind("RESP(", 0) == 0 && type_tag.back() == ')') {
            reqserv.rets.emplace_back(type_tag.substr(5, type_tag.size() - 6), arg_name);
        } else {
            throw VulException("invalid argument type tag '" + type_tag + "'");
        }
    }
}

static inline string parsePathMacroArg(const string &raw_arg) {
    string arg = trim(raw_arg);
    if (arg.size() >= 2) {
        char first = arg.front();
        char last = arg.back();
        if ((first == '"' && last == '"') || (first == '<' && last == '>')) {
            return arg.substr(1, arg.size() - 2);
        }
    }
    return arg;
}

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
        VulErrorContextGuard _err{"Processing REQUEST '" + req.name + "' at " + context.getOriginalPosition(entry.pos)};
        parseReqArgsAndRets(entry.args, 1, req);
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
        VulErrorContextGuard _err{"Processing REQUEST_READY '" + req.name + "' at " + context.getOriginalPosition(entry.pos)};
        parseReqArgsAndRets(entry.args, 1, req);
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
        serv.has_handshake = false;
        serv.cond = "";
        serv.priority = "";
        serv.codelines = entry.body;
        VulErrorContextGuard _err{"Processing SERVICE '" + serv.name + "' at " + context.getOriginalPosition(entry.pos)};
        parseReqArgsAndRets(entry.args, 1, serv);
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
        serv.cond = entry.args[1];
        serv.priority = "";
        serv.codelines = entry.body;
        VulErrorContextGuard _err{"Processing SERVICE_READY '" + serv.name + "' at " + context.getOriginalPosition(entry.pos)};
        parseReqArgsAndRets(entry.args, 2, serv);
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
        serv.cond = "";
        serv.priority = entry.args[1];
        serv.codelines = entry.body;
        VulErrorContextGuard _err{"Processing SERVICE_PRIO '" + serv.name + "' at " + context.getOriginalPosition(entry.pos)};
        parseReqArgsAndRets(entry.args, 2, serv);
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
        serv.priority = entry.args[1];
        serv.cond = entry.args[2];
        serv.codelines = entry.body;
        VulErrorContextGuard _err{"Processing SERVICE_PRIO_READY '" + serv.name + "' at " + context.getOriginalPosition(entry.pos)};
        parseReqArgsAndRets(entry.args, 3, serv);
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
        query.codelines = entry.body;
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
        for (size_t i = 2; i < entry.args.size(); ++i) {
            const string &param_override_raw = entry.args[i];
            const size_t split_pos = param_override_raw.find('=');
            if (split_pos == string::npos) {
                throw VulException("invalid parameter override '" + param_override_raw + "' at " + context.getOriginalPosition(entry.pos));
            }
            const string param_name = trim(param_override_raw.substr(0, split_pos));
            const string param_value = trim(param_override_raw.substr(split_pos + 1));
            if (param_name.empty() || param_value.empty()) {
                throw VulException("invalid parameter override '" + param_override_raw + "' at " + context.getOriginalPosition(entry.pos));
            }
            inst.parameter_overrides.emplace_back(param_name, param_value);
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
        inst.array_dims.push_back(entry.args[2]);
        for (size_t i = 3; i < entry.args.size(); ++i) {
            const string &param_override_raw = entry.args[i];
            const size_t split_pos = param_override_raw.find('=');
            if (split_pos == string::npos) {
                throw VulException("invalid parameter override '" + param_override_raw + "' at " + context.getOriginalPosition(entry.pos));
            }
            const string param_name = trim(param_override_raw.substr(0, split_pos));
            const string param_value = trim(param_override_raw.substr(split_pos + 1));
            if (param_name.empty() || param_value.empty()) {
                throw VulException("invalid parameter override '" + param_override_raw + "' at " + context.getOriginalPosition(entry.pos));
            }
            inst.parameter_overrides.emplace_back(param_name, param_value);
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
        inst.array_dims.push_back(entry.args[2]);
        inst.array_dims.push_back(entry.args[3]);
        for (size_t i = 4; i < entry.args.size(); ++i) {
            const string &param_override_raw = entry.args[i];
            const size_t split_pos = param_override_raw.find('=');
            if (split_pos == string::npos) {
                throw VulException("invalid parameter override '" + param_override_raw + "' at " + context.getOriginalPosition(entry.pos));
            }
            const string param_name = trim(param_override_raw.substr(0, split_pos));
            const string param_value = trim(param_override_raw.substr(split_pos + 1));
            if (param_name.empty() || param_value.empty()) {
                throw VulException("invalid parameter override '" + param_override_raw + "' at " + context.getOriginalPosition(entry.pos));
            }
            inst.parameter_overrides.emplace_back(param_name, param_value);
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
        context.temp.child_service_uses.push_back(std::move(use));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleUSE_CHILD_SERVICE_PORT> _auto_register_USE_CHILD_SERVICE_PORT_handler;

class VCPPModuleUSE_CHILD_QUERY : public VCPPModuleHandler {
public:
    virtual string name() const { return "USE_CHILD_QUERY"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 4) {
            throw VulException("USE_CHILD_QUERY requires exactly 4 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempChildQueryUse use;
        use.instance_expr = entry.args[0];
        use.query_name = entry.args[1];
        use.alias_name = entry.args[2];
        use.ret_type = entry.args[3];
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
        if (entry.args.size() != 5) {
            throw VulException("BRAM requires exactly 5 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempBRAM bram;
        bram.name = entry.args[0];
        bram.data_type = entry.args[1];
        bram.addr_size = entry.args[2];
        bram.read_ports = entry.args[3];
        bram.write_ports = entry.args[4];
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
        if (entry.args.size() != 5) {
            throw VulException("ROM requires exactly 5 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempDigitalROM rom;
        rom.name = entry.args[0];
        rom.data_width = entry.args[1];
        rom.addr_size = entry.args[2];
        rom.read_ports = entry.args[3];
        rom.init_path = entry.args[4];
        context.temp.roms.push_back(std::move(rom));
    }
};

class VCPPModuleQUEUE : public VCPPModuleHandler {
public:
    virtual string name() const { return "QUEUE"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 3) {
            throw VulException("QUEUE requires exactly 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempQueue queue;
        queue.name = entry.args[0];
        queue.type = entry.args[1];
        queue.depth = entry.args[2];
        queue.enq_width = "1";
        queue.deq_width = "1";
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
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleHELPER> _auto_register_HELPER_handler;

VulTempModule _parseTempModule(
    const string &module_name,
    const string &module_filepath
) {
    VulErrorContextGuard _err{"Parsing module '" + module_name + "' from file '" + module_filepath + "'"};

    VCPPModuleContext context;
    context.temp.name = module_name;
    context.temp.filepath = module_filepath;

    vector<string> code_lines = readFileLines(module_filepath);
    auto trim_res = stripComments(code_lines);
    context.line_mapping = std::move(trim_res.mapping);
    code_lines = std::move(trim_res.lines);

    vector<MacroEntry> macro_entries = findAllMacroEntries(code_lines);
    // printf("Found %zu macro entries in module '%s'\n", macro_entries.size(), module_name.c_str());
    for (const auto &entry : macro_entries) {
        // printf("Found macro: %s at %s: ", entry.name.c_str(), context.getOriginalPosition(entry.pos).c_str());
        // for (const auto &arg : entry.args) {
        //     printf("'%s' ", arg.c_str());
        // }
        // printf("\n");
        VCPPModuleHandlerRegistry::instance().run(context, entry);
    }

    return context.temp;
}


struct VCPPTestContext {
    VulStaticTestHarnessModule test;
    VulStaticConfigLib config_lib;
    VulStaticBundleLib bundle_lib;
    std::vector<uint32_t> line_mapping; // new line number -> original line number (1-based)
    inline string getOriginalPosition(const LinePosition &pos) const {
        if (pos.line < 0 || pos.line >= line_mapping.size()) {
            return "Line " + std::to_string(pos.line + 1) + ":" + std::to_string(pos.column + 1);
        }
        return "Line " + std::to_string(line_mapping[pos.line]) + ":" + std::to_string(pos.column + 1);
    }
};

class VCPPTestHandler {
public:
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) = 0;
    virtual string name() const = 0;
};

class VCPPTestHandlerRegistry {
public:
    static VCPPTestHandlerRegistry& instance() {
        static VCPPTestHandlerRegistry registry;
        return registry;
    }

    bool register_handler(std::unique_ptr<VCPPTestHandler> handler) {
        // For simplicity, we assume each handler handles a unique macro name, which is determined by the handler's dynamic type name.
        std::string handler_name = handler->name();
        if (handlers_.count(handler_name) > 0) {
            return false; // handler for this macro already exists
        }
        handlers_[handler_name] = std::move(handler);
        return true;
    }

    void run(VCPPTestContext &context, const MacroEntry &entry) {
        std::string handler_name = entry.name; // assume macro name is the same as handler's dynamic type name for simplicity
        if (handlers_.count(handler_name) == 0) {
            throw VulException("No handler registered for macro: " + entry.name + " at " + context.getOriginalPosition(entry.pos));
        }
        handlers_[handler_name]->run(context, entry);
    }

private:
    VCPPTestHandlerRegistry() = default;
    std::unordered_map<std::string, std::unique_ptr<VCPPTestHandler>> handlers_;
};

template <typename T>
class VCPPTestAutoRegisterHandler {
public:
    explicit VCPPTestAutoRegisterHandler() {
        VCPPTestHandlerRegistry::instance().register_handler(std::make_unique<T>());
    }
};

class VCPPTestPARAMETER : public VCPPTestHandler {
public:
    virtual string name() const { return "PARAMETER"; }
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 2) {
            throw VulException("PARAMETER requires exactly 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulErrorContextGuard _err{"Processing PARAMETER '" + entry.args[0] + "' at " + context.getOriginalPosition(entry.pos)};
        ConfigRealValue value = calculateConstexprValue(entry.args[1], context.config_lib);
        context.test.top_config_overrides[entry.args[0]] = value;
    }
};
static VCPPTestAutoRegisterHandler<VCPPTestPARAMETER> _auto_register_TEST_PARAMETER_handler;


class VCPPTestREQUEST : public VCPPTestHandler {
public:
    virtual string name() const { return "REQUEST"; }
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) {
        if (entry.args.empty()) {
            throw VulException("REQUEST requires at least 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        VulTempReq req;
        req.name = entry.args[0];
        req.has_handshake = false;
        VulErrorContextGuard _err{"Processing REQUEST '" + req.name + "' at " + context.getOriginalPosition(entry.pos)};
        parseReqArgsAndRets(entry.args, 1, req);
        context.test.requests[req.name] = std::move(req);
    }
};

class VCPPTestREQUEST_READY : public VCPPTestHandler {
public:
    virtual string name() const { return "REQUEST_READY"; }
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) {
        if (entry.args.empty()) {
            throw VulException("REQUEST_READY requires at least 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        VulTempReq req;
        req.name = entry.args[0];
        req.has_handshake = true;
        VulErrorContextGuard _err{"Processing REQUEST_READY '" + req.name + "' at " + context.getOriginalPosition(entry.pos)};
        parseReqArgsAndRets(entry.args, 1, req);
        context.test.requests[req.name] = std::move(req);
    }
};
static VCPPTestAutoRegisterHandler<VCPPTestREQUEST> _auto_register_TEST_REQUEST_handler;
static VCPPTestAutoRegisterHandler<VCPPTestREQUEST_READY> _auto_register_TEST_REQUEST_READY_handler;

class VCPPTestSERVICE : public VCPPTestHandler {
public:
    virtual string name() const { return "SERVICE"; }
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) {
        if (entry.args.empty()) {
            throw VulException("SERVICE requires at least 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        VulTempServ serv;
        serv.name = entry.args[0];
        serv.has_handshake = false;
        serv.cond = "";
        serv.priority = "";
        serv.codelines = entry.body;
        VulErrorContextGuard _err{"Processing SERVICE '" + serv.name + "' at " + context.getOriginalPosition(entry.pos)};
        parseReqArgsAndRets(entry.args, 1, serv);
        context.test.services[serv.name] = std::move(serv);
    }
};

class VCPPTestSERVICE_READY : public VCPPTestHandler {
public:
    virtual string name() const { return "SERVICE_READY"; }
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 2) {
            throw VulException("SERVICE_READY requires at least 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempServ serv;
        serv.name = entry.args[0];
        serv.has_handshake = true;
        serv.cond = entry.args[1];
        serv.priority = "";
        serv.codelines = entry.body;
        VulErrorContextGuard _err{"Processing SERVICE_READY '" + serv.name + "' at " + context.getOriginalPosition(entry.pos)};
        parseReqArgsAndRets(entry.args, 2, serv);
        context.test.services[serv.name] = std::move(serv);
    }
};
static VCPPTestAutoRegisterHandler<VCPPTestSERVICE> _auto_register_TEST_SERVICE_handler;
static VCPPTestAutoRegisterHandler<VCPPTestSERVICE_READY> _auto_register_TEST_SERVICE_READY_handler;

class VCPPTestGLOBAL : public VCPPTestHandler {
public:
    virtual string name() const { return "GLOBAL"; }
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) {
        context.test.globalCodes.insert(context.test.globalCodes.end(), entry.body.begin(), entry.body.end());
    }
};
static VCPPTestAutoRegisterHandler<VCPPTestGLOBAL> _auto_register_TEST_GLOBAL_handler;

class VCPPTestTOP : public VCPPTestHandler {
public:
    virtual string name() const { return "TOP"; }
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 1) {
            throw VulException("TOP requires exactly 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        if (!context.test.top_module_path.empty()) {
            throw VulException("Multiple TOP declarations are not allowed at " + context.getOriginalPosition(entry.pos));
        }
        context.test.top_module_path = parsePathMacroArg(entry.args[0]);
    }
};
static VCPPTestAutoRegisterHandler<VCPPTestTOP> _auto_register_TEST_TOP_handler;

class VCPPTestPROJECT : public VCPPTestHandler {
public:
    virtual string name() const { return "PROJECT"; }
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 1) {
            throw VulException("PROJECT requires exactly 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        if (!context.test.project_dir_path.empty()) {
            throw VulException("Multiple PROJECT declarations are not allowed at " + context.getOriginalPosition(entry.pos));
        }
        context.test.project_dir_path = parsePathMacroArg(entry.args[0]);
    }
};
static VCPPTestAutoRegisterHandler<VCPPTestPROJECT> _auto_register_TEST_PROJECT_handler;

class VCPPTestSIMULATION : public VCPPTestHandler {
public:
    virtual string name() const { return "SIMULATION"; }
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) {
        if (!context.test.test_codelines.empty()) {
            throw VulException("Multiple SIMULATION blocks are not allowed at " + context.getOriginalPosition(entry.pos));
        }
        context.test.test_codelines.insert(context.test.test_codelines.end(), entry.body.begin(), entry.body.end());
    }
};
static VCPPTestAutoRegisterHandler<VCPPTestSIMULATION> _auto_register_TEST_SIMULATION_handler;

VulStaticTestHarnessModule _parseTestModule(
    const string &test_filepath,
    const VulStaticConfigLib &config_lib,
    const VulStaticBundleLib &bundle_lib
) {
    VCPPTestContext context;
    context.config_lib = config_lib;
    context.bundle_lib = bundle_lib;

    VulErrorContextGuard _err{"Parsing test module from file '" + test_filepath + "'"};

    vector<string> code_lines = readFileLines(test_filepath);
    auto trim_res = stripComments(code_lines);
    context.line_mapping = std::move(trim_res.mapping);
    code_lines = std::move(trim_res.lines);
    
    const string prefix = "#include";
    for (const auto &line_raw : code_lines) {
        string line = trim(line_raw);
        if (line.rfind(prefix, 0) != 0) {
            continue;
        }
        size_t first_quote = line.find('<');
        char closing_char = '>';
        if (first_quote == string::npos) {
            first_quote = line.find('"');
            closing_char = '"';
        }
        if (first_quote == string::npos) {
            continue;
        }
        size_t second_quote = line.find(closing_char, first_quote + 1);
        if (second_quote == string::npos || second_quote <= first_quote + 1) {
            continue;
        }
        string included_path = trim(line.substr(first_quote + 1, second_quote - first_quote - 1));
        bool escaped = false;
        for (auto s : VulLibFiles) {
            if (included_path == s) {
                escaped = true;
                break;
            }
        }
        for (auto s : VulEscapedHeaders) {
            if (included_path == s) {
                escaped = true;
                break;
            }
        }
        if (escaped) {
            continue;
        }
        context.test.includedHeaders.push_back(included_path);
    }

    vector<MacroEntry> macro_entries = findAllMacroEntries(code_lines);
    for (const auto &entry : macro_entries) {
        VCPPTestHandlerRegistry::instance().run(context, entry);
    }

    return std::move(context.test);
}

static std::pair<string, string> _scanTestModuleBindingPaths(const string &test_filepath) {
    vector<string> code_lines = readFileLines(test_filepath);
    code_lines = stripComments(code_lines).lines;
    vector<MacroEntry> macro_entries = findAllMacroEntries(code_lines);

    string top_module_path;
    string project_dir_path;
    for (const auto &entry : macro_entries) {
        if (entry.name == "TOP") {
            if (entry.args.size() != 1) {
                throw VulException("TOP requires exactly 1 argument");
            }
            if (!top_module_path.empty()) {
                throw VulException("Multiple TOP declarations are not allowed");
            }
            top_module_path = parsePathMacroArg(entry.args[0]);
        } else if (entry.name == "PROJECT") {
            if (entry.args.size() != 1) {
                throw VulException("PROJECT requires exactly 1 argument");
            }
            if (!project_dir_path.empty()) {
                throw VulException("Multiple PROJECT declarations are not allowed");
            }
            project_dir_path = parsePathMacroArg(entry.args[0]);
        }
    }
    return {top_module_path, project_dir_path};
}


VulStaticProject parseVcppStaticProject(
    const string &project_dir,
    const string &top_file_path,
    const string &main_file_path
) {
    VulStaticProject project;

    using namespace std::filesystem;

    bool is_sim = !main_file_path.empty();

    path main_path;
    string scanned_top_module_path;
    string scanned_project_dir_path;
    if (!main_file_path.empty()) {
        main_path = path(main_file_path);
        if (!exists(main_path) || !is_regular_file(main_path)) {
            std::cerr << "Error: Main file does not exist or is not a regular file: " << main_file_path << std::endl;
            assert(0);
        }
        std::tie(scanned_top_module_path, scanned_project_dir_path) = _scanTestModuleBindingPaths(main_path.string());
    }

    auto resolve_from_main = [&](const string &raw_path, const string &field_name) -> path {
        if (raw_path.empty()) {
            return {};
        }
        path p(raw_path);
        if (p.is_absolute()) {
            return p.lexically_normal();
        }
        if (main_path.empty()) {
            throw VulException(field_name + " in TestMain requires a main file");
        }
        return (main_path.parent_path() / p).lexically_normal();
    };

    path top_path;
    if (!top_file_path.empty()) {
        top_path = path(top_file_path);
    } else if (!scanned_top_module_path.empty()) {
        top_path = resolve_from_main(scanned_top_module_path, "TOP");
    }
    if (top_path.empty()) {
        throw VulException("Top module path is not specified. Use -t/--top or TOP(...) in TestMain.");
    }
    if (!exists(top_path) || !is_regular_file(top_path)) {
        throw VulException("Top file does not exist or is not a regular file: " + top_path.string());
    }

    path proj_dir;
    if (!project_dir.empty()) {
        proj_dir = path(project_dir);
    } else if (!scanned_project_dir_path.empty()) {
        proj_dir = resolve_from_main(scanned_project_dir_path, "PROJECT");
    } else {
        proj_dir = top_path.parent_path();
    }
    if (!exists(proj_dir) || !is_directory(proj_dir)) {
        throw VulException("Project directory does not exist or is not a directory: " + proj_dir.string());
    }

    path header_path = proj_dir / "header.hpp";
    bool header_found = false;
    if (exists(header_path) && is_regular_file(header_path)) {
        header_found = true;
    } else if (exists(proj_dir / "header.h") && is_regular_file(proj_dir / "header.h")) {
        header_path = proj_dir / "header.h";
        header_found = true;
    }
    if (header_found) {
        VulErrorContextGuard _err{"parsing header file " + header_path.string()};
        VulTempModule fake_module = _parseTempModule("header", header_path.string());
        for (const auto& item : fake_module.configs) {
            VulErrorContextGuard _err{"evaluating constant " + item.name};
            ConfigRealValue value = calculateConstexprValue(item.value, project.global_configlib);
            project.global_configlib[item.name] = value;
        }
        for (const auto& item : fake_module.bundles) {
            VulErrorContextGuard _err{"staticalizing bundle " + item.name};
            VulStaticBundle static_bundle;
            static_bundle = staticalizeBundle(item, project.global_configlib);
            project.global_bundlelib.push_back(std::move(static_bundle));
        }
    }

    if (!main_file_path.empty()) {
        {
            VulErrorContextGuard _err{"reading test harness module from " + main_path.string()};
            project.test_harness = _parseTestModule(main_path.string(), project.global_configlib, project.global_bundlelib);
        }
    }

    unordered_map<ModuleName, path> module_file_path_cache;
    auto find_module_file = [&](const ModuleName& mod_name) -> std::optional<path> {
        auto iter = module_file_path_cache.find(mod_name);
        if (iter != module_file_path_cache.end()) {
            return iter->second;
        }
        vector<string> candidates = {
            mod_name + ".hpp",
            mod_name + ".cpp",
            mod_name + ".vul",
            mod_name + ".h"
        };
        for (const auto& candidate : candidates) {
            // find in project directory recursively
            vector<path> found_paths;
            for (auto& p : recursive_directory_iterator(proj_dir)) {
                if (p.is_regular_file() && p.path().filename() == candidate) {
                    found_paths.push_back(p.path());
                }
            }
            if (found_paths.size() > 1) {
                string err_str = "Error: Multiple files found for module " + mod_name + ":";
                for (const auto& p : found_paths) {
                    err_str += "\n  " + p.string();
                }
                throw VulException(err_str);
            } else if (!found_paths.empty()) {
                module_file_path_cache[mod_name] = found_paths.front();
                return found_paths.front();
            }
        }
        return std::nullopt;
    };

    struct InstanceToProcess {
        shared_ptr<VulStaticModuleInstance> ptr;
        VulStaticConfigLib config_overrides;
    };
    std::deque<InstanceToProcess> todo_queue;

    shared_ptr<VulStaticModuleInstance> top_instance = std::make_shared<VulStaticModuleInstance>();
    top_instance->module_name = top_path.stem().string();
    if (is_sim) {
        top_instance->instance_path = {"sim", "top"};
    } else {
        top_instance->instance_path = {"top"};
    }
    top_instance->filepath = top_path.string();
    todo_queue.push_back({top_instance, project.test_harness.top_config_overrides});

    uint32_t instance_count = 0;
    VulTempModuleCache temp_module_cache;

    while (!todo_queue.empty()) {
        auto [instance_ptr, config_overrides] = todo_queue.front();
        todo_queue.pop_front();

        VulErrorContextGuard _err{"parsing instance " + instance_ptr->simClassName() + " of module " + instance_ptr->module_name};
        const ModuleName& mod_name = instance_ptr->module_name;
        VulTempModule *temp_mod_ptr = nullptr;

        auto temp_mod_iter = temp_module_cache.find(mod_name);
        if (temp_mod_iter != temp_module_cache.end()) {
            temp_mod_ptr = &temp_mod_iter->second;
        } else {

            auto mod_file_opt = find_module_file(mod_name);
            if (!mod_file_opt.has_value()) {
                throw VulException("Module file not found for module: " + instance_ptr->module_name);
            }
            const path& mod_file = mod_file_opt.value();
            string mod_file_str = mod_file.string();

            VulErrorContextGuard _err_file{"entering module file " + mod_file_str};

            temp_module_cache[mod_name] = _parseTempModule(mod_name, mod_file_str);
            temp_mod_ptr = &temp_module_cache[mod_name];
        }

        instantiateModule(
            *instance_ptr,
            *temp_mod_ptr,
            config_overrides,
            project.global_configlib,
            project.global_bundlelib
        );
        detectRequestCallInLogicBlocks(*instance_ptr);

        instance_ptr->instance_id = instance_count++;

        // process child instances
        for (const auto& [child_name, child_instance_decl] : instance_ptr->instances) {
            shared_ptr<VulStaticModuleInstance> child_instance = std::make_shared<VulStaticModuleInstance>();
            child_instance->instance_path = instance_ptr->instance_path;
            child_instance->instance_path.push_back(child_name);
            child_instance->module_name = child_instance_decl.module_name;
            child_instance->parent = instance_ptr;
            instance_ptr->children.push_back(child_instance);
            todo_queue.push_back({child_instance, child_instance_decl.parameter_overrides});

            if (child_instance_decl.array_dims.size() > 2) {
                throw VulException("Only up to 2 child instance array dimensions are currently supported");
            }
        }
    }
    project.top_module_instance = top_instance;


    printf("Successfully parsed project. Summary:\n");
    printf("Parse %ld modules:\n", module_file_path_cache.size());
    for (const auto& [mod_name, mod_path] : module_file_path_cache) {
        printf("  [%s]: %s\n", mod_name.c_str(), mod_path.string().c_str());
    }
    printf("Instance hierarchy:\n");
    vector<pair<shared_ptr<VulStaticModuleInstance>, size_t>> dfs_stack;
    dfs_stack.reserve(64);
    dfs_stack.push_back({top_instance, 1});
    while (!dfs_stack.empty()) {
        auto [node, depth] = dfs_stack.back();
        dfs_stack.pop_back();

        for (size_t i = 0; i < depth; ++i) {
            putchar(' ');
        }
        std::string instance_path_str;
        for (const auto& name : node->instance_path) {
            if (!instance_path_str.empty()) {
                instance_path_str += "::";
            }
            instance_path_str += name;
        }
        printf("%s [%s]\n", instance_path_str.c_str(), node->module_name.c_str());

        for (size_t i = node->children.size(); i > 0; --i) {
            dfs_stack.push_back({node->children[i - 1], depth + 1});
        }
    }
    printf("Total instances: %d\n", instance_count);
    
    if (!is_sim) {
        printf("No main file provided, skipping simulation setup.\n");
        return project;
    }

    printf("Setting up simulation hierarchy and update sequence...\n");

    shared_ptr<VulStaticModuleInstance> fake_main = std::make_shared<VulStaticModuleInstance>();
    fake_main->instance_path = {"sim", "main"};
    fake_main->module_name = "TestMain";
    fake_main->instance_id = instance_count + 1;
    fake_main->tick_blocks.push_back(VulTickBlock());

    shared_ptr<VulStaticModuleInstance> sim_top = std::make_shared<VulStaticModuleInstance>();
    sim_top->instance_path = {"sim"};
    sim_top->module_name = "SimTop";
    sim_top->filepath = main_file_path;
    sim_top->instance_id = instance_count + 2;
    sim_top->tick_blocks.push_back(VulTickBlock());

    for (const auto &req_entry: project.top_module_instance->requests) {
        fake_main->services[req_entry.first] = req_entry.second;
        VulLogicBlock logic_block;
        logic_block.block_id = fake_main->services.size();
        logic_block.with_priority = false;
        fake_main->serv_logic_blocks[req_entry.first] = logic_block;
        VulReqServConnection conn;
        conn.req_instance = "top";
        conn.req_name = req_entry.first;
        conn.serv_instance = "main";
        conn.serv_name = req_entry.first;
        sim_top->req_connections.push_back(conn);
    }
    for (const auto &serv_entry: project.top_module_instance->services) {
        fake_main->requests[serv_entry.first] = serv_entry.second;
        LogicBlockCall call;
        call.instance = "";
        call.port = serv_entry.first;
        fake_main->tick_blocks[0].call_requests.push_back(call);
        VulReqServConnection conn;
        conn.req_instance = "main";
        conn.req_name = serv_entry.first;
        conn.serv_instance = "top";
        conn.serv_name = serv_entry.first;
        sim_top->req_connections.push_back(conn);
    }

    VulStaticInstanceDecl main_decl;
    main_decl.name = "main";
    main_decl.module_name = "TestMain";
    sim_top->instances["main"] = main_decl;
    VulStaticInstanceDecl top_decl;
    top_decl.name = "top";
    top_decl.module_name = project.top_module_instance->module_name;
    sim_top->instances["top"] = top_decl;

    sim_top->children.push_back(fake_main);
    sim_top->children.push_back(project.top_module_instance);

    fake_main->parent = sim_top;
    project.top_module_instance->parent = sim_top;

    setupUpdateSequence(sim_top);

    printf("Setup complete.\n");

    return project;
}
