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

/**
 * @brief Get all config items in topological order based on their references.
 * @param out_sorted_items Output vector to hold the names of config items in topological order.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulConfigLib::getAllConfigItemsTopoSort(vector<ConfigName> &out_sorted_items) const {
    unordered_set<ConfigName> all_items;
    unordered_map<ConfigName, unordered_set<ConfigName>> rev_references;
    for (const auto &entry : config_items) {
        all_items.insert(entry.first);
        rev_references[entry.first] = entry.second.reverse_references;
    }
    vector<ConfigName> out_loop_nodes;
    auto sorted_items_ptr = topologicalSort(all_items, rev_references, out_loop_nodes);
    if (!sorted_items_ptr) {
        string looped_items_str;
        for (const auto &it : out_loop_nodes) {
            if (!looped_items_str.empty()) looped_items_str += ", ";
            looped_items_str += it;
        }
        return EStr(EItemConfRefLooped, "Config items have circular references: " + looped_items_str);
    }
    out_sorted_items = std::move(*sorted_items_ptr);
    return "";
}

/**
 * @brief Calculate the integer value of a config expression string.
 * @param value The config expression string to calculate.
 * @param overrides A map of config name to override config values.
 * @param out_real_value Output parameter to hold the calculated integer value.
 * @param seen_configs Set of config names seen during the calculation to detect cycles.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulConfigLib::calculateConfigExpression(
    const ConfigValue &value,
    const unordered_map<ConfigName, ConfigRealValue> &overrides,
    ConfigRealValue &out_real_value,
    unordered_set<ConfigName> &seen_configs
) const {
    uint32_t errpos = 0;
    string err;
    auto tokens = config_parser::tokenizeConfigValueExpression(value, errpos, err);
    if (!tokens) {
        return EStr(EItemConfValueTokenInvalid, string("Invalid token grammar at position ") + std::to_string(errpos) + string(": ") + err + string(": ") + value);
    }
    // replace Identifier tokens with their values
    for (auto &tok : *tokens) {
        if (tok.type == config_parser::TokenType::Identifier) {
            // lookup value
            auto over_iter = overrides.find(tok.text);
            if (over_iter != overrides.end()) {
                seen_configs.insert(tok.text);
                tok.type = config_parser::TokenType::Number;
                tok.value = over_iter->second;
                continue;
            }
            auto iter = config_items.find(tok.text);
            if (iter != config_items.end()) {
                seen_configs.insert(tok.text);
                tok.type = config_parser::TokenType::Number;
                tok.value = iter->second.real_value;
            } else {
                return EStr(EItemConfRefNotFound, string("Undefined config identifier: ") + tok.text + string(": ") + value);
            }
        }
    }
    auto ast = config_parser::parseConfigValueExpression(*tokens, errpos, err);
    if (!ast) {
        return EStr(EItemConfValueGrammerInvalid, string("Invalid grammar at position ") + std::to_string(errpos) + string(": ") + err + string(": ") + value);
    }
    out_real_value = config_parser::evaluateConfigValueExpression(*ast, errpos, err);
    if (!err.empty()) {
        return EStr(EItemConfValueGrammerInvalid, string("Error evaluating config value at position ") + std::to_string(errpos) + string(": ") + err + string(": ") + value);
    }
    return "";
}

ErrorMsg VulConfigLib::insertMultiConfigItems(const vector<VulConfigItem> &items, const GroupName &group) {
    unordered_set<ConfigName> new_names;
    unordered_map<ConfigName, VulConfigItem> new_items;
    for (const auto &item : items) {
        if (checkNameConflict(item.name)) {
            return EStr(EItemConfNameInvalid, string("Config item name conflict: ") + item.name);
        }
        if (new_names.find(item.name) != new_names.end()) {
            return EStr(EItemConfNameInvalid, string("Duplicate config item name in input: ") + item.name);
        }
        new_names.insert(item.name);
        new_items[item.name] = item;
    }
    unordered_map<ConfigName, unordered_set<ConfigName>> new_conf_rev_refs;
    for (const auto &item : items) {
        string err;
        uint32_t errpos = 0;
        auto conf_refs = config_parser::parseReferencedIdentifier(item.value, errpos, err);
        if (!err.empty()) {
            return EStr(EItemConfValueGrammerInvalid, string("Invalid config expression for item '") + item.name + "' at position " + std::to_string(errpos) + ": " + err + string(": ") + item.value);
        }
        for (const auto &cn : *conf_refs) {
            if (cn == item.name) {
                return EStr(EItemConfRefLooped, string("Config item '") + item.name + "' references itself.");
            }
            if (config_items.find(cn) == config_items.end() && new_names.find(cn) == new_names.end()) {
                return EStr(EItemConfRefNotFound, string("Config item '") + item.name + "' references undefined config '" + cn + "'.");
            }
            new_conf_rev_refs[cn].insert(item.name);
        }
    }
    vector<ConfigName> looped;
    auto sorted_names_ptr = topologicalSort(new_names, new_conf_rev_refs, looped);
    if (!sorted_names_ptr) {
        string looped_str;
        for (const auto &cn : looped) {
            if (!looped_str.empty()) looped_str += ", ";
            looped_str += cn;
        }
        return EStr(EItemConfRefLooped, string("Config items have circular references: ") + looped_str);
    }
    for (const auto &name : *sorted_names_ptr) {
        const auto &item = new_items[name];
        ConfigRealValue real_value;
        unordered_set<ConfigName> referenced;
        ErrorMsg err = calculateConfigExpression(item.value, real_value, referenced);
        if (!err.empty()) {
            return EStr(EItemConfValueGrammerInvalid, string("Error calculating value for config item '") + item.name + "': " + err.msg);
        }
        ConfigEntry confe;
        confe.item = item;
        confe.group = group;
        confe.real_value = real_value;
        confe.references = referenced;
        confe.reverse_references = new_conf_rev_refs[name];
        config_items[name] = std::move(confe);
    }
    return "";
}

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
        throw VulException(EStr(EItemConfValueTokenInvalid, string("Invalid token grammar at position ") + std::to_string(errpos) + string(": ") + err + string(": ") + value));
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
                throw VulException(EStr(EItemConfRefNotFound, string("Undefined config identifier: ") + tok.text + string(": ") + value));
            }
        }
    }
    auto ast = config_parser::parseConfigValueExpression(*tokens, errpos, err);
    if (!ast) {
        throw VulException(EStr(EItemConfValueGrammerInvalid, string("Invalid grammar at position ") + std::to_string(errpos) + string(": ") + err + string(": ") + value));
    }
    ConfigRealValue real_value = config_parser::evaluateConfigValueExpression(*ast, errpos, err);
    if (!err.empty()) {
        throw VulException(EStr(EItemConfValueGrammerInvalid, string("Error evaluating config value at position ") + std::to_string(errpos) + string(": ") + err + string(": ") + value));
    }
    return real_value;
}
