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

namespace operation_info {

class InfoOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;
    
    virtual VulOperationResponse execute(VulProject &project) override {
        if (!project.is_opened) {
            return EStr(EOPInfoNotOpened, "No project is opened to get info.");
        }

        VulOperationResponse resp;
        resp.results["name"] = project.name;
        resp.results["dirpath"] = project.dirpath;
        resp.results["top_module"] = project.top_module;
        resp.results["is_modified"] = project.is_modified ? "true" : "false";
        resp.results["is_config_modified"] = project.is_config_modified ? "true" : "false";
        resp.results["is_bundle_modified"] = project.is_bundle_modified ? "true" : "false";

        // modified modules
        {
            vector<string> mod_names;
            for (const auto &mod_name : project.modified_modules) {
                mod_names.push_back(mod_name);
            }
            resp.list_results["modified_modules"] = std::move(mod_names);
        }

        // imports
        {
            vector<string> import_names;
            vector<string> import_paths;
            for (const auto &pair : project.imports) {
                import_names.push_back(pair.first);
                import_paths.push_back(pair.second.abspath);
            }
            resp.list_results["import_names"] = std::move(import_names);
            resp.list_results["import_paths"] = std::move(import_paths);
        }

        return resp;
    }

    virtual vector<string> help() const override {
        return {
            "Info Operation:",
            "Retrieve information about the currently opened project.",
            "If no project is opened, return an error.",
            "",
            "Arguments: None.",
            "Results:",
            "  - name: The name of the project.",
            "  - dirpath: The directory path of the project.",
            "  - top_module: The name of the top module of the project.",
            "  - is_modified: Whether the project has unsaved modifications.",
            "  - is_config_modified: Whether the configuration library has unsaved modifications.",
            "  - is_bundle_modified: Whether the bundle library has unsaved modifications.",
            "  - modified_modules: List of names of modified modules.",
            "  - import_names: List of imported modules.",
            "  - import_paths: List of import paths."
        };
    }

};




} // namespace operation_info
