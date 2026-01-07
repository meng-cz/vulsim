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

#include "project.h"

namespace operation_module_list {

class ModuleListOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;
    virtual VulOperationResponse execute(VulProject &project) override;
    virtual vector<string> help() const override {
        return {
            "Module List Operation:",
            "List all modules in the project's module library.",
            "Arguments:",
            "  - [0] name (optional): If specified, only list modules whose names contain this substring.",
            "  - [1] exact (optional): If 'true', only list modules whose names exactly match the specified name. Default: false.",
            "  - [2] imported_only (optional): If 'true', only list imported modules. Default: false.",
            "  - [3] local_only (optional): If 'true', only list locally defined modules. Default: false.",
            "  - [4] list_children (optional): If 'true', for each module, also list its direct child module instances. Default: false.",
            "Results:",
            "  - names: A list of module names matching the criteria.",
            "  - sources: A list of module sources ('imported' or 'local') corresponding to the module names.",
            "  - children: A list of child modules for each module, separated by spaces (only if list_children is true).",
        };
    }
};

VulOperationResponse ModuleListOperation::execute(VulProject &project) {
    auto &modulelib = project.modulelib;

    string name_filter = "";
    {
        auto arg0 = op.getArg("name", 0);
        if (arg0) {
            name_filter = *arg0;
        }
    }
    bool exact = op.getBoolArg("exact", 1);
    bool imported_only = op.getBoolArg("imported_only", 2);
    bool local_only = op.getBoolArg("local_only", 3);
    bool list_children = op.getBoolArg("list_children", 4);

    vector<string> matched_names;
    vector<string> matched_sources;
    vector<string> matched_children;

    for (const auto &pair : modulelib->modules) {
        const auto &mod_name = pair.first;
        const auto &mod_base_ptr = pair.second;

        // Apply name filter
        if (!name_filter.empty()) {
            if (exact) {
                if (mod_name != name_filter) {
                    continue;
                }
            } else {
                if (mod_name.find(name_filter) == string::npos) {
                    continue;
                }
            }
        }

        // Apply imported/local filter
        bool is_imported = false;
        shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_ptr);
        if (!mod_ptr) {
            is_imported = true;
        }
        if (imported_only && !is_imported) {
            continue;
        }
        if (local_only && is_imported) {
            continue;
        }

        matched_names.push_back(mod_name);
        matched_sources.push_back(is_imported ? "imported" : "local");

        if (list_children) {
            if (is_imported) {
                matched_children.push_back("");
                continue;
            }
            unordered_set<ModuleName> child_modules;
            for (const auto &inst_pair : mod_ptr->instances) {
                child_modules.insert(inst_pair.second.module_name);
            }
            string children_str;
            for (const auto &child_mod : child_modules) {
                if (!children_str.empty()) {
                    children_str += " ";
                }
                children_str += child_mod;
            }
            matched_children.push_back(children_str);
        }
    }

    VulOperationResponse response;
    response.list_results["names"] = std::move(matched_names);
    response.list_results["sources"] = std::move(matched_sources);
    if (list_children) {
        response.list_results["children"] = std::move(matched_children);
    }
    return response;
};

auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleListOperation>(op);
};
struct ModuleListOperationRegister {
    ModuleListOperationRegister() {
        VulProject::registerOperation("module.list", factory);
    }
} moduleListOperationRegisterInstance;

} // namespace operation_module_list
