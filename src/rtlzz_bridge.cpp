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

#include "rtlzz_bridge.hpp"

#include "errormsg.hpp"

#include "rtlzz.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace {

std::vector<std::string> collectErrorSignalNames(
    const std::vector<rtlzz::RtlSignalDebugInfo> &signals
) {
    std::vector<std::string> names;
    std::unordered_set<std::string> seen;
    for (const auto &signal : signals) {
        if (signal.signal_name.empty()) {
            continue;
        }
        if (seen.insert(signal.signal_name).second) {
            names.push_back(signal.signal_name);
        }
    }
    return names;
}

} // namespace

RTLzzLogicRTLResult generateLogicRTLWithRTLzz(
    const std::string &source_file,
    const std::string &top_function,
    const std::string &lib_include_dir,
    int unroll_limit
) {
    std::ifstream input(source_file);
    if (!input) {
        RTLzzLogicRTLResult out;
        out.error = "RTLzz failed to open logic source '" + source_file + "'";
        return out;
    }
    std::vector<std::string> source_codelines;
    std::string line;
    while (std::getline(input, line)) {
        source_codelines.push_back(line + "\n");
    }
    if (!input.eof()) {
        RTLzzLogicRTLResult out;
        out.error = "RTLzz failed to read logic source '" + source_file + "'";
        return out;
    }

    rtlzz::CompileOptions options;
    std::filesystem::path source_path(source_file);
    options.source_name = source_file;
    options.source_codelines = std::move(source_codelines);
    options.vullib_dir = lib_include_dir;
    options.top_function = top_function;
    options.unroll_limit = unroll_limit;
    options.clang_args.push_back("-std=c++20");
    options.rtl_debug = rtlzz::RtlDebugMode::Text;

    const auto source_parent = source_path.parent_path();
    if (!source_parent.empty()) {
        options.include_dirs.push_back(source_parent.string());
    }

    auto result = rtlzz::compileToRtl(std::move(options));
    RTLzzLogicRTLResult out;
    if (!result.ok()) {
        out.error = result.error;
        out.error_debug_codelines = std::move(result.error_debug_codelines);
        out.error_signal_debug_text = std::move(result.error_signal_debug_text);
        out.error_signal_names = collectErrorSignalNames(result.error_signal_debug_signals);
        return out;
    }

    std::ostringstream output;
    for (const auto &output_line : result.output_codelines) {
        output << output_line;
        if (!output_line.empty() && output_line.back() != '\n') {
            output << '\n';
        }
    }
    out.ok = true;
    out.rtl_text = output.str();
    out.debug_codelines = std::move(result.debug_codelines);
    return out;
}
