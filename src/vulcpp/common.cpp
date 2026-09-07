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

#include "common.hpp"

#include "../stringop.hpp"

using namespace stringop;

std::optional<pair<string, string>> parseKeyValueArg(const string &raw_arg) {
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

vector<string> splitTopLevelCsv(const string &raw) {
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

void appendDimList(vector<string> &dims, const string &raw_value) {
    for (auto &dim : splitTopLevelCsv(raw_value)) {
        dim = trim(dim);
        if (dim.empty()) {
            throw VulException("empty dimension expression");
        }
        dims.push_back(std::move(dim));
    }
}

bool parseBoolAttribute(const string &key, const string &value) {
    const string v = trim(value);
    if (v == "1" || v == "true" || v == "TRUE") return true;
    if (v == "0" || v == "false" || v == "FALSE") return false;
    throw VulException("attribute '" + key + "' expects 0/1 or true/false, got '" + value + "'");
}

void ensureAttrValue(const string &macro_name, const string &key, const string &value) {
    if (trim(value).empty()) {
        throw VulException(macro_name + " attribute '" + key + "' cannot be empty");
    }
}

bool isAttrKey(const std::optional<pair<string, string>> &attr, const string &key) {
    return attr && attr->first == key;
}

pair<string, string> parseParameterOverrideArg(
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
void setUniqueStringAttr(
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

bool parseReqServAttribute(
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

void parseReqArgsAndRets(
    const vector<string> &macro_args,
    size_t begin_idx,
    VulTempReqServBase &reqserv,
    ReqServParseOptions *options,
    const string &macro_name
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
            reqserv.param_order.push_back({VulReqServParamKind::Arg, reqserv.args.size() - 1});
        } else if (type_tag.size() > 6 && type_tag.rfind("RESP(", 0) == 0 && type_tag.back() == ')') {
            reqserv.rets.emplace_back(type_tag.substr(5, type_tag.size() - 6), arg_name);
            reqserv.param_order.push_back({VulReqServParamKind::Resp, reqserv.rets.size() - 1});
        } else {
            throw VulException("invalid argument type tag '" + type_tag + "'");
        }
    }
}

string parsePathMacroArg(const string &raw_arg) {
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
