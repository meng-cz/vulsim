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
