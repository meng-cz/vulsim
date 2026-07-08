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
#include <string_view>

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

struct VulTempEnumMember {
    BMemberName         name;
    ConfigValue         value;
};

struct VulTempBundleMember {
    BMemberName         name;
    BMemberType         type;
    vector<ConfigValue> dims; // only for array types
};
VulTempBundleMember parseMemberDeclaration(const string &decl);

struct VulTempBundle {
    BundleName                      name;
    Comment                         comment;
    vector<VulTempBundleMember>     members;
    vector<VulTempEnumMember>       enum_members;   // if not empty, other members must be empty
    bool                            is_alias = false; // if true, all other fields only contain single member: alias_target
};

using VulTempBundleLib = vector<VulTempBundle>;

inline constexpr std::string_view UIntClassName = "Int";

struct VulStaticTypeSignature {
    BMemberType type;
    ConfigRealValue uint_length; // only for uint types

    string toString() const {
        if (uint_length > 0) {
            return std::string(UIntClassName) + "<" + std::to_string(uint_length) + ">";
        } else {
            return type;
        }
    }
    inline bool operator==(const VulStaticTypeSignature &other) const {
        if (uint_length > 0 || other.uint_length > 0) {
            return type == other.type && uint_length == other.uint_length;
        }
        return type == other.type;
    }
};

VulStaticTypeSignature parseTypeSignature(const string &type_str, const VulStaticConfigLib &config_lib);


struct VulStaticEnumMember {
    BMemberName         name;
    ConfigRealValue     value;
    bool                has_value = false; // whether the value is explicitly defined in the enum definition
};

struct VulStaticBundleMember {
    BMemberName         name;
    VulStaticTypeSignature  type;
    vector<ConfigRealValue> dims; // only for array types
};

struct VulStaticBundle {
    BundleName                      name;
    vector<VulStaticBundleMember>   members;
    vector<VulStaticEnumMember>     enum_members;   // if not empty, other members must be empty
    bool                            is_alias = false; // if true, all other fields only contain single member: alias_target
};

using VulStaticBundleLib = std::vector<VulStaticBundle>;

VulStaticBundle staticalizeBundle(const VulTempBundle &item, const VulStaticConfigLib &config_lib);

VulStaticBundleLib mergeStaticBundleLibs(const VulStaticBundleLib &global_lib, const VulStaticBundleLib &local_lib);

struct FlatField {
    std::string name;  // 展平后的访问路径
    uint32_t    offset;
    uint32_t    width;
    bool        is_fixint = false;
};

void flatten_bundle(
    const VulStaticBundle& bundle,
    const VulStaticBundleLib& table,
    const std::string& prefix,
    uint32_t& offset,
    std::vector<FlatField>& out
);

void flatten_member(
    const VulStaticBundleMember& m,
    const VulStaticBundleLib& table,
    const std::string& prefix,
    uint32_t& offset,
    std::vector<FlatField>& out
);

inline void flatten_type_signature(
    const VulStaticTypeSignature& t,
    const VulStaticBundleLib& table,
    const std::string& prefix,
    uint32_t& offset,
    std::vector<FlatField>& out
) {
    VulStaticBundleMember temp_member;
    temp_member.name = prefix;
    temp_member.type = t;
    flatten_member(temp_member, table, prefix, offset, out);
}
