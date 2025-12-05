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

#pragma once

#include "errormsg.hpp"
#include "type.h"

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>

using std::pair;
using std::vector;
using std::string;
using std::unordered_map;
using std::unordered_set;
using std::shared_ptr;
using std::unique_ptr;

typedef int64_t ConfigRealValue;
typedef string GroupName;
typedef string ConfigName;
typedef string ConfigValue;

typedef struct {
    ConfigName      name;
    ConfigValue     value;
    Comment         comment;
    bool            is_external; // whether this config item is imported from external definition
} VulConfigItem;

class VulConfigLib {

public:

    const GroupName DefaultGroupName = "__default__";

    /**
     * @brief Get the singleton instance of the VulConfigLib.
     * @return A shared_ptr to the VulConfigLib instance.
     */
    static shared_ptr<VulConfigLib> getInstance();

    /**
     * @brief Check if a config item name already exists in the config library.
     * @param name The config item name to check.
     */
    inline bool checkNameConflict(const ConfigName &name) const {
        return config_items.find(name) != config_items.end();
    }

    /**
     * @brief List all config groups/components in the config library.
     * @param out_groups Output vector to hold the names of all config groups.
     */
    void listGroups(vector<GroupName> &out_groups) const;

    /**
     * @brief List all config items in a given group/component.
     * Output items are sorted by name.
     * @param group_name The component/group name to list config items from.
     * @param out_items Output vector to hold the names of config items.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg listConfigItems(const GroupName &group_name, vector<VulConfigItem> &out_items) const;

    /**
     * @brief Get the value string and real value of a config item.
     * @param item_name The name of the config item to get.
     * @param out_item Output parameter to hold the VulConfigItem.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg getConfigItem(const ConfigName &item_name, VulConfigItem &out_item) const;

    /**
     * @brief Get all config items in topological order based on their references.
     * @param out_sorted_items Output vector to hold the names of config items in topological order.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg getAllConfigItemsTopoSort(vector<ConfigName> &out_sorted_items) const;

    /**
     * @brief Get the value string of a config item.
     * @param item_name The name of the config item to get.
     * @param out_value Output parameter to hold the value string of the config item.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg getConfigStrValue(const ConfigName &item_name, ConfigValue &out_value) const;

    /**
     * @brief Calculate the integer value of a config item.
     * @param item_name The name of the config item to get.
     * @param out_value Output parameter to hold the integer value of the config item.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg getConfigRealValue(const ConfigName &item_name, ConfigRealValue &out_value) const;


    /**
     * @brief Insert a new config item into the config library.
     * Duplicate names across groups are not allowed.
     * New group will be created if it does not exist.
     * @param group_name The component/group name the config item belongs to.
     * @param config_item The VulConfigItem to insert.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg insertConfigItem(const GroupName &group_name, const VulConfigItem &config_item);

    /**
     * @brief Insert a new config item into the config library.
     * Duplicate names across groups are not allowed.
     * @param config_item The VulConfigItem to insert.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    inline ErrorMsg insertConfigItem(const VulConfigItem &config_item) {
        return insertConfigItem(DefaultGroupName, config_item);
    }

    /**
     * @brief Insert a new config group with multiple config items into the config library.
     * Duplicate names across groups are not allowed.
     * New group will be created if it does not exist.
     * @param group_name The component/group name to insert.
     * @param items A vector of VulConfigItems to insert.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg insertConfigGroup(const GroupName &group_name, const vector<VulConfigItem> &items);

    /**
     * @brief Rename a config item in the config library.
     * Duplicate names across groups are not allowed.
     * Any references to the old name in other config items will trigger an error unless update_references is true.
     * @param old_name The current name of the config item.
     * @param new_name The new name to assign to the config item.
     * @param update_references Whether to update references in other config items that refer to this item.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg renameConfigItem(const ConfigName &old_name, const ConfigName &new_name, bool update_references = true);

    /**
     * @brief Update the value of an existing config item.
     * @param item_name The name of the config item to update.
     * @param new_item The new VulConfigItem to set for the config item.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg updateConfigItemValue(const ConfigName &item_name, const VulConfigItem &new_item);

    /**
     * @brief Remove a config item from the config library.
     * Any references to this config item in other config items will trigger an error.
     * @param item_name The name of the config item to remove.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg removeConfigItem(const ConfigName &item_name);

    /**
     * @brief Remove an entire config group from the config library.
     * All config items in the group will be removed.
     * @param group_name The name of the config group to remove.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg removeConfigGroup(const GroupName &group_name);

    /**
     * @brief Calculate the integer value of a config expression string.
     * @param value The config expression string to calculate.
     * @param out_real_value Output parameter to hold the calculated integer value.
     * @param seen_coonfigs Set of config names seen during the calculation to detect cycles.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg calculateConfigExpression(
        const ConfigValue &value,
        ConfigRealValue &out_real_value,
        unordered_set<ConfigName> &seen_configs
    ) const;

    /**
     * @brief Create a snapshot of the current config library state.
     * @return A snapshot ID to identify the snapshot.
     */
    uint64_t snapshot();

    /**
     * @brief Rollback to the specified snapshot of the config library state.
     * @param snapshot_id The snapshot ID to rollback to.
     */
    void rollback(uint64_t snapshot_id);

    /**
     * @brief Commit the specified snapshot of the config library state.
     * @param snapshot_id The snapshot ID to commit.
     */
    void commit(uint64_t snapshot_id);

protected:

    VulConfigLib() = default;

    typedef struct {
        VulConfigItem       item;
        GroupName           group;
        ConfigRealValue     real_value;
        unordered_set<ConfigName>   references;
        unordered_set<ConfigName>   reverse_references;
    } ConfigEntry;

    unordered_map<ConfigName, ConfigEntry> config_items; // config item name -> ConfigEntry
    unordered_map<GroupName, unordered_set<ConfigName>> groups; // group name -> set of config item names
    
    struct SnapshotEntry {
        unordered_map<ConfigName, ConfigEntry> config_items;
        unordered_map<GroupName, unordered_set<ConfigName>> groups;
    };
    unordered_map<uint64_t, SnapshotEntry> snapshots;

};

