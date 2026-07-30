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

#include "context.hpp"

#include "../stringop.hpp"

using namespace stringop;

string VCPPModuleContext::getOriginalPosition(const cppparse::LinePosition &pos) const {
    if (pos.line < 0 || pos.line >= static_cast<int32_t>(line_mapping.size())) {
        return "Line " + std::to_string(pos.line + 1) + ":" + std::to_string(pos.column + 1);
    }
    return "Line " + std::to_string(line_mapping[pos.line]) + ":" + std::to_string(pos.column + 1);
}

VulDebugLoc VCPPModuleContext::toDebugLoc(const cppparse::LinePosition &pos) const {
    VulDebugLoc loc;
    loc.file = temp.filepath;
    if (pos.line < 0 || pos.line >= static_cast<int32_t>(line_mapping.size())) {
        loc.line = static_cast<uint32_t>(pos.line + 1);
    } else {
        loc.line = line_mapping[pos.line] + 1;
    }
    loc.column = static_cast<uint32_t>(pos.column + 1);
    return loc;
}

VulDebugLocs VCPPModuleContext::bodyDebugLocs(const cppparse::MacroEntry &entry) const {
    VulDebugLocs locs;
    locs.reserve(entry.body.size());
    for (size_t i = 0; i < entry.body.size(); ++i) {
        if (i < entry.body_pos.size()) {
            locs.push_back(toDebugLoc(entry.body_pos[i]));
        } else {
            locs.push_back(toDebugLoc(entry.pos));
        }
    }
    return locs;
}

string VCPPModuleContext::macroPosition(const cppparse::MacroEntry &entry) const {
    return getOriginalPosition(entry.pos);
}

void VCPPModuleContext::declareName(
    const string &raw_name,
    const string &kind,
    const cppparse::MacroEntry &entry
) {
    const string declared_name = trim(raw_name);
    const string position = macroPosition(entry);
    VulErrorContextGuard _err{
        "declaring " + kind + " name '" +
        (declared_name.empty() ? string("<empty>") : declared_name) +
        "' at " + position
    };
    if (declared_name.empty()) {
        throw VulException(kind + " name cannot be empty at " + position);
    }
    if (is_global_header) {
        if (global_names == nullptr) {
            throw VulException("Internal error: global header scope is not available");
        }
        auto iter = global_names->find(declared_name);
        if (iter != global_names->end()) {
            throw VulException(
                "Global header name redefinition: '" + declared_name +
                "' was already defined as " + iter->second +
                ", cannot redefine as " + kind + " at " + position
            );
        }
        (*global_names)[declared_name] = kind;
        return;
    }
    auto local_iter = local_names.find(declared_name);
    if (local_iter != local_names.end()) {
        throw VulException(
            "Module scope name redefinition: '" + declared_name +
            "' was already defined as " + local_iter->second +
            ", cannot redefine as " + kind + " at " + position
        );
    }
    if (global_names != nullptr) {
        auto global_iter = global_names->find(declared_name);
        if (global_iter != global_names->end()) {
            throw VulException(
                "Module scope name conflicts with global header name: '" + declared_name +
                "' was already defined globally as " + global_iter->second +
                ", cannot define as " + kind + " at " + position
            );
        }
    }
    local_names[declared_name] = kind;
}

void VCPPModuleContext::declareServiceName(
    const string &raw_name,
    bool is_declaration,
    const cppparse::MacroEntry &entry
) {
    const string declared_name = trim(raw_name);
    const string position = macroPosition(entry);
    const string kind = is_declaration ? "SERVICE declaration" : "SERVICE implementation";
    VulErrorContextGuard _err{
        "declaring " + kind + " name '" +
        (declared_name.empty() ? string("<empty>") : declared_name) +
        "' at " + position
    };
    if (declared_name.empty()) {
        throw VulException(kind + " name cannot be empty at " + position);
    }
    if (is_global_header) {
        throw VulException("SERVICE is not allowed in global header scope at " + position);
    }
    auto local_iter = local_names.find(declared_name);
    if (local_iter != local_names.end() && local_iter->second != "SERVICE") {
        throw VulException(
            "Module scope name redefinition: '" + declared_name +
            "' was already defined as " + local_iter->second +
            ", cannot redefine as " + kind + " at " + position
        );
    }
    if (local_iter == local_names.end() && global_names != nullptr) {
        auto global_iter = global_names->find(declared_name);
        if (global_iter != global_names->end()) {
            throw VulException(
                "Module scope name conflicts with global header name: '" + declared_name +
                "' was already defined globally as " + global_iter->second +
                ", cannot define as " + kind + " at " + position
            );
        }
    }

    ServiceNameState &state = service_names[declared_name];
    if (is_declaration) {
        if (state.has_declaration) {
            throw VulException("Duplicate SERVICE declaration for '" + declared_name + "' at " + position);
        }
        if (state.has_implementation) {
            throw VulException("SERVICE declaration for '" + declared_name + "' must appear before its implementation at " + position);
        }
        state.has_declaration = true;
    } else {
        if (state.has_implementation) {
            throw VulException("Duplicate SERVICE implementation for '" + declared_name + "' at " + position);
        }
        state.has_implementation = true;
    }
    local_names[declared_name] = "SERVICE";
}

string VCPPTestContext::getOriginalPosition(const cppparse::LinePosition &pos) const {
    if (pos.line < 0 || pos.line >= static_cast<int32_t>(line_mapping.size())) {
        return "Line " + std::to_string(pos.line + 1) + ":" + std::to_string(pos.column + 1);
    }
    return "Line " + std::to_string(line_mapping[pos.line]) + ":" + std::to_string(pos.column + 1);
}

VulDebugLoc VCPPTestContext::toDebugLoc(const cppparse::LinePosition &pos) const {
    VulDebugLoc loc;
    loc.file = filepath;
    if (pos.line < 0 || pos.line >= static_cast<int32_t>(line_mapping.size())) {
        loc.line = static_cast<uint32_t>(pos.line + 1);
    } else {
        loc.line = line_mapping[pos.line] + 1;
    }
    loc.column = static_cast<uint32_t>(pos.column + 1);
    return loc;
}

VulDebugLocs VCPPTestContext::bodyDebugLocs(const cppparse::MacroEntry &entry) const {
    VulDebugLocs locs;
    locs.reserve(entry.body.size());
    for (size_t i = 0; i < entry.body.size(); ++i) {
        if (i < entry.body_pos.size()) {
            locs.push_back(toDebugLoc(entry.body_pos[i]));
        } else {
            locs.push_back(toDebugLoc(entry.pos));
        }
    }
    return locs;
}
