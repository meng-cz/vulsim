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
#include "projman.h"

#include <stdexcept>
#include <filesystem>

namespace operation_load {

/**
 * Load Operation:
 * Load a project from disk, including its modules, configs, and bundles.
 * If a project is already opened, return an error.
 * 
 * Arguments:
 * - "name": The project name to load.
 * - "import_paths": (optional) A colon-separated list of import paths to search for import modules.
 */


class LoadOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual bool is_modify() const override { return true; }; // modifies project

    virtual vector<string> help() const override {
        return {
            "Load Operation:",
            "Load a project from disk, including its modules, configs, and bundles.",
            "If a project is already opened, return an error.",
            "",
            "Arguments:",
            "- [0] 'name': The project name to load.",
            "- [1] 'import_paths': (optional) A colon-separated list of import paths to search for import modules."
        };
    }
};



VulOperationResponse LoadOperation::execute(VulProject &project) {
    string proj_name;
    {
        auto arg_ptr = op.getArg("name", 0);
        if (!arg_ptr) {
            return EStr(EOPLoadMissArg, "Missing required argument: name");
        }
        proj_name = *arg_ptr;
    }

    auto _proj_path = VulProjectPathManager::getInstance()->getProjectPath(proj_name);
    if (!_proj_path.has_value()) {
        return EStr(EOPLoadInvalidPath, "Project '" + proj_name + "' does not exist.");
    }

    ErrorMsg resp = project.load(_proj_path.value(), proj_name);

    return resp; // Success
}

OperationFactory loadOperationFactory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<LoadOperation>(op);
};

struct RegisterLoadOperation {
    RegisterLoadOperation() {
        VulProject::registerOperation("load", loadOperationFactory);
    }
} registerLoadOperationInstance;

} // namespace operation_load