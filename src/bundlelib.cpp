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

VulTempBundleMember parseMemberDeclaration(const string &decl) {
    auto strip_trailing_dims = [](string &part, vector<ConfigValue> &dims) {
        vector<ConfigValue> reversed_dims;
        size_t end = part.size();
        while (end > 0) {
            while (end > 0 && std::isspace(static_cast<unsigned char>(part[end - 1]))) {
                --end;
            }
            if (end == 0 || part[end - 1] != ']') {
                break;
            }

            size_t i = end - 1;
            int depth = 1;
            while (i > 0) {
                --i;
                if (part[i] == ']') {
                    ++depth;
                } else if (part[i] == '[') {
                    --depth;
                    if (depth == 0) {
                        break;
                    }
                }
            }
            if (depth != 0) {
                throw VulException("Invalid member declaration: missing '[' in " + part);
            }

            string dim_expr = part.substr(i + 1, end - i - 2);
            stringop::trim_inplace(dim_expr);
            if (dim_expr.empty()) {
                throw VulException("Invalid member declaration: empty dimension in " + part);
            }
            reversed_dims.push_back(std::move(dim_expr));
            end = i;
        }

        part = part.substr(0, end);
        stringop::trim_inplace(part);

        dims.insert(dims.end(), reversed_dims.rbegin(), reversed_dims.rend());
    };

    string s = decl;
    stringop::trim_inplace(s);
    if (!s.empty() && s.back() == ';') {
        s.pop_back();
        stringop::trim_inplace(s);
    }
    if (s.empty()) {
        throw VulException("Invalid member declaration: empty declaration");
    }

    int square_depth = 0;
    size_t split_pos = string::npos;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '[') {
            ++square_depth;
        } else if (c == ']') {
            if (square_depth == 0) {
                throw VulException("Invalid member declaration: unmatched ']' in " + s);
            }
            --square_depth;
        } else if (std::isspace(static_cast<unsigned char>(c)) && square_depth == 0) {
            split_pos = i;
        }
    }
    if (square_depth != 0 || split_pos == string::npos) {
        throw VulException("Invalid member declaration: expected `type name` form in " + s);
    }

    size_t right_start = split_pos;
    while (right_start < s.size() && std::isspace(static_cast<unsigned char>(s[right_start]))) {
        ++right_start;
    }
    if (right_start >= s.size()) {
        throw VulException("Invalid member declaration: missing member name in " + s);
    }

    VulTempBundleMember out;
    out.type = s.substr(0, split_pos);
    out.name = s.substr(right_start);
    stringop::trim_inplace(out.type);
    stringop::trim_inplace(out.name);
    if (out.type.empty() || out.name.empty()) {
        throw VulException("Invalid member declaration: missing type or name in " + s);
    }

    strip_trailing_dims(out.type, out.dims);
    strip_trailing_dims(out.name, out.dims);
    if (out.type.empty() || out.name.empty()) {
        throw VulException("Invalid member declaration: missing type or name in " + s);
    }

    for (char c : out.name) {
        if (std::isspace(static_cast<unsigned char>(c)) || c == '[' || c == ']') {
            throw VulException("Invalid member declaration: bad member name `" + out.name + "`");
        }
    }
    return out;
}

VulStaticTypeSignature parseTypeSignature(const string &type_str, const VulStaticConfigLib &config_lib) {
    VulStaticTypeSignature signature;
    // parse Int<N> to extract N
    const string prefix = std::string(UIntClassName) + "<";
    const string suffix = ">";
    if (type_str.substr(0, prefix.size()) == prefix && type_str.size() > prefix.size() + suffix.size() && type_str.substr(type_str.size() - suffix.size()) == suffix) {
        string num_str = type_str.substr(prefix.size(), type_str.size() - prefix.size() - suffix.size());
        ConfigRealValue value = calculateConstexprValue(num_str, config_lib);
        signature.type = std::string(UIntClassName);
        signature.uint_length = value;
    } else {
        signature.type = type_str;
        signature.uint_length = 0;
    }
    return signature;
}


VulStaticBundle staticalizeBundle(const VulTempBundle &item, const VulStaticConfigLib &config_lib) {
    
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
        VulErrorContextGuard _err{"staticalizing member " + member.name};
        static_member.type = parseTypeSignature(member.type, config_lib);
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

VulStaticBundleLib mergeStaticBundleLibs(const VulStaticBundleLib &global_lib, const VulStaticBundleLib &local_lib) {
    VulStaticBundleLib merged_lib = global_lib;
    for (const auto &bundle : local_lib) {
        auto iter = std::find_if(merged_lib.begin(), merged_lib.end(), [&](const VulStaticBundle &b) {
            return b.name == bundle.name;
        });
        if (iter != merged_lib.end()) {
            *iter = bundle;
        } else {
            merged_lib.push_back(bundle);
        }
    }
    return merged_lib;
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
            if (m.type.uint_length > 0) {
                uint32_t w = m.type.uint_length;
                out.push_back({cur_name, offset, w});
                offset += w;
                return;
            }

            // 基本类型
            const VulStaticBundle *sub_ptr = nullptr;
            for (const auto& b : table) {
                if (b.name == m.type.type) {
                    sub_ptr = &b;
                    break;
                }
            }
            if (sub_ptr == nullptr) {
                uint32_t w = get_basic_width(m.type.type);
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
