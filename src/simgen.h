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

#include <optional>

#include "errormsg.hpp"
#include "type.h"

#include "bundlelib.h"
#include "breakpoint.hpp"
#include "configlib.h"
#include "module.h"
#include "project.h"
#include "trace.hpp"

namespace simgen {

string genCurrentTimeString();

vector<string> genStaticBundle(const VulStaticBundle &bundle);

vector<string> genStaticConfigHeaderCode(const VulStaticConfigLib &configlib);

vector<string> genStaticBundleHeaderCode(const VulStaticBundleLib &bundlelib);

vector<string> genStaticGlobalHelperHeaderCode(const vector<string> &helper_codes);

vector<string> genStaticProjectHeaderCode(
    const VulStaticConfigLib &configlib,
    const VulStaticBundleLib &bundlelib,
    const vector<string> &helper_codes
);

struct StaticModuleCodeHpp {
    vector<string> decl;
    VulDebugLocs decl_debug;
    VulDebugLines decl_debug_lines;
    vector<string> impl;
    VulDebugLocs impl_debug;
    VulDebugLines impl_debug_lines;
    vector<string> resource_files;
};

StaticModuleCodeHpp genStaticModuleCodeHpp(const VulStaticModuleInstance &module_instance, const vector<VulTracedSignal> &traced_signals);

vector<string> genStaticTestHarnessHpp(
    const VulStaticTestHarnessModule &test_module,
    const VulStaticModuleInstance &top_module,
    bool enable_tracing,
    const vector<VulBreakPointSpec> &break_specs,
    uint64_t break_cycles
);

struct StaticTestHarnessCodeHpp {
    vector<string> codes;
    VulDebugLocs debug;
    VulDebugLines debug_lines;
};

StaticTestHarnessCodeHpp genStaticTestHarnessCodeHpp(
    const VulStaticTestHarnessModule &test_module,
    const VulStaticModuleInstance &top_module,
    bool enable_tracing,
    const vector<VulBreakPointSpec> &break_specs,
    uint64_t break_cycles
);

vector<string> genStaticTestMainHpp(shared_ptr<VulStaticModuleInstance> top_module);

} // namespace simgen
