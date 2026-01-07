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

namespace operation_module_udisconn {

// disconnect update sequence connection

class ModuleUdisconnOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const override { return true; };

    virtual vector<string> help() const override {
        return {
            "Module Update Disconnect Operation:",
            "Disconnect update sequence connection(s) that match the specified arguments.",
            "Arguments:",
            "  - [0] module: The name of the module to disconnect the connection from.",
            "  - [1] src_instance (optional): The instance name of the former module (or user tick block).",
            "  - [2] dst_instance (optional): The instance name of the latter module (or user tick block).",
        };
    }

    ModuleName module_name;
    vector<VulSequenceConnection> removed_conns;
};

VulOperationResponse ModuleUdisconnOperation::execute(VulProject &project) {
    auto &modulelib = project.modulelib;
    InstanceName src_instance = "";
    InstanceName dst_instance = "";
    {
        auto arg0 = op.getArg("module", 0);
        if (!arg0) {
            return EStr(EOPModCommonMissArg, "Missing argument: [0] module");
        }
        module_name = *arg0;
        auto arg1 = op.getArg("src_instance", 1);
        if (arg1) {
            src_instance = *arg1;
        }
        auto arg2 = op.getArg("dst_instance", 2);
        if (arg2) {
            dst_instance = *arg2;
        }
    }

    auto iter = modulelib->modules.find(module_name);
    if (iter == modulelib->modules.end()) {
        return EStr(EOPModCommonNotFound, "Module not found: " + module_name);
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(iter->second);
    if (!mod_ptr) {
        return EStr(EOPModCommonImport, "Module is an external module and cannot be modified: " + module_name);
    }

    // find matching connections
    for (auto it = mod_ptr->update_constraints.begin(); it != mod_ptr->update_constraints.end(); ) {
        const VulSequenceConnection &conn = *it;
        if ((!src_instance.empty() && conn.former_instance != src_instance) ||
            (!dst_instance.empty() && conn.latter_instance != dst_instance)) {
            ++it;
            continue;
        }
        // match, remove it
        removed_conns.push_back(conn);
        it = mod_ptr->update_constraints.erase(it);
    }

    project.modified_modules.insert(module_name);
    return VulOperationResponse();
}

string ModuleUdisconnOperation::undo(VulProject &project) {
    auto &modulelib = project.modulelib;
    auto iter = modulelib->modules.find(module_name);
    if (iter == modulelib->modules.end()) [[unlikely]] {
        return "Module '" + module_name + "' not found in undo operation.";
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(iter->second);
    if (!mod_ptr) {
        return "Module '" + module_name + "' is an external module and cannot be modified in undo operation.";
    }

    for (const auto &conn : removed_conns) {
        mod_ptr->update_constraints.insert(conn);
    }

    return "";
}

auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleUdisconnOperation>(op);
};
struct Register {
    Register() {
        VulProject::registerOperation("module.udisconn", factory);
    }
} register_instance;


} // namespace operation_module_udisconn
