// Copyright (c) 2026 Meng Chengzhen, in Shandong University
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

#include "project.h"
#include <optional>

using InstancePath = string;
using SignalPath = string;

struct VulTraceMatcher {
    InstancePath instance_path_matcher;
    SignalPath signal_path_matcher;
    bool uses_double_colon = false;
};

struct VulTracedSignal {
    SignalPath signal_path;
    uint32_t bit_width;
    bool is_fixint = false;
    bool trace_all_instances = true;
    vector<vector<std::optional<ConfigRealValue>>> instance_index_filters;
};

VulTraceMatcher parseTraceMatcher(const string &matcher_str);

using VulTraceTable = std::unordered_map<VulInstanceID, vector<VulTracedSignal>>;

VulTraceTable parseTraceOptions(
    const VulStaticProject &project,
    const vector<VulTraceMatcher> &trace_matchers
);
