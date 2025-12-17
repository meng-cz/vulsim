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
};

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

    /**
     * @brief Build the bundle reference tree (bidirectional) for a given bundle.
     * @param root_bundle_name The name of the root bundle.
     * @param out_root_node Output parameter to hold the root node of the tree.
     */
    void buildBundleReferenceTree(const BundleName &root_bundle_name, shared_ptr<BundleTreeBidirectionalNode> &out_root_node) const;

    /**
     * @brief List all bundle tags in the bundle library.
     * @param out_tags Output vector to hold the names of all bundle tags.
     */
    void listTags(vector<BundleTag> &out_tags) const;

    /**
     * @brief List all bundle names associated with a given tag.
     * @param tag The bundle tag to list bundles from.
     * @param out_bundles Output vector to hold the names of all bundles associated with the tag.
     */
    ErrorMsg listBundlesByTag(const BundleTag &tag, vector<BundleName> &out_bundles) const;

    /**
     * @brief Get the bundle definition for a given bundle name.
     * @param bundle_name The name of the bundle to get.
     * @param out_bundle_item Output parameter to hold the VulBundleItem definition.
     * @param out_tags Output parameter to hold the tags associated with this bundle.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg getBundleDefinition(const BundleName &bundle_name, VulBundleItem &out_bundle_item, unordered_set<BundleTag> &out_tags) const;

    /**
     * @brief Get all bundle definitions in topological order based on their references.
     * @param out_sorted_bundles Output vector to hold the BundleName in topological order.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg getAllBundlesTopoSort(vector<BundleName> &out_sorted_bundles) const;

protected:
    /**
     * @brief Insert a new bundle definition into the bundle library.
     * If a bundle with the same name already exists, check if the definition matches.
     * @param bundle_item The VulBundleItem to insert.
     * @param tags The tag to associate with this bundle.
     * @return An ErrorMsg indicating success or failure.
     */
    ErrorMsg insertBundle(const VulBundleItem &bundle_item, const BundleTag &tags);
public:

    /**
     * @brief Insert a new bundle definition into the bundle library with the default tag.
     * If a bundle with the same name already exists, check if the definition matches.
     * @param bundle_item The VulBundleItem to insert.
     * @return An ErrorMsg indicating success or failure.
     */
    inline ErrorMsg insertBundle(const VulBundleItem &bundle_item) {
        return insertBundle(bundle_item, DefaultTag);
    }

    /**
     * @brief Insert multiple new bundle definitions into the bundle library.
     * If a bundle with the same name already exists, check if the definition matches.
     * @param bundle_items The vector of VulBundleItem to insert.
     * @param tags The tag to associate with these bundles.
     * @return An ErrorMsg indicating success or failure.
     */
    ErrorMsg insertBundles(const vector<VulBundleItem> &bundle_items, const BundleTag &tags);

protected:
    /**
     * @brief Remove a bundle from the bundle library with a specific tag.
     * If the bundle has no more tags after removal, it will be removed from the library.
     * @param bundle_name The name of the bundle to remove.
     * @param tags The tag to remove the bundle from.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg removeBundle(const BundleName &bundle_name, const BundleTag &tags);

public:
    /**
     * @brief Remove a bundle from the bundle library with the default tag.
     * If the bundle has no more tags after removal, it will be removed from the library.
     * @param bundle_name The name of the bundle to remove.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    inline ErrorMsg removeBundle(const BundleName &bundle_name) {
        return removeBundle(bundle_name, DefaultTag);
    }

    /**
     * @brief Rename a bundle in the bundle library.
     * If update_references is true, all bundle definitions that reference the old name will be updated.
     * Otherwise, an error will be raised if any bundle references the old name.
     * @param old_name The current name of the bundle.
     * @param new_name The new name to assign to the bundle.
     * @param update_references If true, update all bundle definitions that reference the old name.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg renameBundle(const BundleName &old_name, const BundleName &new_name, bool update_references = true);

    /**
     * @brief Update the definition of an existing bundle in the bundle library.
     * Only bundles with the default tag can be updated.
     * @param bundle_item The new VulBundleItem definition.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg updateBundleDefinition(const VulBundleItem &bundle_item);

    /**
     * @brief Remove a tag from the bundle library.
     * All bundles associated with this tag will have the tag removed.
     * A bundle with no tags will be removed from the library.
     * @param tag The tag to remove.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg removeTag(const BundleTag &tag);

    /**
     * @brief Notify the bundle library that a config item has been renamed.
     * Update any bundle definitions that reference the old config name.
     * @param old_name The old config item name.
     * @param new_name The new config item name.
     */
    void externalConfigRename(const ConfigName &old_name, const ConfigName &new_name);

    /**
     * @brief Check if a config item is referenced by any bundle in the bundle library.
     * @param conf_name The config item name to check.
     */
    inline unique_ptr<unordered_set<BundleName>> externalConfigReferenced(const ConfigName &conf_name) const {
        auto iter = conf_bundles.find(conf_name);
        if (iter != conf_bundles.end() && !iter->second.empty()) {
            return std::make_unique<unordered_set<BundleName>>(iter->second);
        } else {
            return nullptr;
        }
    }

    /**
     * @brief Create a snapshot of the current bundle library state.
     * @return A snapshot ID to identify the snapshot.
     */
    uint64_t snapshot();

    /**
     * @brief Rollback to the last snapshot of the bundle library state.
     * @param snapshot_id The snapshot ID to rollback to.
     */
    void rollback(uint64_t snapshot_id);

    /**
     * @brief Commit the last snapshot of the bundle library state.
     * @param snapshot_id The snapshot ID to commit.
     */
    void commit(uint64_t snapshot_id);

    /**
     * @brief Extract bundle references and config usages from a bundle definition.
     * @param bundle_item The VulBundleItem to analyze.
     * @param out_references Output set to hold the names of referenced bundles.
     * @param out_confs Output set to hold the names of used config items.
     * @return An ErrorMsg indicating failure, empty if success.
     */
    ErrorMsg extractBundleReferencesAndConfs(
        const VulBundleItem &bundle_item,
        unordered_set<BundleName> &out_references,
        unordered_set<ConfigName> &out_confs
    ) const;

    /**
     * @brief Extract bundle references from a bundle definition without checking.
     * @param bundle_item The VulBundleItem to analyze.
     * @param out_references Output set to hold the names of referenced bundles.
     */
    void extractBundleReferences(
        const VulBundleItem &bundle_item,
        unordered_set<BundleName> &out_references
    ) const;

protected:

    typedef struct {
        VulBundleItem               item;
        unordered_set<BundleTag>    tags;
        unordered_set<BundleName>   references;
        unordered_set<BundleName>   reverse_references;
        unordered_set<ConfigName>   confs;
    } BundleEntry;

    unordered_map<BundleName, BundleEntry> bundles; // bundle name -> BundleEntry

    unordered_map<BundleTag, unordered_set<BundleName>> tag_bundles; // tag -> set of bundle names
    unordered_map<ConfigName, unordered_set<BundleName>> conf_bundles; // config -> set of bundle names using it


    typedef struct {
        unordered_map<BundleName, BundleEntry> bundles;
        unordered_map<BundleTag, unordered_set<BundleName>> tag_bundles;
        unordered_map<ConfigName, unordered_set<BundleName>> conf_bundles;
    } SnapshotEntry;
    unordered_map<uint64_t, SnapshotEntry> snapshots;

    /**
     * @brief Check if two bundle definitions are the same.
     * @param a The first VulBundleItem to compare.
     * @param b The second VulBundleItem to compare.
     * @return true if the definitions are the same, false otherwise.
     */
    bool _isBundleSameDefinition(const VulBundleItem &a, const VulBundleItem &b) const;

};


