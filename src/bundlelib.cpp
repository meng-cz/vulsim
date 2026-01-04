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

#include "bundlelib.h"
#include "errormsg.hpp"
#include "toposort.hpp"
#include "type.h"
#include "configexpr.hpp"

#include <algorithm>
#include <functional>

using std::make_unique;
using std::make_shared;

#include "json.hpp"
using nlohmann::json;

/**
 * json format:
 * {
 *   "members": [
 *    {
 *    "name": "...",
 *    "comment": "...",
 *    "type": "...",
 *    "value": "...",
 *    "uint_length": "...",
 *    "dims": ["...", "...", ...]
 *   },
 *   "enum_members": [
 *   {
 *   "name": "...",
 *   "comment": "...",
 *   "value": "..."
 *   },
 *   "is_alias": true/false
 * }
 */

string VulBundleItem::toMemberJson() const {
    json j;
    j["is_alias"] = is_alias;
    j["members"] = json::array();
    for (const auto &mem : members) {
        json jm;
        jm["name"] = mem.name;
        jm["comment"] = mem.comment;
        jm["type"] = mem.type;
        jm["value"] = mem.value;
        jm["uint_length"] = mem.uint_length;
        jm["dims"] = json::array();
        for (const auto &d : mem.dims) {
            jm["dims"].push_back(d);
        }
        j["members"].push_back(jm);
    }
    j["enum_members"] = json::array();
    for (const auto &emen : enum_members) {
        json je;
        je["name"] = emen.name;
        je["comment"] = emen.comment;
        je["value"] = emen.value;
        j["enum_members"].push_back(je);
    }
    return j.dump();
};
void VulBundleItem::fromMemberJson(const string &json_str) {
    members.clear();
    enum_members.clear();
    json j = json::parse(json_str);
    if (j.contains("is_alias")) {
        is_alias = j.at("is_alias").get<bool>();
    } else {
        is_alias = false;
    }
    if (j.contains("members")) {
        for (const auto &jm : j.at("members")) {
            VulBundleMember mem;
            mem.name = jm.contains("name") ? jm.at("name").get<BMemberName>() : "";
            mem.comment =  jm.contains("comment") ? jm.at("comment").get<Comment>() : "";
            mem.type = jm.contains("type") ? jm.at("type").get<BMemberType>() : "";
            mem.value = jm.contains("value") ? jm.at("value").get<ConfigValue>() : ConfigValue();
            mem.uint_length = jm.contains("uint_length") ? jm.at("uint_length").get<ConfigValue>() : ConfigValue();
            if (jm.contains("dims"))
            {
                for (const auto &d : jm.at("dims")) {
                    mem.dims.push_back(d.get<ConfigValue>());
                }
            }
            members.push_back(mem);
        }
    }
    if (j.contains("enum_members")) {
        for (const auto &je : j.at("enum_members")) {
            VulBundleEnumMember emen;
            emen.name = je.contains("name") ? je.at("name").get<BMemberName>() : "";
            emen.comment = je.contains("comment") ? je.at("comment").get<Comment>() : "";
            emen.value = je.contains("value") ? je.at("value").get<ConfigValue>() : ConfigValue();
            enum_members.push_back(emen);
        }
    }
}

string VulBundleItem::checkAndExtractReferences(unordered_set<BundleName> &out_bundle_refs, unordered_set<ConfigName> &out_config_refs) const {
    
    auto &out_confs = out_config_refs;
    auto &out_references = out_bundle_refs;

    out_confs.clear();
    out_references.clear();

    string err;
    uint32_t errpos;

    // check alias
    if (is_alias) {
        if (members.size() != 1) {
            return string("Alias bundle '") + name + string("' must have exactly one member");
        }
    }

    // check enum
    if (!enum_members.empty()) {
        if (!members.empty()) {
            return string("Bundle '") + name + string("' has invalid definition: enum members cannot coexist with other member types");
        }
        unordered_set<BundleName> enum_names;
        for (const auto &enum_member : enum_members) {
            if (!isValidIdentifier(enum_member.name)) {
                return string("Invalid enum member name '") + enum_member.name + string("' in bundle '") + name + string("'");
            }
            if (!enum_member.value.empty()) {
                auto confs = config_parser::parseReferencedIdentifier(enum_member.value, errpos, err);
                if (!confs) {
                    return string("Invalid enum member value expression '") + enum_member.value + string("' in bundle '") + name + string("': ") + err;
                }
                for (const auto &conf_name : *confs) {
                    out_confs.insert(conf_name);
                }
            }
            if (enum_names.find(enum_member.name) != enum_names.end()) {
                return string("Enum member '") + enum_member.name + string("' in bundle '") + name + string("' has duplicate name");
            }
            enum_names.insert(enum_member.name);
        }
        return "";
    }
    
    // check members
    for (const auto &member : members) {
        if (!isValidIdentifier(member.name)) {
            return string("Invalid member name '") + member.name + string("' in bundle '") + name + string("'");
        }
        // check type
        if (!member.uint_length.empty()) {
            // this is a uint member
            if (!member.type.empty() && member.type != "__uint__") {
                return string("Member '") + member.name + string("' in bundle '") + name + string("' has type/length mismatch: length specified for non-uint type");
            }
            auto confs = config_parser::parseReferencedIdentifier(member.uint_length, errpos, err);
            if (!confs) {
                return string("Invalid uint length expression '") + member.uint_length + string("' in member '") + member.name + string("' in bundle '") + name + string("'");
            }
            for (const auto &conf_name : *confs) {
                out_confs.insert(conf_name);
            }
        } else if (!member.type.empty()) {
            if (!isBasicVulType(member.type)) {
                // check if it's a bundle type
                if (!member.value.empty()) {
                    return string("Member '") + member.name + string("' in bundle '") + name + string("' of bundle type cannot have a default value");
                }
                out_references.insert(member.type);
            }
        } else {
            return string("Member '") + member.name + string("' in bundle '") + name + string("' is missing type or length");
        }
        // check default value
        if (!member.value.empty()) {
            auto confs = config_parser::parseReferencedIdentifier(member.value, errpos, err);
            if (!confs) {
                return string("Invalid default value expression '") + member.value + string("' in member '") + member.name + string("' in bundle '") + name + string("'");
            }
            for (const auto &conf_name : *confs) {
                out_confs.insert(conf_name);
            }
        }
        // check array dimensions
        for (const auto &dim_expr : member.dims) {
            auto confs = config_parser::parseReferencedIdentifier(dim_expr, errpos, err);
            if (!confs) {
                return string("Invalid array dimension expression '") + dim_expr + string("' in member '") + member.name + string("' in bundle '") + name + string("'");
            }
        }
    }

    return "";
}

/**
 * @brief Build the bundle reference tree (bidirectional) for a given bundle.
 * @param root_bundle_name The name of the root bundle.
 * @param out_root_node Output parameter to hold the root node of the tree.
 */
void VulBundleLib::buildBundleReferenceTree(const BundleName &root_bundle_name, shared_ptr<BundleTreeBidirectionalNode> &out_root_node) const {
    out_root_node.reset();

    auto it_root = bundles.find(root_bundle_name);
    if (it_root == bundles.end()) {
        out_root_node = nullptr;
        return;
    }

    auto root = make_shared<BundleTreeBidirectionalNode>();
    root->name = it_root->second.item.name;
    root->comment = it_root->second.item.comment;

    // Downward: follow 'references' to children-only
    std::function<void(const BundleName&, const shared_ptr<BundleTreeBidirectionalNode>&, std::unordered_set<BundleName>&)> buildDown;
    buildDown = [&](const BundleName &name,
                    const shared_ptr<BundleTreeBidirectionalNode> &node,
                    std::unordered_set<BundleName> &path) {
        auto it = bundles.find(name);
        if (it == bundles.end()) return;
        const auto &entry = it->second;
        for (const auto &dep : entry.references) {
            if (path.count(dep)) {
                auto cyc = make_shared<BundleTreeBidirectionalNode>();
                auto jt = bundles.find(dep);
                if (jt != bundles.end()) {
                    cyc->name = jt->second.item.name;
                    cyc->comment = jt->second.item.comment;
                } else {
                    continue;
                }
                node->children.push_back(cyc);
                continue;
            }
            auto jt = bundles.find(dep);
            if (jt == bundles.end()) continue; // ignore library-external
            auto child = make_shared<BundleTreeBidirectionalNode>();
            child->name = jt->second.item.name;
            child->comment = jt->second.item.comment;
            // Do NOT set child->parents for downward subtree
            node->children.push_back(child);

            auto next_path = path;
            next_path.insert(dep);
            buildDown(dep, child, next_path);
        }
    };

    // Upward: follow 'reverse_references' to parents-only
    std::function<void(const BundleName&, const shared_ptr<BundleTreeBidirectionalNode>&, std::unordered_set<BundleName>&)> buildUp;
    buildUp = [&](const BundleName &name,
                  const shared_ptr<BundleTreeBidirectionalNode> &node,
                  std::unordered_set<BundleName> &path) {
        auto it = bundles.find(name);
        if (it == bundles.end()) return;
        const auto &entry = it->second;
        for (const auto &par : entry.reverse_references) {
            if (path.count(par)) {
                auto cyc = make_shared<BundleTreeBidirectionalNode>();
                auto jt = bundles.find(par);
                if (jt != bundles.end()) {
                    cyc->name = jt->second.item.name;
                    cyc->comment = jt->second.item.comment;
                } else {
                    continue;
                }
                node->parents.push_back(cyc);
                continue;
            }
            auto jt = bundles.find(par);
            if (jt == bundles.end()) continue; // ignore library-external
            auto parent = make_shared<BundleTreeBidirectionalNode>();
            parent->name = jt->second.item.name;
            parent->comment = jt->second.item.comment;
            // Do NOT set parent->children for upward subtree
            node->parents.push_back(parent);

            auto next_path = path;
            next_path.insert(par);
            buildUp(par, parent, next_path);
        }
    };

    {
        std::unordered_set<BundleName> down_path; down_path.insert(root->name);
        buildDown(root->name, root, down_path);
    }
    {
        std::unordered_set<BundleName> up_path; up_path.insert(root->name);
        buildUp(root->name, root, up_path);
    }

    out_root_node = root;
}

/**
 * @brief List all bundle tags in the bundle library.
 * @param out_tags Output vector to hold the names of all bundle tags.
 */
void VulBundleLib::listTags(vector<BundleTag> &out_tags) const {
    vector<BundleTag> tmp;
    for (const auto &entry : tag_bundles) {
        tmp.push_back(entry.first);
    }
    std::sort(tmp.begin(), tmp.end());
    out_tags.swap(tmp);
}

/**
 * @brief List all bundle names associated with a given tag.
 * @param tag The bundle tag to list bundles from.
 * @param out_bundles Output vector to hold the names of all bundles associated with the tag.
 */
ErrorMsg VulBundleLib::listBundlesByTag(const BundleTag &tag, vector<BundleName> &out_bundles) const {
    out_bundles.clear();
    auto iter = tag_bundles.find(tag);
    if (iter == tag_bundles.end()) {
        return EStr(EItemBundTagNotFound, string("Tag not found: ") + tag);
    }
    const auto &bundle_set = iter->second;
    out_bundles.insert(out_bundles.end(), bundle_set.begin(), bundle_set.end());
    std::sort(out_bundles.begin(), out_bundles.end());
    return "";
}

/**
 * @brief Get the bundle definition for a given bundle name.
 * @param bundle_name The name of the bundle to get.
 * @param out_bundle_item Output parameter to hold the VulBundleItem definition.
 * @param out_tags Output parameter to hold the tags associated with this bundle.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulBundleLib::getBundleDefinition(const BundleName &bundle_name, VulBundleItem &out_bundle_item, unordered_set<BundleTag> &out_tags) const {
    auto iter = bundles.find(bundle_name);
    if (iter == bundles.end()) {
        return EStr(EItemBundNameNotFound, string("Bundle not found: ") + bundle_name);
    }
    const auto &entry = iter->second;
    out_bundle_item = entry.item;
    out_tags = entry.tags;
    return "";
}

/**
 * @brief Get all bundle definitions in topological order based on their references.
 * @param out_sorted_bundles Output vector to hold the VulBundleItem definitions in topological order.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulBundleLib::getAllBundlesTopoSort(vector<BundleName> &out_sorted_bundles) const {
    unordered_set<BundleName> all_bundles;
    for (const auto &entry : bundles) {
        all_bundles.insert(entry.first);
    }
    unordered_map<BundleName, unordered_set<BundleName>> reverse_references_map;
    for (const auto &entry : bundles) {
        const BundleName &bname = entry.first;
        const BundleEntry &bentry = entry.second;
        reverse_references_map[bname] = bentry.reverse_references;
    }
    vector<BundleName> looped_bundles;
    auto topo_sorted_names = topologicalSort(all_bundles, reverse_references_map, looped_bundles);
    if (!looped_bundles.empty()) {
        string looped_str;
        for (const auto &bname : looped_bundles) {
            if (!looped_str.empty()) looped_str += ", ";
            looped_str += bname;
        }
        return EStr(EItemBundRefLooped, string("Bundle reference loop detected involving bundles: ") + looped_str);
    }
    out_sorted_bundles.swap(*topo_sorted_names);
    return "";
}

/**
 * @brief Insert a new bundle definition into the bundle library.
 * If a bundle with the same name already exists, check if the definition matches.
 * @param bundle_item The VulBundleItem to insert.
 * @param tags The tag to associate with this bundle.
 * @return An ErrorMsg indicating success or failure.
 */
ErrorMsg VulBundleLib::insertBundle(const VulBundleItem &bundle_item, const BundleTag &tags) {
    if (!isValidIdentifier(bundle_item.name)) {
        return EStr(EItemBundNameInvalid, string("Invalid bundle name: ") + bundle_item.name);
    }
    if (!isValidIdentifier(tags)) {
        return EStr(EItemBundTagInvalid, string("Invalid bundle tag: ") + tags);
    }
    auto iter = bundles.find(bundle_item.name);
    if (iter != bundles.end()) {
        // bundle already exists, check definition
        if (!_isBundleSameDefinition(iter->second.item, bundle_item)) {
            return EStr(EItemBundNameDup, string("Bundle name already exists with different definition: ") + bundle_item.name);
        }
        // add tag if not already present
        iter->second.tags.insert(tags);
        tag_bundles[tags].insert(bundle_item.name);
        return "";
    }

    BundleEntry new_entry;
    new_entry.item = bundle_item;
    ErrorMsg err = extractBundleReferencesAndConfs(bundle_item, new_entry.references, new_entry.confs);
    if (!err.empty()) {
        return err;
    }
    // all checks passed, insert new bundle

    new_entry.tags.insert(tags);

    // update reverse references
    for (const auto &ref_name : new_entry.references) {
        auto ref_iter = bundles.find(ref_name);
        if (ref_iter != bundles.end()) [[likely]] {
            ref_iter->second.reverse_references.insert(bundle_item.name);
        }
    }

    bundles[bundle_item.name] = new_entry;

    tag_bundles[tags].insert(bundle_item.name);
    for (const auto &conf_name : new_entry.confs) {
        auto conf_iter = conf_bundles.find(conf_name);;
        if (conf_iter == conf_bundles.end()) {
            conf_iter = conf_bundles.insert({conf_name, unordered_set<BundleName>()}).first;
        }
        conf_iter->second.insert(bundle_item.name);
    }

    return "";
}

/**
 * @brief Insert multiple new bundle definitions into the bundle library.
 * If a bundle with the same name already exists, check if the definition matches.
 * @param bundle_items The vector of VulBundleItem to insert.
 * @param tags The tag to associate with these bundles.
 * @return An ErrorMsg indicating success or failure.
 */
ErrorMsg VulBundleLib::insertBundles(const vector<VulBundleItem> &bundle_items, const BundleTag &tags) {

    ErrorMsg err;
    
    unordered_set<BundleName> inserting_names;
    unordered_map<BundleName, unordered_set<BundleName>> reversed_references_map;
    unordered_map<BundleName, const VulBundleItem*> inserting_items_map;
    for (const auto &bundle_item : bundle_items) {
        if (!isValidIdentifier(bundle_item.name)) {
            return EStr(EItemBundNameInvalid, string("Invalid bundle name: ") + bundle_item.name);
        }
        if (inserting_names.find(bundle_item.name) != inserting_names.end()) {
            return EStr(EItemBundNameDup, string("Duplicate bundle name in insertion set: ") + bundle_item.name);
        }
        inserting_names.insert(bundle_item.name);
        inserting_items_map[bundle_item.name] = &bundle_item;
    }
    // extract references
    for (const auto &bundle_item : bundle_items) {
        unordered_set<BundleName> references;
        extractBundleReferences(bundle_item, references);
        for (const auto &ref_name : references) {
            if (inserting_names.find(ref_name) != inserting_names.end()) {
                auto rref_iter = reversed_references_map.find(ref_name);;
                if (rref_iter == reversed_references_map.end()) {
                    rref_iter = reversed_references_map.insert({ref_name, unordered_set<BundleName>()}).first;
                }
                rref_iter->second.insert(bundle_item.name);
            } else if (bundles.find(ref_name) == bundles.end()) {
                return EStr(EItemBundRefNotFound, string("Bundle '") + bundle_item.name + string("' references undefined bundle: ") + ref_name);
            }
        }
    }
    // topological sort
    vector<BundleName> looped_bundles;
    auto topo_sorted_names = topologicalSort(inserting_names, reversed_references_map, looped_bundles);
    if (!topo_sorted_names) {
        string looped_str;
        for (const auto &bname : looped_bundles) {
            if (!looped_str.empty()) looped_str += ", ";
            looped_str += bname;
        }
        return EStr(EItemBundRefLooped, string("Bundle reference loop detected involving bundles: ") + looped_str);
    }
    // insert bundles in topological order
    // save context before insertion
    uint64_t snapshot_id = snapshot();

    for (const auto &bundle_name : *topo_sorted_names) {
        const auto &bundle_item = *inserting_items_map[bundle_name];
        err = insertBundle(bundle_item, tags);
        if (!err.empty()) {
            // restore context
            rollback(snapshot_id);
            return err;
        }
    }
    commit(snapshot_id);
    return "";
}

/**
 * @brief Remove a bundle from the bundle library with a specific tag.
 * If the bundle has no more tags after removal, it will be removed from the library.
 * @param bundle_name The name of the bundle to remove.
 * @param tags The tag to remove the bundle from.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulBundleLib::removeBundle(const BundleName &bundle_name, const BundleTag &tags) {
    auto iter = bundles.find(bundle_name);
    if (iter == bundles.end()) {
        return EStr(EItemBundNameNotFound, string("Bundle not found: ") + bundle_name);
    }
    auto &entry = iter->second;
    if (entry.tags.find(tags) == entry.tags.end()) {
        return EStr(EItemBundTagNotFound, string("Tag '") + tags + string("' not found in bundle '") + bundle_name + string("'"));
    }
    bool actually_removed = (entry.tags.size() == 1 && entry.tags.find(tags) != entry.tags.end());
    // check reverse references if actually removing
    if (actually_removed && !entry.reverse_references.empty()) {
        string ref_str;
        for (const auto &ref_name : entry.reverse_references) {
            if (!ref_str.empty()) ref_str += ", ";
            ref_str += ref_name;
        }
        return EStr(EItemBundRemoveRef, string("Cannot remove bundle '") + bundle_name + string("' because it is referenced by other bundles: ") + ref_str);
    }

    // remove tag
    entry.tags.erase(tags);
    auto tag_iter = tag_bundles.find(tags);
    if (tag_iter != tag_bundles.end()) [[likely]] {
        tag_iter->second.erase(bundle_name);
        if (tag_iter->second.empty()) {
            tag_bundles.erase(tag_iter);
        }
    }
    if (!entry.tags.empty()) {
        // still has other tags, do not remove bundle
        return "";
    }
    // remove bundle completely
    // update reverse references
    for (const auto &ref_name : entry.references) {
        auto ref_iter = bundles.find(ref_name);
        if (ref_iter != bundles.end()) [[likely]] {
            ref_iter->second.reverse_references.erase(bundle_name);
        }
    }
    // update conf_bundles
    for (const auto &conf_name : entry.confs) {
        auto conf_iter = conf_bundles.find(conf_name);
        if (conf_iter != conf_bundles.end()) [[likely]] {
            conf_iter->second.erase(bundle_name);
            if (conf_iter->second.empty()) {
                conf_bundles.erase(conf_iter);
            }
        }
    }
    bundles.erase(iter);
    return "";
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
ErrorMsg VulBundleLib::renameBundle(const BundleName &old_name, const BundleName &new_name, bool update_references) {
    auto iter = bundles.find(old_name);
    if (iter == bundles.end()) {
        return EStr(EItemBundNameNotFound, string("Bundle not found: ") + old_name);
    }
    auto &entry = iter->second;
    // only can rename __default__ tagged bundles
    if (entry.tags.size() != 1 || entry.tags.find(DefaultTag) == entry.tags.end()) {
        return EStr(EItemBundRenameTagged, string("Renaming bundle '") + old_name + string("' is not allowed: only bundles with the default tag can be renamed"));
    }
    if (bundles.find(new_name) != bundles.end()) {
        return EStr(EItemBundNameDup, string("Cannot rename bundle to '") + new_name + string("': name already exists"));
    }
    // check references
    if (!update_references && !entry.reverse_references.empty()) {
        string ref_str;
        for (const auto &ref_name : entry.reverse_references) {
            if (!ref_str.empty()) ref_str += ", ";
            ref_str += ref_name;
        }
        return EStr(EItemBundRenameRef, string("Cannot rename bundle '") + old_name + string("' because it is referenced by other bundles: ") + ref_str);
    }

    // perform rename
    // update references in other bundles
    for (const auto &ref_name : entry.reverse_references) {
        auto ref_iter = bundles.find(ref_name);
        if (ref_iter != bundles.end()) [[likely]] {
            auto &ref_entry = ref_iter->second;
            // update references set
            ref_entry.references.erase(old_name);
            ref_entry.references.insert(new_name);
            // update bundle definition
            for (auto &member : ref_entry.item.members) {
                if (member.type == old_name) {
                    member.type = new_name;
                }
            }
        }
    }
    for (const auto &ref_name : entry.references) {
        auto ref_iter = bundles.find(ref_name);
        if (ref_iter != bundles.end()) [[likely]] {
            auto &ref_entry = ref_iter->second;
            // update reverse references set
            ref_entry.reverse_references.erase(old_name);
            ref_entry.reverse_references.insert(new_name);
        }
    }
    // update tag_bundles
    for (const auto &tag : entry.tags) {
        auto tag_iter = tag_bundles.find(tag);
        if (tag_iter != tag_bundles.end()) [[likely]] {
            tag_iter->second.erase(old_name);
            tag_iter->second.insert(new_name);
        }
    }
    // update conf_bundles
    for (const auto &conf_name : entry.confs) {
        auto conf_iter = conf_bundles.find(conf_name);
        if (conf_iter != conf_bundles.end()) [[likely]] {
            conf_iter->second.erase(old_name);
            conf_iter->second.insert(new_name);
        }
    }
    // insert new entry and erase old one
    bundles[new_name] = entry;
    bundles.erase(old_name);
    return "";
}

/**
 * @brief Update the definition of an existing bundle in the bundle library.
 * Only bundles with the default tag can be updated.
 * @param bundle_item The new VulBundleItem definition.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulBundleLib::updateBundleDefinition(const VulBundleItem &bundle_item) {
    auto iter = bundles.find(bundle_item.name);
    if (iter == bundles.end()) {
        return EStr(EItemBundNameNotFound, string("Bundle not found: ") + bundle_item.name);
    }
    auto old_entry = iter->second;
    // only can update __default__ tagged bundles
    if (old_entry.tags.size() != 1 || old_entry.tags.find(DefaultTag) == old_entry.tags.end()) {
        return EStr(EItemBundUpdateTagged, string("Updating bundle '") + bundle_item.name + string("' is not allowed: only bundles with the default tag can be updated"));
    }
    // extract and check new references and confs
    BundleEntry new_entry;
    ErrorMsg err = extractBundleReferencesAndConfs(bundle_item, new_entry.references, new_entry.confs);
    if (!err.empty()) {
        return err;
    }

    // all checks passed, update bundle
    new_entry.item = bundle_item;
    new_entry.tags = old_entry.tags;
    auto &old_references = old_entry.references;
    auto &new_references = new_entry.references;
    auto &old_confs = old_entry.confs;
    auto &new_confs = new_entry.confs;

    unordered_set<BundleName> to_add_refs;
    unordered_set<BundleName> to_remove_refs;
    std::set_difference(
        new_references.begin(), new_references.end(),
        old_references.begin(), old_references.end(),
        std::inserter(to_add_refs, to_add_refs.begin())
    );
    std::set_difference(
        old_references.begin(), old_references.end(),
        new_references.begin(), new_references.end(),
        std::inserter(to_remove_refs, to_remove_refs.begin())
    );

    // update reverse references
    for (const auto &ref_name : to_add_refs) {
        auto ref_iter = bundles.find(ref_name);
        if (ref_iter != bundles.end()) [[likely]] {
            ref_iter->second.reverse_references.insert(bundle_item.name);
        }
    }
    for (const auto &ref_name : to_remove_refs) {
        auto ref_iter = bundles.find(ref_name);
        if (ref_iter != bundles.end()) [[likely]] {
            ref_iter->second.reverse_references.erase(bundle_item.name);
        }
    }

    // update conf_bundles
    for (const auto &conf_name : old_confs) {
        if (new_confs.find(conf_name) == new_confs.end()) {
            auto conf_iter = conf_bundles.find(conf_name);
            if (conf_iter != conf_bundles.end()) [[likely]] {
                conf_iter->second.erase(bundle_item.name);
                if (conf_iter->second.empty()) {
                    conf_bundles.erase(conf_iter);
                }
            }
        }
    }
    for (const auto &conf_name : new_confs) {
        if (old_confs.find(conf_name) == old_confs.end()) {
            auto conf_iter = conf_bundles.find(conf_name);
            if (conf_iter != conf_bundles.end()) [[likely]] {
                conf_iter->second.insert(bundle_item.name);
            }
        }
    }

    iter->second = new_entry;
    return "";
}

/**
 * @brief Remove a tag from the bundle library.
 * All bundles associated with this tag will have the tag removed.
 * A bundle with no tags will be removed from the library.
 * @param tag The tag to remove.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulBundleLib::removeTag(const BundleTag &tag) {
    auto tag_iter = tag_bundles.find(tag);
    if (tag_iter == tag_bundles.end()) {
        return EStr(EItemBundTagNotFound, string("Tag not found: ") + tag);
    }
    auto bundle_names = tag_iter->second;

    unordered_set<BundleName> to_remove_bundles;
    unordered_set<BundleName> to_untag_bundles;
    // only remove bundles that have no more tags after tag removal
    for (const auto &bundle_name : bundle_names) {
        auto bundle_iter = bundles.find(bundle_name);
        if (bundle_iter != bundles.end()) [[likely]] {
            auto &entry = bundle_iter->second;
            if (entry.tags.size() == 1 && entry.tags.find(tag) != entry.tags.end()) {
                to_remove_bundles.insert(bundle_name);
            } else {
                to_untag_bundles.insert(bundle_name);
            }
        }
    }
    // remove as topolocogical order with reference as edges
    // A ref B means A depends on B, so A should be removed before B, otherwise B cannot be removed due to A's reference
    unordered_map<BundleName, unordered_set<BundleName>> references_map;
    // check references
    for (const auto &bundle_name : to_remove_bundles) {
        auto bundle_iter = bundles.find(bundle_name);
        if (bundle_iter != bundles.end()) [[likely]] {
            auto &entry = bundle_iter->second;
            unordered_set<BundleName> external_ref_by;;
            for (const auto &ref_name : entry.reverse_references) {
                if (to_remove_bundles.find(ref_name) == to_remove_bundles.end()) {
                    external_ref_by.insert(ref_name);
                } else {
                    auto ref_iter = references_map.find(ref_name);
                    if (ref_iter == references_map.end()) {
                        ref_iter = references_map.insert({ref_name, unordered_set<BundleName>()}).first;
                    }
                    ref_iter->second.insert(bundle_name);
                }
            }
            if (!external_ref_by.empty()) {
                string ref_str;
                for (const auto &ref_name : external_ref_by) {
                    if (!ref_str.empty()) ref_str += ", ";
                    ref_str += ref_name;
                }
                return EStr(EItemBundRemoveRef, string("Cannot remove tag '") + tag + string("' because bundle '") + bundle_name + string("' is referenced by other bundles: ") + ref_str);
            }
        }
    }
    vector<BundleName> looped_bundles;
    auto topo_sorted_names = topologicalSort(to_remove_bundles, references_map, looped_bundles);
    if (!looped_bundles.empty()) {
        // should not happen
        string looped_str;
        for (const auto &bname : looped_bundles) {
            if (!looped_str.empty()) looped_str += ", ";
            looped_str += bname;
        }
        return EStr(EItemBundRefLooped, string("BUNDLE LIB BROKEN !!!: Bundle reference loop detected involving bundles: ") + looped_str);
    }

    uint64_t snapshot_id = snapshot();
    for (const auto &bundle_name : *topo_sorted_names) {
        ErrorMsg err = removeBundle(bundle_name, tag);
        if (!err.empty()) {
            rollback(snapshot_id);
            return err;
        }
    }
    rollback(snapshot_id);
    for (const auto &bundle_name : to_untag_bundles) {
        auto iter = bundles.find(bundle_name);;
        if (iter != bundles.end()) [[likely]] {
            auto &entry = iter->second;
            entry.tags.erase(tag);
        }
    }
    tag_bundles.erase(tag);
    return "";
}

/**
 * @brief Notify the bundle library that a config item has been renamed.
 * Update any bundle definitions that reference the old config name.
 * @param old_name The old config item name.
 * @param new_name The new config item name.
 */
void VulBundleLib::externalConfigRename(const ConfigName &old_name, const ConfigName &new_name) {
    auto conf_iter = conf_bundles.find(old_name);
    if (conf_iter == conf_bundles.end()) {
        return;
    }
    auto bundle_names = conf_iter->second;
    for (const auto &bundle_name : bundle_names) {
        auto bundle_iter = bundles.find(bundle_name);
        if (bundle_iter != bundles.end()) [[likely]] {
            auto &entry = bundle_iter->second;
            // update confs set
            entry.confs.erase(old_name);
            entry.confs.insert(new_name);
            // update bundle definition
            for (auto &member : entry.item.members) {
                member.uint_length = identifierReplace(member.uint_length, old_name, new_name);
                for (auto &dim : member.dims) {
                    dim = identifierReplace(dim, old_name, new_name);
                }
            }
            // update enum members
            for (auto &member : entry.item.enum_members) {
                member.value = identifierReplace(member.value, old_name, new_name);
            }
        }
    }
    conf_bundles[new_name] = conf_iter->second;
    conf_bundles.erase(conf_iter);
}

/**
 * @brief Create a snapshot of the current bundle library state.
 * @return A snapshot ID to identify the snapshot.
 */
uint64_t VulBundleLib::snapshot() {
    static uint64_t snapshot_id_counter = 1;
    uint64_t snapshot_id = snapshot_id_counter++;
    snapshots[snapshot_id] = SnapshotEntry{
        bundles,
        tag_bundles,
        conf_bundles
    };
    return snapshot_id;
}

/**
 * @brief Rollback to the last snapshot of the bundle library state.
 * @param snapshot_id The snapshot ID to rollback to.
 */
void VulBundleLib::rollback(uint64_t snapshot_id) {
    auto iter = snapshots.find(snapshot_id);
    if (iter != snapshots.end()) {
        bundles = iter->second.bundles;
        tag_bundles = iter->second.tag_bundles;
        conf_bundles = iter->second.conf_bundles;
    }
    commit(snapshot_id);
}
/**
 * @brief Commit the last snapshot of the bundle library state.
 * @param snapshot_id The snapshot ID to commit.
 */
void VulBundleLib::commit(uint64_t snapshot_id) {
    for (auto iter = snapshots.begin(); iter != snapshots.end(); ) {
        if (iter->first >= snapshot_id) {
            iter = snapshots.erase(iter);
        } else {
            ++iter;
        }
    }
}

bool VulBundleLib::_isBundleSameDefinition(const VulBundleItem &a, const VulBundleItem &b) const {
    // compare bundle definitions
    if (a.members.size() != b.members.size() ||
        a.enum_members.size() != b.enum_members.size()) {
        return false;
    }
    if (a.is_alias != b.is_alias) {
        return false;
    }
    for (size_t i = 0; i < a.members.size(); ++i) {
        if (a.members[i].name != b.members[i].name ||
            a.members[i].type != b.members[i].type ||
            a.members[i].value != b.members[i].value ||
            a.members[i].uint_length != b.members[i].uint_length ||
            a.members[i].dims != b.members[i].dims
        ) {
            return false;
        }
    }
    for (size_t i = 0; i < a.enum_members.size(); ++i) {
        if (a.enum_members[i].name != b.enum_members[i].name ||
            a.enum_members[i].value != b.enum_members[i].value) {
            return false;
        }
    }
    return true;
}


/**
 * @brief Extract bundle references and config usages from a bundle definition.
 * @param bundle_item The VulBundleItem to analyze.
 * @param out_references Output set to hold the names of referenced bundles.
 * @param out_confs Output set to hold the names of used config items.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulBundleLib::extractBundleReferencesAndConfs(
    const VulBundleItem &bundle_item,
    unordered_set<BundleName> &out_references,
    unordered_set<ConfigName> &out_confs
) const {

    out_confs.clear();
    out_references.clear();

    string err;
    uint32_t errpos;


    // check enum
    if (!bundle_item.enum_members.empty()) {
        if (!bundle_item.members.empty()) {
            return EStr(EItemBundTypeMixed, string("Bundle '") + bundle_item.name + string("' has invalid definition: enum members cannot coexist with other member types"));
        }
        if (!err.empty()) {
            return err;
        }
        unordered_set<BundleName> enum_names;
        unordered_set<ConfigRealValue> enum_values;
        for (const auto &enum_member : bundle_item.enum_members) {
            if (!isValidIdentifier(enum_member.name)) {
                return EStr(EItemBundMemNameInvalid, string("Invalid enum member name '") + enum_member.name + string("' in bundle '") + bundle_item.name + string("'"));
            }
            if (!enum_member.value.empty()) {
                auto confs = config_parser::parseReferencedIdentifier(enum_member.value, errpos, err);
                if (!confs) {
                    return EStr(EItemConfValueGrammerInvalid, string("Invalid enum member value expression '") + enum_member.value + string("' in bundle '") + bundle_item.name + string("': ") + err);
                }
                for (const auto &conf_name : *confs) {
                    out_confs.insert(conf_name);
                }
            }
            if (enum_names.find(enum_member.name) != enum_names.end()) {
                return EStr(EItemBundEnumNameDup, string("Enum member '") + enum_member.name + string("' in bundle '") + bundle_item.name + string("' has duplicate name"));
            }
            enum_names.insert(enum_member.name);
        }
        return "";
    }

    // check alias
    if (bundle_item.is_alias) {
        if (bundle_item.members.size() != 1) {
            return EStr(EItemBundAliasInvalid, string("Alias bundle '") + bundle_item.name + string("' must have exactly one member"));
        }
    }

    // check members
    for (const auto &member : bundle_item.members) {
        if (!isValidIdentifier(member.name)) {
            return EStr(EItemBundMemNameInvalid, string("Invalid member name '") + member.name + string("' in bundle '") + bundle_item.name + string("'"));
        }
        // check type
        if (!member.uint_length.empty()) {
            // this is a uint member
            if (!member.type.empty() && member.type != "__uint__") {
                return EStr(EItemBundTypeMixed, string("Member '") + member.name + string("' in bundle '") + bundle_item.name + string("' has type/length mismatch: length specified for non-uint type"));
            }
            auto confs = config_parser::parseReferencedIdentifier(member.uint_length, errpos, err);
            if (!confs) {
                return EStr(EItemConfValueGrammerInvalid, string("Invalid uint length expression '") + member.uint_length + string("' in member '") + member.name + string("' in bundle '") + bundle_item.name + string("'"));
            }
            for (const auto &conf_name : *confs) {
                out_confs.insert(conf_name);
            }
        } else if (!member.type.empty()) {
            if (!isBasicVulType(member.type)) {
                // check if it's a bundle type
                if (!checkNameConflict(member.type)) {
                    return EStr(EItemBundRefNotFound, string("Member '") + member.name + string("' in bundle '") + bundle_item.name + string("' has invalid type: ") + member.type);
                }
                if (!member.value.empty()) {
                    return EStr(EItemBundInitWithValue, string("Member '") + member.name + string("' in bundle '") + bundle_item.name + string("' of bundle type cannot have a default value"));
                }
                out_references.insert(member.type);
            }
        } else {
            return EStr(EItemBundTypeMixed, string("Member '") + member.name + string("' in bundle '") + bundle_item.name + string("' is missing type or length"));
        }
        // check default value
        if (!member.value.empty()) {
            auto confs = config_parser::parseReferencedIdentifier(member.value, errpos, err);
            if (!confs) {
                return EStr(EItemConfValueGrammerInvalid, string("Invalid default value expression '") + member.value + string("' in member '") + member.name + string("' in bundle '") + bundle_item.name + string("'"));
            }
            for (const auto &conf_name : *confs) {
                out_confs.insert(conf_name);
            }
        }
        // check array dimensions
        for (const auto &dim_expr : member.dims) {
            auto confs = config_parser::parseReferencedIdentifier(dim_expr, errpos, err);
            if (!confs) {
                return EStr(EItemConfValueGrammerInvalid, string("Invalid array dimension expression '") + dim_expr + string("' in member '") + member.name + string("' in bundle '") + bundle_item.name + string("'"));
            }
        }
    }

    return "";
}

/**
 * @brief Extract bundle references from a bundle definition without checking bundle reference validity.
 * @param bundle_item The VulBundleItem to analyze.
 * @param out_references Output set to hold the names of referenced bundles.
 */
void VulBundleLib::extractBundleReferences(
    const VulBundleItem &bundle_item,
    unordered_set<BundleName> &out_references
) const {
    ErrorMsg err;
    ConfigRealValue rvalue;

    out_references.clear();

    for (const auto &member : bundle_item.members) {
        if (!member.type.empty() && !isBasicVulType(member.type)) {
            // assume it's a bundle type
            out_references.insert(member.type);
        }
    }
}
