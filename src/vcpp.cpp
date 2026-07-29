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

#include "vcpp.hpp"
#include "project.h"
#include "configexpr.hpp"
#include "cppparse.hpp"
#include "stringop.hpp"
#include "toposort.hpp"
#include "vullib.hpp"
#include "validate.hpp"

using namespace cppparse;
using namespace stringop;

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <assert.h>
#include <optional>
#include <algorithm>
#include <deque>
#include <unordered_map>
#include <unordered_set>

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
    std::vector<uint32_t> line_mapping; // new line number -> original line number (1-based)
    inline string getOriginalPosition(const LinePosition &pos) const {
        if (pos.line < 0 || pos.line >= line_mapping.size()) {
            return "Line " + std::to_string(pos.line + 1) + ":" + std::to_string(pos.column + 1);
        }
        return "Line " + std::to_string(line_mapping[pos.line]) + ":" + std::to_string(pos.column + 1);
    }
    inline VulDebugLoc toDebugLoc(const LinePosition &pos) const {
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
    inline VulDebugLocs bodyDebugLocs(const MacroEntry &entry) const {
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
    inline string macroPosition(const MacroEntry &entry) const {
        return getOriginalPosition(entry.pos);
    }
    void declareName(const string &raw_name, const string &kind, const MacroEntry &entry) {
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
    void declareServiceName(const string &raw_name, bool is_declaration, const MacroEntry &entry) {
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
};

class VCPPModuleHandler {
public:
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) = 0;
    virtual string name() const = 0;
};

class VCPPModuleHandlerRegistry {
public:
    static VCPPModuleHandlerRegistry& instance() {
        static VCPPModuleHandlerRegistry registry;
        return registry;
    }

    bool register_handler(std::unique_ptr<VCPPModuleHandler> handler) {
        // For simplicity, we assume each handler handles a unique macro name, which is determined by the handler's dynamic type name.
        std::string handler_name = handler->name();
        if (handlers_.count(handler_name) > 0) {
            return false; // handler for this macro already exists
        }
        handlers_[handler_name] = std::move(handler);
        return true;
    }

    void run(VCPPModuleContext &context, const MacroEntry &entry) {
        std::string handler_name = entry.name; // assume macro name is the same as handler's dynamic type name for simplicity
        if (handlers_.count(handler_name) == 0) {
            throw VulException("No handler registered for macro: " + entry.name + " at " + context.getOriginalPosition(entry.pos));
        }
        handlers_[handler_name]->run(context, entry);
    }

private:
    VCPPModuleHandlerRegistry() = default;
    std::unordered_map<std::string, std::unique_ptr<VCPPModuleHandler>> handlers_;
};

template <typename T>
class VCPPModuleAutoRegisterHandler {
public:
    explicit VCPPModuleAutoRegisterHandler() {
        VCPPModuleHandlerRegistry::instance().register_handler(std::make_unique<T>());
    }
};

static inline std::optional<pair<string, string>> parseKeyValueArg(const string &raw_arg) {
    const size_t split_pos = raw_arg.find('=');
    if (split_pos == string::npos) {
        return std::nullopt;
    }
    string key = trim(raw_arg.substr(0, split_pos));
    string value = trim(raw_arg.substr(split_pos + 1));
    if (key.empty()) {
        return std::nullopt;
    }
    return pair<string, string>{std::move(key), std::move(value)};
}

static vector<string> splitTopLevelCsv(const string &raw) {
    string text = trim(raw);
    if (text.size() >= 2 &&
        ((text.front() == '(' && text.back() == ')') ||
         (text.front() == '[' && text.back() == ']'))) {
        text = trim(text.substr(1, text.size() - 2));
    }

    vector<string> out;
    size_t begin = 0;
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;
    int angle_depth = 0;
    bool in_string = false;
    bool in_char = false;
    bool escape = false;

    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (in_string || in_char) {
            if (escape) {
                escape = false;
            } else if (c == '\\') {
                escape = true;
            } else if (in_string && c == '"') {
                in_string = false;
            } else if (in_char && c == '\'') {
                in_char = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
            continue;
        }
        if (c == '\'') {
            in_char = true;
            continue;
        }
        if (c == '(') ++paren_depth;
        else if (c == ')' && paren_depth > 0) --paren_depth;
        else if (c == '[') ++bracket_depth;
        else if (c == ']' && bracket_depth > 0) --bracket_depth;
        else if (c == '{') ++brace_depth;
        else if (c == '}' && brace_depth > 0) --brace_depth;
        else if (c == '<') ++angle_depth;
        else if (c == '>' && angle_depth > 0) --angle_depth;
        else if (c == ',' && paren_depth == 0 && bracket_depth == 0 &&
                 brace_depth == 0 && angle_depth == 0) {
            string item = trim(text.substr(begin, i - begin));
            if (!item.empty()) out.push_back(std::move(item));
            begin = i + 1;
        }
    }
    string last = trim(text.substr(begin));
    if (!last.empty()) out.push_back(std::move(last));
    return out;
}

static inline void appendDimList(vector<string> &dims, const string &raw_value) {
    for (auto &dim : splitTopLevelCsv(raw_value)) {
        dim = trim(dim);
        if (dim.empty()) {
            throw VulException("empty dimension expression");
        }
        dims.push_back(std::move(dim));
    }
}

static inline bool parseBoolAttribute(const string &key, const string &value) {
    const string v = trim(value);
    if (v == "1" || v == "true" || v == "TRUE") return true;
    if (v == "0" || v == "false" || v == "FALSE") return false;
    throw VulException("attribute '" + key + "' expects 0/1 or true/false, got '" + value + "'");
}

static inline void ensureAttrValue(const string &macro_name, const string &key, const string &value) {
    if (trim(value).empty()) {
        throw VulException(macro_name + " attribute '" + key + "' cannot be empty");
    }
}

static inline bool isAttrKey(const std::optional<pair<string, string>> &attr, const string &key) {
    return attr && attr->first == key;
}

static inline pair<string, string> parseParameterOverrideArg(
    const string &raw_arg,
    const string &macro_name
) {
    auto attr = parseKeyValueArg(raw_arg);
    if (!attr) {
        throw VulException(macro_name + " parameter override expects name=value or PARAM(name)=value, got '" + raw_arg + "'");
    }
    string param_name = trim(attr->first);
    string param_value = trim(attr->second);
    if (param_name.size() > 7 && param_name.rfind("PARAM(", 0) == 0 && param_name.back() == ')') {
        param_name = trim(param_name.substr(6, param_name.size() - 7));
    }
    if (param_name.empty() || param_value.empty()) {
        throw VulException(macro_name + " has invalid parameter override '" + raw_arg + "'");
    }
    return {std::move(param_name), std::move(param_value)};
}

struct ReqServParseOptions {
    bool allow_handshake = false;
    bool allow_ready = false;
    bool allow_priority = false;
    bool allow_array = true;
    std::optional<bool> handshake;
    std::optional<string> ready;
    std::optional<string> priority;
};

static inline void setUniqueStringAttr(
    std::optional<string> &target,
    const string &macro_name,
    const string &key,
    const string &value
) {
    ensureAttrValue(macro_name, key, value);
    if (target.has_value()) {
        throw VulException(macro_name + " attribute '" + key + "' is specified more than once");
    }
    target = trim(value);
}

static inline bool parseReqServAttribute(
    VulTempReqServBase &reqserv,
    ReqServParseOptions &options,
    const string &macro_name,
    const string &raw_arg
) {
    auto attr = parseKeyValueArg(raw_arg);
    if (!attr) return false;
    const string &key = attr->first;
    const string &value = attr->second;

    if (key == "array") {
        if (!options.allow_array) {
            throw VulException(macro_name + " does not support attribute 'array'");
        }
        ensureAttrValue(macro_name, key, value);
        if (!reqserv.array_size.empty()) {
            throw VulException(macro_name + " array size is specified more than once");
        }
        reqserv.array_size = trim(value);
        return true;
    }
    if (key == "handshake") {
        if (!options.allow_handshake) {
            throw VulException(macro_name + " does not support attribute 'handshake'");
        }
        if (options.handshake.has_value()) {
            throw VulException(macro_name + " attribute 'handshake' is specified more than once");
        }
        options.handshake = parseBoolAttribute(key, value);
        return true;
    }
    if (key == "ready") {
        if (!options.allow_ready) {
            throw VulException(macro_name + " does not support attribute 'ready'");
        }
        setUniqueStringAttr(options.ready, macro_name, key, value);
        return true;
    }
    if (key == "priority") {
        if (!options.allow_priority) {
            throw VulException(macro_name + " does not support attribute 'priority'");
        }
        setUniqueStringAttr(options.priority, macro_name, key, value);
        return true;
    }

    throw VulException(macro_name + " has unknown attribute '" + key + "'");
}


class VCPPModuleCONFIG : public VCPPModuleHandler {
public:
    virtual string name() const { return "CONFIG"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 2) {
            throw VulException("CONFIG requires exactly 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempConfig config;
        config.name = entry.args[0];
        config.value = entry.args[1];
        context.declareName(config.name, "CONFIG", entry);
        context.temp.configs.push_back(std::move(config));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleCONFIG> _auto_register_CONFIG_handler;

class VCPPModulePARAMETER : public VCPPModuleHandler {
public:
    virtual string name() const { return "PARAMETER"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 2) {
            throw VulException("PARAMETER requires exactly 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempConfig param;
        param.name = entry.args[0];
        param.value = entry.args[1];
        context.declareName(param.name, "PARAMETER", entry);
        context.temp.params.push_back(std::move(param));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModulePARAMETER> _auto_register_PARAMETER_handler;

class VCPPModuleALIAS : public VCPPModuleHandler {
public:
    virtual string name() const { return "ALIAS"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 2) {
            throw VulException("ALIAS requires at least 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempBundle bundle;
        bundle.name = entry.args[0];
        bundle.is_alias = true;
        context.declareName(bundle.name, "ALIAS", entry);
        VulTempBundleMember alias_member;
        alias_member.name = "target";
        alias_member.type = entry.args[1];
        for (size_t i = 2; i < entry.args.size(); ++i) {
            auto attr = parseKeyValueArg(entry.args[i]);
            if (isAttrKey(attr, "dims")) {
                appendDimList(alias_member.dims, attr->second);
            } else if (isAttrKey(attr, "dim")) {
                appendDimList(alias_member.dims, attr->second);
            } else if (attr) {
                throw VulException("ALIAS has unknown attribute '" + attr->first + "' at " + context.getOriginalPosition(entry.pos));
            } else {
                alias_member.dims.push_back(entry.args[i]);
            }
        }
        bundle.members.push_back(std::move(alias_member));
        context.temp.bundles.push_back(std::move(bundle));
    }
};

class VCPPModuleALIAS_ARRAY1 : public VCPPModuleHandler {
public:
    virtual string name() const { return "ALIAS_ARRAY1"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 3) {
            throw VulException("ALIAS_ARRAY1 requires exactly 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempBundle bundle;
        bundle.name = entry.args[0];
        bundle.is_alias = true;
        context.declareName(bundle.name, "ALIAS_ARRAY1", entry);
        VulTempBundleMember alias_member;
        alias_member.name = "target";
        alias_member.type = entry.args[1];
        for (size_t i = 2; i < entry.args.size(); ++i) {
            alias_member.dims.push_back(entry.args[i]);
        }
        bundle.members.push_back(std::move(alias_member));
        context.temp.bundles.push_back(std::move(bundle));
    }
};

class VCPPModuleALIAS_ARRAY2 : public VCPPModuleHandler {
public:
    virtual string name() const { return "ALIAS_ARRAY2"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 4) {
            throw VulException("ALIAS_ARRAY2 requires exactly 4 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempBundle bundle;
        bundle.name = entry.args[0];
        bundle.is_alias = true;
        context.declareName(bundle.name, "ALIAS_ARRAY2", entry);
        VulTempBundleMember alias_member;
        alias_member.name = "target";
        alias_member.type = entry.args[1];
        for (size_t i = 2; i < entry.args.size(); ++i) {
            alias_member.dims.push_back(entry.args[i]);
        }
        bundle.members.push_back(std::move(alias_member));
        context.temp.bundles.push_back(std::move(bundle));
    }
};

static VCPPModuleAutoRegisterHandler<VCPPModuleALIAS> _auto_register_ALIAS_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleALIAS_ARRAY1> _auto_register_ALIAS_ARRAY1_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleALIAS_ARRAY2> _auto_register_ALIAS_ARRAY2_handler;

class VCPPModuleSTRUCT : public VCPPModuleHandler {
public:
    virtual string name() const { return "STRUCT"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 1) {
            throw VulException("STRUCT requires exactly 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        VulErrorContextGuard _err{"Processing STRUCT '" + entry.args[0] + "' at " + context.getOriginalPosition(entry.pos)};
        VulTempBundle bundle;
        bundle.name = entry.args[0];
        bundle.is_alias = false;
        context.declareName(bundle.name, "STRUCT", entry);
        string body;
        for (const auto line : entry.body) {
            body += trim(line);
        }
        vector<string> member_strs = split(body, ';');
        for (const auto &member_str : member_strs) {
            if (member_str.empty()) continue;
            VulTempBundleMember member = parseMemberDeclaration(member_str);
            bundle.members.push_back(std::move(member));
        }
        context.temp.bundles.push_back(std::move(bundle));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleSTRUCT> _auto_register_STRUCT_handler;

class VCPPModuleENUM : public VCPPModuleHandler {
public:
    virtual string name() const { return "ENUM"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 1) {
            throw VulException("ENUM requires exactly 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        VulErrorContextGuard _err{"Processing ENUM '" + entry.args[0] + "' at " + context.getOriginalPosition(entry.pos)};
        VulTempBundle bundle;
        bundle.name = entry.args[0];
        bundle.is_alias = false;
        context.declareName(bundle.name, "ENUM", entry);

        string body;
        for (const auto &line : entry.body) {
            body += trim(line);
        }
        vector<string> member_strs = split(body, ',');
        for (const auto &member_str_raw : member_strs) {
            string member_str = trim(member_str_raw);
            if (member_str.empty()) continue;
            VulTempEnumMember member;
            size_t eq_pos = member_str.find('=');
            if (eq_pos == string::npos) {
                member.name = trim(member_str);
                member.value = "";
            } else {
                member.name = trim(member_str.substr(0, eq_pos));
                member.value = trim(member_str.substr(eq_pos + 1));
            }
            if (member.name.empty()) {
                throw VulException("ENUM member name cannot be empty");
            }
            bundle.enum_members.push_back(std::move(member));
        }
        context.temp.bundles.push_back(std::move(bundle));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleENUM> _auto_register_ENUM_handler;

class VCPPModuleREGISTER : public VCPPModuleHandler {
public:
    virtual string name() const { return "REGISTER"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 2) {
            throw VulException("REGISTER requires at least 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempRegister reg;
        reg.name = entry.args[0];
        reg.type = entry.args[1];
        context.declareName(reg.name, "REGISTER", entry);
        bool saw_positional_port = false;
        if (entry.args.size() >= 3) {
            for (size_t i = 2; i < entry.args.size(); ++i) {
                auto attr = parseKeyValueArg(entry.args[i]);
                if (isAttrKey(attr, "ports") || isAttrKey(attr, "portnum")) {
                    if (!reg.portnum.empty()) {
                        throw VulException("REGISTER port count is specified more than once at " + context.getOriginalPosition(entry.pos));
                    }
                    ensureAttrValue("REGISTER", attr->first, attr->second);
                    reg.portnum = trim(attr->second);
                } else if (isAttrKey(attr, "dims") || isAttrKey(attr, "dim")) {
                    appendDimList(reg.dims, attr->second);
                } else if (attr) {
                    throw VulException("REGISTER has unknown attribute '" + attr->first + "' at " + context.getOriginalPosition(entry.pos));
                } else if (!saw_positional_port && reg.portnum.empty()) {
                    reg.portnum = entry.args[i];
                    saw_positional_port = true;
                } else {
                    reg.dims.push_back(entry.args[i]);
                }
            }
        }
        reg.reset_codelines = entry.body;
        reg.reset_codelines_debug = context.bodyDebugLocs(entry);
        context.temp.registers.push_back(std::move(reg));
    }
};
class VCPPModuleREGISTER_MUL : public VCPPModuleHandler {
public:
    virtual string name() const { return "REGISTER_MUL"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 3) {
            throw VulException("REGISTER_MUL requires at least 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempRegister reg;
        reg.name = entry.args[0];
        reg.type = entry.args[1];
        context.declareName(reg.name, "REGISTER_MUL", entry);
        bool saw_port = false;
        for (size_t i = 3; i < entry.args.size(); ++i) {
            auto attr = parseKeyValueArg(entry.args[i]);
            if (isAttrKey(attr, "dims") || isAttrKey(attr, "dim")) {
                appendDimList(reg.dims, attr->second);
            } else if (attr) {
                throw VulException("REGISTER_MUL has unknown attribute '" + attr->first + "' at " + context.getOriginalPosition(entry.pos));
            } else {
                reg.dims.push_back(entry.args[i]);
            }
        }
        auto port_attr = parseKeyValueArg(entry.args[2]);
        if (isAttrKey(port_attr, "ports") || isAttrKey(port_attr, "portnum")) {
            ensureAttrValue("REGISTER_MUL", port_attr->first, port_attr->second);
            reg.portnum = trim(port_attr->second);
            saw_port = true;
        } else if (port_attr) {
            throw VulException("REGISTER_MUL has unknown attribute '" + port_attr->first + "' at " + context.getOriginalPosition(entry.pos));
        }
        if (!saw_port) {
            reg.portnum = entry.args[2];
        }
        reg.reset_codelines = entry.body;
        reg.reset_codelines_debug = context.bodyDebugLocs(entry);
        context.temp.registers.push_back(std::move(reg));
    }
};
class VCPPModuleREGISTER_ARRAY1 : public VCPPModuleHandler {
public:
    virtual string name() const { return "REGISTER_ARRAY1"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 4) {
            throw VulException("REGISTER_ARRAY1 requires exactly 4 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempRegister reg;
        reg.name = entry.args[0];
        reg.type = entry.args[1];
        context.declareName(reg.name, "REGISTER_ARRAY1", entry);
        reg.portnum = entry.args[3];
        reg.dims.push_back(entry.args[2]);
        reg.reset_codelines = entry.body;
        reg.reset_codelines_debug = context.bodyDebugLocs(entry);
        context.temp.registers.push_back(std::move(reg));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleREGISTER> _auto_register_REGISTER_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleREGISTER_MUL> _auto_register_REGISTER_MUL_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleREGISTER_ARRAY1> _auto_register_REGISTER_ARRAY1_handler;

class VCPPModuleWIRE : public VCPPModuleHandler {
public:
    virtual string name() const { return "WIRE"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 2) {
            throw VulException("WIRE requires exactly 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempWire wire;
        wire.name = entry.args[0];
        wire.type = entry.args[1];
        context.declareName(wire.name, "WIRE", entry);
        wire.reset_codelines = entry.body;
        wire.reset_codelines_debug = context.bodyDebugLocs(entry);
        context.temp.wires.push_back(std::move(wire));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleWIRE> _auto_register_WIRE_handler;

static inline void parseReqArgsAndRets(
    const vector<string> &macro_args,
    size_t begin_idx,
    VulTempReqServBase &reqserv,
    ReqServParseOptions *options = nullptr,
    const string &macro_name = "REQUEST/SERVICE"
) {
    size_t i = begin_idx;
    for (; i < macro_args.size(); ++i) {
        const string &arg_decl_raw = macro_args[i];
        const string arg_decl = trim(arg_decl_raw);
        if (arg_decl.size() > 7 && arg_decl.rfind("ARRAY(", 0) == 0 && arg_decl.back() == ')') {
            if (!reqserv.array_size.empty()) {
                throw VulException(macro_name + " array size is specified more than once");
            }
            reqserv.array_size = trim(arg_decl.substr(6, arg_decl.size() - 7));
            if (reqserv.array_size.empty()) {
                throw VulException("empty ARRAY() expression");
            }
            continue;
        }
        if (options != nullptr && parseReqServAttribute(reqserv, *options, macro_name, arg_decl)) {
            continue;
        }
        const size_t split_pos = arg_decl.find_last_of(" \t\r\n");
        if (split_pos == string::npos) {
            throw VulException("invalid argument declaration '" + arg_decl_raw + "'");
        }

        const string type_tag = trim(arg_decl.substr(0, split_pos));
        const string arg_name = trim(arg_decl.substr(split_pos + 1));
        if (type_tag.empty() || arg_name.empty()) {
            throw VulException("invalid argument declaration '" + arg_decl_raw + "'");
        }

        if (type_tag.size() > 5 && type_tag.rfind("ARG(", 0) == 0 && type_tag.back() == ')') {
            reqserv.args.emplace_back(type_tag.substr(4, type_tag.size() - 5), arg_name);
        } else if (type_tag.size() > 6 && type_tag.rfind("RESP(", 0) == 0 && type_tag.back() == ')') {
            reqserv.rets.emplace_back(type_tag.substr(5, type_tag.size() - 6), arg_name);
        } else {
            throw VulException("invalid argument type tag '" + type_tag + "'");
        }
    }
}

static inline string parsePathMacroArg(const string &raw_arg) {
    string arg = trim(raw_arg);
    if (arg.size() >= 2) {
        char first = arg.front();
        char last = arg.back();
        if ((first == '"' && last == '"') || (first == '<' && last == '>')) {
            return arg.substr(1, arg.size() - 2);
        }
    }
    return arg;
}

class VCPPModuleREQUEST : public VCPPModuleHandler {
public:
    virtual string name() const { return "REQUEST"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.empty()) {
            throw VulException("REQUEST requires at least 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        VulTempReq req;
        req.name = entry.args[0];
        req.has_handshake = false;
        context.declareName(req.name, "REQUEST", entry);
        VulErrorContextGuard _err{"Processing REQUEST '" + req.name + "' at " + context.getOriginalPosition(entry.pos)};
        ReqServParseOptions options;
        options.allow_handshake = true;
        options.allow_array = true;
        parseReqArgsAndRets(entry.args, 1, req, &options, "REQUEST");
        if (options.handshake.has_value()) {
            req.has_handshake = *options.handshake;
        }
        context.temp.requests.push_back(std::move(req));
    }
};

class VCPPModuleREQUEST_READY : public VCPPModuleHandler {
public:
    virtual string name() const { return "REQUEST_READY"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.empty()) {
            throw VulException("REQUEST_READY requires at least 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        VulTempReq req;
        req.name = entry.args[0];
        req.has_handshake = true;
        context.declareName(req.name, "REQUEST_READY", entry);
        VulErrorContextGuard _err{"Processing REQUEST_READY '" + req.name + "' at " + context.getOriginalPosition(entry.pos)};
        ReqServParseOptions options;
        options.allow_array = true;
        parseReqArgsAndRets(entry.args, 1, req, &options, "REQUEST_READY");
        context.temp.requests.push_back(std::move(req));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleREQUEST> _auto_register_REQUEST_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleREQUEST_READY> _auto_register_REQUEST_READY_handler;

class VCPPModuleSERVICE : public VCPPModuleHandler {
public:
    virtual string name() const { return "SERVICE"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.empty()) {
            throw VulException("SERVICE requires at least 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        VulTempServ serv;
        serv.name = entry.args[0];
        serv.is_declaration = !entry.has_body;
        serv.has_handshake = false;
        context.declareServiceName(serv.name, serv.is_declaration, entry);
        serv.cond = "";
        serv.priority = "";
        serv.codelines = entry.body;
        serv.codelines_debug = context.bodyDebugLocs(entry);
        VulErrorContextGuard _err{
            string("Processing SERVICE ") + (serv.is_declaration ? "declaration" : "implementation") +
            " '" + serv.name + "' at " + context.getOriginalPosition(entry.pos)
        };
        ReqServParseOptions options;
        options.allow_handshake = true;
        options.allow_ready = true;
        options.allow_priority = true;
        options.allow_array = true;
        parseReqArgsAndRets(entry.args, 1, serv, &options, "SERVICE");
        if (options.handshake.has_value()) {
            serv.has_handshake = *options.handshake;
        }
        if (options.ready.has_value()) {
            if (serv.is_declaration) {
                throw VulException("SERVICE declaration cannot specify ready=<condition> at " + context.getOriginalPosition(entry.pos));
            }
            if (options.handshake.has_value() && !*options.handshake) {
                throw VulException("SERVICE ready=<condition> conflicts with handshake=0 at " + context.getOriginalPosition(entry.pos));
            }
            serv.cond = *options.ready;
            serv.cond_debug = context.toDebugLoc(entry.pos);
            serv.has_handshake = true;
        }
        if (!serv.is_declaration && serv.has_handshake && serv.cond.empty()) {
            throw VulException("SERVICE with handshake=1 requires ready=<condition> at " + context.getOriginalPosition(entry.pos));
        }
        if (options.priority.has_value()) {
            serv.priority = *options.priority;
        }
        context.temp.services.push_back(std::move(serv));
    }
};

class VCPPModuleSERVICE_READY : public VCPPModuleHandler {
public:
    virtual string name() const { return "SERVICE_READY"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 2) {
            throw VulException("SERVICE_READY requires at least 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempServ serv;
        serv.name = entry.args[0];
        serv.has_handshake = true;
        context.declareName(serv.name, "SERVICE_READY", entry);
        serv.cond = entry.args[1];
        serv.cond_debug = context.toDebugLoc(entry.pos);
        serv.priority = "";
        serv.codelines = entry.body;
        serv.codelines_debug = context.bodyDebugLocs(entry);
        VulErrorContextGuard _err{"Processing SERVICE_READY '" + serv.name + "' at " + context.getOriginalPosition(entry.pos)};
        ReqServParseOptions options;
        options.allow_array = true;
        parseReqArgsAndRets(entry.args, 2, serv, &options, "SERVICE_READY");
        context.temp.services.push_back(std::move(serv));
    }
};

class VCPPModuleSERVICE_PRIO : public VCPPModuleHandler {
public:
    virtual string name() const { return "SERVICE_PRIO"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 2) {
            throw VulException("SERVICE_PRIO requires at least 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempServ serv;
        serv.name = entry.args[0];
        serv.has_handshake = false;
        context.declareName(serv.name, "SERVICE_PRIO", entry);
        serv.cond = "";
        serv.priority = entry.args[1];
        serv.codelines = entry.body;
        serv.codelines_debug = context.bodyDebugLocs(entry);
        VulErrorContextGuard _err{"Processing SERVICE_PRIO '" + serv.name + "' at " + context.getOriginalPosition(entry.pos)};
        ReqServParseOptions options;
        options.allow_array = true;
        parseReqArgsAndRets(entry.args, 2, serv, &options, "SERVICE_PRIO");
        context.temp.services.push_back(std::move(serv));
    }
};

class VCPPModuleSERVICE_PRIO_READY : public VCPPModuleHandler {
public:
    virtual string name() const { return "SERVICE_PRIO_READY"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 3) {
            throw VulException("SERVICE_PRIO_READY requires at least 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempServ serv;
        serv.name = entry.args[0];
        serv.has_handshake = true;
        context.declareName(serv.name, "SERVICE_PRIO_READY", entry);
        serv.priority = entry.args[1];
        serv.cond = entry.args[2];
        serv.cond_debug = context.toDebugLoc(entry.pos);
        serv.codelines = entry.body;
        serv.codelines_debug = context.bodyDebugLocs(entry);
        VulErrorContextGuard _err{"Processing SERVICE_PRIO_READY '" + serv.name + "' at " + context.getOriginalPosition(entry.pos)};
        ReqServParseOptions options;
        options.allow_array = true;
        parseReqArgsAndRets(entry.args, 3, serv, &options, "SERVICE_PRIO_READY");
        context.temp.services.push_back(std::move(serv));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleSERVICE> _auto_register_SERVICE_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleSERVICE_READY> _auto_register_SERVICE_READY_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleSERVICE_PRIO> _auto_register_SERVICE_PRIO_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleSERVICE_PRIO_READY> _auto_register_SERVICE_PRIO_READY_handler;

class VCPPModuleQUERY : public VCPPModuleHandler {
public:
    virtual string name() const { return "QUERY"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 2) {
            throw VulException("QUERY requires exactly 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempQuery query;
        query.name = entry.args[0];
        query.ret_type = entry.args[1];
        context.declareName(query.name, "QUERY", entry);
        query.codelines = entry.body;
        query.codelines_debug = context.bodyDebugLocs(entry);
        VulErrorContextGuard _err{"Processing QUERY '" + query.name + "' at " + context.getOriginalPosition(entry.pos)};
        context.temp.queries.push_back(std::move(query));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleQUERY> _auto_register_QUERY_handler;

class VCPPModuleTICK_IMPL : public VCPPModuleHandler {
public:
    virtual string name() const { return "TICK_IMPL"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 0) {
            throw VulException("TICK_IMPL does not take any arguments at " + context.getOriginalPosition(entry.pos));
        }
        context.temp.tick_blocks.push_back(entry.body);
        context.temp.tick_blocks_debug.push_back(context.bodyDebugLocs(entry));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleTICK_IMPL> _auto_register_TICK_IMPL_handler;

class VCPPModuleCHILD_INSTANCE : public VCPPModuleHandler {
public:
    virtual string name() const { return "CHILD_INSTANCE"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 2) {
            throw VulException("CHILD_INSTANCE requires at least 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempInstance inst;
        inst.name = entry.args[1];
        inst.module_name = entry.args[0];
        context.declareName(inst.name, "CHILD_INSTANCE", entry);
        for (size_t i = 2; i < entry.args.size(); ++i) {
            const string &param_override_raw = entry.args[i];
            auto attr = parseKeyValueArg(param_override_raw);
            if (!attr) {
                throw VulException("invalid parameter override '" + param_override_raw + "' at " + context.getOriginalPosition(entry.pos));
            }
            if (attr->first == "dims" || attr->first == "dim") {
                appendDimList(inst.array_dims, attr->second);
                continue;
            }
            inst.parameter_overrides.push_back(parseParameterOverrideArg(param_override_raw, "CHILD_INSTANCE"));
        }
        context.temp.instances.push_back(std::move(inst));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleCHILD_INSTANCE> _auto_register_CHILD_INSTANCE_handler;

class VCPPModuleCHILD_INSTANCE_ARRAY1 : public VCPPModuleHandler {
public:
    virtual string name() const { return "CHILD_INSTANCE_ARRAY1"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 3) {
            throw VulException("CHILD_INSTANCE_ARRAY1 requires at least 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempInstance inst;
        inst.name = entry.args[1];
        inst.module_name = entry.args[0];
        context.declareName(inst.name, "CHILD_INSTANCE_ARRAY1", entry);
        inst.array_dims.push_back(entry.args[2]);
        for (size_t i = 3; i < entry.args.size(); ++i) {
            const string &param_override_raw = entry.args[i];
            const size_t split_pos = param_override_raw.find('=');
            if (split_pos == string::npos) {
                throw VulException("invalid parameter override '" + param_override_raw + "' at " + context.getOriginalPosition(entry.pos));
            }
            inst.parameter_overrides.push_back(parseParameterOverrideArg(param_override_raw, "CHILD_INSTANCE_ARRAY1"));
        }
        context.temp.instances.push_back(std::move(inst));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleCHILD_INSTANCE_ARRAY1> _auto_register_CHILD_INSTANCE_ARRAY1_handler;

class VCPPModuleCHILD_INSTANCE_ARRAY2 : public VCPPModuleHandler {
public:
    virtual string name() const { return "CHILD_INSTANCE_ARRAY2"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 4) {
            throw VulException("CHILD_INSTANCE_ARRAY2 requires at least 4 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempInstance inst;
        inst.name = entry.args[1];
        inst.module_name = entry.args[0];
        context.declareName(inst.name, "CHILD_INSTANCE_ARRAY2", entry);
        inst.array_dims.push_back(entry.args[2]);
        inst.array_dims.push_back(entry.args[3]);
        for (size_t i = 4; i < entry.args.size(); ++i) {
            const string &param_override_raw = entry.args[i];
            const size_t split_pos = param_override_raw.find('=');
            if (split_pos == string::npos) {
                throw VulException("invalid parameter override '" + param_override_raw + "' at " + context.getOriginalPosition(entry.pos));
            }
            inst.parameter_overrides.push_back(parseParameterOverrideArg(param_override_raw, "CHILD_INSTANCE_ARRAY2"));
        }
        context.temp.instances.push_back(std::move(inst));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleCHILD_INSTANCE_ARRAY2> _auto_register_CHILD_INSTANCE_ARRAY2_handler;

class VCPPModuleUSE_CHILD_SERVICE_PORT : public VCPPModuleHandler {
public:
    virtual string name() const { return "USE_CHILD_SERVICE_PORT"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 3) {
            throw VulException("USE_CHILD_SERVICE_PORT requires at least 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempChildServiceUse use;
        use.instance_expr = entry.args[0];
        use.service_name = entry.args[1];
        use.alias_name = entry.args[2];
        context.declareName(use.alias_name, "USE_CHILD_SERVICE_PORT", entry);
        context.temp.child_service_uses.push_back(std::move(use));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleUSE_CHILD_SERVICE_PORT> _auto_register_USE_CHILD_SERVICE_PORT_handler;

class VCPPModuleUSE_CHILD_SERVICE : public VCPPModuleHandler {
public:
    virtual string name() const { return "USE_CHILD_SERVICE"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 3) {
            throw VulException("USE_CHILD_SERVICE requires at least 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        string array_size;
        for (size_t i = 3; i < entry.args.size(); ++i) {
            auto attr = parseKeyValueArg(entry.args[i]);
            if (!attr) {
                continue;
            }
            if (attr->first != "array") {
                throw VulException("USE_CHILD_SERVICE has unknown attribute '" + attr->first + "' at " + context.getOriginalPosition(entry.pos));
            }
            ensureAttrValue("USE_CHILD_SERVICE", attr->first, attr->second);
            if (!array_size.empty()) {
                throw VulException("USE_CHILD_SERVICE array size is specified more than once at " + context.getOriginalPosition(entry.pos));
            }
            array_size = trim(attr->second);
        }
        VulTempChildServiceUse use;
        use.instance_expr = entry.args[0];
        use.service_name = entry.args[1];
        use.alias_name = entry.args[2];
        use.array_size = std::move(array_size);
        context.declareName(use.alias_name, "USE_CHILD_SERVICE", entry);
        context.temp.child_service_uses.push_back(std::move(use));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleUSE_CHILD_SERVICE> _auto_register_USE_CHILD_SERVICE_handler;

class VCPPModuleUSE_CHILD_QUERY : public VCPPModuleHandler {
public:
    virtual string name() const { return "USE_CHILD_QUERY"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 4) {
            throw VulException("USE_CHILD_QUERY requires at least 4 arguments at " + context.getOriginalPosition(entry.pos));
        }
        string array_size;
        for (size_t i = 4; i < entry.args.size(); ++i) {
            auto attr = parseKeyValueArg(entry.args[i]);
            if (!attr || attr->first != "array") {
                throw VulException("USE_CHILD_QUERY only supports optional array=<N> after rettype at " + context.getOriginalPosition(entry.pos));
            }
            ensureAttrValue("USE_CHILD_QUERY", attr->first, attr->second);
            if (!array_size.empty()) {
                throw VulException("USE_CHILD_QUERY array size is specified more than once at " + context.getOriginalPosition(entry.pos));
            }
            array_size = trim(attr->second);
        }
        VulTempChildQueryUse use;
        use.instance_expr = entry.args[0];
        use.query_name = entry.args[1];
        use.alias_name = entry.args[2];
        use.ret_type = entry.args[3];
        use.array_size = std::move(array_size);
        context.declareName(use.alias_name, "USE_CHILD_QUERY", entry);
        context.temp.child_query_uses.push_back(std::move(use));
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleUSE_CHILD_QUERY> _auto_register_USE_CHILD_QUERY_handler;

class VCPPModuleCONNECT_CR_CS : public VCPPModuleHandler {
public:
    virtual string name() const { return "CONNECT_CR_CS"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 4) {
            throw VulException("CONNECT_CR_CS requires exactly 4 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulReqServConnection conn;
        conn.req_instance = entry.args[0];
        conn.req_name = entry.args[1];
        conn.serv_instance = entry.args[2];
        conn.serv_name = entry.args[3];
        context.temp.req_connections.push_back(std::move(conn));
    }
};

class VCPPModuleCONNECT_CR_S : public VCPPModuleHandler {
public:
    virtual string name() const { return "CONNECT_CR_S"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 3) {
            throw VulException("CONNECT_CR_S requires exactly 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulReqServConnection conn;
        conn.req_instance = entry.args[0];
        conn.req_name = entry.args[1];
        conn.serv_instance = "";
        conn.serv_name = entry.args[2];
        context.temp.req_connections.push_back(std::move(conn));
    }
};

class VCPPModuleCONNECT_CR_R : public VCPPModuleHandler {
public:
    virtual string name() const { return "CONNECT_CR_R"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 3) {
            throw VulException("CONNECT_CR_R requires exactly 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulReqServConnection conn;
        conn.req_instance = entry.args[0];
        conn.req_name = entry.args[1];
        conn.serv_instance = "";
        conn.serv_name = entry.args[2];
        context.temp.req_connections.push_back(std::move(conn));
    }
};

class VCPPModuleCONNECT_S_CS : public VCPPModuleHandler {
public:
    virtual string name() const { return "CONNECT_S_CS"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 3) {
            throw VulException("CONNECT_S_CS requires exactly 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulReqServConnection conn;
        conn.req_instance = "";
        conn.req_name = entry.args[0];
        conn.serv_instance = entry.args[1];
        conn.serv_name = entry.args[2];
        context.temp.req_connections.push_back(std::move(conn));
    }
};

static VCPPModuleAutoRegisterHandler<VCPPModuleCONNECT_CR_CS> _auto_register_CONNECT_CR_CS_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleCONNECT_CR_S> _auto_register_CONNECT_CR_S_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleCONNECT_CR_R> _auto_register_CONNECT_CR_R_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleCONNECT_S_CS> _auto_register_CONNECT_S_CS_handler;

class VCPPModuleBRAM : public VCPPModuleHandler {
public:
    virtual string name() const { return "BRAM"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 3) {
            throw VulException("BRAM requires at least 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempBRAM bram;
        bram.name = entry.args[0];
        bram.data_type = entry.args[1];
        context.declareName(bram.name, "BRAM", entry);
        bram.addr_size = entry.args[2];
        bram.read_ports = "1";
        bram.write_ports = "1";
        bool mode_1rw = false;
        size_t positional_idx = 0;
        for (size_t i = 3; i < entry.args.size(); ++i) {
            auto attr = parseKeyValueArg(entry.args[i]);
            if (isAttrKey(attr, "read_ports") || isAttrKey(attr, "readports")) {
                ensureAttrValue("BRAM", attr->first, attr->second);
                bram.read_ports = trim(attr->second);
            } else if (isAttrKey(attr, "write_ports") || isAttrKey(attr, "writeports")) {
                ensureAttrValue("BRAM", attr->first, attr->second);
                bram.write_ports = trim(attr->second);
            } else if (isAttrKey(attr, "mode")) {
                ensureAttrValue("BRAM", attr->first, attr->second);
                const string mode = trim(attr->second);
                if (mode == "1rw") {
                    mode_1rw = true;
                } else if (mode == "generic") {
                    mode_1rw = false;
                } else {
                    throw VulException("BRAM mode must be 'generic' or '1rw' at " + context.getOriginalPosition(entry.pos));
                }
            } else if (attr) {
                throw VulException("BRAM has unknown attribute '" + attr->first + "' at " + context.getOriginalPosition(entry.pos));
            } else if (positional_idx == 0) {
                bram.read_ports = entry.args[i];
                ++positional_idx;
            } else if (positional_idx == 1) {
                bram.write_ports = entry.args[i];
                ++positional_idx;
            } else {
                throw VulException("BRAM has too many positional arguments at " + context.getOriginalPosition(entry.pos));
            }
        }
        if (mode_1rw) {
            bram.read_ports = "";
            bram.write_ports = "";
        }
        context.temp.brams.push_back(std::move(bram));
    }
};

class VCPPModuleBRAM_1RW : public VCPPModuleHandler {
public:
    virtual string name() const { return "BRAM_1RW"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 3) {
            throw VulException("BRAM_1RW requires exactly 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempBRAM bram;
        bram.name = entry.args[0];
        bram.data_type = entry.args[1];
        context.declareName(bram.name, "BRAM_1RW", entry);
        bram.addr_size = entry.args[2];
        bram.read_ports = "";
        bram.write_ports = "";
        context.temp.brams.push_back(std::move(bram));
    }
};

class VCPPModuleROM : public VCPPModuleHandler {
public:
    virtual string name() const { return "ROM"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 3) {
            throw VulException("ROM requires at least 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempDigitalROM rom;
        rom.name = entry.args[0];
        rom.data_width = entry.args[1];
        context.declareName(rom.name, "ROM", entry);
        rom.addr_size = entry.args[2];
        rom.read_ports = "1";
        size_t positional_idx = 0;
        for (size_t i = 3; i < entry.args.size(); ++i) {
            auto attr = parseKeyValueArg(entry.args[i]);
            if (isAttrKey(attr, "read_ports") || isAttrKey(attr, "readports")) {
                ensureAttrValue("ROM", attr->first, attr->second);
                rom.read_ports = trim(attr->second);
            } else if (isAttrKey(attr, "init") || isAttrKey(attr, "init_path")) {
                ensureAttrValue("ROM", attr->first, attr->second);
                rom.init_path = trim(attr->second);
            } else if (attr) {
                throw VulException("ROM has unknown attribute '" + attr->first + "' at " + context.getOriginalPosition(entry.pos));
            } else if (positional_idx == 0) {
                rom.read_ports = entry.args[i];
                ++positional_idx;
            } else if (positional_idx == 1) {
                rom.init_path = entry.args[i];
                ++positional_idx;
            } else {
                throw VulException("ROM has too many positional arguments at " + context.getOriginalPosition(entry.pos));
            }
        }
        if (rom.init_path.empty()) {
            throw VulException("ROM requires init=<path> or a positional init_path at " + context.getOriginalPosition(entry.pos));
        }
        context.temp.roms.push_back(std::move(rom));
    }
};

class VCPPModuleQUEUE : public VCPPModuleHandler {
public:
    virtual string name() const { return "QUEUE"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 3) {
            throw VulException("QUEUE requires at least 3 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempQueue queue;
        queue.name = entry.args[0];
        queue.type = entry.args[1];
        context.declareName(queue.name, "QUEUE", entry);
        queue.depth = entry.args[2];
        queue.enq_width = "1";
        queue.deq_width = "1";
        size_t positional_idx = 0;
        for (size_t i = 3; i < entry.args.size(); ++i) {
            auto attr = parseKeyValueArg(entry.args[i]);
            if (isAttrKey(attr, "enq_width") || isAttrKey(attr, "enqwidth")) {
                ensureAttrValue("QUEUE", attr->first, attr->second);
                queue.enq_width = trim(attr->second);
            } else if (isAttrKey(attr, "deq_width") || isAttrKey(attr, "deqwidth")) {
                ensureAttrValue("QUEUE", attr->first, attr->second);
                queue.deq_width = trim(attr->second);
            } else if (attr) {
                throw VulException("QUEUE has unknown attribute '" + attr->first + "' at " + context.getOriginalPosition(entry.pos));
            } else if (positional_idx == 0) {
                queue.enq_width = entry.args[i];
                ++positional_idx;
            } else if (positional_idx == 1) {
                queue.deq_width = entry.args[i];
                ++positional_idx;
            } else {
                throw VulException("QUEUE has too many positional arguments at " + context.getOriginalPosition(entry.pos));
            }
        }
        context.temp.queues.push_back(std::move(queue));
    }
};

class VCPPModuleQUEUE_MP : public VCPPModuleHandler {
public:
    virtual string name() const { return "QUEUE_MP"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 5) {
            throw VulException("QUEUE_MP requires exactly 5 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempQueue queue;
        queue.name = entry.args[0];
        queue.type = entry.args[1];
        context.declareName(queue.name, "QUEUE_MP", entry);
        queue.depth = entry.args[2];
        queue.enq_width = entry.args[3];
        queue.deq_width = entry.args[4];
        context.temp.queues.push_back(std::move(queue));
    }
};

static VCPPModuleAutoRegisterHandler<VCPPModuleBRAM> _auto_register_BRAM_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleBRAM_1RW> _auto_register_BRAM_1RW_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleROM> _auto_register_ROM_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleQUEUE> _auto_register_QUEUE_handler;
static VCPPModuleAutoRegisterHandler<VCPPModuleQUEUE_MP> _auto_register_QUEUE_MP_handler;

class VCPPModuleHELPER : public VCPPModuleHandler {
public:
    virtual string name() const { return "HELPER"; }
    virtual void run(VCPPModuleContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 0) {
            throw VulException("HELPER does not take any arguments at " + context.getOriginalPosition(entry.pos));
        }
        context.temp.helper_codes = entry.body;
        context.temp.helper_codes_debug = context.bodyDebugLocs(entry);
    }
};
static VCPPModuleAutoRegisterHandler<VCPPModuleHELPER> _auto_register_HELPER_handler;

static bool macroHasReadyAttribute(const MacroEntry &entry) {
    for (size_t i = 1; i < entry.args.size(); ++i) {
        auto attr = parseKeyValueArg(entry.args[i]);
        if (isAttrKey(attr, "ready")) {
            return true;
        }
    }
    return false;
}

static vector<MacroEntry> parseNestedMacroEntriesPreservingPositions(const MacroEntry &entry) {
    return findAllMacroEntriesPreservingLinePositions(entry.body, entry.body_pos);
}

static vector<MacroEntry> selectVersionedModuleEntries(
    const vector<MacroEntry> &top_entries,
    const VCPPModuleContext &context
) {
    size_t use_version_count = 0;
    size_t use_version_index = 0;
    string selected_version;
    for (size_t i = 0; i < top_entries.size(); ++i) {
        const auto &entry = top_entries[i];
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
    if (use_version_count == 0) {
        return top_entries;
    }
    if (use_version_count > 1) {
        throw VulException("Only one USE_VERSION declaration is allowed in a module");
    }
    if (top_entries.empty() || top_entries[0].name != "INTERFACE") {
        throw VulException("A versioned module must start with INTERFACE() before USE_VERSION/VERSION");
    }
    if (use_version_index != 1) {
        throw VulException("USE_VERSION must appear immediately after INTERFACE() at " +
                           context.getOriginalPosition(top_entries[use_version_index].pos));
    }

    const MacroEntry &interface_entry = top_entries[0];
    if (!interface_entry.args.empty()) {
        throw VulException("INTERFACE does not take any arguments at " + context.getOriginalPosition(interface_entry.pos));
    }
    if (!interface_entry.has_body) {
        throw VulException("INTERFACE must use a code block at " + context.getOriginalPosition(interface_entry.pos));
    }

    vector<MacroEntry> interface_entries = parseNestedMacroEntriesPreservingPositions(interface_entry);
    unordered_set<string> declared_services;
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
            declared_services.insert(trim(entry.args[0]));
        }
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
    for (const auto &entry : version_entries) {
        if (entry.name == "PARAMETER" || entry.name == "REQUEST") {
            throw VulException(
                "VERSION block cannot contain '" + entry.name +
                "'; declare module interface ports and parameters in INTERFACE at " +
                context.getOriginalPosition(entry.pos)
            );
        }
        if (entry.name == "SERVICE") {
            if (!entry.has_body) {
                throw VulException("SERVICE in VERSION must provide an implementation block at " +
                                   context.getOriginalPosition(entry.pos));
            }
            if (entry.args.empty()) {
                throw VulException("SERVICE requires at least 1 argument at " + context.getOriginalPosition(entry.pos));
            }
            string serv_name = trim(entry.args[0]);
            if (declared_services.find(serv_name) == declared_services.end()) {
                throw VulException("SERVICE implementation '" + serv_name +
                                   "' in VERSION has no declaration in INTERFACE at " +
                                   context.getOriginalPosition(entry.pos));
            }
        }
    }

    vector<MacroEntry> selected_entries;
    selected_entries.reserve(interface_entries.size() + version_entries.size());
    selected_entries.insert(selected_entries.end(), interface_entries.begin(), interface_entries.end());
    selected_entries.insert(selected_entries.end(), version_entries.begin(), version_entries.end());
    return selected_entries;
}

VulTempModule _parseTempModule(
    const string &module_name,
    const string &module_filepath,
    unordered_map<string, string> *global_names = nullptr,
    bool is_global_header = false
) {
    VulErrorContextGuard _err{"Parsing module '" + module_name + "' from file '" + module_filepath + "'"};

    VCPPModuleContext context;
    context.temp.name = module_name;
    context.temp.filepath = module_filepath;
    context.global_names = global_names;
    context.is_global_header = is_global_header;

    vector<string> code_lines = readFileLines(module_filepath);
    auto trim_res = stripComments(code_lines);
    context.line_mapping = std::move(trim_res.mapping);
    code_lines = std::move(trim_res.lines);

    vector<MacroEntry> macro_entries = findAllMacroEntries(code_lines);
    if (!is_global_header) {
        macro_entries = selectVersionedModuleEntries(macro_entries, context);
    }
    // printf("Found %zu macro entries in module '%s'\n", macro_entries.size(), module_name.c_str());
    for (const auto &entry : macro_entries) {
        // printf("Found macro: %s at %s: ", entry.name.c_str(), context.getOriginalPosition(entry.pos).c_str());
        // for (const auto &arg : entry.args) {
        //     printf("'%s' ", arg.c_str());
        // }
        // printf("\n");
        VCPPModuleHandlerRegistry::instance().run(context, entry);
    }

    return context.temp;
}


struct VCPPTestContext {
    VulStaticTestHarnessModule test;
    VulStaticConfigLib config_lib;
    VulStaticBundleLib bundle_lib;
    string filepath;
    std::vector<uint32_t> line_mapping; // new line number -> original line number (1-based)
    inline string getOriginalPosition(const LinePosition &pos) const {
        if (pos.line < 0 || pos.line >= line_mapping.size()) {
            return "Line " + std::to_string(pos.line + 1) + ":" + std::to_string(pos.column + 1);
        }
        return "Line " + std::to_string(line_mapping[pos.line]) + ":" + std::to_string(pos.column + 1);
    }
    inline VulDebugLoc toDebugLoc(const LinePosition &pos) const {
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
    inline VulDebugLocs bodyDebugLocs(const MacroEntry &entry) const {
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
};

class VCPPTestHandler {
public:
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) = 0;
    virtual string name() const = 0;
};

class VCPPTestHandlerRegistry {
public:
    static VCPPTestHandlerRegistry& instance() {
        static VCPPTestHandlerRegistry registry;
        return registry;
    }

    bool register_handler(std::unique_ptr<VCPPTestHandler> handler) {
        // For simplicity, we assume each handler handles a unique macro name, which is determined by the handler's dynamic type name.
        std::string handler_name = handler->name();
        if (handlers_.count(handler_name) > 0) {
            return false; // handler for this macro already exists
        }
        handlers_[handler_name] = std::move(handler);
        return true;
    }

    void run(VCPPTestContext &context, const MacroEntry &entry) {
        std::string handler_name = entry.name; // assume macro name is the same as handler's dynamic type name for simplicity
        if (handlers_.count(handler_name) == 0) {
            throw VulException("No handler registered for macro: " + entry.name + " at " + context.getOriginalPosition(entry.pos));
        }
        handlers_[handler_name]->run(context, entry);
    }

private:
    VCPPTestHandlerRegistry() = default;
    std::unordered_map<std::string, std::unique_ptr<VCPPTestHandler>> handlers_;
};

template <typename T>
class VCPPTestAutoRegisterHandler {
public:
    explicit VCPPTestAutoRegisterHandler() {
        VCPPTestHandlerRegistry::instance().register_handler(std::make_unique<T>());
    }
};

class VCPPTestPARAMETER : public VCPPTestHandler {
public:
    virtual string name() const { return "PARAMETER"; }
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 2) {
            throw VulException("PARAMETER requires exactly 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulErrorContextGuard _err{"Processing PARAMETER '" + entry.args[0] + "' at " + context.getOriginalPosition(entry.pos)};
        ConfigRealValue value = calculateConstexprValue(entry.args[1], context.config_lib);
        context.test.top_config_overrides[entry.args[0]] = value;
    }
};
static VCPPTestAutoRegisterHandler<VCPPTestPARAMETER> _auto_register_TEST_PARAMETER_handler;


class VCPPTestREQUEST : public VCPPTestHandler {
public:
    virtual string name() const { return "REQUEST"; }
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) {
        if (entry.args.empty()) {
            throw VulException("REQUEST requires at least 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        VulTempReq req;
        req.name = entry.args[0];
        req.has_handshake = false;
        VulErrorContextGuard _err{"Processing REQUEST '" + req.name + "' at " + context.getOriginalPosition(entry.pos)};
        ReqServParseOptions options;
        options.allow_handshake = true;
        options.allow_array = true;
        parseReqArgsAndRets(entry.args, 1, req, &options, "REQUEST");
        if (options.handshake.has_value()) {
            req.has_handshake = *options.handshake;
        }
        context.test.requests[req.name] = std::move(req);
    }
};

class VCPPTestREQUEST_READY : public VCPPTestHandler {
public:
    virtual string name() const { return "REQUEST_READY"; }
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) {
        if (entry.args.empty()) {
            throw VulException("REQUEST_READY requires at least 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        VulTempReq req;
        req.name = entry.args[0];
        req.has_handshake = true;
        VulErrorContextGuard _err{"Processing REQUEST_READY '" + req.name + "' at " + context.getOriginalPosition(entry.pos)};
        ReqServParseOptions options;
        options.allow_array = true;
        parseReqArgsAndRets(entry.args, 1, req, &options, "REQUEST_READY");
        context.test.requests[req.name] = std::move(req);
    }
};
static VCPPTestAutoRegisterHandler<VCPPTestREQUEST> _auto_register_TEST_REQUEST_handler;
static VCPPTestAutoRegisterHandler<VCPPTestREQUEST_READY> _auto_register_TEST_REQUEST_READY_handler;

class VCPPTestSERVICE : public VCPPTestHandler {
public:
    virtual string name() const { return "SERVICE"; }
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) {
        if (entry.args.empty()) {
            throw VulException("SERVICE requires at least 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        VulTempServ serv;
        serv.name = entry.args[0];
        serv.has_handshake = false;
        serv.cond = "";
        serv.priority = "";
        serv.codelines = entry.body;
        VulErrorContextGuard _err{"Processing SERVICE '" + serv.name + "' at " + context.getOriginalPosition(entry.pos)};
        ReqServParseOptions options;
        options.allow_handshake = true;
        options.allow_ready = true;
        options.allow_priority = true;
        options.allow_array = true;
        parseReqArgsAndRets(entry.args, 1, serv, &options, "SERVICE");
        if (options.handshake.has_value()) {
            serv.has_handshake = *options.handshake;
        }
        if (options.ready.has_value()) {
            if (options.handshake.has_value() && !*options.handshake) {
                throw VulException("SERVICE ready=<condition> conflicts with handshake=0 at " + context.getOriginalPosition(entry.pos));
            }
            serv.cond = *options.ready;
            serv.cond_debug = context.toDebugLoc(entry.pos);
            serv.has_handshake = true;
        }
        if (serv.has_handshake && serv.cond.empty()) {
            throw VulException("SERVICE with handshake=1 requires ready=<condition> at " + context.getOriginalPosition(entry.pos));
        }
        if (options.priority.has_value()) {
            serv.priority = *options.priority;
        }
        context.test.services[serv.name] = std::move(serv);
    }
};

class VCPPTestSERVICE_READY : public VCPPTestHandler {
public:
    virtual string name() const { return "SERVICE_READY"; }
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) {
        if (entry.args.size() < 2) {
            throw VulException("SERVICE_READY requires at least 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        VulTempServ serv;
        serv.name = entry.args[0];
        serv.has_handshake = true;
        serv.cond = entry.args[1];
        serv.cond_debug = context.toDebugLoc(entry.pos);
        serv.priority = "";
        serv.codelines = entry.body;
        serv.codelines_debug = context.bodyDebugLocs(entry);
        VulErrorContextGuard _err{"Processing SERVICE_READY '" + serv.name + "' at " + context.getOriginalPosition(entry.pos)};
        ReqServParseOptions options;
        options.allow_array = true;
        parseReqArgsAndRets(entry.args, 2, serv, &options, "SERVICE_READY");
        context.test.services[serv.name] = std::move(serv);
    }
};
static VCPPTestAutoRegisterHandler<VCPPTestSERVICE> _auto_register_TEST_SERVICE_handler;
static VCPPTestAutoRegisterHandler<VCPPTestSERVICE_READY> _auto_register_TEST_SERVICE_READY_handler;

class VCPPTestQUERY : public VCPPTestHandler {
public:
    virtual string name() const { return "QUERY"; }
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 2) {
            throw VulException("QUERY requires exactly 2 arguments at " + context.getOriginalPosition(entry.pos));
        }
        if (!entry.body.empty()) {
            throw VulException("QUERY in TestMain must be a declaration without code block at " + context.getOriginalPosition(entry.pos));
        }
        VulTempQuery query;
        query.name = entry.args[0];
        query.ret_type = entry.args[1];
        VulErrorContextGuard _err{"Processing QUERY '" + query.name + "' at " + context.getOriginalPosition(entry.pos)};
        VulStaticQuery static_query;
        static_query.name = query.name;
        static_query.ret_type = parseTypeSignature(query.ret_type, context.config_lib);
        context.test.queries[query.name] = std::move(static_query);
    }
};
static VCPPTestAutoRegisterHandler<VCPPTestQUERY> _auto_register_TEST_QUERY_handler;

class VCPPTestGLOBAL : public VCPPTestHandler {
public:
    virtual string name() const { return "GLOBAL"; }
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) {
        context.test.globalCodes.insert(context.test.globalCodes.end(), entry.body.begin(), entry.body.end());
        auto locs = context.bodyDebugLocs(entry);
        context.test.globalCodes_debug.insert(context.test.globalCodes_debug.end(), locs.begin(), locs.end());
    }
};
static VCPPTestAutoRegisterHandler<VCPPTestGLOBAL> _auto_register_TEST_GLOBAL_handler;

class VCPPTestTOP : public VCPPTestHandler {
public:
    virtual string name() const { return "TOP"; }
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 1) {
            throw VulException("TOP requires exactly 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        if (!context.test.top_module_path.empty()) {
            throw VulException("Multiple TOP declarations are not allowed at " + context.getOriginalPosition(entry.pos));
        }
        context.test.top_module_path = parsePathMacroArg(entry.args[0]);
    }
};
static VCPPTestAutoRegisterHandler<VCPPTestTOP> _auto_register_TEST_TOP_handler;

class VCPPTestPROJECT : public VCPPTestHandler {
public:
    virtual string name() const { return "PROJECT"; }
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) {
        if (entry.args.size() != 1) {
            throw VulException("PROJECT requires exactly 1 argument at " + context.getOriginalPosition(entry.pos));
        }
        if (!context.test.project_dir_path.empty()) {
            throw VulException("Multiple PROJECT declarations are not allowed at " + context.getOriginalPosition(entry.pos));
        }
        context.test.project_dir_path = parsePathMacroArg(entry.args[0]);
    }
};
static VCPPTestAutoRegisterHandler<VCPPTestPROJECT> _auto_register_TEST_PROJECT_handler;

class VCPPTestSIMULATION : public VCPPTestHandler {
public:
    virtual string name() const { return "SIMULATION"; }
    virtual void run(VCPPTestContext &context, const MacroEntry &entry) {
        if (!context.test.test_codelines.empty()) {
            throw VulException("Multiple SIMULATION blocks are not allowed at " + context.getOriginalPosition(entry.pos));
        }
        context.test.test_codelines.insert(context.test.test_codelines.end(), entry.body.begin(), entry.body.end());
        auto locs = context.bodyDebugLocs(entry);
        context.test.test_codelines_debug.insert(context.test.test_codelines_debug.end(), locs.begin(), locs.end());
    }
};
static VCPPTestAutoRegisterHandler<VCPPTestSIMULATION> _auto_register_TEST_SIMULATION_handler;

VulStaticTestHarnessModule _parseTestModule(
    const string &test_filepath,
    const VulStaticConfigLib &config_lib,
    const VulStaticBundleLib &bundle_lib
) {
    VCPPTestContext context;
    context.config_lib = config_lib;
    context.bundle_lib = bundle_lib;
    context.filepath = test_filepath;

    VulErrorContextGuard _err{"Parsing test module from file '" + test_filepath + "'"};

    vector<string> code_lines = readFileLines(test_filepath);
    auto trim_res = stripComments(code_lines);
    context.line_mapping = std::move(trim_res.mapping);
    code_lines = std::move(trim_res.lines);
    
    const string prefix = "#include";
    for (const auto &line_raw : code_lines) {
        string line = trim(line_raw);
        if (line.rfind(prefix, 0) != 0) {
            continue;
        }
        size_t first_quote = line.find('<');
        if (first_quote == string::npos) {
            continue;
        }
        size_t second_quote = line.find('>', first_quote + 1);
        if (second_quote == string::npos || second_quote <= first_quote + 1) {
            continue;
        }
        string included_path = trim(line.substr(first_quote + 1, second_quote - first_quote - 1));
        bool escaped = false;
        for (auto s : VulLibFiles) {
            if (included_path == s) {
                escaped = true;
                break;
            }
        }
        for (auto s : VulEscapedHeaders) {
            if (included_path == s) {
                escaped = true;
                break;
            }
        }
        if (escaped) {
            continue;
        }
        context.test.includedHeaders.push_back(included_path);
    }

    vector<MacroEntry> macro_entries = findAllMacroEntries(code_lines);
    for (const auto &entry : macro_entries) {
        VCPPTestHandlerRegistry::instance().run(context, entry);
    }

    return std::move(context.test);
}

static std::pair<string, string> _scanTestModuleBindingPaths(const string &test_filepath) {
    vector<string> code_lines = readFileLines(test_filepath);
    code_lines = stripComments(code_lines).lines;
    vector<MacroEntry> macro_entries = findAllMacroEntries(code_lines);

    string top_module_path;
    string project_dir_path;
    for (const auto &entry : macro_entries) {
        if (entry.name == "TOP") {
            if (entry.args.size() != 1) {
                throw VulException("TOP requires exactly 1 argument");
            }
            if (!top_module_path.empty()) {
                throw VulException("Multiple TOP declarations are not allowed");
            }
            top_module_path = parsePathMacroArg(entry.args[0]);
        } else if (entry.name == "PROJECT") {
            if (entry.args.size() != 1) {
                throw VulException("PROJECT requires exactly 1 argument");
            }
            if (!project_dir_path.empty()) {
                throw VulException("Multiple PROJECT declarations are not allowed");
            }
            project_dir_path = parsePathMacroArg(entry.args[0]);
        }
    }
    return {top_module_path, project_dir_path};
}

static bool isHeaderSourceFile(const std::filesystem::path &p) {
    const string ext = p.extension().string();
    return ext == ".h" || ext == ".hpp";
}

static string pathInHeaderDirKey(
    const std::filesystem::path &p,
    const std::filesystem::path &header_dir_canon
) {
    using namespace std::filesystem;
    path canon = canonical(p);
    path rel = relative(canon, header_dir_canon);
    if (rel.empty()) {
        return "";
    }
    for (const auto &part : rel) {
        if (part == "..") {
            return "";
        }
    }
    return rel.generic_string();
}

static vector<string> scanHeaderIncludes(const std::filesystem::path &header_file) {
    vector<string> out;
    vector<string> code_lines = stripComments(readFileLines(header_file.string())).lines;
    const string prefix = "#include";

    for (const auto &line_raw : code_lines) {
        string line = trim(line_raw);
        if (line.rfind(prefix, 0) != 0) {
            continue;
        }

        size_t open_pos = line.find_first_of("\"<", prefix.size());
        if (open_pos == string::npos) {
            continue;
        }
        char open = line[open_pos];
        char close = (open == '"') ? '"' : '>';
        size_t close_pos = line.find(close, open_pos + 1);
        if (close_pos == string::npos || close_pos <= open_pos + 1) {
            continue;
        }

        string included = trim(line.substr(open_pos + 1, close_pos - open_pos - 1));
        if (!included.empty()) {
            out.push_back(std::move(included));
        }
    }
    return out;
}

static vector<std::filesystem::path> collectHeaderParseOrder(const std::filesystem::path &header_dir) {
    using namespace std::filesystem;

    path header_dir_canon = canonical(header_dir);
    unordered_set<string> all_items;
    unordered_map<string, path> item_to_path;

    for (const auto &entry : recursive_directory_iterator(header_dir_canon)) {
        if (!entry.is_regular_file() || !isHeaderSourceFile(entry.path())) {
            continue;
        }
        string key = pathInHeaderDirKey(entry.path(), header_dir_canon);
        if (key.empty()) {
            continue;
        }
        all_items.insert(key);
        item_to_path[key] = entry.path();
    }

    if (all_items.empty()) {
        throw VulException("Header directory contains no .h or .hpp files: " + header_dir.string());
    }

    unordered_map<string, unordered_set<string>> edges_former_to_latter;
    for (const auto &[key, file_path] : item_to_path) {
        for (const auto &included_raw : scanHeaderIncludes(file_path)) {
            vector<path> candidates;
            path included_path(included_raw);
            if (included_path.is_absolute()) {
                candidates.push_back(included_path);
            } else {
                candidates.push_back(file_path.parent_path() / included_path);
                candidates.push_back(header_dir_canon / included_path);
            }

            string included_key;
            for (const auto &candidate : candidates) {
                std::error_code ec;
                if (!exists(candidate, ec) || !is_regular_file(candidate, ec) || !isHeaderSourceFile(candidate)) {
                    continue;
                }
                string maybe_key = pathInHeaderDirKey(candidate, header_dir_canon);
                if (!maybe_key.empty() && all_items.find(maybe_key) != all_items.end()) {
                    included_key = maybe_key;
                    break;
                }
            }
            if (!included_key.empty()) {
                edges_former_to_latter[included_key].insert(key);
            }
        }
    }

    vector<string> loop_nodes;
    auto sorted_keys = topologicalSort(all_items, edges_former_to_latter, loop_nodes);
    if (!sorted_keys) {
        string err = "Cycle detected in header include graph:";
        std::sort(loop_nodes.begin(), loop_nodes.end());
        for (const auto &node : loop_nodes) {
            err += "\n  " + node;
        }
        throw VulException(err);
    }

    vector<path> out;
    out.reserve(sorted_keys->size());
    for (const auto &key : *sorted_keys) {
        out.push_back(item_to_path.at(key));
    }
    return out;
}

static void importGlobalHeaderModule(
    VulStaticProject &project,
    const VulTempModule &header_module
) {
    for (const auto& item : header_module.configs) {
        VulErrorContextGuard _err{"evaluating global header constant " + item.name};
        ConfigRealValue value = calculateConstexprValue(item.value, project.global_configlib);
        project.global_configlib[item.name] = value;
    }
    for (const auto& item : header_module.bundles) {
        VulErrorContextGuard _err{"staticalizing global header bundle " + item.name};
        VulStaticBundle static_bundle = staticalizeBundle(item, project.global_configlib);
        project.global_bundlelib.push_back(std::move(static_bundle));
    }
    project.global_helper_codes.insert(
        project.global_helper_codes.end(),
        header_module.helper_codes.begin(),
        header_module.helper_codes.end()
    );
}

static void parseProjectHeaders(
    VulStaticProject &project,
    const std::filesystem::path &proj_dir
) {
    using namespace std::filesystem;

    const path header_dir = proj_dir / "header";
    const path header_hpp = proj_dir / "header.hpp";
    const path header_h = proj_dir / "header.h";
    const bool has_header_dir = exists(header_dir) && is_directory(header_dir);
    const bool has_header_hpp = exists(header_hpp) && is_regular_file(header_hpp);
    const bool has_header_h = exists(header_h) && is_regular_file(header_h);

    if (has_header_hpp && has_header_h) {
        throw VulException("Both header.hpp and header.h exist in project root; keep only one single-header file");
    }
    if (has_header_dir && (has_header_hpp || has_header_h)) {
        throw VulException("Both header directory and single-header file exist in project root; keep only one header system");
    }
    if (!has_header_dir && !has_header_hpp && !has_header_h) {
        throw VulException("Project root must contain either a header directory or a header.hpp/header.h file: " + proj_dir.string());
    }

    vector<path> header_files;
    if (has_header_dir) {
        header_files = collectHeaderParseOrder(header_dir);
    } else {
        header_files.push_back(has_header_hpp ? header_hpp : header_h);
    }

    for (const auto &header_path : header_files) {
        VulErrorContextGuard _err{"parsing global header file " + header_path.string()};
        VulTempModule fake_module = _parseTempModule(
            header_path.stem().string(),
            header_path.string(),
            &project.global_names,
            true
        );
        importGlobalHeaderModule(project, fake_module);
    }
}


VulStaticProject parseVcppStaticProject(
    const string &project_dir,
    const string &top_file_path,
    const string &main_file_path
) {
    VulStaticProject project;

    using namespace std::filesystem;

    bool is_sim = !main_file_path.empty();

    path main_path;
    string scanned_top_module_path;
    string scanned_project_dir_path;
    if (!main_file_path.empty()) {
        main_path = path(main_file_path);
        if (!exists(main_path) || !is_regular_file(main_path)) {
            std::cerr << "Error: Main file does not exist or is not a regular file: " << main_file_path << std::endl;
            assert(0);
        }
        std::tie(scanned_top_module_path, scanned_project_dir_path) = _scanTestModuleBindingPaths(main_path.string());
    }

    auto resolve_from_main = [&](const string &raw_path, const string &field_name) -> path {
        if (raw_path.empty()) {
            return {};
        }
        path p(raw_path);
        if (p.is_absolute()) {
            return p.lexically_normal();
        }
        if (main_path.empty()) {
            throw VulException(field_name + " in TestMain requires a main file");
        }
        return (main_path.parent_path() / p).lexically_normal();
    };

    path top_path;
    if (!top_file_path.empty()) {
        top_path = path(top_file_path);
    } else if (!scanned_top_module_path.empty()) {
        top_path = resolve_from_main(scanned_top_module_path, "TOP");
    }
    if (top_path.empty()) {
        throw VulException("Top module path is not specified. Use -t/--top or TOP(...) in TestMain.");
    }
    if (!exists(top_path) || !is_regular_file(top_path)) {
        throw VulException("Top file does not exist or is not a regular file: " + top_path.string());
    }

    path proj_dir;
    if (!project_dir.empty()) {
        proj_dir = path(project_dir);
    } else if (!scanned_project_dir_path.empty()) {
        proj_dir = resolve_from_main(scanned_project_dir_path, "PROJECT");
    } else {
        proj_dir = top_path.parent_path();
    }
    if (!exists(proj_dir) || !is_directory(proj_dir)) {
        throw VulException("Project directory does not exist or is not a directory: " + proj_dir.string());
    }

    parseProjectHeaders(project, proj_dir);

    if (!main_file_path.empty()) {
        {
            VulErrorContextGuard _err{"reading test harness module from " + main_path.string()};
            project.test_harness = _parseTestModule(main_path.string(), project.global_configlib, project.global_bundlelib);
        }
    }

    unordered_map<ModuleName, path> module_file_path_cache;
    auto find_module_file = [&](const ModuleName& mod_name) -> std::optional<path> {
        auto iter = module_file_path_cache.find(mod_name);
        if (iter != module_file_path_cache.end()) {
            return iter->second;
        }
        vector<string> candidates = {
            mod_name + ".hpp",
            mod_name + ".cpp",
            mod_name + ".vul",
            mod_name + ".h"
        };
        for (const auto& candidate : candidates) {
            // find in project directory recursively
            vector<path> found_paths;
            for (auto& p : recursive_directory_iterator(proj_dir)) {
                if (p.is_regular_file() && p.path().filename() == candidate) {
                    found_paths.push_back(p.path());
                }
            }
            if (found_paths.size() > 1) {
                string err_str = "Error: Multiple files found for module " + mod_name + ":";
                for (const auto& p : found_paths) {
                    err_str += "\n  " + p.string();
                }
                throw VulException(err_str);
            } else if (!found_paths.empty()) {
                module_file_path_cache[mod_name] = found_paths.front();
                return found_paths.front();
            }
        }
        return std::nullopt;
    };

    struct InstanceToProcess {
        shared_ptr<VulStaticModuleInstance> ptr;
        VulStaticConfigLib config_overrides;
    };
    std::deque<InstanceToProcess> todo_queue;

    shared_ptr<VulStaticModuleInstance> top_instance = std::make_shared<VulStaticModuleInstance>();
    top_instance->module_name = top_path.stem().string();
    if (is_sim) {
        top_instance->instance_path = {"sim", "top"};
    } else {
        top_instance->instance_path = {"top"};
    }
    top_instance->filepath = top_path.string();
    todo_queue.push_back({top_instance, project.test_harness.top_config_overrides});

    uint32_t instance_count = 0;
    VulTempModuleCache temp_module_cache;

    while (!todo_queue.empty()) {
        auto [instance_ptr, config_overrides] = todo_queue.front();
        todo_queue.pop_front();

        VulErrorContextGuard _err{"parsing instance " + instance_ptr->simClassName() + " of module " + instance_ptr->module_name};
        const ModuleName& mod_name = instance_ptr->module_name;
        VulTempModule *temp_mod_ptr = nullptr;

        auto temp_mod_iter = temp_module_cache.find(mod_name);
        if (temp_mod_iter != temp_module_cache.end()) {
            temp_mod_ptr = &temp_mod_iter->second;
        } else {

            auto mod_file_opt = find_module_file(mod_name);
            if (!mod_file_opt.has_value()) {
                throw VulException("Module file not found for module: " + instance_ptr->module_name);
            }
            const path& mod_file = mod_file_opt.value();
            string mod_file_str = mod_file.string();

            VulErrorContextGuard _err_file{"entering module file " + mod_file_str};

            temp_module_cache[mod_name] = _parseTempModule(
                mod_name,
                mod_file_str,
                &project.global_names,
                false
            );
            temp_mod_ptr = &temp_module_cache[mod_name];
        }

        instantiateModule(
            *instance_ptr,
            *temp_mod_ptr,
            config_overrides,
            project.global_configlib,
            project.global_bundlelib
        );
        detectRequestCallInLogicBlocks(*instance_ptr);

        instance_ptr->instance_id = instance_count++;

        // process child instances
        for (const auto& [child_name, child_instance_decl] : instance_ptr->instances) {
            shared_ptr<VulStaticModuleInstance> child_instance = std::make_shared<VulStaticModuleInstance>();
            child_instance->instance_path = instance_ptr->instance_path;
            child_instance->instance_path.push_back(child_name);
            child_instance->module_name = child_instance_decl.module_name;
            child_instance->parent = instance_ptr;
            instance_ptr->children.push_back(child_instance);
            todo_queue.push_back({child_instance, child_instance_decl.parameter_overrides});

            if (child_instance_decl.array_dims.size() > 2) {
                throw VulException("Only up to 2 child instance array dimensions are currently supported");
            }
        }
    }
    project.top_module_instance = top_instance;

    validateStaticProject(project);

    printf("Successfully parsed project. Summary:\n");
    printf("Parse %ld modules:\n", module_file_path_cache.size());
    for (const auto& [mod_name, mod_path] : module_file_path_cache) {
        printf("  [%s]: %s\n", mod_name.c_str(), mod_path.string().c_str());
    }
    printf("Instance hierarchy:\n");
    vector<pair<shared_ptr<VulStaticModuleInstance>, size_t>> dfs_stack;
    dfs_stack.reserve(64);
    dfs_stack.push_back({top_instance, 1});
    while (!dfs_stack.empty()) {
        auto [node, depth] = dfs_stack.back();
        dfs_stack.pop_back();

        for (size_t i = 0; i < depth; ++i) {
            putchar(' ');
        }
        std::string instance_path_str;
        for (const auto& name : node->instance_path) {
            if (!instance_path_str.empty()) {
                instance_path_str += "::";
            }
            instance_path_str += name;
        }
        printf("%s [%s]\n", instance_path_str.c_str(), node->module_name.c_str());

        for (size_t i = node->children.size(); i > 0; --i) {
            dfs_stack.push_back({node->children[i - 1], depth + 1});
        }
    }
    printf("Total instances: %d\n", instance_count);
    
    if (!is_sim) {
        printf("No main file provided, skipping simulation setup.\n");
        return project;
    }

    printf("Setting up simulation hierarchy and update sequence...\n");

    shared_ptr<VulStaticModuleInstance> fake_main = std::make_shared<VulStaticModuleInstance>();
    fake_main->instance_path = {"sim", "main"};
    fake_main->module_name = "TestMain";
    fake_main->instance_id = instance_count + 1;
    fake_main->tick_blocks.push_back(VulTickBlock());

    shared_ptr<VulStaticModuleInstance> sim_top = std::make_shared<VulStaticModuleInstance>();
    sim_top->instance_path = {"sim"};
    sim_top->module_name = "SimTop";
    sim_top->filepath = main_file_path;
    sim_top->instance_id = instance_count + 2;
    sim_top->tick_blocks.push_back(VulTickBlock());

    for (const auto &req_entry: project.top_module_instance->requests) {
        fake_main->services[req_entry.first] = req_entry.second;
        VulLogicBlock logic_block;
        logic_block.block_id = fake_main->services.size();
        logic_block.with_priority = false;
        fake_main->serv_logic_blocks[req_entry.first] = logic_block;
        VulReqServConnection conn;
        conn.req_instance = "top";
        conn.req_name = req_entry.first;
        conn.serv_instance = "main";
        conn.serv_name = req_entry.first;
        sim_top->req_connections.push_back(conn);
    }
    for (const auto &serv_entry: project.top_module_instance->services) {
        fake_main->requests[serv_entry.first] = serv_entry.second;
        LogicBlockCall call;
        call.instance = "";
        call.port = serv_entry.first;
        fake_main->tick_blocks[0].call_requests.push_back(call);
        VulReqServConnection conn;
        conn.req_instance = "main";
        conn.req_name = serv_entry.first;
        conn.serv_instance = "top";
        conn.serv_name = serv_entry.first;
        sim_top->req_connections.push_back(conn);
    }

    VulStaticInstanceDecl main_decl;
    main_decl.name = "main";
    main_decl.module_name = "TestMain";
    sim_top->instances["main"] = main_decl;
    VulStaticInstanceDecl top_decl;
    top_decl.name = "top";
    top_decl.module_name = project.top_module_instance->module_name;
    sim_top->instances["top"] = top_decl;

    sim_top->children.push_back(fake_main);
    sim_top->children.push_back(project.top_module_instance);

    fake_main->parent = sim_top;
    project.top_module_instance->parent = sim_top;

    setupUpdateSequence(sim_top);

    printf("Setup complete.\n");

    return project;
}
