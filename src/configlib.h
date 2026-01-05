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
} VulConfigItem;

class ConfigTreeBidirectionalNode {
public:
    vector<shared_ptr<ConfigTreeBidirectionalNode>>  parents;
    vector<shared_ptr<ConfigTreeBidirectionalNode>>  children;
    VulConfigItem   item;
};

class VulConfigLib {

public:

    const GroupName DefaultGroupName = "__default__";

    inline void clear() {
        config_items.clear();
    }

    /**
     * @brief Check if a config item name already exists in the config library.
     * @param name The config item name to check.
     */
    inline bool checkNameConflict(const ConfigName &name) const {
        return config_items.find(name) != config_items.end();
    }

        /**
     * @brief Get the value string and real value of a config item.
     * @param item_name The name of the config item to get.
     * @param out_item Output parameter to hold the VulConfigItem.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    inline ErrorMsg getConfigItem(const ConfigName &item_name, VulConfigItem &out_item) const {
        auto iter = config_items.find(item_name);
        if (iter == config_items.end()) {
            return EStr(EItemConfRefNotFound, string("Config item not found: ") + item_name);
        }
        out_item = iter->second.item;
        return "";
    }

    /**
     * @brief Get all config items in topological order based on their references.
     * @param out_sorted_items Output vector to hold the names of config items in topological order.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg getAllConfigItemsTopoSort(vector<ConfigName> &out_sorted_items) const;

    /**
     * @brief Calculate the integer value of a config expression string.
     * @param value The config expression string to calculate.
     * @param overrides A map of config name to override config values.
     * @param out_real_value Output parameter to hold the calculated integer value.
     * @param seen_coonfigs Set of config names seen during the calculation to detect cycles.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg calculateConfigExpression(
        const ConfigValue &value,
        const unordered_map<ConfigName, ConfigRealValue> &overrides,
        ConfigRealValue &out_real_value,
        unordered_set<ConfigName> &seen_configs
    ) const;

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
    ) const {
        unordered_map<ConfigName, ConfigRealValue> dummy_overrides;
        return calculateConfigExpression(value, dummy_overrides, out_real_value, seen_configs);
    }

    typedef struct {
        VulConfigItem       item;
        GroupName           group;
        ConfigRealValue     real_value;
        unordered_set<ConfigName>   references;
        unordered_set<ConfigName>   reverse_references;
    } ConfigEntry;

    unordered_map<ConfigName, ConfigEntry> config_items; // config item name -> ConfigEntry

};

