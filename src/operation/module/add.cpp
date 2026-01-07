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

namespace operation_module_add {

class ModuleAddOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const { return true; };

    virtual vector<string> help() const override {
        return {
            "Module Add Operation:",
            "Add a new empty module to the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module to add.",
            "  - [1] (optional) comment: The comment for the module.",
        };
    }

    ModuleName added_module_name;
    string added_module_comment;
};

VulOperationResponse ModuleAddOperation::execute(VulProject &project) {
    string name;
    string comment = "";
    auto &modulelib = project.modulelib;

    {
        auto name_arg = op.getArg("name", 0);
        if (!name_arg) {
            return EStr(EOPModAddMissArg, "Missing argument: [0] name");
        }
        name = *name_arg;
        auto comment_arg = op.getArg("comment", 1);
        if (comment_arg) {
            comment = *comment_arg;
        }
    }

    // Validate name
    if (!isValidIdentifier(name)) {
        return EStr(EOPModAddNameInvalid, "Invalid module name: '" + name + "'");
    }
    if (project.globalNameConflictCheck(name) || modulelib->modules.find(name) != modulelib->modules.end()) {
        return EStr(EOPModAddNameConflict, "Module name conflict: '" + name + "'");
    }


    // Add the module
    auto new_module = std::make_shared<VulModule>();
    new_module->name = name;
    new_module->comment = comment;
    modulelib->modules[name] = std::move(new_module);

    project.is_modified = true;
    project.modified_modules.insert(name);
    added_module_name = name;
    added_module_comment = comment;

    return VulOperationResponse(); // Success
};

string ModuleAddOperation::undo(VulProject &project) {
    auto &modulelib = project.modulelib;
    auto iter = modulelib->modules.find(added_module_name);
    if (iter == modulelib->modules.end()) [[unlikely]] {
        return "Module '" + added_module_name + "' not found in undo operation.";
    }
    modulelib->modules.erase(iter);
    project.is_modified = true;
    project.modified_modules.insert(added_module_name);
    return "";
}

auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleAddOperation>(op);
};
struct RegisterModuleAddOperation {
    RegisterModuleAddOperation() {
        VulProject::registerOperation("module.add", factory);
    }
} registerModuleAddOperationInstance;

} // namespace operation_module_add
