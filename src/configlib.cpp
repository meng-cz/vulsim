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
