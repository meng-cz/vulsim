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

namespace operation_module_localbund_get {

class ModuleLocalBundleGetOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override {
        string module_name;
        string bundle_name;
        auto &modulelib = project.modulelib;

        {
            auto name_arg = op.getArg("name", 0);
            if (!name_arg) {
                return EStr(EOPModCommonMissArg, "Missing argument: [0] name");
            }
            module_name = *name_arg;
            auto bund_arg = op.getArg("bundle", 1);
            if (!bund_arg) {
                return EStr(EOPModCommonMissArg, "Missing argument: [1] bundle");
            }
            bundle_name = *bund_arg;
        }

        // check module existence
        auto mod_base_iter = modulelib->modules.find(module_name);
        if (mod_base_iter == modulelib->modules.end()) {
            return EStr(EOPModCommonNotFound, "Module not found: " + module_name);
        }
        shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
        if (!mod_ptr) {
            return EStr(EOPModCommonImport, "Cannot get local bundle from external module: " + module_name);
        }

        // check local bundle existence
        auto bund_iter = mod_ptr->local_bundles.find(bundle_name);
        if (bund_iter == mod_ptr->local_bundles.end()) {
            return EStr(EOPModBundNotFound, "Local bundle '" + bundle_name + "' not found in module '" + module_name + "'");
        }
        const VulBundleItem &bund_item = bund_iter->second;

        // serialize and return
        string bund_json = serialize::serializeBundleItemToJSON(bund_item);
        VulOperationResponse resp;
        resp.results["definition"] = bund_json;
        return resp;
    }

    virtual vector<string> help() const override {
        return {
            "Module Local Bundle Get Operation:",
            "Get a local bundle definition from a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module to get the local bundle from.",
            "  - [1] bundle: The name of the local bundle to get.",
            "Results:",
            "  - definition: The JSON definition of the local bundle.",
        };
    }
};

auto factory_module_localbund_get = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleLocalBundleGetOperation>(op);
};
struct RegisterModuleLocalBundleGetOperation {
    RegisterModuleLocalBundleGetOperation() {
        VulProject::registerOperation("module.bundle.get", factory_module_localbund_get);
    }
} register_module_localbund_get_operation;

} // namespace operation_module_localbund_get
