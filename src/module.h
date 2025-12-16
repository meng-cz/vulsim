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
#include "bundlelib.h"

#include <map>

using std::pair;
using std::map;

typedef string ModuleName;

typedef string ReqServName;
typedef string PipePortName;
typedef string ArgName;

typedef struct {
    ArgName     name;
    DataType    type;
    Comment     comment;
} VulArg;

class VulReqServ {
public:
    ReqServName    name;
    Comment        comment;
    vector<VulArg>  args;
    vector<VulArg>  rets;
    // ConfigValue     array_size; // empty if not an array request
    bool            has_handshake;

    inline bool allowMultiConnect() const {
        // return (array_size.empty() && !has_handshake && rets.empty());
        return (!has_handshake && rets.empty());
    }

    /**
     * @brief Whether two VulReqServ have the same signature (args, rets, array_size, has_handshake).
     * @param a The first VulReqServ.
     * @param b The second VulReqServ.
     * @return true if they have the same signature, false otherwise.
     */
    inline bool match(const VulReqServ &a) const {
        if (a.has_handshake != has_handshake) return false;
        // if (!a.array_size.empty() || !array_size.empty()) {
        //     return config_parser::tokenEq(a.array_size, array_size);
        // }
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
};


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

class ModuleTreeNode {
public:
    vector<shared_ptr<ModuleTreeNode>>  submodules;
    ModuleName  name;
    Comment     comment;
    bool        is_external;
};

class ModuleTreeBidirectionalNode {
public:
    vector<shared_ptr<ModuleTreeBidirectionalNode>>  parents;
    vector<shared_ptr<ModuleTreeBidirectionalNode>>  children;
    ModuleName  name;
    Comment     comment;
    bool        is_external;
};

class VulModuleBase {
public:
    /**
     * @brief Get the singleton instance of the module library.
     * @return A shared_ptr to the module library instance.
     */
    static shared_ptr<unordered_map<ModuleName, shared_ptr<VulModuleBase>>> getModuleLibInstance();

    /**
     * @brief Build the module tree from the module library.
     * Must be called after 'updateDynamicReferences' are called for all modules.
     * @param root_modules Output parameter to hold the root modules of each tree.
     */
    static void buildModuleTree(vector<shared_ptr<ModuleTreeNode>> &root_modules);

    /**
     * @brief Build a single module reference tree (bidirectional) from the module library.
     * Must be called after 'updateDynamicReferences' are called for all modules.
     * @param root_module_name The name of the root module.
     * @param out_root_node Output parameter to hold the root node of the tree. Nullptr if the module does not exist.
     */
    static void buildSingleModuleReferenceTree(const ModuleName &root_module_name, shared_ptr<ModuleTreeBidirectionalNode> &out_root_node);

    ModuleName                  name;
    Comment                     comment;
    map<ConfigName, VulLocalConfigItem>         local_configs; // configs private to instance, must be ordered by name
    unordered_map<BundleName, VulBundleItem>    local_bundles; // bundles private to this module
    unordered_map<ReqServName, VulReqServ>      requests;
    unordered_map<ReqServName, VulReqServ>      services;
    unordered_map<PipePortName, VulPipePort>    pipe_inputs;
    unordered_map<PipePortName, VulPipePort>    pipe_outputs;
    
    unordered_set<ConfigName> _dyn_referenced_configs;
    unordered_set<BundleName> _dyn_referenced_bundles;
    unordered_set<ModuleName> _dyn_referenced_modules;

    /**
     * @brief Update the dynamic references (_dyn_referenced_configs, _dyn_referenced_bundles, _dyn_referenced_modules) based on current module definition.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    virtual ErrorMsg updateDynamicReferences();

    virtual bool isExternalModule() const = 0; 
};

typedef string EModuleDir;

class VulExternalModule : public VulModuleBase {
public:
    virtual bool isExternalModule() const override { return true; } 

    EModuleDir      directory;
};

typedef string InstanceName;

typedef struct {
    InstanceName        name;
    ModuleName          module_name;
    Comment             comment;
    unordered_map<ConfigName, LocalConfigValue> local_config_overrides; // local config overrides for this instance
} VulInstance;

class VulReqServConnection {
public:
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

class VulModulePipeConnection {
public:
    InstanceName    instance;
    PipeName        instance_pipe_port;
    InstanceName    pipe_instance;
    PipeName        top_pipe_port;         // valid when pipe_instance is TopInterface

    inline string toString() const {
        return "'" + instance + "'.'" + instance_pipe_port + "' <-> '" + pipe_instance + "'.'" + top_pipe_port + "'";
    }
};

inline bool operator==(const VulModulePipeConnection &a, const VulModulePipeConnection &b) {
    return std::tie(a.instance, a.instance_pipe_port, a.pipe_instance, a.top_pipe_port) ==
           std::tie(b.instance, b.instance_pipe_port, b.pipe_instance, b.top_pipe_port);
}

class VulSequenceConnection {
public:
    InstanceName    former_instance;
    InstanceName    latter_instance;

    inline string toString() const {
        return "'" + former_instance + "' -> '" + latter_instance + "'";
    }
};

inline bool operator==(const VulSequenceConnection &a, const VulSequenceConnection &b) {
    return std::tie(a.former_instance, a.latter_instance) ==
           std::tie(b.former_instance, b.latter_instance);
}


typedef string CCodeLine;

typedef string StorageName;
typedef VulBundleMember VulStorage;

typedef struct {
    InstanceName        name;
    Comment             comment;
    vector<CCodeLine>   codelines;     // user tick function body codelines
} VulTickCodeBlock;

class VulModule : public VulModuleBase {
public:

    inline static const InstanceName TopInterface = string("__top__");

    virtual bool isExternalModule() const override { return false; }

    bool is_hpp_generated = true; // TODO: always true for now
    bool is_inline_generated = true;

    // vector<CCodeLine>   user_applytick_codelines;       // user applytick function body codelines
    vector<CCodeLine>   user_header_field_codelines;    // user header field codelines (private section)

    unordered_map<InstanceName, VulTickCodeBlock>   user_tick_codeblocks;
    unordered_map<ReqServName, vector<CCodeLine>>   serv_codelines; // un-connected service function body codelines
    unordered_map<InstanceName, unordered_map<ReqServName, vector<CCodeLine>>> req_codelines; // un-connected child request function body codelines

    unordered_map<StorageName, VulStorage>  storages;
    unordered_map<StorageName, VulStorage>  storagenexts;
    unordered_map<StorageName, VulStorage>  storagetmp; // reset each tick

    unordered_map<InstanceName, VulInstance>    instances;
    unordered_map<InstanceName, VulPipe>        pipe_instances;

    struct ReqServConnHash {
        size_t operator()(const VulReqServConnection &conn) const {
            return std::hash<string>()(conn.req_name) ^ std::hash<string>()(conn.serv_name) ^ std::hash<string>()(conn.req_instance) ^ std::hash<string>()(conn.serv_instance);
        }
    };

    struct PipeConnHash {
        size_t operator()(const VulModulePipeConnection &conn) const {
            return std::hash<string>()(conn.instance) ^ std::hash<string>()(conn.instance_pipe_port) ^ std::hash<string>()(conn.pipe_instance) ^ std::hash<string>()(conn.top_pipe_port);
        }
    };

    unordered_map<InstanceName, unordered_set<VulReqServConnection, ReqServConnHash>>   req_connections;
    unordered_map<InstanceName, unordered_set<VulModulePipeConnection, PipeConnHash>>   mod_pipe_connections;

    struct SeqConnHash {
        size_t operator()(const VulSequenceConnection &conn) const {
            return std::hash<string>()(conn.former_instance) ^ std::hash<string>()(conn.latter_instance);
        }
    };

    unordered_set<VulSequenceConnection, SeqConnHash>    stalled_connections;
    unordered_set<VulSequenceConnection, SeqConnHash>    update_constraints;

    /**
     * @brief Check if the given name conflicts with existing names in local scope.
     * @param name The name to check.
     */
    bool localCheckNameConflict(const string &name);

    /**
     * @brief Update the dynamic references (_dyn_referenced_configs, _dyn_referenced_bundles, _dyn_referenced_modules) based on current module definition.
     * Consumes the module definition is validated.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    virtual ErrorMsg updateDynamicReferences() override;


    /**
     * @brief Validate the module definition for internal consistency and correctness.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    inline ErrorMsg validateAll() const {
        ErrorMsg err;

        if (!(err = _0_validateBrokenIndexes()).empty()) return err;
        if (!(err = _1_validateNameConflicts()).empty()) return err;
        if (!(err = _2_validateLocalConfigBundleDefinitions()).empty()) return err;
        if (!(err = _3_validateInstanceDefinitions()).empty()) return err;
        if (!(err = _4_validateReqServConnections()).empty()) return err;
        if (!(err = _5_validatePipeConnections()).empty()) return err;
        if (!(err = _6_validateStallSequenceConnections()).empty()) return err;

        return "";
    }

    /**
     * @brief Validate broken indexes in the module definition.
     */
    ErrorMsg _0_validateBrokenIndexes() const;

    /**
     * @brief Validate name conflicts within the module.
     * 1. Check name valid within local scope.
     * 2. Check name conflicts within local scope.
     * 3. Check name conflicts from local name to global scope (config, bundle, module).
     */
    ErrorMsg _1_validateNameConflicts() const;

    /**
     * @brief Validate local config value and bundle definitions.
     * 1. Check local config value expressions, referencing only global configs.
     * 2. Check local bundle definitions and value expressions within, referencing global/local bundles/configs.
     * 3. Check storages definitions, similar to bundle members.
     * 4. Check local bundle referencing loop.
     */
    ErrorMsg _2_validateLocalConfigBundleDefinitions() const;

    /**
     * @brief Validate instance definitions.
     * 1. Check instance module existence.
     * 2. Check local config overrides valid for each instance.
     */
    ErrorMsg _3_validateInstanceDefinitions() const;

    /**
     * @brief Validate request-service connections.
     * 1. For each req-serv connection, check existence and signature match.
     * 2. For each service, it must be:
     *     a. Connected to child service with matching signature
     *     b. Implemented with service code lines
     * 3. For each child request, it must be:
     *     a. Connected to request with matching signature
     *     b. Connected to child service with matching signature
     *     c. Implemented with request code lines
     */
    ErrorMsg _4_validateReqServConnections() const;

    /**
     * @brief Validate pipe connections.
     * 1. For each pipe connection, check existence and type match.
     * 2. For each child pipe port, it must be:
     *     a. Connected to top interface pipe port with matching type
     *     b. Connected to pipe instance with matching type
     */
    ErrorMsg _5_validatePipeConnections() const;

    /**
     * @brief Validate stall and sequence connections.
     * 1. For each stall & sequence connection, check existence.
     * 2. Check no loop in stall & sequence connections.
     * 3. Check no loop in stall connections with TopInterface.
     */
    ErrorMsg _6_validateStallSequenceConnections() const;


};

