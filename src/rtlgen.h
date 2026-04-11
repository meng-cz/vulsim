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

#include "errormsg.hpp"
#include "type.h"

#include "bundlelib.h"
#include "configlib.h"
#include "module.h"
#include "project.h"

namespace rtlgen {

using BundleTable = std::unordered_map<BundleName, VulBundleItem>;

inline string LogicSubModuleName(const ModuleName &module_name) {
    return "LogicSubModule_" + module_name;
}
inline string RegisterSubModuleName(const ModuleName &module_name) {
    return "RegisterSubModule_" + module_name;
}

ErrorMsg genHLSMainFunc(
    const VulModule &module,
    const BundleTable &bundle_table,
    const unordered_map<ConfigName, ConfigValue> &config_overrides,
    const unordered_map<ConfigName, ConfigRealValue> &calculated_configlib,
    const VulProject &project,
    vector<string> &out_lines
);

ErrorMsg genModuleRTLSkeletonWithReg(
    const VulModule &module,
    const BundleTable &bundle_table,
    const unordered_map<ConfigName, ConfigValue> &param_override,
    const VulProject &project,
    vector<string> &out_lines
);




} // namespace rtlgen
