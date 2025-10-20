/*
 * Copyright (c) 2025 Meng-CZ
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <unordered_map>
#include <memory>

#include "type.h"

using std::unordered_map;
using std::unique_ptr;
using std::make_unique;
using std::tuple;

#ifndef VERSION_MAJOR
#define VERSION_MAJOR 0
#endif
#ifndef VERSION_MINOR
#define VERSION_MINOR 0
#endif
#ifndef VERSION_PATCH
#define VERSION_PATCH 1
#endif

class VulDesign {
public:

    ~VulDesign() = default;

    /**
     * @brief Load a VulDesign from a project file.
     * @param filename The name of the file to load.
     * @param err Error message in case of failure.
     * @return A unique_ptr to the loaded VulDesign, or nullptr on failure.
     */
    static unique_ptr<VulDesign> loadFromFile(const string &filename, string &err);

    /**
     * @brief Initialize a new VulDesign with given project name and directory.
     * This creates the necessary directory structure and an initial design.xml file.
     * @param project_name The name of the new project.
     * @param project_dir The directory to create the project in.
     * @param err Error message in case of failure.
     * @return A unique_ptr to the initialized VulDesign, or nullptr on failure.
     */
    static unique_ptr<VulDesign> initNewDesign(const string &project_name, const string &project_dir, string &err);

    string project_dir;
    string project_name;
    tuple<int, int, int> version = {VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH}; // major, minor, patch

    unordered_map<string, VulBundle> bundles; // Indexed by bundle name

    unordered_map<string, VulConfigItem> config_lib; // Indexed by config item name

    unordered_map<string, VulCombine> combines; // Indexed by combine name

    unordered_map<string, VulInstance> instances; // Indexed by instance name
    unordered_map<string, VulPipe> pipes; // Indexed by pipe name

    vector<VulReqConnection> req_connections;
    vector<VulModulePipeConnection> mod_pipe_connections;
    vector<VulPipeModuleConnection> pipe_mod_connections;
    vector<VulStalledConnection> stalled_connections;
    vector<VulUpdateSequence> update_constraints;

    unordered_map<string, VulPrefab> prefabs; // Indexed by prefab name

    /**
     * @brief Check for global name conflicts across bundles, combines, instances, pipes, prefabs, and config items.
     * @param name The name to check for conflicts.
     * @return An empty string if no conflict, or an error message if there is a conflict.
     */
    string checkGlobalNameConflict(const string &name);

    /**
     * @brief Add a prefab to the design, along with its dependent bundles.
     * @param prefab The VulPrefab to add.
     * @param dep_bundles The list of VulBundles that the prefab depends on.
     * @return An empty string on success, or an error message on failure (e.g. name conflict).
     */
    string addPrefab(VulPrefab &prefab, vector<VulBundle> &dep_bundles);

    vector<VulVisBlock> vis_blocks;
    vector<VulVisText> vis_texts;

    bool dirty_config_lib = false;
    bool dirty_bundles = false;
    bool dirty_combines = false;

    /**
     * @brief Save the entire project to the project directory.
     * @return An empty string on success, or an error message on failure.
     */
    string saveProject();


    /**
     * @brief Check for name conflicts:
     * (1) No duplicate names within: bundles, combines, instances, pipes, and config_lib.
     * (2) All instances must refer to an existing combine.
     * (3) All names must be valid C++ identifiers and not C++ reserved keywords.
     * (4) All config reference names in combines must exist in config_lib.
     */
    string _checkNameError();

    /**
     * @brief Check for type errors:
     * (1) All types must be valid VulSim data types within: bundles, combines, and pipes.
     * (2) All types must be valid VulSim Basic Data Types within: storagetick
     * (3) All bundle types must not be looped (i.e. a bundle cannot contain itself directly or indirectly).
     */
    string _checkTypeError();

    /**
     * @brief Check for request-service connection errors:
     * (1) All requests connections must refer to existing instances and requests.
     * (2) All services connections must refer to existing instances and services.
     * (3) All connections must match in argument and return types exactly in type and sequence.
     * (4) Request without return can connect to multiple services or not connect at all.
     * (5) All requests with return must be connected to one service.
     */
    string _checkReqConnectionError();

    /**
     * @brief Check for pipe connection errors:
     * (1) Module-Pipe connections must refer to existing instance with valid pipeout port, to a valid pipe input potr index.
     * (2) Pipe-Module connections must refer to existing instance with valid pipein port, to a valid pipe output port index.
     * (3) The pipe port type must match the connected module port type exactly.
     */
    string _checkPipeConnectionError();

    /**
     * @brief Check for update sequence connection errors:
     * (1) All update sequence connections must refer to existing instances.
     * (2) All stalled connections must refer to existing instances with stallable flag in its combine as true.
     * (3) No loops in the combined update graph of:
     *   (a) Update sequence connections: former_instance -> latter_instance
     *   (b) Stalled connections: src_instance -> dest_instance
     */
    string _checkSeqConnectionError();

    /**
     * @brief Check All Errors in this design.
     */
    string checkAllError() {
        string err;
        err = _checkNameError(); if (!err.empty()) return err;
        err = _checkTypeError(); if (!err.empty()) return err;
        err = _checkReqConnectionError(); if (!err.empty()) return err;
        err = _checkPipeConnectionError(); if (!err.empty()) return err;
        err = _checkSeqConnectionError(); if (!err.empty()) return err;
        return string();
    }

    /**
     * @brief Check if the given type is a valid VulSim Data Type.
     * List of valid types:
     * Basic types; Bundles;
     * vector<Valid types>; array<Valid types, N>; set<Valid types>; map<Valid types, Valid types>
     * @param type The type string to check.
     * @return true if the type is a valid type, false otherwise.
     */
    bool isValidTypeName(const string &type) {
        vector<string> dummy;
        return isValidTypeName(type, dummy);
    }
    
    /**
     * @brief Check if the given type is a valid VulSim Data Type.
     * List of valid types:
     * Basic types; Bundles;
     * vector<Valid types>; array<Valid types, N>; set<Valid types>; map<Valid types, Valid types>
     * @param type The type string to check.
     * @param seen_bundles A list of bundles that have already been seen (to detect circular dependencies).
     * @return true if the type is a valid type, false otherwise.
     */
    bool isValidTypeName(const string &type, vector<string> &seen_bundles);

    /**
     * @brief Get the VulCombine definition for a given combine name.
     * If the combine does not exist, create a fake combine from a prefab with the same name.
     * @param combine_name The name of the combine to get.
     * @param is_fake Output parameter indicating whether the returned combine is fake (from prefab) or real.
     * @return A unique_ptr to the VulCombine definition.
     */
    unique_ptr<VulCombine> getOrFakeCombineReadonly(const string &combine_name, bool &is_fake);
    /**
     * @brief Get the VulCombine definition for a given combine name.
     * If the combine does not exist, create a fake combine from a prefab with the same name.
     * @param combine_name The name of the combine to get.
     * @return A unique_ptr to the VulCombine definition.
     */
    unique_ptr<VulCombine> getOrFakeCombineReadonly(const string &combine_name) {
        bool is_fake;
        return getOrFakeCombineReadonly(combine_name, is_fake);
    }

protected:
    VulDesign() {};
};

