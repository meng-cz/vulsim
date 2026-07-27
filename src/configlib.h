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
#include "type.h"

#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <unordered_set>
#include <memory>

using std::pair;
using std::vector;
using std::string;
using std::unordered_map;
using std::map;
using std::unordered_set;
using std::shared_ptr;
using std::unique_ptr;

typedef int64_t ConfigRealValue;
typedef string GroupName;
typedef string ConfigName;
typedef string ConfigValue;

struct VulTempConfig {
    ConfigName name;
    ConfigValue value;
};

using VulTempConfigLib = std::vector<VulTempConfig>;


/**
 * 不再长期维护配置间的依赖关系，而是每次插入时直接计算并存储配置的实际整数值。
 * 用于命令行工具的单次解析-生成工具流，而非是一个长期维护与更新的配置库。
 */
using VulStaticConfigLib = std::map<ConfigName, ConfigRealValue>;

void insertStaticConfig(VulStaticConfigLib &config_lib, const ConfigName &name, const ConfigValue &value);

inline void insertStaticConfig(VulStaticConfigLib &config_lib, const VulTempConfig &conf) {
    insertStaticConfig(config_lib, conf.name, conf.value);
}

inline void insertStaticConfigs(VulStaticConfigLib &config_lib, const VulTempConfigLib &confs) {
    for (const auto &conf : confs) {
        insertStaticConfig(config_lib, conf.name, conf.value);
    }
}

ConfigRealValue calculateConstexprValue(const ConfigValue &value, const VulStaticConfigLib &config_lib);

VulStaticConfigLib mergeStaticConfigLibs(const VulStaticConfigLib &global_lib, const VulStaticConfigLib &local_lib);

// TOBE CLEANUP:

struct VulConfigItem {
    ConfigName name;
    ConfigValue value;
    string comment;
};
