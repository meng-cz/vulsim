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


#pragma once

#include "type.h"
#include "configlib.h"
#include "configexpr.hpp"
#include "bundlelib.h"

#include <algorithm>
#include <map>

using std::pair;
using std::map;

typedef string ModuleName;

typedef string ReqServName;
typedef string PipePortName;
typedef string ArgName;

using InstanceName = string;
using CCodeLine = string;

enum class VulConnIndexKind {
    ConstantExpr,
    LoopVar,
    GeneralExpr,
    Wildcard,
};

struct VulConnIndexExpr {
    VulConnIndexKind kind = VulConnIndexKind::ConstantExpr;
    int32_t loop_dim = -1; // 0 for '$', 1 for '?'
    int64_t offset = 0;
    string expr; // original constant/general expression or "*" for wildcard

    inline string toString() const {
        if (kind == VulConnIndexKind::Wildcard) {
            return "*";
        }
        if (kind == VulConnIndexKind::LoopVar) {
            string out = (loop_dim == 0 ? "$" : "?");
            if (offset > 0) out += "+" + std::to_string(offset);
            if (offset < 0) out += std::to_string(offset);
            return out;
        }
        return expr;
    }
};

inline bool operator==(const VulConnIndexExpr &a, const VulConnIndexExpr &b) {
    return std::tie(a.kind, a.loop_dim, a.offset, a.expr) ==
           std::tie(b.kind, b.loop_dim, b.offset, b.expr);
}

struct VulReqServConnection {
    InstanceName    req_instance;
    ReqServName     req_name;       // should be ServiceName when req_instance is TopInterface
    InstanceName    req_instance_base;
    vector<VulConnIndexExpr> req_indices;
    InstanceName    serv_instance;
    ReqServName     serv_name;      // should be RequestName when serv_instance is TopInterface
    InstanceName    serv_instance_base;
    vector<VulConnIndexExpr> serv_indices;

    inline string toString() const {
        string req_side = "'" + req_instance + "'.'" + req_name + "'";
        if (!req_indices.empty()) {
            req_side = "'" + req_instance_base + "'";
            for (const auto &idx : req_indices) {
                req_side += "[" + idx.toString() + "]";
            }
            req_side += ".'" + req_name + "'";
        }
        string serv_side = "'" + serv_instance + "'.'" + serv_name + "'";
        if (!serv_indices.empty()) {
            serv_side = "'" + serv_instance_base + "'";
            for (const auto &idx : serv_indices) {
                serv_side += "[" + idx.toString() + "]";
            }
            serv_side += ".'" + serv_name + "'";
        }
        return req_side + " -> " + serv_side;
    }
};

inline bool operator==(const VulReqServConnection &a, const VulReqServConnection &b) {
    return std::tie(a.req_instance, a.req_name, a.req_instance_base, a.req_indices,
                    a.serv_instance, a.serv_name, a.serv_instance_base, a.serv_indices) ==
           std::tie(b.req_instance, b.req_name, b.req_instance_base, b.req_indices,
                    b.serv_instance, b.serv_name, b.serv_instance_base, b.serv_indices);
}


struct VulTempRegister {
    string name;
    string type;
    string portnum;
    vector<string> dims;
    vector<string> reset_codelines;
};

struct VulTempWire {
    string name;
    string type;
    vector<string> reset_codelines;
};

struct VulTempReqServBase {
    string name;
    string array_size; // empty means non-arrayed request/service port
    vector<pair<string, string>> args; // pair of arg name and arg type
    vector<pair<string, string>> rets; // pair of ret name and ret type
    bool has_handshake;

    inline string signatureArgNameList() const {
        string sig;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) sig += ", ";
            sig += args[i].second;
        }
        for (size_t i = 0; i < rets.size(); ++i) {
            if (i > 0 || args.size() > 0) sig += ", ";
            sig += rets[i].second;
        }
        return sig;
    }

    inline string signatureArgTypeOnly() const {
        string sig;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) sig += ", ";
            sig += string("const ") + args[i].first + " & ";
        }
        for (size_t i = 0; i < rets.size(); ++i) {
            if (i > 0 || args.size() > 0) sig += ", ";
            sig += rets[i].first + " & ";
        }
        return sig;
    }

    inline string signatureArgOnly() const {
        string sig;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) sig += ", ";
            sig += (string("const ") + args[i].first + " & ") + args[i].second;
        }
        for (size_t i = 0; i < rets.size(); ++i) {
            if (i > 0 || args.size() > 0) sig += ", ";
            sig += (rets[i].first + " & " + rets[i].second);
        }
        return sig;
    }

    inline string returnType() const {
        return (has_handshake ? "bool" : "void");
    }

    inline string signatureNoRetType(const string name_prefix = "") const {
        return  name_prefix + name + "(" + signatureArgOnly() + ")";
    }

    inline string signatureFull(const string name_prefix = "") const {
        return (has_handshake ? "bool " : "void ") + signatureNoRetType(name_prefix);
    }
};

struct VulTempReq : public VulTempReqServBase {
};

struct VulTempServ : public VulTempReqServBase {
    string priority;
    string cond;
    vector<string> codelines;
};

using VulTempTickBlock = vector<string>;

struct VulTempInstance {
    string name;
    string module_name;
    vector<string> array_dims;
    vector<pair<string, string>> parameter_overrides; // pair of parameter name and value
};

struct VulTempChildServiceUse {
    string instance_expr;
    string service_name;
    string alias_name;
};

struct VulTempBRAM {
    string name;
    string data_type;
    string addr_size;
    string read_ports;
    string write_ports;
};

struct VulTempDigitalROM {
    string name;
    string data_width;
    string addr_size;
    string read_ports;
    string init_path;
};

struct VulTempQueue {
    string name;
    string type;
    string depth;
    string enq_width;
    string deq_width;
};

struct VulTempModule {
    string name;
    string filepath;
    vector<VulTempConfig> configs;
    vector<VulTempConfig> params;
    vector<VulTempBundle> bundles;
    vector<VulTempReq> requests;
    vector<VulTempServ> services;
    vector<VulTempRegister> registers;
    vector<VulTempWire> wires;
    vector<VulTempBRAM> brams;
    vector<VulTempDigitalROM> roms;
    vector<VulTempQueue> queues;
    vector<VulTempInstance> instances;
    vector<VulTempTickBlock> tick_blocks;
    vector<VulReqServConnection>  req_connections;
    vector<VulTempChildServiceUse> child_service_uses;
    vector<string> helper_codes;
};

using VulTempModuleCache = std::unordered_map<string, VulTempModule>; // map from module name to its parsed temp module




using VulInstanceID = uint32_t;
using VulLogicBlockID = uint32_t;

struct LogicBlockCall {
    InstanceName instance; // empty if call is in the module itself
    ReqServName port; // request port for self-call, or service port for child instance call
};

struct VulLogicBlock {
    vector<string> codelines;
    vector<string> cond_codelines;
    vector<LogicBlockCall> call_requests; // all transaction ports called within this block
    VulLogicBlockID block_id; // instance-unique block id, assigned during module tree construction
    int32_t priority; // only for service logic block, smaller value means higher priority, default 0
    bool with_priority = false; // whether the priority is specified by user
};

struct VulTickBlock {
    vector<string> codelines;
    vector<LogicBlockCall> call_requests; // all transaction ports called within this block
};

struct VulStaticInstanceDecl {
    InstanceName        name;
    ModuleName          module_name;
    vector<ConfigRealValue> array_dims;
    VulStaticConfigLib parameter_overrides;

    inline bool isArrayed() const {
        return !array_dims.empty();
    }
};

struct VulStaticChildServiceUse {
    ReqServName alias_name;
    InstanceName instance_name;
    ReqServName service_name;
    bool alias_indexed = false;
    ConfigRealValue alias_index = 0;
};

struct VulStaticArg {
    ArgName     name;
    VulStaticTypeSignature type;
};

struct VulStaticReqServ {
    ReqServName name;
    bool is_arrayed = false;
    ConfigRealValue array_size = 1;
    vector<VulStaticArg> args;
    vector<VulStaticArg> rets;
    bool has_handshake;

    inline bool match(const VulStaticReqServ &a) const {
        if (a.has_handshake != has_handshake) return false;
        if (a.is_arrayed != is_arrayed) return false;
        if (a.array_size != array_size) return false;
        if (a.args.size() != args.size()) return false;
        for (size_t i = 0; i < a.args.size(); ++i) {
            if (a.args[i].type != args[i].type) return false;
        }
        if (a.rets.size() != rets.size()) return false;
        for (size_t i = 0; i < a.rets.size(); ++i) {
            if (a.rets[i].type != rets[i].type) return false;
        }
        return true;
    }

    inline string signatureArgNameList() const {
        string sig;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) sig += ", ";
            sig += args[i].name;
        }
        for (size_t i = 0; i < rets.size(); ++i) {
            if (i > 0 || args.size() > 0) sig += ", ";
            sig += rets[i].name;
        }
        return sig;
    }

    inline string signatureArgTypeOnly() const {
        string sig;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) sig += ", ";
            sig += string("const ") + args[i].type.toString() + " & ";
        }
        for (size_t i = 0; i < rets.size(); ++i) {
            if (i > 0 || args.size() > 0) sig += ", ";
            sig += rets[i].type.toString() + " & ";
        }
        return sig;
    }

    inline string signatureArgOnly() const {
        string sig;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) sig += ", ";
            sig += (string("const ") + args[i].type.toString() + " & ") + args[i].name;
        }
        for (size_t i = 0; i < rets.size(); ++i) {
            if (i > 0 || args.size() > 0) sig += ", ";
            sig += (rets[i].type.toString() + " & " + rets[i].name);
        }
        return sig;
    }

    inline string returnType() const {
        return (has_handshake ? "bool" : "void");
    }

    inline string signatureNoRetType(const string name_prefix = "") const {
        return  name_prefix + name + "(" + signatureArgOnly() + ")";
    }

    inline string signatureFull(const string name_prefix = "") const {
        return (has_handshake ? "bool " : "void ") + signatureNoRetType(name_prefix);
    }
};

VulStaticReqServ staticalizeReqServ(const VulTempReqServBase &item, const VulStaticConfigLib &config_lib);

struct VulStaticRegister {
    BMemberName name;
    VulStaticTypeSignature signature;
    vector<ConfigRealValue> dims; // only for array types
    ConfigRealValue ports;
    vector<string> reset_codelines;
};

struct VulStaticWire {
    BMemberName name;
    VulStaticTypeSignature signature;
    vector<string> reset_codelines;
};

struct VulStaticBRAM {
    InstanceName name;
    VulStaticTypeSignature data_type;
    ConfigRealValue addr_size;
    ConfigRealValue addr_width;
    ConfigRealValue read_ports;
    ConfigRealValue write_ports;
};

struct VulStaticDigitalROM {
    InstanceName name;
    ConfigRealValue data_width;
    ConfigRealValue addr_size;
    ConfigRealValue addr_width;
    ConfigRealValue read_ports;
    string init_path;
};

struct VulStaticQueue {
    InstanceName name;
    VulStaticTypeSignature type;
    ConfigRealValue depth;
    ConfigRealValue enq_width;
    ConfigRealValue deq_width;
};

struct VulStaticModuleInstance {

    inline vector<string> normalizedInstancePath() const {
        vector<const VulStaticModuleInstance *> chain;
        const VulStaticModuleInstance *cur = this;
        while (cur != nullptr) {
            chain.push_back(cur);
            cur = cur->parent.get();
        }
        std::reverse(chain.begin(), chain.end());

        vector<string> out;
        out.reserve(chain.size());
        for (const auto *node : chain) {
            if (!node->instance_decl_name.empty() && !node->instance_array_indices.empty()) {
                out.push_back(node->instance_decl_name);
            } else if (!node->instance_path.empty()) {
                out.push_back(node->instance_path.back());
            }
        }
        return out;
    }

    shared_ptr<VulStaticModuleInstance> parent;
    vector<shared_ptr<VulStaticModuleInstance>> children;

    VulInstanceID instance_id; // global unique instance id, assigned during module tree construction

    vector<InstanceName> instance_path; // full instance path from top
    string filepath; // relative filepath to top module dir
    ModuleName module_name;

    inline string simClassName() const {
        string name = module_name;
        for (const auto &inst : normalizedInstancePath()) {
            name += "_" + inst;
        }
        return name;
    };
    inline string simDeclPath() const {
        string path = "";
        for (const auto &inst : normalizedInstancePath()) {
            path += "/" + inst;
        }
        return path.substr(1) + ".decl.hpp";
    }
    inline string simImplPath() const {
        string path = "";
        for (const auto &inst : normalizedInstancePath()) {
            path += "/" + inst;
        }
        return path.substr(1) + ".impl.hpp";
    }
    inline string rtlHlsPath() const {
        string path = "";
        for (const auto &inst : normalizedInstancePath()) {
            path += "/" + inst;
        }
        return path.substr(1) + ".logic.cpp";
    }
    inline string rtlSvPath() const {
        string path = "";
        for (const auto &inst : normalizedInstancePath()) {
            path += "/" + inst;
        }
        return path.substr(1) + ".sv";
    }
    inline string concatInstancePath(const string &sep, bool include_topsim = false) const {
        string path = "";
        uint64_t i = (include_topsim ? 0 : 1);
        for (; i < instance_path.size(); ++i) {
            path += (path.empty() ? "" : sep) + instance_path[i];
        }
        return path;
    }

    VulStaticConfigLib local_parameters;
    VulStaticConfigLib local_consts;
    VulStaticBundleLib local_bundles;

    unordered_map<ReqServName, VulStaticReqServ>      requests;
    unordered_map<ReqServName, VulStaticReqServ>      services;

    vector<VulStaticRegister> registers;
    vector<VulStaticWire> wires;
    vector<VulStaticBRAM> brams;
    vector<VulStaticDigitalROM> roms;
    vector<VulStaticQueue> queues;

    unordered_map<ReqServName, VulLogicBlock> serv_logic_blocks;
    vector<VulTickBlock> tick_blocks;
    vector<string> helper_codes;

    unordered_map<InstanceName, VulStaticInstanceDecl> instances;

    vector<VulReqServConnection>  req_connections;
    vector<VulStaticChildServiceUse> child_service_uses;

    unordered_set<ReqServName> exported_services; // services that are connected to parent instance, and should not be connected or called within the module itself
    vector<VulInstanceID> update_seq; // topological order of instance update in simulation

    InstanceName instance_decl_name; // empty for top / synthetic instances
    vector<ConfigRealValue> instance_array_indices; // empty for scalar instance or top
};

void instantiateModule(
    VulStaticModuleInstance &instance,
    const VulTempModule &temp,
    const VulStaticConfigLib &param_overrides,
    const VulStaticConfigLib &global_config,
    const VulStaticBundleLib &global_bundles
);

void detectRequestCallInLogicBlocks(VulStaticModuleInstance &module_instance);

void setupUpdateSequence(shared_ptr<VulStaticModuleInstance> &top);


struct VulStaticTestHarnessModule {

    VulStaticConfigLib top_config_overrides;

    unordered_map<ReqServName, VulTempReq>      requests;
    unordered_map<ReqServName, VulTempServ>      services;

    vector<CCodeLine> test_codelines;

    vector<string> includedHeaders;
    vector<string> globalCodes;
};
