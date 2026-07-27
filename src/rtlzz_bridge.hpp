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

#include <string>
#include <vector>

struct RTLzzLogicRTLResult {
    bool ok = false;
    std::string rtl_text;
    std::vector<std::string> debug_codelines;
    std::string error;
    std::vector<std::string> error_debug_codelines;
    std::string error_signal_debug_text;
    std::vector<std::string> error_signal_names;
};

RTLzzLogicRTLResult generateLogicRTLWithRTLzz(
    const std::string &source_file,
    const std::string &top_function,
    const std::string &lib_include_dir,
    int unroll_limit = 1024
);
