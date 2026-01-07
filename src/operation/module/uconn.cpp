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

namespace operation_module_uconn {

// update sequence connection operation

class ModuleUconnOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const { return true; };

    virtual vector<string> help() const override {
        return {
            "Module Update Connection Operation:",
            "Update sequence connection(s) that match the specified arguments.",
            "Arguments:",
            "  - [0] module: The name of the module to update the connection in.",
            "  - [1] src_instance: The instance name of the former module (or user tick block).",
            "  - [2] dst_instance: The instance name of the latter module (or user tick block).",
        };
    }

    ModuleName module_name;
    VulSequenceConnection added_connection;
};

VulOperationResponse ModuleUconnOperation::execute(VulProject &project) {
    auto &modulelib = project.modulelib;
    ModuleName module_name;
    InstanceName src_instance;
    InstanceName dst_instance;
    {
        auto arg0 = op.getArg("module", 0);
        if (!arg0) {
            return EStr(EOPModUConnMissArg, "Missing argument: [0] module");
        }
        module_name = *arg0;
        auto arg1 = op.getArg("src_instance", 1);
        if (!arg1) {
            return EStr(EOPModUConnMissArg, "Missing argument: [1] src_instance");
        }
        src_instance = *arg1;
        auto arg2 = op.getArg("dst_instance", 2);
        if (!arg2) {
            return EStr(EOPModUConnMissArg, "Missing argument: [2] dst_instance");
        }
        dst_instance = *arg2;
    }

    auto iter = modulelib->modules.find(module_name);
    if (iter == modulelib->modules.end()) {
        return EStr(EOPModCommonNotFound, "Module not found: " + module_name);
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(iter->second);
    if (!mod_ptr) {
        return EStr(EOPModCommonImport, "Module is an external module and cannot be modified: " + module_name);
    }

    if (mod_ptr->user_tick_codeblocks.find(src_instance) == mod_ptr->user_tick_codeblocks.end() && mod_ptr->instances.find(src_instance) == mod_ptr->instances.end()) {
        return EStr(EOPModUConnSrcInstNotFound, "Source instance not found: " + src_instance);
    }
    if (mod_ptr->user_tick_codeblocks.find(dst_instance) == mod_ptr->user_tick_codeblocks.end() && mod_ptr->instances.find(dst_instance) == mod_ptr->instances.end()) {
        return EStr(EOPModUConnDstInstNotFound, "Destination instance not found: " + dst_instance);
    }
    if (src_instance == dst_instance) {
        return EStr(EOPModUConnSelfLoop, "Source and destination instance cannot be the same: " + src_instance);
    }

    added_connection.former_instance = src_instance;
    added_connection.latter_instance = dst_instance;
    vector<InstanceName> looped_insts;
    auto update_order_ptr = mod_ptr->getInstanceUpdateOrder(added_connection, looped_insts);
    if (!update_order_ptr) {
        string looped_inst_str;
        for (const auto &inst_name : looped_insts) {
            if (!looped_inst_str.empty()) {
                looped_inst_str += ", ";
            }
            looped_inst_str += "'" + inst_name + "'";
        }
        return EStr(EOPModUConnLoop, "Adding this update connection would create a loop in the instance update order: " + looped_inst_str);
    }

    mod_ptr->update_constraints.insert(added_connection);
    project.modified_modules.insert(module_name);

    return VulOperationResponse();
}

string ModuleUconnOperation::undo(VulProject &project) {
    auto &modulelib = project.modulelib;

    auto iter = modulelib->modules.find(module_name);
    if (iter == modulelib->modules.end()) {
        return "Module not found during undo: " + module_name;
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(iter->second);
    if (!mod_ptr) {
        return "Module is an external module and cannot be modified during undo: " + module_name;
    }

    mod_ptr->update_constraints.erase(added_connection);
    project.modified_modules.insert(module_name);
    return "";
}


auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleUconnOperation>(op);
};
struct Register {
    Register() {
        VulProject::registerOperation("module.uconn", factory);
    }
} register_instance;

} // namespace operation_module_uconn