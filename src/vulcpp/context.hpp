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

#include "../cppparse.hpp"
#include "../project.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

struct VCPPModuleContext {
    VulTempModule temp;
    unordered_map<string, string> local_names;
    struct ServiceNameState {
        bool has_declaration = false;
        bool has_implementation = false;
    };
    unordered_map<string, ServiceNameState> service_names;
    unordered_map<string, string> *global_names = nullptr;
    bool is_global_header = false;
    std::vector<uint32_t> line_mapping;

    string getOriginalPosition(const cppparse::LinePosition &pos) const;
    VulDebugLoc toDebugLoc(const cppparse::LinePosition &pos) const;
    VulDebugLocs bodyDebugLocs(const cppparse::MacroEntry &entry) const;
    string macroPosition(const cppparse::MacroEntry &entry) const;
    void declareName(const string &raw_name, const string &kind, const cppparse::MacroEntry &entry);
    void declareServiceName(const string &raw_name, bool is_declaration, const cppparse::MacroEntry &entry);
};

struct VCPPTestContext {
    VulStaticTestHarnessModule test;
    VulStaticConfigLib config_lib;
    VulStaticBundleLib bundle_lib;
    string filepath;
    std::vector<uint32_t> line_mapping;

    string getOriginalPosition(const cppparse::LinePosition &pos) const;
    VulDebugLoc toDebugLoc(const cppparse::LinePosition &pos) const;
    VulDebugLocs bodyDebugLocs(const cppparse::MacroEntry &entry) const;
};
