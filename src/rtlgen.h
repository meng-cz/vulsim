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

#pragma once

#include "errormsg.hpp"
#include "type.h"

#include "bundlelib.h"
#include "configlib.h"
#include "module.h"
#include "project.h"

namespace rtlgen {

inline string LogicSubModuleName(const ModuleName &module_name) {
    return "LogicSubModule_" + module_name;
}

struct RTLGenResult {
    bool has_logic_submodule = true;
    vector<string> logic_hls_codes;
    VulDebugLocs logic_hls_debug;
    VulDebugLines logic_hls_debug_lines;
    vector<string> rtl_skeleten_codes;
    VulDebugLocs rtl_skeleten_debug;
    VulDebugLines rtl_skeleten_debug_lines;
    vector<string> resource_files; // additional resource files needed by ROM
};

struct RTLV2LogicRTLResult {
    bool ok = true;
    vector<string> debug_codelines;
    string error;
    vector<string> error_debug_codelines;
    string error_signal_debug_text;
    vector<string> error_signal_names;
};

RTLGenResult genModuleRTL(
    const VulStaticModuleInstance &module,
    const VulStaticConfigLib &configlib,
    const VulStaticBundleLib &bundlelib,
    const vector<string> &global_helper_codes
);

RTLGenResult genModuleRTLV2(
    const VulStaticModuleInstance &module,
    const VulStaticConfigLib &configlib,
    const VulStaticBundleLib &bundlelib,
    const vector<string> &global_helper_codes
);

RTLV2LogicRTLResult appendRTLV2LogicRTL(
    RTLGenResult &result,
    const VulStaticModuleInstance &module,
    const string &logic_hls_filepath,
    const string &lib_include_dir,
    int unroll_limit = 1024
);

vector<string> genVerilatorTestMainCpp(
    const VulStaticProject &project,
    const string &top_verilator_class_name = ""
);


} // namespace rtlgen
