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

namespace operation_module_rename {

class ModuleRenameOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const { return true; };

    virtual vector<string> help() const override {
        return {
            "Module Rename Operation:",
            "Rename a module in the project's module library.",
            "Arguments:",
            "  - [0] old_name: The current name of the module to rename.",
            "  - [1] new_name: The new name for the module.",
            "  - [2] update_references (optional): Whether to update external references in other modules (true/false). Default: true.",
        };
    }

    ModuleName old_name;
    ModuleName new_name;
    bool update_references = false;
};

VulOperationResponse ModuleRenameOperation::execute(VulProject &project) {
    auto &modulelib = project.modulelib;

    {
        auto arg0 = op.getArg("old_name", 0);
        if (!arg0) {
            return EStr(EOPModRenameMissArg, "Missing argument: [0] old_name");
        }
        old_name = *arg0;
        auto arg1 = op.getArg("new_name", 0);
        if (!arg1) {
            return EStr(EOPModRenameMissArg, "Missing argument: [1] new_name");
        }
        new_name = *arg1;
    }
    update_references = op.getBoolArg("update_references", 0);

    // Check existence
    auto mod_iter = modulelib->modules.find(old_name);
    if (mod_iter == modulelib->modules.end()) {
        return EStr(EOPModRenameNameNotFound, "Module does not exist: '" + old_name + "'");
    }
    if (!isValidIdentifier(new_name)) {
        return EStr(EOPModRenameNameInvalid, "Invalid module name: '" + new_name + "'");
    }
    if (project.globalNameConflictCheck(new_name) || modulelib->modules.find(new_name) != modulelib->modules.end()) {
        return EStr(EOPModRenameNameConflict, "Module already exists: '" + new_name + "'");
    }

    // Rename module
    auto mod_base_ptr = mod_iter->second;
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_ptr);
    if (!mod_ptr) {
        return EStr(EOPModRenameImport, "Cannot rename external module: '" + old_name + "'");
    }

    unordered_set<ModuleName> referencing_modules;
    for (const auto &pair : modulelib->modules) {
        if (pair.first == old_name) {
            continue;
        }
        shared_ptr<VulModule> other_mod_ptr = std::dynamic_pointer_cast<VulModule>(pair.second);
        if (!other_mod_ptr) {
            continue;
        }
        for (const auto &inst_pair : other_mod_ptr->instances) {
            const auto &inst = inst_pair.second;
            if (inst.module_name == old_name) {
                referencing_modules.insert(pair.first);
            }
        }
    }
    if (!update_references && !referencing_modules.empty()) {
        string ref_mods;
        for (const auto &ref_mod_name : referencing_modules) {
            if (!ref_mods.empty()) {
                ref_mods += ", ";
            }
            ref_mods += "'" + ref_mod_name + "'";
        }
        return EStr(EOPModRenameNameConflict, "Module is referenced by other modules: " + ref_mods + ". Set update_references to true to update references.");
    }

    for (const auto &ref_mod_name : referencing_modules) {
        auto ref_mod_iter = modulelib->modules.find(ref_mod_name);
        if (ref_mod_iter == modulelib->modules.end()) {
            continue;
        }
        shared_ptr<VulModule> ref_mod_ptr = std::dynamic_pointer_cast<VulModule>(ref_mod_iter->second);
        if (!ref_mod_ptr) {
            continue;
        }
        for (auto &inst_pair : ref_mod_ptr->instances) {
            auto &inst = inst_pair.second;
            if (inst.module_name == old_name) {
                inst.module_name = new_name;
            }
        }
        ref_mod_ptr->updateDynamicReferences();
        project.modified_modules.insert(ref_mod_name);
    }

    mod_ptr->name = new_name;
    modulelib->modules[new_name] = mod_ptr;
    modulelib->modules.erase(mod_iter);

    project.modified_modules.insert(old_name);
    project.modified_modules.insert(new_name);
    project.is_modified = true;

    return VulOperationResponse();
}

string ModuleRenameOperation::undo(VulProject &project) {
    auto &modulelib = project.modulelib;

    // Check existence
    auto mod_iter = modulelib->modules.find(new_name);
    if (mod_iter == modulelib->modules.end()) {
        return "Module does not exist: '" + new_name + "'";
    }
    if (project.globalNameConflictCheck(old_name) || modulelib->modules.find(old_name) != modulelib->modules.end()) {
        return "Module already exists: '" + old_name + "'";
    }

    // Rename module back
    auto mod_base_ptr = mod_iter->second;
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_ptr);
    if (!mod_ptr) {
        return "Cannot rename external module: '" + new_name + "'";
    }

    for (const auto &pair : modulelib->modules) {
        if (pair.first == new_name) {
            continue;
        }
        shared_ptr<VulModule> other_mod_ptr = std::dynamic_pointer_cast<VulModule>(pair.second);
        if (!other_mod_ptr) {
            continue;
        }
        bool modified = false;
        for (auto &inst_pair : other_mod_ptr->instances) {
            auto &inst = inst_pair.second;
            if (inst.module_name == new_name) {
                inst.module_name = old_name;
                modified = true;
            }
        }
        if (modified) {
            other_mod_ptr->updateDynamicReferences();
            project.modified_modules.insert(pair.first);
        }
    }

    mod_ptr->name = old_name;
    modulelib->modules[old_name] = mod_ptr;
    modulelib->modules.erase(mod_iter);

    project.modified_modules.insert(old_name);
    project.modified_modules.insert(new_name);
    project.is_modified = true;
    return "";
}



auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleRenameOperation>(op);
};
struct ModuleRenameOperationRegister {
    ModuleRenameOperationRegister() {
        VulProject::registerOperation("module.rename", factory);
    }
} moduleRenameOperationRegisterInstance;

} // namespace operation_module_rename
