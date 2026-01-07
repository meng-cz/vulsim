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

namespace operation_module_remove {

class ModuleRemoveOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const override { return true; };
    virtual vector<string> help() const override {
        return {
            "Module Remove Operation:",
            "Remove an existing empty module from the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module to remove.",
        };
    }

    ModuleName removed_module_name;
    string removed_module_comment;
};

VulOperationResponse ModuleRemoveOperation::execute(VulProject &project) {
    string name;
    auto &modulelib = project.modulelib;

    {
        auto name_arg = op.getArg("name", 0);
        if (!name_arg) {
            return EStr(EOPModRemoveMissArg, "Missing argument: [0] name");
        }
        name = *name_arg;
    }

    // check existence
    auto iter = modulelib->modules.find(name);
    if (iter == modulelib->modules.end()) {
        return EStr(EOPModRemoveNameNotFound, "Module '" + name + "' does not exist in the module library");
    }

    // check empty
    auto &modbase = iter->second;
    shared_ptr<VulModule> mod = std::dynamic_pointer_cast<VulModule>(modbase);
    if (!mod) {
        return EStr(EOPModRemoveImport, "Module '" + name + "' is imported and cannot be removed");
    }
    if (!mod->isEmptyModule()) {
        return EStr(EOPModRemoveNotEmpty, "Module '" + name + "' is not empty and cannot be removed");
    }
    
    // remove
    removed_module_name = name;
    removed_module_comment = mod->comment;
    modulelib->modules.erase(name);

    project.is_modified = true;
    project.modified_modules.insert(name);

    return VulOperationResponse();
}

string ModuleRemoveOperation::undo(VulProject &project) {
    auto &modulelib = project.modulelib;

    // check existence
    auto iter = modulelib->modules.find(removed_module_name);
    if (iter != modulelib->modules.end()) {
        return "Module '" + removed_module_name + "' already exists in the module library, cannot undo removal.";
    }

    // add back
    auto new_module = std::make_shared<VulModule>();
    new_module->name = removed_module_name;
    new_module->comment = removed_module_comment;
    modulelib->modules[removed_module_name] = std::move(new_module);

    project.is_modified = true;
    project.modified_modules.insert(removed_module_name);

    return "";
}

auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return make_unique<ModuleRemoveOperation>(op);
};
struct ModuleRemoveOperationRegister {
    ModuleRemoveOperationRegister() {
        VulProject::registerOperation("module.remove", factory);
    }
} module_remove_operation_register_instance;

} // namespace operation_module_remove
