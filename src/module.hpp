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
#include "pipetype.hpp"

using std::pair;

typedef string ModuleName;

typedef string ReqServName;
typedef string PipePortName;
typedef string ArgName;

typedef struct {
    ArgName     name;
    DataType    type;
    Comment     comment;
} VulArg;

typedef struct {
    ReqServName    name;
    Comment        comment;
    vector<VulArg>  args;
    vector<VulArg>  rets;
    ConfigValue     array_size; // empty if not an array request
    bool            has_handshake;
} VulReqServ;

/**
 * @brief Whether two VulReqServ have the same signature (args, rets, array_size, has_handshake).
 * @param a The first VulReqServ.
 * @param b The second VulReqServ.
 * @return true if they have the same signature, false otherwise.
 */
inline bool operator==(const VulReqServ &a, const VulReqServ &b) {
    if (a.has_handshake != b.has_handshake) return false;
    if (!a.array_size.empty() || !b.array_size.empty()) {
        return config_parser::tokenEq(a.array_size, b.array_size);
    }
    if (a.args.size() != b.args.size()) return false;
    for (size_t i = 0; i < a.args.size(); ++i) {
        if (a.args[i].type != b.args[i].type) return false;
    }
    if (a.rets.size() != b.rets.size()) return false;
    for (size_t i = 0; i < a.rets.size(); ++i) {
        if (a.rets[i].type != b.rets[i].type) return false;
    }
    return true;
}

typedef struct {
    PipePortName    name;
    DataType        type;
    Comment         comment;
} VulPipePort;

/**
 * @brief Whether two VulPipePort have the same type.
 * @param a The first VulPipePort.
 * @param b The second VulPipePort.
 * @return true if they have the same type, false otherwise.
 */
inline bool operator==(const VulPipePort &a, const VulPipePort &b) {
    return (a.type == b.type);
}

typedef string LocalConfigValue; // This value expression will NOT be updated automatically when config library is changed.

typedef struct {
    ConfigName          name;
    LocalConfigValue    value;
    Comment             comment;
} VulLocalConfigItem;

class VulModuleBase {
public:
    ModuleName                  name;
    Comment                     comment;
    unordered_map<ConfigName, VulLocalConfigItem> local_configs; // configs private to instance
    unordered_map<ReqServName, VulReqServ>      requests;
    unordered_map<ReqServName, VulReqServ>      services;
    unordered_map<PipePortName, VulPipePort>    pipe_inputs;
    unordered_map<PipePortName, VulPipePort>    pipe_outputs;
    bool                        _is_external = false; // whether the module is imported from external definition
};

typedef string EModuleDir;

class VulExternalModule : public VulModuleBase {
public:
    EModuleDir      directory;
};

/**
 * @brief Get the module definition that matches the given pipe characteristics.
 * @param pipe The VulPipe definition.
 * @return The VulModuleBase definition that matches the pipe.
 */
inline VulModuleBase getModuleDefinitionForPipe(const VulPipe &pipe) {
    VulModuleBase ret;
    ret.name = pipe.name + "_module";
    ret.comment = "Auto-generated module for pipe " + pipe.name;
    // add pipe input
    ret.pipe_inputs["in"] = VulPipePort{
        .name = "in",
        .type = pipe.type,
        .comment = "Input port for pipe " + pipe.name
    };
    // add pipe output
    ret.pipe_outputs["out"] = VulPipePort{
        .name = "out",
        .type = pipe.type,
        .comment = "Output port for pipe " + pipe.name
    };
    // add clear service
    ret.services["clear"] = VulReqServ{
        .name = "clear",
        .comment = "Clear the pipe " + pipe.name,
        .args = {},
        .rets = {},
        .array_size = "",
        .has_handshake = false
    };
    ret._is_external = true;
    return ret;
}

typedef string InstanceName;

typedef struct {
    InstanceName        name;
    ModuleName          module_name;
    Comment             comment;
    unordered_map<ConfigName, LocalConfigValue> local_config_overrides; // local config overrides for this instance
} VulInstance;

typedef struct {
    InstanceName    req_instance;
    ReqServName     req_name;       // should be ServiceName when req_instance is TopInterface
    InstanceName    serv_instance;
    ReqServName     serv_name;      // should be RequestName when serv_instance is TopInterface
} VulReqServConnection;

typedef struct {
    InstanceName    instance;
    PipeName        instance_pipe_port;
    InstanceName    pipe_instance;
    PipeName        _top_pipe_port;         // valid when pipe_instance is TopInterface
    bool            is_direction_mod_out;   // true: module -> pipe, false: pipe -> module
} VulModulePipeConnection;

typedef struct {
    InstanceName    from_instance;
    InstanceName    to_instance;
} VulStallConnection;

typedef struct {
    InstanceName    former_instance;
    InstanceName    latter_instance;
} VulSequenceConnection;

typedef string CCodeLine;

typedef string StorageName;
typedef struct {
    StorageName         name;
    DataType            type;
    ConfigValue         value;  // valid only for basic types, zero-initialized when empty
    Comment             comment;
    vector<ConfigValue> dims;      // empty if not an array
} VulStorage;

typedef struct {
    InstanceName        name;
    Comment             comment;
    vector<CCodeLine>   codelines;     // user tick function body codelines
} VulTickCodeBlock;

class VulModule : public VulModuleBase {
public:

    const InstanceName TopInterface = string("__top__");
    const InstanceName TopStallInput = string("__top_stall_input__");
    const InstanceName TopStallOutput = string("__top_stall_output__");

    bool is_hpp_generated = true; // TODO: always true for now
    bool is_inline_generated = true;

    // vector<CCodeLine>   user_applytick_codelines;       // user applytick function body codelines
    vector<CCodeLine>   user_header_filed_codelines;    // user header field codelines (private section)

    unordered_map<InstanceName, VulTickCodeBlock>   user_tick_codeblocks;
    unordered_map<ReqServName, vector<CCodeLine>>   serv_codelines; // un-connected service function body codelines
    unordered_map<pair<InstanceName, ReqServName>, vector<CCodeLine>> req_codelines; // un-connected child request function body codelines

    unordered_map<StorageName, VulStorage>  storages;
    unordered_map<StorageName, VulStorage>  storagenexts;
    unordered_map<StorageName, VulStorage>  storagetmp; // reset each tick

    unordered_map<InstanceName, VulInstance>    instances;
    unordered_map<InstanceName, VulPipe>        pipe_instances;

    unordered_map<InstanceName, vector<VulReqServConnection>>       req_connections;
    unordered_map<InstanceName, vector<VulModulePipeConnection>>    mod_pipe_connections;

    vector<VulStallConnection>        stalled_connections;
    vector<VulSequenceConnection>     update_constraints;

    inline bool localCheckNameConflict(const string &name) {
        if (instances.find(name) != instances.end()) {
            return true;
        }
        if (pipe_instances.find(name) != pipe_instances.end()) {
            return true;
        }
        if (storages.find(name) != storages.end()) {
            return true;
        }
        if (storagenexts.find(name) != storagenexts.end()) {
            return true;
        }
        if (storagetmp.find(name) != storagetmp.end()) {
            return true;
        }
        if (requests.find(name) != requests.end()) {
            return true;
        }
        if (services.find(name) != services.end()) {
            return true;
        }
        if (pipe_inputs.find(name) != pipe_inputs.end()) {
            return true;
        }
        if (pipe_outputs.find(name) != pipe_outputs.end()) {
            return true;
        }
        if (local_configs.find(name) != local_configs.end()) {
            return true;
        }
        if (user_tick_codeblocks.find(name) != user_tick_codeblocks.end()) {
            return true;
        }
        return false;
    }
};

