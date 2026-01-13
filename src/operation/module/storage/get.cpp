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

namespace operation_module_storage_get {

class ModuleStorageGetOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override {
        string module_name, storage_name;
        {
            auto name_arg = op.getArg("name", 0);
            if (!name_arg) {
                return EStr(EOPModCommonMissArg, "Missing argument: [0] name");
            }
            module_name = *name_arg;
            auto storagename_arg = op.getArg("storagename", 1);
            if (!storagename_arg) {
                return EStr(EOPModCommonMissArg, "Missing argument: [1] storagename");
            }
            storage_name = *storagename_arg;
        }
        auto mod_iter = project.modulelib->modules.find(module_name);
        if (mod_iter == project.modulelib->modules.end()) {
            return EStr(EOPModCommonNotFound, "Module not found: " + module_name);
        }
        auto &mod_base_ptr = mod_iter->second;
        shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_ptr);
        if (!mod_ptr) {
            return EStr(EOPModCommonImport, "Module not found: " + module_name);
        }
        
        bool found = false;
        VulOperationResponse response;
        // search in storages
        {
            auto stor_iter = mod_ptr->storages.find(storage_name);
            if (stor_iter != mod_ptr->storages.end()) {
                found = true;
                response.results["category"] = "regular";
                response.results["definition"] = serialize::serializeStorageToJSON(stor_iter->second);
            }
        }
        {
            auto stor_iter = mod_ptr->storagenexts.find(storage_name);
            if (stor_iter != mod_ptr->storagenexts.end()) {
                found = true;
                response.results["category"] = "register";
                response.results["definition"] = serialize::serializeStorageToJSON(stor_iter->second);
            }
        }
        {
            auto stor_iter = mod_ptr->storagetmp.find(storage_name);
            if (stor_iter != mod_ptr->storagetmp.end()) {
                found = true;
                response.results["category"] = "temporary";
                response.results["definition"] = serialize::serializeStorageToJSON(stor_iter->second);
            }
        }
        if (!found) {
            return EStr(EOPModStorageNotFound, "Storage not found: " + storage_name);
        }
        return response;
    }

    virtual vector<string> help() const override {
        return {
            "Module Storage Get Operation:",
            "Get a storage value from a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module to get the storage from.",
            "  - [1] storagename: The name of the storage to get.",
            "Results:",
            "  - category: The type of the storage (\"regular\", \"register\", \"temporary\", etc.).",
            "  - definition: The definition JSON of the storage.",
        };
    }
};

auto factory_module_storage_get = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleStorageGetOperation>(op);
};
struct RegisterModuleStorageGetOperation {
    RegisterModuleStorageGetOperation() {
        VulProject::registerOperation("module.storage.get", factory_module_storage_get);
    }
} register_module_storage_get_operation;

} // namespace operation_module_storage_get
