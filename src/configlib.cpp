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

#include "configlib.h"
#include "type.h"
#include "toposort.hpp"
#include "configexpr.hpp"
#include "bundlelib.h"

#include <algorithm>
#include <functional>
#include <queue>

using std::unique_ptr;
using std::make_unique;


void insertStaticConfig(VulStaticConfigLib &config_lib, const ConfigName &name, const ConfigValue &value) {
    if (config_lib.find(name) != config_lib.end()) {
        config_lib.erase(name);
    }
    ConfigRealValue real_value = calculateConstexprValue(value, config_lib);
    config_lib[name] = real_value;
}

ConfigRealValue calculateConstexprValue(const ConfigValue &value, const VulStaticConfigLib &config_lib) {
    uint32_t errpos = 0;
    string err;
    auto tokens = config_parser::tokenizeConfigValueExpression(value, errpos, err);
    if (!tokens) {
        throw VulException(string("Invalid token grammar at position ") + std::to_string(errpos) + string(": ") + err + string(": ") + value);
    }
    // replace Identifier tokens with their values
    for (auto &tok : *tokens) {
        if (tok.type == config_parser::TokenType::Identifier) {
            // lookup value
            auto over_iter = config_lib.find(tok.text);
            if (over_iter != config_lib.end()) {
                tok.type = config_parser::TokenType::Number;
                tok.value = over_iter->second;
            } else {
                throw VulException(string("Undefined config identifier: ") + tok.text + string(": ") + value);
            }
        }
    }
    auto ast = config_parser::parseConfigValueExpression(*tokens, errpos, err);
    if (!ast) {
        throw VulException(string("Invalid grammar at position ") + std::to_string(errpos) + string(": ") + err + string(": ") + value);
    }
    ConfigRealValue real_value = config_parser::evaluateConfigValueExpression(*ast, errpos, err);
    if (!err.empty()) {
        throw VulException(string("Error evaluating config value at position ") + std::to_string(errpos) + string(": ") + err + string(": ") + value);
    }
    return real_value;
}

VulStaticConfigLib mergeStaticConfigLibs(const VulStaticConfigLib &global_lib, const VulStaticConfigLib &local_lib) {
    VulStaticConfigLib merged_lib = global_lib;
    for (const auto &[name, value] : local_lib) {
        merged_lib[name] = value;
    }
    return merged_lib;
}

