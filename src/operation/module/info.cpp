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

namespace operation_module_info {

class ModuleInfoOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;

    virtual vector<string> help() const override {
        return {
            "Module Info Operation:",
            "Retrieve information about a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module to retrieve information about.",
            "Returns:",
            "  - info: The information json of the specified module.",
        };
    }
};

VulOperationResponse ModuleInfoOperation::execute(VulProject &project) {
    auto &modulelib = project.modulelib;
    ModuleName module_name;
    {
        auto arg0 = op.getArg("name", 0);
        if (!arg0) {
            return EStr(EOPModCommonMissArg, "Missing argument: [0] name");
        }
        module_name = *arg0;
    }

    auto mod_iter = modulelib->modules.find(module_name);
    if (mod_iter == modulelib->modules.end()) {
        return EStr(EOPModCommonNotFound, "Module does not exist: " + module_name);
    }

    VulOperationResponse response;

    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_iter->second);
    if (!mod_ptr) {
        response.results["info"] = serialize::serializeModuleBaseInfoToJSON(*(mod_iter->second));
    } else {
        response.results["info"] = serialize::serializeModuleInfoToJSON(*mod_ptr, modulelib);
    }

    return response;
}


auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleInfoOperation>(op);
};
struct Register {
    Register() {
        VulProject::registerOperation("module.info", factory);
    }
} register_instance;

} // namespace operation_module_info
