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

namespace operation_module_storage_set {

class ModuleStorageSetOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const override { return true; };

    virtual vector<string> help() const override {
        return {
            "Module Storage Set Operation:",
            "Set a storage value in a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module to set the storage in.",
            "  - [1] oldname (optional): The name of the storage to set.",
            "  - [2] newname (optional): The new name of the storage.",
            "  - [3] definition (optional): The new definition of the storage.",
            "  - [4] category (optional): The new category of the storage.",
        };
    }

    ModuleName module_name;
    bool has_old = false;
    VulStorage old_storage;
    string old_category;
    bool has_new = false;
    VulStorage new_storage;
    string new_category;
};

VulOperationResponse ModuleStorageSetOperation::execute(VulProject &project) {
    {
        auto name_arg = op.getArg("name", 0);
        if (!name_arg) {
            return EStr(EOPModCommonMissArg, "Missing argument: [0] name");
        }
        module_name = *name_arg;
    }
    auto module_iter = project.modulelib->modules.find(module_name);
    if (module_iter == project.modulelib->modules.end()) {
        return EStr(EOPModCommonNotFound, "Module not found: " + module_name);
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(module_iter->second);
    if (!mod_ptr) {
        return EStr(EOPModCommonImport, "Cannot set storage in external module: " + module_name);
    }
    {
        auto oldname_arg = op.getArg("oldname", 1);
        if (oldname_arg) {
            has_old = true;
            old_storage.name = *oldname_arg;
            if (mod_ptr->storages.find(old_storage.name) != mod_ptr->storages.end()) {
                old_storage = mod_ptr->storages[old_storage.name];
                old_category = "regular";
            } else if (mod_ptr->storagenexts.find(old_storage.name) != mod_ptr->storagenexts.end()) {
                old_storage = mod_ptr->storagenexts[old_storage.name];
                old_category = "register";
            } else if (mod_ptr->storagetmp.find(old_storage.name) != mod_ptr->storagetmp.end()) {
                old_storage = mod_ptr->storagetmp[old_storage.name];
                old_category = "temporary";
            } else {
                return EStr(EOPModStorageNotFound, "Storage not found in module '" + module_name + "': " + old_storage.name);
            }
            new_storage = old_storage;
            new_category = old_category;
        }
    }
    {
        auto newname_arg = op.getArg("newname", 2);
        if (newname_arg) {
            has_new = true;
            new_storage.name = *newname_arg;
            if (!isValidIdentifier(new_storage.name)) {
                return EStr(EOPModStorageNameInvalid, "Invalid storage name: " + new_storage.name);
            }
            if (project.globalNameConflictCheck(new_storage.name)) {
                return EStr(EOPModStorageNameInvalid, "Storage name conflicts with existing global name: " + new_storage.name);
            }
            if (mod_ptr->localCheckNameConflict(new_storage.name)) {
                return EStr(EOPModStorageNameInvalid, "Storage name conflicts with existing local name in module '" + module_name + "': " + new_storage.name);
            }
        }
        auto definition_arg = op.getArg("definition", 3);
        if (definition_arg) {
            has_new = true;
            serialize::parseStorageFromJSON(*definition_arg, new_storage);
        }
        auto category_arg = op.getArg("category", 4);
        if (category_arg) {
            has_new = true;
            new_category = *category_arg;
            if (new_category != "regular" && new_category != "register" && new_category != "temporary") {
                return EStr(EOPModStorageDefinitionInvalid, "Invalid storage category: " + new_category);
            }
        }
    }

    // apply changes
    if (has_old) {
        // remove old storage
        if (old_category == "regular") {
            mod_ptr->storages.erase(old_storage.name);
        } else if (old_category == "register") {
            mod_ptr->storagenexts.erase(old_storage.name);
        } else if (old_category == "temporary") {
            mod_ptr->storagetmp.erase(old_storage.name);
        }
    }
    if (has_new) {
        // add new storage
        if (new_category == "regular") {
            mod_ptr->storages[new_storage.name] = new_storage;
        } else if (new_category == "register") {
            mod_ptr->storagenexts[new_storage.name] = new_storage;
        } else if (new_category == "temporary") {
            mod_ptr->storagetmp[new_storage.name] = new_storage;
        }
    }
    project.modified_modules.insert(module_name);

    return VulOperationResponse();
}

string ModuleStorageSetOperation::undo(VulProject &project) {
    auto module_iter = project.modulelib->modules.find(module_name);
    if (module_iter == project.modulelib->modules.end()) [[unlikely]] {
        return "Module '" + module_name + "' not found in undo operation.";
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(module_iter->second);
    if (!mod_ptr) [[unlikely]] {
        return "Cannot undo set storage in external module: " + module_name;
    }
    if (has_new) {
        // remove new storage
        if (new_category == "regular") {
            mod_ptr->storages.erase(new_storage.name);
        } else if (new_category == "register") {
            mod_ptr->storagenexts.erase(new_storage.name);
        } else if (new_category == "temporary") {
            mod_ptr->storagetmp.erase(new_storage.name);
        }
    }
    if (has_old) {
        // add old storage
        if (old_category == "regular") {
            mod_ptr->storages[old_storage.name] = old_storage;
        } else if (old_category == "register") {
            mod_ptr->storagenexts[old_storage.name] = old_storage;
        } else if (old_category == "temporary") {
            mod_ptr->storagetmp[old_storage.name] = old_storage;
        }
    }
    project.modified_modules.insert(module_name);
    return "";
}



auto factory_module_storage_set = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleStorageSetOperation>(op);
};
struct RegisterModuleStorageSetOperation {
    RegisterModuleStorageSetOperation() {
        VulProject::registerOperation("module.storage.set", factory_module_storage_set);
    }
} register_module_storage_set_operation;


} // namespace operation_module_storage_set
