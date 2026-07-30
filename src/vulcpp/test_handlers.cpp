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

#include "test_handlers.hpp"

#include "common.hpp"

using namespace cppparse;

VCPPTestHandlerRegistry& VCPPTestHandlerRegistry::instance() {
    static VCPPTestHandlerRegistry registry;
    return registry;
}

bool VCPPTestHandlerRegistry::register_handler(std::unique_ptr<VCPPTestHandler> handler) {
    std::string handler_name = handler->name();
    if (handlers_.count(handler_name) > 0) {
        return false;
    }
    handlers_[handler_name] = std::move(handler);
    return true;
}

void VCPPTestHandlerRegistry::run(VCPPTestContext &context, const MacroEntry &entry) {
    std::string handler_name = entry.name;
    if (handlers_.count(handler_name) == 0) {
        throw VulException("No handler registered for macro: " + entry.name + " at " + context.getOriginalPosition(entry.pos));
    }
    handlers_[handler_name]->run(context, entry);
}

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
        ReqServParseOptions options;
        options.allow_handshake = true;
        options.allow_array = true;
        parseReqArgsAndRets(entry.args, 1, req, &options, "REQUEST");
        if (options.handshake.has_value()) {
            req.has_handshake = *options.handshake;
        }
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
        ReqServParseOptions options;
        options.allow_array = true;
        parseReqArgsAndRets(entry.args, 1, req, &options, "REQUEST_READY");
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
            if (options.handshake.has_value() && !*options.handshake) {
                throw VulException("SERVICE ready=<condition> conflicts with handshake=0 at " + context.getOriginalPosition(entry.pos));
            }
            serv.cond = *options.ready;
            serv.cond_debug = context.toDebugLoc(entry.pos);
            serv.has_handshake = true;
        }
        if (serv.has_handshake && serv.cond.empty()) {
            throw VulException("SERVICE with handshake=1 requires ready=<condition> at " + context.getOriginalPosition(entry.pos));
        }
        if (options.priority.has_value()) {
            serv.priority = *options.priority;
        }
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
        serv.cond_debug = context.toDebugLoc(entry.pos);
        serv.priority = "";
        serv.codelines = entry.body;
        serv.codelines_debug = context.bodyDebugLocs(entry);
        VulErrorContextGuard _err{"Processing SERVICE_READY '" + serv.name + "' at " + context.getOriginalPosition(entry.pos)};
        ReqServParseOptions options;
        options.allow_array = true;
        parseReqArgsAndRets(entry.args, 2, serv, &options, "SERVICE_READY");
        context.test.services[serv.name] = std::move(serv);
    }
};
static VCPPTestAutoRegisterHandler<VCPPTestSERVICE> _auto_register_TEST_SERVICE_handler;
static VCPPTestAutoRegisterHandler<VCPPTestSERVICE_READY> _auto_register_TEST_SERVICE_READY_handler;

class VCPPTestQUERY : public VCPPTestHandler {
public:
    virtual string name() const { return "QUERY"; }
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 2) {
            throw VulException("QUERY requires exactly 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        if (!entry.body.empty()) {
            throw VulException("QUERY in TestMain must be a declaration without code block at " + context.getOriginalPosition(entry.pos));
        }
        VulTempQuery query;
        query.name = entry.args[0];
        query.ret_type = entry.args[1];
        VulErrorContextGuard _err{"Processing QUERY '" + query.name + "' at " + context.getOriginalPosition(entry.pos)};
        VulStaticQuery static_query;
        static_query.name = query.name;
        static_query.ret_type = parseTypeSignature(query.ret_type, context.config_lib);
        context.test.queries[query.name] = std::move(static_query);
    }
};
static VCPPTestAutoRegisterHandler<VCPPTestQUERY> _auto_register_TEST_QUERY_handler;

class VCPPTestGLOBAL : public VCPPTestHandler {
public:
    virtual string name() const { return "GLOBAL"; }
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) {
        context.test.globalCodes.insert(context.test.globalCodes.end(), entry.body.begin(), entry.body.end());
        auto locs = context.bodyDebugLocs(entry);
        context.test.globalCodes_debug.insert(context.test.globalCodes_debug.end(), locs.begin(), locs.end());
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
        auto locs = context.bodyDebugLocs(entry);
        context.test.test_codelines_debug.insert(context.test.test_codelines_debug.end(), locs.begin(), locs.end());
    }
};
static VCPPTestAutoRegisterHandler<VCPPTestSIMULATION> _auto_register_TEST_SIMULATION_handler;
