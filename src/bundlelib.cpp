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

ErrorMsg calculateBundleConstexprValue(VulBundleItem &item, const VulConfigLib &config_lib, const unordered_map<ConfigName, ConfigRealValue> &overrides) {
    // 计算 bundle 中所有成员的 constexpr 值，包括 enumerate, uint 长度，数组维度，初始值 value

    for (auto &member : item.members) {
        // 计算 uint 长度
        if (!member.uint_length.empty()) {
            ConfigRealValue uint_len_value;
            unordered_set<ConfigName> seen_configs;
            auto err = config_lib.calculateConfigExpression(member.uint_length, overrides, uint_len_value, seen_configs);
            if (err) {
                return EStr(EItemBundConstGrammarInvalid, string("Failed to calculate uint length expression '") + member.uint_length + string("' for member '") + member.name + string("' in bundle '") + item.name + string("': ") + err.msg);
            }
            member.uint_length = std::to_string(uint_len_value);
        }
        // 计算数组维度
        for (auto &dim_expr : member.dims) {
            ConfigRealValue dim_value;
            unordered_set<ConfigName> seen_configs;
            auto err = config_lib.calculateConfigExpression(dim_expr, overrides, dim_value, seen_configs);
            if (err) {
                return EStr(EItemBundConstGrammarInvalid, string("Failed to calculate array dimension expression '") + dim_expr + string("' for member '") + member.name + string("' in bundle '") + item.name + string("': ") + err.msg);
            }
            dim_expr = std::to_string(dim_value);
        }
        // 计算初始值
        if (!member.value.empty()) {
            ConfigRealValue init_value;
            unordered_set<ConfigName> seen_configs;
            auto err = config_lib.calculateConfigExpression(member.value, overrides, init_value, seen_configs);
            if (err) {
                return EStr(EItemBundConstGrammarInvalid, string("Failed to calculate default value expression '") + member.value + string("' for member '") + member.name + string("' in bundle '") + item.name + string("': ") + err.msg);
            }
            member.value = std::to_string(init_value);
        }
    }
    // 计算 enum 成员的初始值
    for (auto &enum_member : item.enum_members) {
        if (!enum_member.value.empty()) {
            ConfigRealValue init_value;
            unordered_set<ConfigName> seen_configs;
            auto err = config_lib.calculateConfigExpression(enum_member.value, overrides, init_value, seen_configs);
            if (err) {
                return EStr(EItemBundConstGrammarInvalid, string("Failed to calculate enum member value expression '") + enum_member.value + string("' for enum member '") + enum_member.name + string("' in bundle '") + item.name + string("': ") + err.msg);
            }
            enum_member.value = std::to_string(init_value);
        }
    }
    return "";
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

bool operator==(const VulBundleItem &a, const VulBundleItem &b) {
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


ErrorMsg VulBundleLib::insertMultiBundles(const vector<VulBundleItem> &bundles, const unordered_set<BundleTag> & tags) {
    struct BundleWithRefs {
        VulBundleItem item;
        unordered_set<BundleName> bundle_refs;
        unordered_set<ConfigName> config_refs; 
    };
    
    unordered_set<BundleName> new_names;
    unordered_map<BundleName, BundleWithRefs> new_items;
    for (const auto &item : bundles) {
        BundleWithRefs item_with_refs;
        item_with_refs.item = item;
        string err = item.checkAndExtractReferences(item_with_refs.bundle_refs, item_with_refs.config_refs);
        if (!err.empty()) {
            return EStr(EOPBundAddDefinitionInvalid, string("Invalid bundle definition for bundle '") + item.name + string(": ") + err);
        }
        if (checkNameConflict(item.name)) {
            return EStr(EOPBundAddDefinitionInvalid, string("Bundle name conflict: ") + item.name);
        }
        if (new_names.find(item.name) != new_names.end()) {
            return EStr(EOPBundAddDefinitionInvalid, string("Duplicate bundle name in input: ") + item.name);
        }
        new_names.insert(item.name);
        new_items[item.name] = std::move(item_with_refs);
    }
    unordered_map<BundleName, unordered_set<BundleName>> new_bundle_rev_refs;
    for (const auto &kv : new_items) {
        const auto &item = kv.second;
        for (const auto &ref_name : item.bundle_refs) {
            if (!checkNameConflict(ref_name) && new_names.find(ref_name) == new_names.end()) {
                return EStr(EOPBundAddDefinitionInvalid, string("Referenced bundle '") + ref_name + string("' in definition of bundle '") + item.item.name + string("' does not exist in bundle library"));
            }
            new_bundle_rev_refs[ref_name].insert(item.item.name);
        }
    }
    vector<BundleName> looped_bundles;
    auto sorted_names_ptr = topologicalSort(new_names, new_bundle_rev_refs, looped_bundles);
    if (!sorted_names_ptr) {
        string looped_str;
        for (const auto &bname : looped_bundles) {
            if (!looped_str.empty()) looped_str += ", ";
            looped_str += bname;
        }
        return EStr(EOPBundAddDefinitionInvalid, string("Bundle reference loop detected among new bundles: ") + looped_str);
    }
    for (const auto &name : *sorted_names_ptr) {
        const auto &item_with_refs = new_items[name];
        const auto &item = item_with_refs.item;
        BundleEntry bunde;
        bunde.item = item;
        bunde.tags = tags;
        bunde.references = item_with_refs.bundle_refs;
        bunde.confs = item_with_refs.config_refs;
        this->bundles[name] = std::move(bunde);
    }
    return "";
}

