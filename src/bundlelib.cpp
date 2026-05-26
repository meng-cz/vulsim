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
#include "stringop.hpp"

#include <algorithm>
#include <functional>

using std::make_unique;
using std::make_shared;



VulStaticBundle staticalizeBundle(const VulBundleItem &item, const VulStaticConfigLib &config_lib) {
    
    VulStaticBundle static_item;
    static_item.name = item.name;
    static_item.is_alias = item.is_alias;

    for (const auto &enum_member : item.enum_members) {
        VulStaticEnumMember static_enum_member;
        static_enum_member.name = enum_member.name;
        static_enum_member.has_value = !enum_member.value.empty();
        VulErrorContextGuard _err{"staticalizing enum member " + enum_member.name};
        if (static_enum_member.has_value) {
            ConfigRealValue value = calculateConstexprValue(enum_member.value, config_lib);
            static_enum_member.value = value;
        }
        static_item.enum_members.push_back(std::move(static_enum_member));
    }

    for (const auto &member : item.members) {
        VulStaticBundleMember static_member;
        static_member.name = member.name;
        static_member.type = member.type;
        VulErrorContextGuard _err{"staticalizing member " + member.name};
        // 计算 uint 长度，同时判断 type 是否是 UInt<N>
        stringop::trim_inplace(static_member.type);
        string uint_length_str = "";
        if (!member.uint_length.empty()) {
            uint_length_str = member.uint_length;
        } else if (static_member.type.starts_with("UInt<") && static_member.type.ends_with(">")) {
            uint_length_str = static_member.type.substr(5, static_member.type.size() - 6);
            static_member.type = "UInt";
        }

        if (!uint_length_str.empty()) {
            VulErrorContextGuard _err{"calculating uint length " + uint_length_str};
            ConfigRealValue uint_len_value = calculateConstexprValue(uint_length_str, config_lib);
            static_member.uint_length = uint_len_value;
        } else {
            static_member.uint_length = 0;
        }
        // 计算数组维度
        for (const auto &dim_expr : member.dims) {
            ConfigRealValue dim_value;
            VulErrorContextGuard _err{"calculating array dimension " + dim_expr};
            dim_value = calculateConstexprValue(dim_expr, config_lib);
            static_member.dims.push_back(dim_value);
        }
        static_item.members.push_back(std::move(static_member));
    }
    return static_item;
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

    throw VulException("Unsupported type: " + type);
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
