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
#include "pipetype.hpp"

using std::pair;

typedef string ModuleName;

typedef string RequestName;
typedef string ServiceName;
typedef string PipeInputName;
typedef string PipeOutputName;

typedef string ArgName;
typedef string RetName;

typedef struct {
    ArgName     name;
    DataType    type;
    Comment     comment;
} VulArg;

typedef struct {
    RetName     name;
    DataType    type;
    Comment     comment;
} VulRet;

typedef struct {
    RequestName     name;
    Comment         comment;
    vector<VulArg>  args;
    vector<VulRet>  rets;
    ConfigValue     array_size; // empty if not an array request
    bool            has_handshake;
} VulRequest;

typedef struct {
    ServiceName     name;
    Comment         comment;
    vector<VulArg>  args;
    vector<VulRet>  rets;
    ConfigValue     array_size; // empty if not an array service
    bool            has_handshake;
} VulService;

typedef struct {
    PipeInputName   name;
    DataType        type;
    Comment         comment;
} VulPipeInput;

typedef struct {
    PipeOutputName  name;
    DataType        type;
    Comment         comment;
} VulPipeOutput;


class VulModuleBase {
public:
    ModuleName                  name;
    Comment                     comment;
    unordered_map<RequestName, VulRequest>          requests;
    unordered_map<ServiceName, VulService>          services;
    unordered_map<PipeInputName, VulPipeInput>      pipe_inputs;
    unordered_map<PipeOutputName, VulPipeOutput>    pipe_outputs;
    bool                        _is_external = false; // whether the module is imported from external definition
};

/**
 * @brief Get the module definition that matches the given pipe characteristics.
 * @param pipe The VulPipe definition.
 * @return The VulModuleBase definition that matches the pipe.
 */
VulModuleBase getModuleDefinitionForPipe(const VulPipe &pipe) {
    VulModuleBase ret;
    ret.name = pipe.name + "_module";
    ret.comment = "Auto-generated module for pipe " + pipe.name;
    // add pipe input
    ret.pipe_inputs["in"] = VulPipeInput{
        .name = "in",
        .type = pipe.type,
        .comment = "Input port for pipe " + pipe.name
    };
    // add pipe output
    ret.pipe_outputs["out"] = VulPipeOutput{
        .name = "out",
        .type = pipe.type,
        .comment = "Output port for pipe " + pipe.name
    };
    // add clear service
    ret.services["clear"] = VulService{
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
} VulInstance;

typedef struct {
    InstanceName    req_instance;
    RequestName     req_name;       // should be ServiceName when req_instance is TopInterface
    InstanceName    serv_instance;
    ServiceName     serv_name;      // should be RequestName when serv_instance is TopInterface
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

class VulModule : public VulModuleBase {
public:

    const InstanceName TopInterface = string("__top__");
    const InstanceName TopStallInput = string("__top_stall_input__");
    const InstanceName TopStallOutput = string("__top_stall_output__");
    const InstanceName TopUpdateNode = string("__top_update_node__");

    bool is_hpp_generated = true; // TODO: always true for now
    bool is_inline_generated = true;

    vector<CCodeLine>   user_tick_codelines;            // user tick function body codelines
    vector<CCodeLine>   user_applytick_codelines;       // user applytick function body codelines
    vector<CCodeLine>   user_header_filed_codelines;    // user header field codelines (private section)

    unordered_map<ServiceName, vector<CCodeLine>>   serv_codelines; // un-connected service function body codelines
    unordered_map<pair<InstanceName, RequestName>, vector<CCodeLine>> req_codelines; // un-connected child request function body codelines

    unordered_map<StorageName, VulStorage>  storages;
    unordered_map<StorageName, VulStorage>  storagenexts;
    unordered_map<StorageName, VulStorage>  storagetmp; // reset each tick

    unordered_map<InstanceName, VulInstance>    instances;
    unordered_map<InstanceName, VulPipe>        pipe_instances;

    unordered_map<InstanceName, VulReqServConnection>       req_connections;
    unordered_map<InstanceName, VulModulePipeConnection>    mod_pipe_connections;

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
        if (name == TopInterface) {
            return true;
        }
        return false;
    }
};

