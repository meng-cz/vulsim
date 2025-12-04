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


unique_ptr<vector<ConfigName>> VulConfigLib::_parseConfigReferences(
    const ConfigValue &value,
    uint32_t &out_error_pos,
    ErrorMsg &err
) const {
    using namespace config_parser;
    out_error_pos = 0; err.clear();
    auto tokens = tokenizeConfigValueExpression(value, out_error_pos, err);
    if (!tokens) {
        return nullptr;
    }
    unique_ptr<vector<ConfigName>> result = make_unique<vector<ConfigName>>();
    for (const auto &tok : *tokens) {
        if (tok.type == TokenType::Identifier) {
            result->push_back(tok.text);
        }
    }
    return result;
}

ConfigRealValue VulConfigLib::_calculateConfigRealValue (
    const ConfigValue &value,
    uint32_t &out_error_pos,
    ErrorMsg &err
) const {
    using namespace config_parser;
    out_error_pos = 0; err.clear();
    auto tokens = tokenizeConfigValueExpression(value, out_error_pos, err);
    if (!tokens) {
        return 0;
    }
    // replace Identifier tokens with their values
    for (auto &tok : *tokens) {
        if (tok.type == TokenType::Identifier) {
            // lookup value
            auto iter = config_items.find(tok.text);
            if (iter != config_items.end()) {
                tok.type = TokenType::Number;
                tok.value = iter->second.real_value;
            } else {
                err = string("Undefined config identifier: ") + tok.text;
                out_error_pos = static_cast<uint32_t>(tok.pos);
                return 0;
            }
        }
    }
    auto ast = parseConfigValueExpression(*tokens, out_error_pos, err);
    if (!ast) {
        return 0;
    }
    ConfigRealValue result = evaluateConfigValueExpression(*ast, out_error_pos, err);
    if (!err.empty()) {
        return 0;
    }
    return result;
}

static shared_ptr<VulConfigLib> _instance;

/**
 * @brief Get the singleton instance of the VulConfigLib.
 * @return A shared_ptr to the VulConfigLib instance.
 */
shared_ptr<VulConfigLib> VulConfigLib::getInstance() {
    if (!_instance) {
        _instance = shared_ptr<VulConfigLib>(new VulConfigLib());
    }
    return _instance;
}

/**
 * @brief List all config groups/components in the config library.
 * @param out_groups Output vector to hold the names of all config groups.
 */
void VulConfigLib::listGroups(vector<GroupName> &out_groups) const {
    vector<GroupName> tmp;
    for (const auto &entry : groups) {
        tmp.push_back(entry.first);
    }
    std::sort(tmp.begin(), tmp.end());
    out_groups.swap(tmp);
}

/**
 * @brief List all config items in a given group/component.
 * Output items are sorted by name.
 * @param group_name The component/group name to list config items from.
 * @param out_items Output vector to hold the names of config items.
 * @param out_values Output vector to hold the values of config items.
 * @param out_real_values Output vector to hold the real values of config items.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulConfigLib::listConfigItems(
    const GroupName &group_name,
    vector<ConfigName> &out_items,
    vector<ConfigValue> &out_values,
    vector<ConfigRealValue> &out_real_values
) const {
    auto iter = groups.find(group_name);
    if (iter == groups.end()) {
        return EStr(EItemConfGroupNameNotFound, string("Group not found: ") + group_name);
    }
    const auto &group = iter->second;
    vector<ConfigName> tmp_items;
    for (const auto &item_name : group) {
        auto item_iter = config_items.find(item_name);
        if (item_iter != config_items.end()) {
            tmp_items.push_back(item_name);
        }
    }
    std::sort(tmp_items.begin(), tmp_items.end());
    out_items.clear();
    out_values.clear();
    out_real_values.clear();
    for (const auto &item_name : tmp_items) {
        auto item_iter = config_items.find(item_name);
        if (item_iter != config_items.end()) {
            out_items.push_back(item_name);
            out_values.push_back(item_iter->second.str_value);
            out_real_values.push_back(item_iter->second.real_value);
        }
    }
    return "";
}

/**
 * @brief Get the value string and real value of a config item.
 * @param item_name The name of the config item to get.
 * @param out_value Output parameter to hold the value string of the config item.
 * @param out_real_value Output parameter to hold the integer value of the config item.
 */
ErrorMsg VulConfigLib::getConfigItem(
    const ConfigName &item_name,
    ConfigValue &out_value,
    ConfigRealValue &out_real_value
) const {
    auto iter = config_items.find(item_name);
    if (iter == config_items.end()) {
        return EStr(EItemConfNameNotFound, string("Config item not found: ") + item_name);
    }
    out_value = iter->second.str_value;
    out_real_value = iter->second.real_value;
    return "";
}

/**
 * @brief Get the value string of a config item.
 * @param item_name The name of the config item to get.
 * @param out_value Output parameter to hold the value string of the config item.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulConfigLib::getConfigStrValue(const ConfigName &item_name, ConfigValue &out_value) const {
    auto iter = config_items.find(item_name);
    if (iter == config_items.end()) {
        return EStr(EItemConfNameNotFound, string("Config item not found: ") + item_name);
    }
    out_value = iter->second.str_value;
    return "";
}

/**
 * @brief Calculate the integer value of a config item.
 * @param item_name The name of the config item to get.
 * @param out_value Output parameter to hold the integer value of the config item.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulConfigLib::getConfigRealValue(const ConfigName &item_name, ConfigRealValue &out_value) const {
    auto iter = config_items.find(item_name);
    if (iter == config_items.end()) {
        return EStr(EItemConfNameNotFound, string("Config item not found: ") + item_name);
    }
    out_value = iter->second.real_value;
    return "";
}

/**
 * @brief Get all config items in topological order based on their references.
 * @param out_sorted_items Output vector to hold the names of config items in topological order.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulConfigLib::getAllConfigItemsTopoSort(vector<ConfigName> &out_sorted_items) const {
    unordered_set<ConfigName> all_items;
    for (const auto &entry : config_items) {
        all_items.insert(entry.first);
    }
    vector<ConfigName> out_loop_nodes;
    auto sorted_items_ptr = topologicalSort(all_items, references, out_loop_nodes);
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
 * @brief Insert a new config item into the config library.
 * Duplicate names across groups are not allowed.
 * New group will be created if it does not exist.
 * @param group_name The component/group name the config item belongs to.
 * @param item_name The name of the config item.
 * @param item_value The value of the config item.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulConfigLib::insertConfigItem(const GroupName &group_name, const ConfigName &item_name, const ConfigValue &item_value) {
    if (config_items.find(item_name) != config_items.end()) {
        return EStr(EItemConfNameDup, string("Duplicate config item name: ") + item_name);
    }
    if (!isValidIdentifier(item_name)) {
        return EStr(EItemConfNameInvalid, string("Invalid config item name: ") + item_name);
    }
    if (!isValidIdentifier(group_name)) {
        return EStr(EItemConfGroupNameInvalid, string("Invalid config group name: ") + group_name);
    }
    uint32_t errpos = 0;
    ErrorMsg err;
    auto reference_items = _parseConfigReferences(item_value, errpos, err);
    if (!err.empty()) {
        return EStr(EItemConfValueTokenInvalid, string("Invalid config value grammar at position ") + std::to_string(errpos) + string(": ") + err);
    }
    for (const auto &ref_item : *reference_items) {
        if (config_items.find(ref_item) == config_items.end()) {
            return EStr(EItemConfRefNotFound, string("Config value references undefined item: ") + ref_item);
        }
    }
    ConfigRealValue real_value = _calculateConfigRealValue(item_value, errpos, err);
    if (!err.empty()) {
        return EStr(EItemConfValueGrammerInvalid, string("Error evaluating config value at position ") + std::to_string(errpos) + string(": ") + err);
    }

    // insert item
    ConfigItem new_item;
    new_item.str_value = item_value;
    new_item.real_value = real_value;
    new_item.group = group_name;
    config_items[item_name] = new_item;

    // insert into group
    if (groups.find(group_name) == groups.end()) {
        groups[group_name] = unordered_set<ConfigName>{item_name};
    } else {
        groups[group_name].insert(item_name);
    }

    // update references
    references[item_name] = unordered_set<ConfigName>();
    for (const auto &ref_item : *reference_items) {
        reverse_references[ref_item].insert(item_name);
        references[item_name].insert(ref_item);
    }

    return "";
}

/**
 * @brief Insert a new config group with multiple config items into the config library.
 * Duplicate names across groups are not allowed.
 * New group will be created if it does not exist.
 * @param group_name The component/group name to insert.
 * @param items A map of config item names to their values.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulConfigLib::insertConfigGroup(const GroupName &group_name, const unordered_map<ConfigName, ConfigValue> &items) {
    if (!isValidIdentifier(group_name)) {
        return EStr(EItemConfGroupNameInvalid, string("Invalid config group name: ") + group_name);
    }
    unordered_map<ConfigName, unordered_set<ConfigName>> ingroup_reverse_references;
    unordered_set<ConfigName> ingroup_items;
    for (const auto &entry : items) {
        const ConfigName &item_name = entry.first;
        const ConfigValue &item_value = entry.second;
        if (config_items.find(item_name) != config_items.end()) {
            return EStr(EItemConfNameDup, string("Duplicate config item name: ") + item_name);
        }
        if (!isValidIdentifier(item_name)) {
            return EStr(EItemConfNameInvalid, string("Invalid config item name: ") + item_name);
        }
        uint32_t errpos = 0;
        ErrorMsg err;
        auto reference_items = _parseConfigReferences(item_value, errpos, err);
        if (!err.empty()) {
            return EStr(EItemConfValueTokenInvalid, string("Invalid config value grammar at position ") + std::to_string(errpos) + string(": ") + err);
        }
        for (const auto &ref_item : *reference_items) {
            if (config_items.find(ref_item) == config_items.end()) {
                return EStr(EItemConfRefNotFound, string("Config value references undefined item: ") + ref_item);
            }
            if (items.find(ref_item) != items.end()) {
                ingroup_reverse_references[ref_item].insert(item_name);
            }
        }
        ingroup_items.insert(item_name);
    }
    vector<ConfigName> looped_items;
    auto topo_sorted_items = topologicalSort(ingroup_items, ingroup_reverse_references, looped_items);
    if (!topo_sorted_items) {
        string looped_items_str;
        for (const auto &it : looped_items) {
            if (!looped_items_str.empty()) looped_items_str += ", ";
            looped_items_str += it;
        }
        return EStr(EItemConfRefLooped, string("Cyclic dependency detected in config group: ") + looped_items_str);
    }

    // save context before insertion
    uint64_t snapshot_id = snapshot();
    for (const auto &item_name : *topo_sorted_items) {
        ErrorMsg res = insertConfigItem(group_name, item_name, items.at(item_name));
        if (!res.empty()) {
            // rollback
            rollback(snapshot_id);
            return res;
        }
    }
    commit(snapshot_id);

    return "";
}

/**
 * @brief Rename a config item in the config library.
 * Duplicate names across groups are not allowed.
 * Any references to the old name in other config items will trigger an error unless update_references is true.
 * @param old_name The current name of the config item.
 * @param new_name The new name to assign to the config item.
 * @param update_references Whether to update references in other config items that refer to this item.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulConfigLib::renameConfigItem(const ConfigName &old_name, const ConfigName &new_name, bool update_references) {
    if (old_name == new_name) {
        return "";
    }
    if (config_items.find(old_name) == config_items.end()) {
        return EStr(EItemConfNameNotFound, string("Config item not found: ") + old_name);
    }
    if (config_items.find(new_name) != config_items.end()) {
        return EStr(EItemConfNameDup, string("Duplicate config item name: ") + new_name);
    }
    if (!isValidIdentifier(new_name)) {
        return EStr(EItemConfNameInvalid, string("Invalid config item name: ") + new_name);
    }
    if (!update_references) {
        auto rev_iter = reverse_references.find(old_name);
        if (rev_iter != reverse_references.end() && !rev_iter->second.empty()) {
            return EStr(EItemConfRenameRef, string("Config item '") + old_name + string("' is referenced by other config items"));
        }
    }

    // proceed with renaming
    config_items[new_name] = config_items[old_name];
    config_items.erase(old_name);
    auto &item = config_items[new_name];

    // update groups
    auto group_iter = groups.find(item.group);
    if (group_iter != groups.end()) {
        group_iter->second.erase(old_name);
        group_iter->second.insert(new_name);
    }

    // update references
    auto ref_iter = references.find(old_name);
    if (ref_iter != references.end()) {
        references[new_name] = ref_iter->second;
        references.erase(old_name);
    }
    auto rev_ref_iter = reverse_references.find(old_name);
    if (rev_ref_iter != reverse_references.end()) {
        for (const auto &ref_by_item : rev_ref_iter->second) {
            auto &ref_by_item_entry = config_items[ref_by_item];
            // update value string with word replacement
            ref_by_item_entry.str_value = identifierReplace(
                ref_by_item_entry.str_value,
                old_name,
                new_name
            );
            // update references of ref_by_item_entry
            if (references.find(ref_by_item) != references.end()) {
                references[ref_by_item].erase(old_name);
                references[ref_by_item].insert(new_name);
            }
        }
    }

    // update bundle lib
    VulBundleLib::getInstance()->externalConfigRename(old_name, new_name);
    return "";
}

/**
 * @brief Update the value of an existing config item.
 * @param item_name The name of the config item to update.
 * @param new_value The new value to set for the config item.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulConfigLib::updateConfigItemValue(const ConfigName &item_name, const ConfigValue &new_value) {
    if (config_items.find(item_name) == config_items.end()) {
        return EStr(EItemConfNameNotFound, string("Config item not found: ") + item_name);
    }
    // parse references and evaluate new value
    uint32_t errpos = 0;
    ErrorMsg err;
    auto reference_items = _parseConfigReferences(new_value, errpos, err);
    if (!err.empty()) {
        return EStr(EItemConfValueTokenInvalid, string("Invalid config value grammar at position ") + std::to_string(errpos) + string(": ") + err);
    }
    for (const auto &ref_item : *reference_items) {
        if (config_items.find(ref_item) == config_items.end()) {
            return EStr(EItemConfRefNotFound, string("Config value references undefined item: ") + ref_item);
        }
    }
    ConfigRealValue real_value = _calculateConfigRealValue(new_value, errpos, err);
    if (!err.empty()) {
        return EStr(EItemConfValueGrammerInvalid, string("Error evaluating config value at position ") + std::to_string(errpos) + string(": ") + err);
    }

    // update the config item value and references
    auto &item = config_items[item_name];
    ConfigRealValue old_real_value = item.real_value;
    item.str_value = new_value;
    item.real_value = real_value;

    // update references
    auto old_references = references[item_name];
    unordered_set<ConfigName> new_references;
    for (const auto &ref_item : *reference_items) {
        new_references.insert(ref_item);
    }
    // remove old reverse references
    for (const auto &old_ref_item : old_references) {
        reverse_references[old_ref_item].erase(item_name);
    }
    // add new reverse references
    for (const auto &new_ref_item : new_references) {
        reverse_references[new_ref_item].insert(item_name);
    }
    references[item_name] = new_references;

    if (old_real_value != real_value) {
        // update dependent items' real values (recursive)
        std::queue<ConfigName> to_update;
        for (const auto &dep_item : reverse_references[item_name]) {
            to_update.push(dep_item);
        }
        while (!to_update.empty()) {
            ConfigName current_item_name = to_update.front();
            to_update.pop();
            auto &current_item = config_items[current_item_name];
            ConfigRealValue updated_real_value = _calculateConfigRealValue(current_item.str_value, errpos, err);
            if (!err.empty()) {
                return EStr(EItemConfValueGrammerInvalid, string("Error evaluating config value of dependent item '") + current_item_name + string("' at position ") + std::to_string(errpos) + string(": ") + err);
            }
            if (current_item.real_value != updated_real_value) {
                current_item.real_value = updated_real_value;
                // enqueue its dependents
                for (const auto &dep_item : reverse_references[current_item_name]) {
                    to_update.push(dep_item);
                }
            }
        }
    }

    return "";
}

/**
 * @brief Remove a config item from the config library.
 * Any references to this config item in other config items will trigger an error.
 * @param item_name The name of the config item to remove.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulConfigLib::removeConfigItem(const ConfigName &item_name) {
    if (config_items.find(item_name) == config_items.end()) {
        return EStr(EItemConfNameNotFound, string("Config item not found: ") + item_name);
    }
    auto rev_iter = reverse_references.find(item_name);
    if (rev_iter != reverse_references.end() && !rev_iter->second.empty()) {
        return EStr(EItemConfRemoveRef, string("Config item '") + item_name + string("' is referenced by other config items"));
    }

    // remove references
    auto iter = references.find(item_name);
    if (iter != references.end()) {
        for (const auto &ref_item : iter->second) {
            reverse_references[ref_item].erase(item_name);
        }
        references.erase(item_name);
    }
    reverse_references.erase(item_name);
    // remove from groups
    auto &item = config_items[item_name];
    auto group_iter = groups.find(item.group);
    if (group_iter != groups.end()) {
        group_iter->second.erase(item_name);
        if (group_iter->second.empty()) {
            groups.erase(group_iter);
        }
    }

    config_items.erase(item_name);
    return "";
}

/**
 * @brief Remove an entire config group from the config library.
 * All config items in the group will be removed.
 * @param group_name The name of the config group to remove.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulConfigLib::removeConfigGroup(const GroupName &group_name) {
    auto group_iter = groups.find(group_name);
    if (group_iter == groups.end()) {
        return EStr(EItemConfGroupNameNotFound, string("Config group not found: ") + group_name);
    }

    auto &group_items = group_iter->second;

    // check external references to group items
    for (const auto &item_name : group_items) {
        auto rev_iter = reverse_references.find(item_name);
        if (rev_iter != reverse_references.end()) {
            for (const auto &ref_by_item : rev_iter->second) {
                if (group_items.find(ref_by_item) == group_items.end()) {
                    return EStr(EItemConfRemoveRef, string("Config item '") + item_name + string("' in group '") + group_name + string("' is referenced by other config item: ") + ref_by_item);
                }
            }
        }
    }

    // proceed with removal
    for (const auto &item_name : group_items) {
        auto iter = references.find(item_name);
        if (iter != references.end()) {
            for (const auto &ref_item : iter->second) {
                reverse_references[ref_item].erase(item_name);
            }
            references.erase(item_name);
        }
        reverse_references.erase(item_name);
        config_items.erase(item_name);
    }
    groups.erase(group_iter);

    return "";
}

/**
 * @brief Calculate the integer value of a config expression string.
 * @param value The config expression string to calculate.
 * @param out_real_value Output parameter to hold the calculated integer value.
 * @param seen_configs Set of config names seen during the calculation to detect cycles.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulConfigLib::calculateConfigExpression(
    const ConfigValue &value,
    ConfigRealValue &out_real_value,
    unordered_set<ConfigName> &seen_configs
) const {
    uint32_t errpos = 0;
    ErrorMsg err;
    auto tokens = config_parser::tokenizeConfigValueExpression(value, errpos, err);
    if (!tokens) {
        return EStr(EItemConfValueTokenInvalid, string("Invalid token grammar at position ") + std::to_string(errpos) + string(": ") + err + string(": ") + value);
    }
    // replace Identifier tokens with their values
    for (auto &tok : *tokens) {
        if (tok.type == config_parser::TokenType::Identifier) {
            // lookup value
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

/**
 * @brief Create a snapshot of the current config library state.
 * @return A snapshot ID to identify the snapshot.
 */
uint64_t VulConfigLib::snapshot() {
    static uint64_t next_snapshot_id = 1;
    uint64_t snapshot_id = next_snapshot_id++;
    snapshots[snapshot_id] = SnapshotEntry{
        config_items,
        references,
        reverse_references,
        groups
    };
    return snapshot_id;
}

/**
 * @brief Rollback to the specified snapshot of the config library state.
 * @param snapshot_id The snapshot ID to rollback to.
 */
void VulConfigLib::rollback(uint64_t snapshot_id) {
    auto iter = snapshots.find(snapshot_id);
    if (iter != snapshots.end()) {
        config_items = iter->second.config_items;
        references = iter->second.references;
        reverse_references = iter->second.reverse_references;
        groups = iter->second.groups;
    }
    commit(snapshot_id);
}

/**
 * @brief Commit the specified snapshot of the config library state.
 * @param snapshot_id The snapshot ID to commit.
 */
void VulConfigLib::commit(uint64_t snapshot_id) {
    // erase snapshots after and including the committed one
    for (auto it = snapshots.begin(); it != snapshots.end(); ) {
        if (it->first >= snapshot_id) {
            it = snapshots.erase(it);
        } else {
            ++it;
        }
    }
}
