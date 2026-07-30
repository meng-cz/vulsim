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

#include "test_parser.hpp"

#include "common.hpp"
#include "test_handlers.hpp"
#include "../cppparse.hpp"
#include "../stringop.hpp"
#include "../vullib.hpp"

using namespace cppparse;
using namespace stringop;

VulStaticTestHarnessModule parseTestModule(
    const string &test_filepath,
    const VulStaticConfigLib &config_lib,
    const VulStaticBundleLib &bundle_lib
) {
    VCPPTestContext context;
    context.config_lib = config_lib;
    context.bundle_lib = bundle_lib;
    context.filepath = test_filepath;

    VulErrorContextGuard _err{"Parsing test module from file '" + test_filepath + "'"};

    vector<string> code_lines = readFileLines(test_filepath);
    auto trim_res = stripComments(code_lines);
    context.line_mapping = std::move(trim_res.mapping);
    code_lines = std::move(trim_res.lines);

    const string prefix = "#include";
    for (const auto &line_raw : code_lines) {
        string line = trim(line_raw);
        if (line.rfind(prefix, 0) != 0) {
            continue;
        }
        size_t first_quote = line.find('<');
        if (first_quote == string::npos) {
            continue;
        }
        size_t second_quote = line.find('>', first_quote + 1);
        if (second_quote == string::npos || second_quote <= first_quote + 1) {
            continue;
        }
        string included_path = trim(line.substr(first_quote + 1, second_quote - first_quote - 1));
        bool escaped = false;
        for (auto s : VulLibFiles) {
            if (included_path == s) {
                escaped = true;
                break;
            }
        }
        for (auto s : VulEscapedHeaders) {
            if (included_path == s) {
                escaped = true;
                break;
            }
        }
        if (escaped) {
            continue;
        }
        context.test.includedHeaders.push_back(included_path);
    }

    vector<MacroEntry> macro_entries = findAllMacroEntries(code_lines);
    for (const auto &entry : macro_entries) {
        VCPPTestHandlerRegistry::instance().run(context, entry);
    }

    return std::move(context.test);
}

std::pair<string, string> scanTestModuleBindingPaths(const string &test_filepath) {
    vector<string> code_lines = readFileLines(test_filepath);
    code_lines = stripComments(code_lines).lines;
    vector<MacroEntry> macro_entries = findAllMacroEntries(code_lines);

    string top_module_path;
    string project_dir_path;
    for (const auto &entry : macro_entries) {
        if (entry.name == "TOP") {
            if (entry.args.size() != 1) {
                throw VulException("TOP requires exactly 1 argument");
            }
            if (!top_module_path.empty()) {
                throw VulException("Multiple TOP declarations are not allowed");
            }
            top_module_path = parsePathMacroArg(entry.args[0]);
        } else if (entry.name == "PROJECT") {
            if (entry.args.size() != 1) {
                throw VulException("PROJECT requires exactly 1 argument");
            }
            if (!project_dir_path.empty()) {
                throw VulException("Multiple PROJECT declarations are not allowed");
            }
            project_dir_path = parsePathMacroArg(entry.args[0]);
        }
    }
    return {top_module_path, project_dir_path};
}
