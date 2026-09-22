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

#include "rtlgen.h"

#include "debugmap.hpp"
#include "rtlzz_bridge.hpp"

#include <algorithm>

namespace rtlgen {

namespace {

void appendTextAsLines(vector<string> &lines, const string &text) {
    size_t pos = 0;
    while (pos < text.size()) {
        size_t next = text.find('\n', pos);
        if (next == string::npos) {
            lines.push_back(text.substr(pos) + "\n");
            return;
        }
        lines.push_back(text.substr(pos, next - pos + 1));
        pos = next + 1;
    }
}

} // namespace

LogicRTLResult appendLogicRTL(
    RTLGenResult &result,
    const VulStaticModuleInstance &module,
    const string &logic_hls_filepath,
    const string &lib_include_dir,
    int unroll_limit,
    bool release,
    bool concurrent_progress
) {
    if (!result.has_logic_submodule || result.logic_hls_codes.empty()) {
        return {};
    }

    VulErrorContextGuard rtlzz_err("running RTLzz for logic submodule: " + module.simClassName());
    const auto logic_module_name = LogicSubModuleName(module.simClassName());
    RTLzzLogicRTLResult logic_rtl = generateLogicRTLWithRTLzz(
        logic_hls_filepath,
        logic_module_name,
        lib_include_dir,
        result.logic_port_bindings,
        unroll_limit,
        release,
        concurrent_progress
    );
    if (!logic_rtl.ok) {
        LogicRTLResult out;
        out.ok = false;
        out.error = "RTLzz compile failed for '" + logic_module_name + "' in '" + logic_hls_filepath + "': " + logic_rtl.error;
        out.error_debug_codelines = std::move(logic_rtl.error_debug_codelines);
        out.error_signal_debug_text = std::move(logic_rtl.error_signal_debug_text);
        out.error_signal_names = std::move(logic_rtl.error_signal_names);
        return out;
    }

    auto endmodule = std::find(result.rtl_skeleten_codes.begin(),
                               result.rtl_skeleten_codes.end(), "endmodule\n");
    if (endmodule == result.rtl_skeleten_codes.end()) {
        LogicRTLResult out;
        out.ok = false;
        out.error = "RTL module framework is missing endmodule";
        return out;
    }
    vector<string> body_lines;
    body_lines.push_back("\n");
    appendTextAsLines(body_lines, logic_rtl.rtl_text);
    const auto offset = static_cast<size_t>(endmodule - result.rtl_skeleten_codes.begin());
    result.rtl_skeleten_codes.insert(endmodule, body_lines.begin(), body_lines.end());
    result.rtl_skeleten_debug.insert(result.rtl_skeleten_debug.begin() + offset,
                                     body_lines.size(), {});
    vulDebugNormalize(result.rtl_skeleten_codes, result.rtl_skeleten_debug);
    result.rtl_skeleten_debug_lines = vulDebugBuildGeneratedMap(result.rtl_skeleten_debug);
    const string line_prefix = "  rtl_line: ";
    for (auto &line : logic_rtl.debug_codelines) {
        if (line.rfind(line_prefix, 0) != 0) continue;
        const size_t number_begin = line_prefix.size();
        const size_t number_end = line.find_first_not_of("0123456789", number_begin);
        if (number_end == number_begin) continue;
        const auto relative_line = std::stoul(line.substr(number_begin, number_end - number_begin));
        if (relative_line == 0) continue;
        line.replace(number_begin, number_end - number_begin,
                     std::to_string(relative_line + offset + 1));
    }
    LogicRTLResult out;
    out.debug_codelines = std::move(logic_rtl.debug_codelines);
    return out;
}

} // namespace rtlgen
