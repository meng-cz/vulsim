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

#include "configlib.h"
#include "bundlelib.h"
#include "module.h"
#include "errormsg.hpp"

namespace serialize {

struct ConfigItemRaw {
    string  name;
    string  value;
    string  comment;
    string  group;
};

/**
 * @brief Parse config library from an XML file.
 * @param filepath The path to the XML file.
 * @param out_configs Output parameter to hold the parsed ConfigItemRaw items.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg parseConfigLibFromXMLFile(const string &filepath, vector<ConfigItemRaw> &out_configs);

/**
 * @brief Write config library to an XML file.
 * @param filepath The path to the XML file.
 * @param configs The ConfigItemRaw items to write.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg writeConfigLibToXMLFile(const string &filepath, const vector<ConfigItemRaw> &configs);

struct BundleMemberRaw {
    string  name;
    string  type;
    string  uintlen;
    string  value;
    string  comment;
    vector<string> dims;
};

struct BundleItemRaw {
    string  name;
    string  comment;
    bool    isenum;
    bool    isalias;
    vector<BundleMemberRaw> members;
};

/**
 * @brief Parse bundle library from an XML file.
 * @param filepath The path to the XML file.
 * @param out_bundles Output parameter to hold the parsed BundleItemRaw items.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg parseBundleLibFromXMLFile(const string &filepath, vector<BundleItemRaw> &out_bundles);

/**
 * @brief Write bundle library to an XML file.
 * @param filepath The path to the XML file.
 * @param bundles The BundleItemRaw items to write.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg writeBundleLibToXMLFile(const string &filepath, const vector<BundleItemRaw> &bundles);

struct ArgRaw {
    string  name;
    string  type;
    string  comment;
};

struct ReqServRaw {
    string          name;
    string          comment;
    vector<ArgRaw>  args;
    vector<ArgRaw>  rets;
    string          arraysize;
    bool            handshake;
};

struct PipePortRaw {
    string  name;
    string  type;
    string  comment;
};

struct LocalConfigRaw {
    string  name;
    string  value;
    string  comment;
};

struct ModuleBaseRaw {
    string  name;
    string  comment;
    vector<LocalConfigRaw>      local_configs;
    vector<BundleItemRaw>       local_bundles;
    vector<ReqServRaw>          requests;
    vector<ReqServRaw>          services;
    vector<PipePortRaw>         pipe_inputs;
    vector<PipePortRaw>         pipe_outputs;
};

struct InstanceRaw {
    string          name;
    string          module_name;
    string          comment;
    vector<LocalConfigRaw>  local_config_overrides;
};

struct PipeRaw {
    string  name;
    string  type;
    string  comment;
    string  input_size;
    string  output_size;
    string  buffer_size;
    string  latency;
    bool    has_handshake;
    bool    has_valid;
};

struct ConnectionRaw {
    string  src_instance;
    string  src_name;
    string  dst_instance;
    string  dst_name;
};

struct SeqConnectionRaw {
    string  former_instance;
    string  latter_instance;
};

struct CodeblockRaw {
    string instname;
    string blockname;
    vector<string> code_lines;
};

struct ModuleRaw {
    string  name;
    string  comment;
    vector<LocalConfigRaw>      local_configs;
    vector<BundleItemRaw>       local_bundles;
    vector<ReqServRaw>          requests;
    vector<ReqServRaw>          services;
    vector<PipePortRaw>         pipe_inputs;
    vector<PipePortRaw>         pipe_outputs;
    vector<InstanceRaw>         instances;
    vector<PipeRaw>             pipes;
    vector<ConnectionRaw>       reqserv_connections;
    vector<ConnectionRaw>       pipe_connections;
    vector<SeqConnectionRaw>    stall_connections;
    vector<SeqConnectionRaw>    sequence_connections;
    vector<CodeblockRaw>        codeblocks;
    vector<string>              user_header_code_lines;
};

/**
 * @brief Parse a base module from an XML file.
 * @param filepath The path to the XML file.
 * @param out_module Output parameter to hold the parsed VulModuleBase.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg parseModuleBaseFromXMLFile(const string &filepath, ModuleBaseRaw &out_module);

/**
 * @brief Write a base module to an XML file.
 * @param filepath The path to the XML file.
 * @param module The ModuleBaseRaw to write.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg writeModuleBaseToXMLFile(const string &filepath, const ModuleBaseRaw &module);

/**
 * @brief Parse a full module from an XML file.
 * @param filepath The path to the XML file.
 * @param out_module Output parameter to hold the parsed ModuleRaw.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg parseModuleFromXMLFile(const string &filepath, ModuleRaw &out_module);

/**
 * @brief Write a full module to an XML file.
 * @param filepath The path to the XML file.
 * @param module The ModuleRaw to write.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg writeModuleToXMLFile(const string &filepath, const ModuleRaw &module);


struct ImportRaw {
    string abspath;
    string name;
    vector<LocalConfigRaw> config_overrides;
};

struct ProjectRaw {
    string topmodule;
    vector<ImportRaw> imports;
};

/**
 * @brief Parse a project from an XML file.
 * @param filepath The path to the XML file.
 * @param out_project Output parameter to hold the parsed ProjectRaw.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg parseProjectFromXMLFile(const string &filepath, ProjectRaw &out_project);

/**
 * @brief Write a project to an XML file.
 * @param filepath The path to the XML file.
 * @param project The ProjectRaw to write.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg writeProjectToXMLFile(const string &filepath, const ProjectRaw &project);

} // namespace serialize
