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

string withoutLeadingTimescaleDirective(string text) {
    const size_t directive_pos = text.find_first_not_of(" \t\r\n");
    if (directive_pos == string::npos || text.compare(directive_pos, 10, "`timescale") != 0) {
        return text;
    }
    const size_t line_end = text.find('\n', directive_pos);
    if (line_end == string::npos) {
        return "";
    }
    text.erase(0, line_end + 1);
    return text;
}

} // namespace

RTLV2LogicRTLResult appendRTLV2LogicRTL(
    RTLGenResult &result,
    const VulStaticModuleInstance &module,
    const string &logic_hls_filepath,
    const string &lib_include_dir,
    int unroll_limit
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
        unroll_limit
    );
    if (!logic_rtl.ok) {
        RTLV2LogicRTLResult out;
        out.ok = false;
        out.error = "RTLzz compile failed for '" + logic_module_name + "' in '" + logic_hls_filepath + "': " + logic_rtl.error;
        out.error_debug_codelines = std::move(logic_rtl.error_debug_codelines);
        out.error_signal_debug_text = std::move(logic_rtl.error_signal_debug_text);
        out.error_signal_names = std::move(logic_rtl.error_signal_names);
        return out;
    }

    result.rtl_skeleten_codes.push_back("\n");
    appendTextAsLines(
        result.rtl_skeleten_codes,
        withoutLeadingTimescaleDirective(std::move(logic_rtl.rtl_text)));
    vulDebugNormalize(result.rtl_skeleten_codes, result.rtl_skeleten_debug);
    result.rtl_skeleten_debug_lines = vulDebugBuildGeneratedMap(result.rtl_skeleten_debug);
    RTLV2LogicRTLResult out;
    out.debug_codelines = std::move(logic_rtl.debug_codelines);
    return out;
}

} // namespace rtlgen
