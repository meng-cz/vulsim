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

namespace operation_module_pipe_get {

class ModulePipeGetOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override {
        string module_name;
        string instance_name;
        auto &modulelib = project.modulelib;

        {
            auto name_arg = op.getArg("name", 0);
            if (!name_arg) {
                return EStr(EOPModCommonMissArg, "Missing argument: [0] name");
            }
            module_name = *name_arg;
            auto inst_arg = op.getArg("pipe", 1);
            if (!inst_arg) {
                return EStr(EOPModCommonMissArg, "Missing argument: [1] pipe");
            }
            instance_name = *inst_arg;
        }

        auto mod_base_iter = modulelib->modules.find(module_name);
        if (mod_base_iter == modulelib->modules.end()) {
            return EStr(EOPModCommonNotFound, "Module not found: " + module_name);
        }
        shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
        if (!mod_ptr) {
            return EStr(EOPModCommonImport, "Cannot get instance from external module: " + module_name);
        }

        auto inst_iter = mod_ptr->pipe_instances.find(instance_name);
        if (inst_iter == mod_ptr->pipe_instances.end()) {
            return EStr(EOPModInstanceNotFound, "Instance not found: " + instance_name + " in module " + module_name);
        }
        auto &inst = inst_iter->second;

        VulOperationResponse resp;
        resp.results["definition"] = serialize::serializePipeToJSON(inst);
        return resp;
    }

    virtual vector<string> help() const override {
        return {
            "Module Pipe Get Operation:",
            "Get the definition of a pipe from a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module to get the pipe from.",
            "  - [1] pipe: The name of the pipe to get.",
            "Results:",
            "  - definition: The JSON definition of the pipe.",
        };
    }
};

auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModulePipeGetOperation>(op);
};
struct ModulePipeGetOperationRegister {
    ModulePipeGetOperationRegister() {
        VulProject::registerOperation("module.pipe.get", factory);
    }
} modulePipeGetOperationRegisterInstance;

} // namespace operation_module_pipe_get