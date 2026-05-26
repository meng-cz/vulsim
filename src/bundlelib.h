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

struct VulStaticEnumMember {
    BMemberName         name;
    ConfigRealValue     value;
    bool                has_value = false; // whether the value is explicitly defined in the enum definition
};

struct VulStaticBundleMember {
    BMemberName         name;
    BMemberType         type;
    ConfigRealValue     uint_length; // only for uint types
    vector<ConfigRealValue> dims; // only for array types
};

struct VulStaticBundle {
    BundleName                      name;
    vector<VulStaticBundleMember>   members;
    vector<VulStaticEnumMember>     enum_members;   // if not empty, other members must be empty
    bool                            is_alias = false; // if true, all other fields only contain single member: alias_target
};

using VulStaticBundleLib = std::vector<VulStaticBundle>;



typedef struct {
    BMemberName         name;
    ConfigValue         value;
    Comment             comment;
} VulBundleEnumMember;

class VulBundleMember {
public:
    BMemberName         name;
    ConfigValue         value; // only for basic types and uint types, default zero-initialized
    Comment             comment;

    BMemberType         type;
    ConfigValue         uint_length; // only for uint types

    vector<ConfigValue> dims;

    inline string typeString() const {
        string t = (uint_length.empty() ? type : ("UInt<" + uint_length + ">"));
        for (const auto &d : dims) {
            t += "[" + d + "]";
        }
        return t;
    }
};

class VulBundleItem {
public:
    BundleName                      name;
    Comment                         comment;
    vector<VulBundleMember>         members;
    vector<VulBundleEnumMember>     enum_members;   // if not empty, other members must be empty
    bool                            is_alias = false; // if true, all other fields only contain single member: alias_target
};

VulStaticBundle staticalizeBundle(const VulBundleItem &item, const VulStaticConfigLib &config_lib);

struct FlatField {
    std::string name;  // 展平后的访问路径
    uint32_t    offset;
    uint32_t    width;
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

