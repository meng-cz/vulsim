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

#include "serializejson.h"

namespace operation_module_pipe_set {

class ModulePipeSetOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const override { return false; };

    virtual vector<string> help() const override {
        return {
            "Module Pipe Set Operation:",
            "Add/Set/Rename/Remove a Pipe in a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module to set the pipe in.",
            "  - [1] oldname: (optional) The name of the pipe to modify.",
            "  - [2] newname: (optional) The new name of the pipe.",
            "  - [3] definition: (optional) The new definition JSON of the pipe.",
        };
    }
    ModuleName module_name;
    bool has_old = false;
    VulPipe old_instance;
    bool has_new = false;
    VulPipe new_instance;
};

VulOperationResponse ModulePipeSetOperation::execute(VulProject &project) {
    {
        auto name_arg = op.getArg("name", 0);
        if (!name_arg) {
            return EStr(EOPModCommonMissArg, "Missing argument: [0] name");
        }
        module_name = *name_arg;
    }

    // check module existence
    auto &modulelib = project.modulelib;
    auto mod_base_iter = modulelib->modules.find(module_name);
    if (mod_base_iter == modulelib->modules.end()) {
        return EStr(EOPModCommonNotFound, "Module not found: " + module_name);
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
    if (!mod_ptr) {
        return EStr(EOPModCommonImport, "Cannot modify local bundle of external module: " + module_name);
    }
    {
        auto oldname_arg = op.getArg("oldname", 1);
        if (oldname_arg) {
            has_old = true;
            auto iter = mod_ptr->pipe_instances.find(*oldname_arg);
            if (iter == mod_ptr->pipe_instances.end()) {
                return EStr(EOPModPipeNotFound, "Pipe not found: " + *oldname_arg + " in module " + module_name);
            }
            old_instance = iter->second;
            new_instance = old_instance;
        }
    }
    {
        auto newname_arg = op.getArg("newname", 2);
        if (newname_arg) {
            has_new = true;
            new_instance.name = *newname_arg;
            if (!isValidIdentifier(new_instance.name)) {
                return EStr(EOPModPipeNameInvalid, "Invalid pipe name in module '" + module_name + "': " + new_instance.name);
            }
            if (new_instance.name != old_instance.name && mod_ptr->localCheckNameConflict(new_instance.name)) {
                return EStr(EOPModPipeNameDup, "Local pipe name conflict in module '" + module_name + "': " + new_instance.name);
            }
            if (new_instance.name != old_instance.name && project.globalNameConflictCheck(new_instance.name)) {
                return EStr(EOPModPipeNameDup, "Local pipe name conflict in module '" + module_name + "': " + new_instance.name);
            }
        }
        auto def_arg = op.getArg("definition", 3);
        if (def_arg) {
            has_new = true;
            serialize::parsePipeFromJSON(*def_arg, new_instance);
        }
    }

    if (has_old) mod_ptr->pipe_instances.erase(old_instance.name);
    if (has_new) mod_ptr->pipe_instances[new_instance.name] = new_instance;
    project.modified_modules.insert(module_name);
    return VulOperationResponse();
}

string ModulePipeSetOperation::undo(VulProject &project) {
    auto &modulelib = project.modulelib;
    auto mod_base_iter = modulelib->modules.find(module_name);
    if (mod_base_iter == modulelib->modules.end()) {
        return "Cannot undo local bundle change of non-existing module: " + module_name;
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
    if (!mod_ptr) {
        return "Cannot undo local bundle change of external module: " + module_name;
    }

    if (has_new) mod_ptr->pipe_instances.erase(new_instance.name);
    if (has_old) mod_ptr->pipe_instances[old_instance.name] = old_instance;
    project.modified_modules.insert(module_name);
    return "";
}


auto factory_module_pipe_set = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModulePipeSetOperation>(op);
};
struct RegisterModulePipeSetOperation {
    RegisterModulePipeSetOperation() {
        VulProject::registerOperation("module.pipe.set", factory_module_pipe_set);
    }
} register_module_pipe_set_operation;

} // namespace operation_module_pipe_set
