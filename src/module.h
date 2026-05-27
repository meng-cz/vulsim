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

#include <map>

using std::pair;
using std::map;

typedef string ModuleName;

typedef string ReqServName;
typedef string PipePortName;
typedef string ArgName;

using InstanceName = string;
using CCodeLine = string;

struct VulReqServConnection {
    InstanceName    req_instance;
    ReqServName     req_name;       // should be ServiceName when req_instance is TopInterface
    InstanceName    serv_instance;
    ReqServName     serv_name;      // should be RequestName when serv_instance is TopInterface

    inline string toString() const {
        return "'" + req_instance + "'.'" + req_name + "' -> '" + serv_instance + "'.'" + serv_name + "'";
    }
};

inline bool operator==(const VulReqServConnection &a, const VulReqServConnection &b) {
    return std::tie(a.req_instance, a.req_name, a.serv_instance, a.serv_name) ==
           std::tie(b.req_instance, b.req_name, b.serv_instance, b.serv_name);
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
    vector<pair<string, string>> parameter_overrides; // pair of parameter name and value
    vector<string> referenced_services;
};

struct VulTempBRAM {
    string name;
    string data_type;
    string addr_width;
    string read_ports;
    string write_ports;
};

struct VulTempDigitalROM {
    string name;
    string data_width;
    string addr_width;
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
    unordered_set<ReqServName> referenced_services;
    VulStaticConfigLib parameter_overrides;
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
    ConfigRealValue addr_width;
    ConfigRealValue read_ports;
    ConfigRealValue write_ports;
};

struct VulStaticDigitalROM {
    InstanceName name;
    ConfigRealValue data_width;
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

    shared_ptr<VulStaticModuleInstance> parent;
    vector<shared_ptr<VulStaticModuleInstance>> children;

    VulInstanceID instance_id; // global unique instance id, assigned during module tree construction

    vector<InstanceName> instance_path; // full instance path from top
    string filepath; // relative filepath to top module dir
    ModuleName module_name;

    inline string simClassName() const {
        string name = module_name;
        for (const auto &inst : instance_path) {
            name += "_" + inst;
        }
        return name;
    };
    inline string simDeclPath() const {
        string path = "";
        for (const auto &inst : instance_path) {
            path += "/" + inst;
        }
        return path.substr(1) + ".decl.hpp";
    }
    inline string simImplPath() const {
        string path = "";
        for (const auto &inst : instance_path) {
            path += "/" + inst;
        }
        return path.substr(1) + ".impl.hpp";
    }
    inline string rtlHlsPath() const {
        string path = "";
        for (const auto &inst : instance_path) {
            path += "/" + inst;
        }
        return path.substr(1) + ".logic.cpp";
    }
    inline string rtlSvPath() const {
        string path = "";
        for (const auto &inst : instance_path) {
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

    unordered_map<InstanceName, VulStaticInstanceDecl> instances;

    vector<VulReqServConnection>  req_connections;

    unordered_set<ReqServName> exported_services; // services that are connected to parent instance, and should not be connected or called within the module itself
    vector<VulInstanceID> update_seq; // topological order of instance update in simulation
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
