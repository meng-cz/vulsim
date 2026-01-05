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
#include "configlib.h"
#include "type.h"

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>

using std::vector;
using std::string;
using std::shared_ptr;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;

typedef string BundleName;
typedef string BMemberName;
typedef string BMemberType;
typedef string BundleTag;

typedef struct {
    BMemberName         name;
    ConfigValue         value;
    Comment             comment;
} VulBundleEnumMember;

typedef struct {
    BMemberName         name;
    ConfigValue         value; // only for basic types and uint types, default zero-initialized
    Comment             comment;

    BMemberType         type;
    ConfigValue         uint_length; // only for uint types

    vector<ConfigValue> dims;
} VulBundleMember;

class VulBundleItem {
public:
    BundleName                      name;
    Comment                         comment;
    vector<VulBundleMember>         members;
    vector<VulBundleEnumMember>     enum_members;   // if not empty, other members must be empty
    bool                            is_alias = false; // if true, all other fields only contain single member: alias_target

    string toMemberJson() const;
    void fromMemberJson(const string &json_str);

    string checkAndExtractReferences(unordered_set<BundleName> &out_bundle_refs, unordered_set<ConfigName> &out_config_refs) const;

};

bool operator==(const VulBundleItem &a, const VulBundleItem &b);
inline bool operator!=(const VulBundleItem &a, const VulBundleItem &b) {
    return !(a == b);
}

class BundleTreeBidirectionalNode {
public:
    vector<shared_ptr<BundleTreeBidirectionalNode>>  parents;
    vector<shared_ptr<BundleTreeBidirectionalNode>>  children;
    BundleName  name;
    Comment     comment;
};

class VulBundleLib {

public:

    const BundleTag DefaultTag = string("__default__");

    /**
     * @brief Check if a bundle name already exists in the bundle library.
     * @param name The bundle name to check.
     */
    inline bool checkNameConflict(const BundleName &name) const {
        return bundles.find(name) != bundles.end();
    }

    inline void clear() {
        bundles.clear();
    }

    /**
     * @brief Get the bundle definition for a given bundle name.
     * @param bundle_name The name of the bundle to get.
     * @param out_bundle_item Output parameter to hold the VulBundleItem definition.
     * @param out_tags Output parameter to hold the tags associated with this bundle.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    inline ErrorMsg getBundleDefinition(const BundleName &bundle_name, VulBundleItem &out_bundle_item, unordered_set<BundleTag> &out_tags) const {
        auto iter = bundles.find(bundle_name);
        if (iter == bundles.end()) {
            return EStr(EItemBundNameNotFound, string("Bundle definition not found: ") + bundle_name);
        }
        out_bundle_item = iter->second.item;
        out_tags = iter->second.tags;
        return "";
    }

    /**
     * @brief Get all bundle definitions in topological order based on their references.
     * @param out_sorted_bundles Output vector to hold the BundleName in topological order.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg getAllBundlesTopoSort(vector<BundleName> &out_sorted_bundles) const;

    typedef struct {
        VulBundleItem               item;
        unordered_set<BundleTag>    tags;
        unordered_set<BundleName>   references;
        unordered_set<BundleName>   reverse_references;
        unordered_set<ConfigName>   confs;
    } BundleEntry;

    unordered_map<BundleName, BundleEntry> bundles; // bundle name -> BundleEntry

};


