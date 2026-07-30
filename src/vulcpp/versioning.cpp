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

#include "versioning.hpp"

#include "common.hpp"
#include "../stringop.hpp"

using namespace cppparse;
using namespace stringop;

bool macroHasReadyAttribute(const MacroEntry &entry) {
    for (size_t i = 1; i < entry.args.size(); ++i) {
        auto attr = parseKeyValueArg(entry.args[i]);
        if (isAttrKey(attr, "ready")) {
            return true;
        }
    }
    return false;
}

vector<MacroEntry> parseNestedMacroEntriesPreservingPositions(const MacroEntry &entry) {
    return findAllMacroEntriesPreservingLinePositions(entry.body, entry.body_pos);
}
InterfaceEntries parseInterfaceEntries(
    const MacroEntry &interface_entry,
    const VCPPModuleContext &context
) {
    if (!interface_entry.args.empty()) {
        throw VulException("INTERFACE does not take any arguments at " + context.getOriginalPosition(interface_entry.pos));
    }
    if (!interface_entry.has_body) {
        throw VulException("INTERFACE must use a code block at " + context.getOriginalPosition(interface_entry.pos));
    }

    InterfaceEntries out;
    vector<MacroEntry> interface_entries = parseNestedMacroEntriesPreservingPositions(interface_entry);
    for (const auto &entry : interface_entries) {
        if (entry.name != "PARAMETER" && entry.name != "REQUEST" && entry.name != "SERVICE") {
            throw VulException(
                "INTERFACE may only contain PARAMETER, REQUEST, and SERVICE declarations; got '" +
                entry.name + "' at " + context.getOriginalPosition(entry.pos)
            );
        }
        if (entry.has_body) {
            throw VulException(
                "INTERFACE entry '" + entry.name + "' must be a declaration without code block at " +
                context.getOriginalPosition(entry.pos)
            );
        }
        if (entry.name == "SERVICE") {
            if (entry.args.empty()) {
                throw VulException("SERVICE requires at least 1 argument at " + context.getOriginalPosition(entry.pos));
            }
            if (macroHasReadyAttribute(entry)) {
                throw VulException("SERVICE declaration in INTERFACE cannot specify ready=<condition> at " +
                                   context.getOriginalPosition(entry.pos));
            }
            out.service_names.insert(trim(entry.args[0]));
        }
    }
    out.entries = std::move(interface_entries);
    return out;
}

void validateInterfaceImplementationEntries(
    const vector<MacroEntry> &entries,
    const unordered_set<string> &interface_services,
    const VCPPModuleContext &context,
    const string &scope_name
) {
    for (const auto &entry : entries) {
        if (entry.name == "INTERFACE" || entry.name == "USE_VERSION" || entry.name == "VERSION") {
            throw VulException(
                scope_name + " cannot contain top-level versioning macro '" +
                entry.name + "' at " + context.getOriginalPosition(entry.pos)
            );
        }
        if (entry.name == "PARAMETER" || entry.name == "REQUEST") {
            throw VulException(
                scope_name + " cannot contain '" + entry.name +
                "'; declare module interface ports and parameters in INTERFACE at " +
                context.getOriginalPosition(entry.pos)
            );
        }
        if (entry.name == "SERVICE") {
            if (!entry.has_body) {
                throw VulException("SERVICE in " + scope_name +
                                   " must provide an implementation block at " +
                                   context.getOriginalPosition(entry.pos));
            }
            if (entry.args.empty()) {
                throw VulException("SERVICE requires at least 1 argument at " + context.getOriginalPosition(entry.pos));
            }
            string serv_name = trim(entry.args[0]);
            if (interface_services.find(serv_name) == interface_services.end()) {
                throw VulException("SERVICE implementation '" + serv_name +
                                   "' in " + scope_name +
                                   " has no declaration in INTERFACE at " +
                                   context.getOriginalPosition(entry.pos));
            }
        }
    }
}

vector<MacroEntry> selectVersionedModuleEntries(
    const vector<MacroEntry> &top_entries,
    const VCPPModuleContext &context
) {
    size_t interface_count = 0;
    size_t interface_index = 0;
    size_t use_version_count = 0;
    size_t use_version_index = 0;
    string selected_version;
    for (size_t i = 0; i < top_entries.size(); ++i) {
        const auto &entry = top_entries[i];
        if (entry.name == "INTERFACE") {
            ++interface_count;
            interface_index = i;
            continue;
        }
        if (entry.name != "USE_VERSION") {
            continue;
        }
        ++use_version_count;
        use_version_index = i;
        if (entry.args.size() != 1) {
            throw VulException("USE_VERSION requires exactly 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        if (entry.has_body) {
            throw VulException("USE_VERSION must be a declaration without code block at " + context.getOriginalPosition(entry.pos));
        }
        selected_version = trim(entry.args[0]);
        if (selected_version.empty()) {
            throw VulException("USE_VERSION version name cannot be empty at " + context.getOriginalPosition(entry.pos));
        }
    }
    if (interface_count == 0 && use_version_count == 0) {
        return top_entries;
    }
    if (interface_count > 1) {
        throw VulException("Only one INTERFACE block is allowed in a module");
    }
    if (use_version_count > 1) {
        throw VulException("Only one USE_VERSION declaration is allowed in a module");
    }
    if (interface_count == 0) {
        throw VulException("A module with USE_VERSION must define INTERFACE() first");
    }
    if (interface_index != 0 || top_entries.empty() || top_entries[0].name != "INTERFACE") {
        throw VulException("INTERFACE must be the first top-level macro in a module when it is defined");
    }

    const MacroEntry &interface_entry = top_entries[0];
    InterfaceEntries interface_data = parseInterfaceEntries(interface_entry, context);

    if (use_version_count == 0) {
        vector<MacroEntry> default_entries;
        default_entries.reserve(top_entries.size() > 0 ? top_entries.size() - 1 : 0);
        for (size_t i = 1; i < top_entries.size(); ++i) {
            if (top_entries[i].name == "VERSION") {
                throw VulException("VERSION blocks require USE_VERSION; default implementation follows INTERFACE directly at " +
                                   context.getOriginalPosition(top_entries[i].pos));
            }
            default_entries.push_back(top_entries[i]);
        }
        validateInterfaceImplementationEntries(default_entries, interface_data.service_names, context, "default implementation");
        vector<MacroEntry> selected_entries;
        selected_entries.reserve(interface_data.entries.size() + default_entries.size());
        selected_entries.insert(selected_entries.end(), interface_data.entries.begin(), interface_data.entries.end());
        selected_entries.insert(selected_entries.end(), default_entries.begin(), default_entries.end());
        return selected_entries;
    }

    if (use_version_index != 1) {
        throw VulException("USE_VERSION must appear immediately after INTERFACE() at " +
                           context.getOriginalPosition(top_entries[use_version_index].pos));
    }

    const MacroEntry *selected_version_entry = nullptr;
    unordered_set<string> version_names;
    for (size_t i = 2; i < top_entries.size(); ++i) {
        const auto &entry = top_entries[i];
        if (entry.name != "VERSION") {
            throw VulException(
                "A versioned module may only contain VERSION blocks after USE_VERSION; got '" +
                entry.name + "' at " + context.getOriginalPosition(entry.pos)
            );
        }
        if (entry.args.size() != 1) {
            throw VulException("VERSION requires exactly 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        if (!entry.has_body) {
            throw VulException("VERSION must use a code block at " + context.getOriginalPosition(entry.pos));
        }
        string version_name = trim(entry.args[0]);
        if (version_name.empty()) {
            throw VulException("VERSION name cannot be empty at " + context.getOriginalPosition(entry.pos));
        }
        if (!version_names.insert(version_name).second) {
            throw VulException("Duplicate VERSION '" + version_name + "' at " + context.getOriginalPosition(entry.pos));
        }
        if (version_name == selected_version) {
            selected_version_entry = &entry;
        }
    }
    if (selected_version_entry == nullptr) {
        throw VulException("USE_VERSION selects VERSION '" + selected_version + "', but no such VERSION block exists");
    }

    vector<MacroEntry> version_entries = parseNestedMacroEntriesPreservingPositions(*selected_version_entry);
    validateInterfaceImplementationEntries(version_entries, interface_data.service_names, context, "VERSION block");

    vector<MacroEntry> selected_entries;
    selected_entries.reserve(interface_data.entries.size() + version_entries.size());
    selected_entries.insert(selected_entries.end(), interface_data.entries.begin(), interface_data.entries.end());
    selected_entries.insert(selected_entries.end(), version_entries.begin(), version_entries.end());
    return selected_entries;
}
