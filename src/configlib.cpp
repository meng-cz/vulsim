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
 * @brief Build the config reference tree (bidirectional) for a given config item.
 * @param root_config_name The name of the root config item.
 * @param out_root_node Output parameter to hold the root node of the tree.
 */
void VulConfigLib::buildConfigReferenceTree(const ConfigName &root_config_name, shared_ptr<ConfigTreeBidirectionalNode> &out_root_node) const {
    out_root_node.reset();

    // Root must exist
    auto it_root = config_items.find(root_config_name);
    if (it_root == config_items.end()) {
        out_root_node = nullptr;
        return;
    }

    // Create root
    auto root_node = std::make_shared<ConfigTreeBidirectionalNode>();
    root_node->item = it_root->second.item;

    // Downward: build children-only (dependencies this config references)
    std::function<void(const ConfigName&, const std::shared_ptr<ConfigTreeBidirectionalNode>&, std::unordered_set<ConfigName>&)> buildDown;
    buildDown = [&](const ConfigName &name,
                    const std::shared_ptr<ConfigTreeBidirectionalNode> &node,
                    std::unordered_set<ConfigName> &path) {
        auto it = config_items.find(name);
        if (it == config_items.end()) return;
        const auto &entry = it->second;
        for (const auto &dep : entry.references) {
            if (path.count(dep)) {
                // Cycle on this path: create a leaf and stop deeper
                auto cyc = std::make_shared<ConfigTreeBidirectionalNode>();
                auto jt = config_items.find(dep);
                if (jt != config_items.end()) {
                    cyc->item = jt->second.item;
                } else {
                    // ignore non-existing
                    continue;
                }
                node->children.push_back(cyc);
                continue;
            }
            auto jt = config_items.find(dep);
            if (jt == config_items.end()) continue; // ignore library-external
            auto child = std::make_shared<ConfigTreeBidirectionalNode>();
            child->item = jt->second.item;
            node->children.push_back(child); // do not populate child->parents for downward tree

            auto next_path = path;
            next_path.insert(dep);
            buildDown(dep, child, next_path);
        }
    };

    // Upward: build parents-only (configs that depend on this one)
    std::function<void(const ConfigName&, const std::shared_ptr<ConfigTreeBidirectionalNode>&, std::unordered_set<ConfigName>&)> buildUp;
    buildUp = [&](const ConfigName &name,
                  const std::shared_ptr<ConfigTreeBidirectionalNode> &node,
                  std::unordered_set<ConfigName> &path) {
        auto it = config_items.find(name);
        if (it == config_items.end()) return;
        const auto &entry = it->second;
        for (const auto &par : entry.reverse_references) {
            if (path.count(par)) {
                // Cycle on this path: create a leaf and stop deeper
                auto cyc = std::make_shared<ConfigTreeBidirectionalNode>();
                auto jt = config_items.find(par);
                if (jt != config_items.end()) {
                    cyc->item = jt->second.item;
                } else {
                    // ignore non-existing
                    continue;
                }
                node->parents.push_back(cyc);
                continue;
            }
            auto jt = config_items.find(par);
            if (jt == config_items.end()) continue; // ignore library-external
            auto parent = std::make_shared<ConfigTreeBidirectionalNode>();
            parent->item = jt->second.item;
            node->parents.push_back(parent); // do not populate parent->children for upward tree

            auto next_path = path;
            next_path.insert(par);
            buildUp(par, parent, next_path);
        }
    };

    {
        std::unordered_set<ConfigName> down_path; down_path.insert(root_config_name);
        buildDown(root_config_name, root_node, down_path);
    }
    {
        std::unordered_set<ConfigName> up_path; up_path.insert(root_config_name);
        buildUp(root_config_name, root_node, up_path);
    }

    out_root_node = root_node;
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
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulConfigLib::listConfigItems(const GroupName &group_name, vector<VulConfigItem> &out_items) const {
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
    for (const auto &item_name : tmp_items) {
        auto item_iter = config_items.find(item_name);
        if (item_iter != config_items.end()) {
            out_items.push_back(item_iter->second.item);
        }
    }
    return "";
}

/**
 * @brief Get the value string and real value of a config item.
 * @param item_name The name of the config item to get.
 * @param out_item Output parameter to hold the VulConfigItem.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulConfigLib::getConfigItem(const ConfigName &item_name, VulConfigItem &out_item) const{
    auto iter = config_items.find(item_name);
    if (iter == config_items.end()) {
        return EStr(EItemConfNameNotFound, string("Config item not found: ") + item_name);
    }
    out_item = iter->second.item;
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
    out_value = iter->second.item.value;
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
 * @brief Insert a new config item into the config library.
 * Duplicate names across groups are not allowed.
 * New group will be created if it does not exist.
 * @param group_name The component/group name the config item belongs to.
 * @param config_item The VulConfigItem to insert.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulConfigLib::insertConfigItem(const GroupName &group_name, const VulConfigItem &config_item) {
    if (config_items.find(config_item.name) != config_items.end()) {
        return EStr(EItemConfNameDup, string("Duplicate config item name: ") + config_item.name);
    }
    if (!isValidIdentifier(config_item.name)) {
        return EStr(EItemConfNameInvalid, string("Invalid config item name: ") + config_item.name);
    }
    if (!isValidIdentifier(group_name)) {
        return EStr(EItemConfGroupNameInvalid, string("Invalid config group name: ") + group_name);
    }
    ConfigRealValue real_value = 0;
    unordered_set<ConfigName> seen_configs;
    ErrorMsg err = calculateConfigExpression(config_item.value, real_value, seen_configs);
    if (!err.empty()) {
        return err;
    }

    // insert item
    config_items[config_item.name] = ConfigEntry {
        config_item,
        group_name,
        real_value,
        seen_configs,
        unordered_set<ConfigName>{}
    };

    // insert into group
    if (groups.find(group_name) == groups.end()) {
        groups[group_name] = unordered_set<ConfigName>{config_item.name};
    } else {
        groups[group_name].insert(config_item.name);
    }


    return "";
}

/**
 * @brief Insert a new config group with multiple config items into the config library.
 * Duplicate names across groups are not allowed.
 * New group will be created if it does not exist.
 * @param group_name The component/group name to insert.
 * @param items A vector of VulConfigItems to insert.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulConfigLib::insertConfigGroup(const GroupName &group_name, const vector<VulConfigItem> &items) {
    if (!isValidIdentifier(group_name)) {
        return EStr(EItemConfGroupNameInvalid, string("Invalid config group name: ") + group_name);
    }
    unordered_map<ConfigName, unordered_set<ConfigName>> ingroup_reverse_references;
    unordered_set<ConfigName> ingroup_items;
    unordered_map<ConfigName, const VulConfigItem*> item_map;
    for (const auto &entry : items) {
        const ConfigName &item_name = entry.name;
        if (config_items.find(item_name) != config_items.end()) {
            return EStr(EItemConfNameDup, string("Duplicate config item name: ") + item_name);
        }
        if (!isValidIdentifier(item_name)) {
            return EStr(EItemConfNameInvalid, string("Invalid config item name: ") + item_name);
        }
        ingroup_items.insert(item_name);
        item_map[item_name] = &entry;
    }
    for (const auto &entry : items) {
        uint32_t errpos = 0;
        ErrorMsg err;
        auto reference_items = config_parser::parseReferencedIdentifier(entry.value, errpos, err);
        if (!err.empty()) {
            return EStr(EItemConfValueTokenInvalid, string("Invalid config value grammar at position ") + std::to_string(errpos) + string(": ") + err);
        }
        for (const auto &ref_item : *reference_items) {
            if (config_items.find(ref_item) == config_items.end()) {
                return EStr(EItemConfRefNotFound, string("Config value references undefined item: ") + ref_item);
            }
            if (ingroup_items.find(ref_item) != ingroup_items.end()) {
                ingroup_reverse_references[ref_item].insert(entry.name);
            }
        }
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
        ErrorMsg res = insertConfigItem(group_name, *item_map[item_name]);
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
    auto iter = config_items.find(old_name);
    if (iter == config_items.end()) {
        return EStr(EItemConfNameNotFound, string("Config item not found: ") + old_name);
    }
    auto &item = iter->second;
    if (config_items.find(new_name) != config_items.end()) {
        return EStr(EItemConfNameDup, string("Duplicate config item name: ") + new_name);
    }
    if (!isValidIdentifier(new_name)) {
        return EStr(EItemConfNameInvalid, string("Invalid config item name: ") + new_name);
    }
    if (!update_references) {;
        if (item.reverse_references.size() > 0) {
            string ref_confs;
            for (const auto &ref_by_item : item.reverse_references) {
                if (!ref_confs.empty()) ref_confs += ", ";
                ref_confs += ref_by_item;
            }
            return EStr(EItemConfRenameRef, string("Config item '") + old_name + string("' is referenced by other config items: ") + ref_confs);
        }
    }

    // proceed with renaming

    // update groups
    auto group_iter = groups.find(item.group);
    if (group_iter != groups.end()) {
        group_iter->second.erase(old_name);
        group_iter->second.insert(new_name);
    }

    // update references
    for (const auto &ref_item : item.references) {
        auto &ref_item_entry = config_items[ref_item];
        ref_item_entry.reverse_references.erase(old_name);
        ref_item_entry.reverse_references.insert(new_name);
    }
    // update reverse references
    for (const auto &ref_by_item : item.reverse_references) {
        auto &ref_by_item_entry = config_items[ref_by_item];
        ref_by_item_entry.references.erase(old_name);
        ref_by_item_entry.references.insert(new_name);
        ref_by_item_entry.item.value = identifierReplace(
            ref_by_item_entry.item.value,
            old_name,
            new_name
        );
    }

    // rename the item
    config_items[new_name] = item;
    config_items.erase(old_name);

    // update bundle lib
    VulBundleLib::getInstance()->externalConfigRename(old_name, new_name);
    return "";
}

/**
 * @brief Update the value of an existing config item.
 * @param item_name The name of the config item to update.
 * @param new_item The new VulConfigItem to set for the config item.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulConfigLib::updateConfigItemValue(const ConfigName &item_name, const VulConfigItem &new_item) {
    auto iter = config_items.find(item_name);
    if (iter == config_items.end()) {
        return EStr(EItemConfNameNotFound, string("Config item not found: ") + item_name);
    }
    auto &entry = iter->second;
    auto &item = entry.item;

    if (item.value == new_item.value) {
        // no change
        item.comment = new_item.comment;
        return "";
    }

    // parse references and evaluate new value
    ConfigRealValue real_value = 0;
    unordered_set<ConfigName> seen_configs;
    ErrorMsg err = calculateConfigExpression(new_item.value, real_value, seen_configs);
    if (!err.empty()) {
        return err;
    }

    // update
    ConfigRealValue old_real_value = entry.real_value;
    item.value = new_item.value;
    item.comment = new_item.comment;
    entry.real_value = real_value;

    // update references
    auto old_references = entry.references;
    // remove old reverse references
    for (const auto &old_ref_item : old_references) {
        config_items[old_ref_item].reverse_references.erase(item_name);
    }
    // add new reverse references
    for (const auto &new_ref_item : seen_configs) {
        config_items[new_ref_item].reverse_references.insert(item_name);
    }
    entry.references = seen_configs;

    if (old_real_value != real_value) {
        // update dependent items' real values (recursive)
        std::queue<ConfigName> to_update;
        for (const auto &dep_item : entry.reverse_references) {
            to_update.push(dep_item);
        }
        while (!to_update.empty()) {
            ConfigName current_item_name = to_update.front();
            to_update.pop();
            auto &current_item = config_items[current_item_name];
            ConfigRealValue updated_real_value = 0;
            unordered_set<ConfigName> dummy_seen;
            err = calculateConfigExpression(current_item.item.value, updated_real_value, dummy_seen);
            if (!err.empty()) {
                return err + ", in dependent config item: " + current_item_name;
            }
            if (current_item.real_value != updated_real_value) {
                current_item.real_value = updated_real_value;
                // enqueue its dependents
                for (const auto &dep_item : current_item.reverse_references) {
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
    auto iter = config_items.find(item_name);
    if (iter == config_items.end()) {
        return EStr(EItemConfNameNotFound, string("Config item not found: ") + item_name);
    }
    auto &entry = iter->second;
    if (!entry.reverse_references.empty()) {
        string ref_confs;
        for (const auto &ref_by_item : entry.reverse_references) {
            if (!ref_confs.empty()) ref_confs += ", ";
            ref_confs += ref_by_item;
        }
        return EStr(EItemConfRemoveRef, string("Config item '") + item_name + string("' is referenced by other config items: ") + ref_confs);
    }
    auto bundlelib = VulBundleLib::getInstance();
    auto referencing_bundles = bundlelib->externalConfigReferenced(item_name);
    if (referencing_bundles) {
        string bundle_list;
        for (const auto &bundlename : *referencing_bundles) {
            if (!bundle_list.empty()) {
                bundle_list += ", ";
            }
            bundle_list += bundlename;
        }
        return EStr(EItemConfRemoveRef, string("Config item '") + item_name + string("' is referenced by bundle definitions: ") + bundle_list);
    }

    // remove references
    for (const auto &ref_item : entry.references) {
        config_items[ref_item].reverse_references.erase(item_name);
    }
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
    auto bundlelib = VulBundleLib::getInstance();

    // check external references to group items
    for (const auto &item_name : group_items) {
        for (const auto &ref_by_item : config_items[item_name].reverse_references) {
            if (group_items.find(ref_by_item) == group_items.end()) {
                return EStr(EItemConfRemoveRef, string("Config item '") + item_name + string("' in group '") + group_name + string("' is referenced by other config item: ") + ref_by_item);
            }
        }
        auto referencing_bundles = bundlelib->externalConfigReferenced(item_name);
        if (referencing_bundles) {
            string bundle_list;
            for (const auto &bundlename : *referencing_bundles) {
                if (!bundle_list.empty()) {
                    bundle_list += ", ";
                }
                bundle_list += bundlename;
            }
            return EStr(EItemConfRemoveRef, string("Config item '") + item_name + string("' in group '") + group_name + string("' is referenced by bundle definitions: ") + bundle_list);
        }
    }

    // proceed with removal
    for (const auto &item_name : group_items) {
        // remove references
        for (const auto &ref_item : config_items[item_name].references) {
            config_items[ref_item].reverse_references.erase(item_name);
        }
        config_items.erase(item_name);
    }
    groups.erase(group_iter);

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
    ErrorMsg err;
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

/**
 * @brief Create a snapshot of the current config library state.
 * @return A snapshot ID to identify the snapshot.
 */
uint64_t VulConfigLib::snapshot() {
    static uint64_t next_snapshot_id = 1;
    uint64_t snapshot_id = next_snapshot_id++;
    snapshots[snapshot_id] = SnapshotEntry{
        config_items,
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
