/*
 * Copyright (c) 2025 Meng-CZ
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include <string>
#include <vector>
#include <unordered_set>

using std::vector;
using std::pair;
using std::string;
using std::unordered_set;
using std::tuple;

/**
 * @brief Check if the given name is a valid identifier.
 * A valid identifier starts with a letter or underscore, followed by letters, digits, or underscores.
 * @param name The identifier name to check.
 * @return true if the name is a valid identifier, false otherwise.
 */
bool isValidIdentifier(const string &s);

/**
 * @brief Replace all occurrences of an identifier in a string with a new identifier.
 * Only whole-word matches are replaced.
 * @param s The input string.
 * @param old_name The identifier to be replaced.
 * @param new_name The new identifier to replace with.
 * @return A new string with the replacements made.
 */
string identifierReplace(const string &s, const string &old_name, const string &new_name);

/**
 * @brief Check if the given type is a valid VulSim Basic Data Type.
 * List of valid basic types:
 * uint8, uint16, uint32, uint64, uint128, int8, int16, int32, int64, int128, bool
 * @param type The type string to check.
 * @return true if the type is a valid basic type, false otherwise.
 */
bool isBasicVulType(const string &s);

typedef struct {
    string project_name;
    string project_dir;
    tuple<int, int, int> version;               // major, minor, patch
    vector<std::pair<string, string>> prefabs;  // name, directory path
} VulProject;

typedef struct {
    string type;
    string name;
    string comment;
} VulArgument;

typedef struct {
    string name;
    string comment;
    vector<VulArgument> members;
    vector<string> nested_bundles; // Names of bundles used as member types
    vector<string> ref_prefabs; // Names of prefabs depending on this bundle
} VulBundle;

typedef struct {
    string code;    // Raw C++ code to include
    string file;    // Using code from this file if code is empty
    string name;    // Function name
} VulCppFunc;

typedef struct {
    string name;
    string type;
    string comment;
} VulPipePort;

typedef struct {
    string name;
    string comment;
    vector<VulArgument> arg;
    vector<VulArgument> ret;
} VulRequest;

typedef struct {
    string name;
    string comment;
    vector<VulArgument> arg;
    vector<VulArgument> ret;
    VulCppFunc cppfunc;
} VulService;

typedef struct {
    string type;
    string name;
    string value;
    string comment;
} VulStorage;

typedef struct {
    string type;
    string name;
    string size;
    string value;
    string comment;
} VulStorageArray;

typedef struct {
    string name;
    string value;
    string comment;
} VulConfigItem;

typedef struct {
    string name;
    string comment;
    string value;
    unordered_set<string> references; // names of config items referenced by this value
} VulConfig;

/**
 * @brief Extract referenced config item names from a VulConfig's value string.
 * This function parses the value string to identify identifiers.
 * @param vc The VulConfig object to extract references for. The references field will be populated.
 */
void extractConfigReferences(VulConfig &vc);

typedef struct {
    string name;
    string comment;
    vector<VulPipePort> pipein;
    vector<VulPipePort> pipeout;
    vector<VulRequest> request;
    vector<VulService> service;
    vector<VulStorage> storage;
    vector<VulStorage> storagenext;
    vector<VulStorage> storagetick;
    vector<VulStorageArray> storagenextarray;
    VulCppFunc tick;
    VulCppFunc applytick;
    VulCppFunc init;
    vector<VulConfig> config;
    bool stallable;
} VulCombine;

typedef struct {
    string name;
    string path;
    string comment;
    
    vector<VulPipePort> pipein;
    vector<VulPipePort> pipeout;
    vector<VulRequest> request;
    vector<VulService> service;
    vector<VulConfig> config;

    vector<string> ref_bundles; // Names of bundles this prefab depends on

    bool stallable;
} VulPrefab;

/**
 * @brief Create a fake VulCombine from a VulPrefab for design-time representation.
 * @param prefab The VulPrefab to convert.
 * @param out_combine The output VulCombine object to populate.
 */
void fakeCombineFromPrefab(const VulPrefab &prefab, VulCombine &out_combine);

typedef struct {
    long x = 0, y = 0;
    long w = 0, h = 0;

    bool visible = true;
    bool enabled = true;
    bool selected = false;

    string color;
    string bordercolor;
    unsigned int borderwidth = 1;
} VulVisualization;

typedef struct {
    string name;
    string comment;
    string combine;
    vector<pair<string, string>> config_override;

    VulVisualization vis;
} VulInstance;

typedef struct {
    string name;
    string type;
    string comment;
    unsigned int inputsize;
    unsigned int outputsize;
    unsigned int buffersize;
    bool handshake;

    VulVisualization vis;
} VulPipe;

enum class VulPipeType {
    invalid,
    simple_handshake,       // one-input, one-output, handshake
    simple_nonhandshake,    // one-input, one-output, non-handshake
    simple_buffered,        // one-input, one-output, buffered handshake
    // buffered_handshake,     // multi-input, multi-output, buffered handshake
};

/**
 * @brief Detect the type of a VulPipe based on its properties.
 * @param vp The VulPipe to analyze.
 * @return The detected VulPipeType.
 */
VulPipeType detectVulPipeType(const VulPipe &vp);

typedef struct {
    string req_instance;
    string req_name;
    string serv_instance;
    string serv_name;

    VulVisualization vis;
} VulReqConnection;

typedef struct {
    string instance;
    string pipeoutport;
    string pipe;
    unsigned int portindex;
    
    VulVisualization vis;
} VulModulePipeConnection;

typedef struct {
    string instance;
    string pipeinport;
    string pipe;
    unsigned int portindex;
    
    VulVisualization vis;
} VulPipeModuleConnection;

typedef struct {
    string src_instance;
    string dest_instance;

    VulVisualization vis;
} VulStalledConnection;

typedef struct {
    string former_instance;
    string latter_instance;

    VulVisualization vis;
} VulUpdateSequence;

typedef struct {
    string name;
    VulVisualization vis;
} VulVisBlock;

typedef struct {
    string text;
    VulVisualization vis;
} VulVisText;

