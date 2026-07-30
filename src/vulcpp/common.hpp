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

#include <optional>
#include <utility>
#include <vector>

std::optional<pair<string, string>> parseKeyValueArg(const string &raw_arg);
vector<string> splitTopLevelCsv(const string &raw);
void appendDimList(vector<string> &dims, const string &raw_value);
bool parseBoolAttribute(const string &key, const string &value);
void ensureAttrValue(const string &macro_name, const string &key, const string &value);
bool isAttrKey(const std::optional<pair<string, string>> &attr, const string &key);
pair<string, string> parseParameterOverrideArg(const string &raw_arg, const string &macro_name);

struct ReqServParseOptions {
    bool allow_handshake = false;
    bool allow_ready = false;
    bool allow_priority = false;
    bool allow_array = true;
    std::optional<bool> handshake;
    std::optional<string> ready;
    std::optional<string> priority;
};

void setUniqueStringAttr(
    std::optional<string> &target,
    const string &macro_name,
    const string &key,
    const string &value
);
bool parseReqServAttribute(
    VulTempReqServBase &reqserv,
    ReqServParseOptions &options,
    const string &macro_name,
    const string &raw_arg
);
void parseReqArgsAndRets(
    const vector<string> &macro_args,
    size_t begin_idx,
    VulTempReqServBase &reqserv,
    ReqServParseOptions *options = nullptr,
    const string &macro_name = "REQUEST/SERVICE"
);
string parsePathMacroArg(const string &raw_arg);
