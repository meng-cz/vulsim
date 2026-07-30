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

#include "context.hpp"

#include <unordered_set>
#include <vector>

bool macroHasReadyAttribute(const cppparse::MacroEntry &entry);
vector<cppparse::MacroEntry> parseNestedMacroEntriesPreservingPositions(const cppparse::MacroEntry &entry);

struct InterfaceEntries {
    vector<cppparse::MacroEntry> entries;
    unordered_set<string> service_names;
};

InterfaceEntries parseInterfaceEntries(
    const cppparse::MacroEntry &interface_entry,
    const VCPPModuleContext &context
);
void validateInterfaceImplementationEntries(
    const vector<cppparse::MacroEntry> &entries,
    const unordered_set<string> &interface_services,
    const VCPPModuleContext &context,
    const string &scope_name
);
vector<cppparse::MacroEntry> selectVersionedModuleEntries(
    const vector<cppparse::MacroEntry> &top_entries,
    const VCPPModuleContext &context
);
