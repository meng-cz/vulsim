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

#include "module_parser.hpp"

#include "context.hpp"
#include "module_handlers.hpp"
#include "versioning.hpp"
#include "../cppparse.hpp"

using namespace cppparse;

VulTempModule parseTempModule(
    const string &module_name,
    const string &module_filepath,
    unordered_map<string, string> *global_names,
    bool is_global_header
) {
    VulErrorContextGuard _err{"Parsing module '" + module_name + "' from file '" + module_filepath + "'"};

    VCPPModuleContext context;
    context.temp.name = module_name;
    context.temp.filepath = module_filepath;
    context.global_names = global_names;
    context.is_global_header = is_global_header;

    vector<string> code_lines = readFileLines(module_filepath);
    auto trim_res = stripComments(code_lines);
    context.line_mapping = std::move(trim_res.mapping);
    code_lines = std::move(trim_res.lines);

    vector<MacroEntry> macro_entries = findAllMacroEntries(code_lines);
    if (!is_global_header) {
        macro_entries = selectVersionedModuleEntries(macro_entries, context);
    }
    // printf("Found %zu macro entries in module '%s'\n", macro_entries.size(), module_name.c_str());
    for (const auto &entry : macro_entries) {
        // printf("Found macro: %s at %s: ", entry.name.c_str(), context.getOriginalPosition(entry.pos).c_str());
        // for (const auto &arg : entry.args) {
        //     printf("'%s' ", arg.c_str());
        // }
        // printf("\n");
        VCPPModuleHandlerRegistry::instance().run(context, entry);
    }

    return context.temp;
}

