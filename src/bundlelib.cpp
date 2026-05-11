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

static inline void trim(string &s) {
    size_t l = 0;
    while (l < s.size() && std::isspace(s[l])) l++;
    size_t r = s.size();
    while (r > l && std::isspace(s[r - 1])) r--;
    s = s.substr(l, r - l);
}

bool VulBundleMember::fromDefinitionString(const string &def_str) {
    string s = def_str;

    // -----------------------------
    // 1. 处理注释 //
    // -----------------------------
    size_t comment_pos = s.find("//");
    if (comment_pos != string::npos) {
        comment = s.substr(comment_pos + 2);
        trim(comment);
        s = s.substr(0, comment_pos);
    } else {
        comment = "";
    }

    trim(s);

    // -----------------------------
    // 2. 去掉结尾分号
    // -----------------------------
    if (!s.empty() && s.back() == ';') {
        s.pop_back();
    }

    trim(s);

    // -----------------------------
    // 3. 处理初始化 (= ...)
    // -----------------------------
    size_t eq_pos = s.find('=');
    if (eq_pos != string::npos) {
        value = s.substr(eq_pos + 1);
        trim(value);
        s = s.substr(0, eq_pos);
    } else {
        value = "0";
    }

    trim(s);

    // -----------------------------
    // 4. 提取数组维度 [xxx]
    // -----------------------------
    dims.clear();
    while (true) {
        size_t l = s.find('[');
        if (l == string::npos) break;
        size_t r = s.find(']', l);
        if (r == string::npos) return false;

        string dim = s.substr(l + 1, r - l - 1);
        trim(dim);
        dims.push_back(dim);

        // 删除这一段
        s.erase(l, r - l + 1);
    }

    trim(s);

    // -----------------------------
    // 5. 分离 type 和 name
    // -----------------------------
    // 从后往前找最后一个空格
    size_t split = string::npos;
    for (int i = (int)s.size() - 1; i >= 0; --i) {
        if (std::isspace(s[i])) {
            split = i;
            break;
        }
    }

    if (split == string::npos) return false;

    type = s.substr(0, split);
    name = s.substr(split + 1);

    trim(type);
    trim(name);

    // -----------------------------
    // 6. 处理 UInt<N>
    // -----------------------------
    uint_length = "";
    if (type.find("UInt<") == 0) {
        size_t l = type.find('<');
        size_t r = type.find('>');
        if (l == string::npos || r == string::npos || r <= l) return false;

        uint_length = type.substr(l + 1, r - l - 1);
        trim(uint_length);

        type = "UInt";
    }

    return true;
}


ErrorMsg resolveBundleTable(const VulBundleLib &bundle_lib, const VulConfigLib &config_lib, const unordered_map<ConfigName, ConfigRealValue> &overrides, BundleTable &out_bundle_table) {
    unordered_map<ConfigName, ConfigRealValue> calculated_configlib;

    for (const auto &conf_entry : config_lib.config_items) {
        const string &conf_name = conf_entry.first;
        const ConfigRealValue &real_value = conf_entry.second.real_value;
        calculated_configlib[conf_name] = real_value;
    }
    for (const auto &conf_entry : overrides) {
        calculated_configlib[conf_entry.first] = conf_entry.second;
    }
    for (const auto &bundle_entry : bundle_lib.bundles) {
        const string &bundle_name = bundle_entry.first;
        VulBundleItem item = bundle_entry.second.item;
        ErrorMsg err = calculateBundleConstexprValue(item, config_lib, calculated_configlib);
        if (err.error()) {
            return err;
        }
        out_bundle_table[bundle_name] = item;
    }
    return "";
}



ErrorMsg staticalizeBundle(const VulBundleItem &item, const VulStaticConfigLib &config_lib, VulStaticBundle &out_item) {
    
    VulStaticBundle static_item;
    static_item.name = item.name;
    static_item.is_alias = item.is_alias;

    for (const auto &enum_member : item.enum_members) {
        VulStaticEnumMember static_enum_member;
        static_enum_member.name = enum_member.name;
        static_enum_member.has_value = !enum_member.value.empty();
        if (static_enum_member.has_value) {
            ConfigRealValue value = 0;
            auto err = calculateConstexprValue(enum_member.value, config_lib, value);
            if (err.error()) {
                return EStr(err.code, string("Failed to calculate enum member value expression '") + enum_member.value + string("' for enum member '") + enum_member.name + string("' in bundle '") + item.name + string("': ") + err.msg);
            }
            static_enum_member.value = value;
        }
        static_item.enum_members.push_back(std::move(static_enum_member));
    }

    for (const auto &member : item.members) {
        VulStaticBundleMember static_member;
        static_member.name = member.name;
        static_member.type = member.type;
        // 计算 uint 长度
        if (!member.uint_length.empty()) {
            ConfigRealValue uint_len_value;
            auto err = calculateConstexprValue(member.uint_length, config_lib, uint_len_value);
            if (err) {
                return EStr(EItemBundConstGrammarInvalid, string("Failed to calculate uint length expression '") + member.uint_length + string("' for member '") + member.name + string("' in bundle '") + item.name + string("': ") + err.msg);
            }
            static_member.uint_length = uint_len_value;
        } else {
            static_member.uint_length = 0;
        }
        // 计算数组维度
        for (const auto &dim_expr : member.dims) {
            ConfigRealValue dim_value;
            auto err = calculateConstexprValue(dim_expr, config_lib, dim_value);
            if (err) {
                return EStr(EItemBundConstGrammarInvalid, string("Failed to calculate array dimension expression '") + dim_expr + string("' for member '") + member.name + string("' in bundle '") + item.name + string("': ") + err.msg);
            }
            static_member.dims.push_back(dim_value);
        }
        static_item.members.push_back(std::move(static_member));
    }
    out_item = std::move(static_item);
    return "";
}









uint32_t get_basic_width(const std::string& type) {
    if (type == "bool") return 1;

    auto parse_int_type = [&](const std::string& s, const std::string& prefix) -> uint32_t {
        if (s.rfind(prefix, 0) != 0) return 0;

        size_t pos = prefix.size();

        // 读取数字部分
        size_t num_start = pos;
        while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
            ++pos;
        }

        if (num_start == pos) return 0; // 没有数字

        uint32_t bits = std::stoul(s.substr(num_start, pos - num_start));

        // 允许 _t 或没有
        if (pos == s.size()) {
            return bits;
        }
        if (s.substr(pos) == "_t") {
            return bits;
        }

        return 0;
    };

    // int / uint
    uint32_t bits = 0;

    bits = parse_int_type(type, "int");
    if (bits != 0) return bits;

    bits = parse_int_type(type, "uint");
    if (bits != 0) return bits;

    throw std::runtime_error("Unsupported type: " + type);
}

void flatten_member(
    const VulBundleMember& m,
    const BundleTable& table,
    const std::string& prefix,
    uint32_t& offset,
    std::vector<FlatField>& out
) {
    // ===== 1️⃣ 计算数组总元素数 =====
    std::vector<uint32_t> dims;
    for (const auto& d : m.dims) {
        dims.push_back(std::stoul(d)); // 假设已是常量
    }

    uint32_t total = 1;
    for (auto d : dims) total *= d;

    // ===== 2️⃣ 递归展开数组（多维）=====
    std::function<void(int, std::string)> expand_array =
    [&](int dim_idx, std::string cur_name) {
        if (dim_idx == (int)dims.size()) {
            // ===== 3️⃣ 处理单个元素 =====

            // uint<N>
            if (!m.uint_length.empty()) {
                uint32_t w = std::stoul(m.uint_length);
                out.push_back({cur_name, offset, w});
                offset += w;
                return;
            }

            // 基本类型
            if (table.find(m.type) == table.end()) {
                uint32_t w = get_basic_width(m.type);
                out.push_back({cur_name, offset, w});
                offset += w;
                return;
            }

            // ===== struct 类型 =====
            const auto& sub = table.at(m.type);

            if (sub.is_alias) {
                // alias → 展开目标
                const auto& alias_member = sub.members[0];
                flatten_member(alias_member, table, cur_name, offset, out);
                return;
            }

            if (!sub.enum_members.empty()) {
                // enum → 用最小bit宽
                uint32_t n = sub.enum_members.size();
                uint32_t w = 0;
                while ((1u << w) < n) ++w;
                out.push_back({cur_name, offset, w});
                offset += w;
                return;
            }

            // 普通 struct
            flatten_bundle(sub, table, cur_name, offset, out);
        } else {
            for (uint32_t i = 0; i < dims[dim_idx]; ++i) {
                expand_array(dim_idx + 1,
                    cur_name + "[" + std::to_string(i) + "]");
            }
        }
    };

    expand_array(0, prefix);
}

void flatten_bundle(
    const VulBundleItem& bundle,
    const BundleTable& table,
    const std::string& prefix,
    uint32_t& offset,
    std::vector<FlatField>& out
) {
    for (const auto& m : bundle.members) {
        std::string name = prefix.empty() ? m.name : prefix + "." + m.name;
        flatten_member(m, table, name, offset, out);
    }
}

void flatten_member(
    const VulStaticBundleMember& m,
    const VulStaticBundleLib& table,
    const std::string& prefix,
    uint32_t& offset,
    std::vector<FlatField>& out
) {
    // ===== 1️⃣ 计算数组总元素数 =====
    std::vector<uint32_t> dims;
    for (const auto& d : m.dims) {
        dims.push_back(d);
    }

    uint32_t total = 1;
    for (auto d : dims) total *= d;

    // ===== 2️⃣ 递归展开数组（多维）=====
    std::function<void(int, std::string)> expand_array =
    [&](int dim_idx, std::string cur_name) {
        if (dim_idx == (int)dims.size()) {
            // ===== 3️⃣ 处理单个元素 =====

            // uint<N>
            if (m.uint_length > 0) {
                uint32_t w = m.uint_length;
                out.push_back({cur_name, offset, w});
                offset += w;
                return;
            }

            // 基本类型
            const VulStaticBundle *sub_ptr = nullptr;
            for (const auto& b : table) {
                if (b.name == m.type) {
                    sub_ptr = &b;
                    break;
                }
            }
            if (sub_ptr == nullptr) {
                uint32_t w = get_basic_width(m.type);
                out.push_back({cur_name, offset, w});
                offset += w;
                return;
            }

            // ===== struct 类型 =====
            const auto& sub = *sub_ptr;

            if (sub.is_alias) {
                // alias → 展开目标
                const auto& alias_member = sub.members[0];
                flatten_member(alias_member, table, cur_name, offset, out);
                return;
            }

            if (!sub.enum_members.empty()) {
                // enum → 用最小bit宽
                uint32_t n = sub.enum_members.size();
                uint32_t w = 0;
                while ((1u << w) < n) ++w;
                out.push_back({cur_name, offset, w});
                offset += w;
                return;
            }

            // 普通 struct
            flatten_bundle(sub, table, cur_name, offset, out);
        } else {
            for (uint32_t i = 0; i < dims[dim_idx]; ++i) {
                expand_array(dim_idx + 1,
                    cur_name + "[" + std::to_string(i) + "]");
            }
        }
    };

    expand_array(0, prefix);
}

void flatten_bundle(
    const VulStaticBundle& bundle,
    const VulStaticBundleLib& table,
    const std::string& prefix,
    uint32_t& offset,
    std::vector<FlatField>& out
) {
    if (bundle.is_alias) {
        const auto& alias_member = bundle.members[0];
        flatten_member(alias_member, table, prefix, offset, out);
        return;
    } else if(!bundle.enum_members.empty()) {
        uint64_t n = bundle.enum_members.size();
        for (const auto& enum_member : bundle.enum_members) {
            uint64_t value = static_cast<uint64_t>(enum_member.value);
            if (enum_member.has_value && value > n) {
                n = value + 1;
            }
        }
        uint32_t w = 0;
        while ((1u << w) < n) ++w;
        out.push_back({prefix, offset, w});
        offset += w;
        return;
    } else {
        for (const auto& m : bundle.members) {
            std::string name = prefix.empty() ? m.name : prefix + "." + m.name;
            flatten_member(m, table, name, offset, out);
        }
    }
}
