// Copyright (c) 2025 Meng Chengzhen, in Shandong University
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
    std::string enum_type; // empty means non-enum
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
