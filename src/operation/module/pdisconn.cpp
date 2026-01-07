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

namespace operation_module_pdisconn {

class ModulePdisconnOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const { return true; };

    virtual vector<string> help() const override {
        return {
            "Instance-Pipe Disconnect Operation:",
            "Disconnect pipe port between instances and pipe instances (or __top__ instance).",
            "Arguments:",
            "  - [0] module: The name of the module to remove the pipe connection from.",
            "  - [1] instance (optional): The source instance name.",
            "  - [2] port (optional): The name of the pipe port to disconnect.",
            "  - [3] pipe (optional): The pipe instance name (or __top__ instance).",
            "  - [4] pipe_port (optional): The name of the pipe port to disconnect from. Valid when pipe is __top__.",
        };
    }

    ModuleName module_name;
    vector<VulModulePipeConnection> removed_conns;
};

VulOperationResponse ModulePdisconnOperation::execute(VulProject &project) {
    auto &modulelib = project.modulelib;
    InstanceName src_instance = "";
    PipePortName src_port = "";
    InstanceName dst_instance = "";
    PipePortName dst_port = "";
    {
        auto arg0 = op.getArg("module", 0);
        if (!arg0) {
            return EStr(EOPModCommonMissArg, "Missing argument: [0] module");
        }
        module_name = *arg0;
        auto arg1 = op.getArg("instance", 1);
        if (arg1) {
            src_instance = *arg1;
        }
        auto arg2 = op.getArg("port", 2);
        if (arg2) {
            src_port = *arg2;
        }
        auto arg3 = op.getArg("pipe", 3);
        if (arg3) {
            dst_instance = *arg3;
        }
        auto arg4 = op.getArg("pipe_port", 4);
        if (arg4) {
            dst_port = *arg4;
        }
    }
    if (dst_instance != VulModule::TopInterface) {
        dst_port = "";
    }

    auto iter = modulelib->modules.find(module_name);
    if (iter == modulelib->modules.end()) {
        return EStr(EOPModCommonNotFound, string("Module not found: ") + module_name);
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(iter->second);
    if (!mod_ptr) {
        return EStr(EOPModCommonImport, string("Module is an external module and cannot be modified: ") + module_name);
    }

    // find matching connections
    for (auto &pair : mod_ptr->mod_pipe_connections) {
        const InstanceName &inst_name = pair.first;
        if (!src_instance.empty() && inst_name != src_instance) {
            continue;
        }
        for (auto it = pair.second.begin(); it != pair.second.end(); ) {
            const VulModulePipeConnection &conn = *it;
            if ((!src_port.empty() && conn.instance_pipe_port != src_port) ||
                (!dst_instance.empty() && conn.pipe_instance != dst_instance) ||
                (!dst_port.empty() && conn.top_pipe_port != dst_port)) {
                ++it;
                continue;
            }
            // match, remove it
            removed_conns.push_back(conn);
            it = pair.second.erase(it);
        }
    }

    project.modified_modules.insert(module_name);
    return VulOperationResponse();
}

string ModulePdisconnOperation::undo(VulProject &project) {
    auto &modulelib = project.modulelib;

    auto iter = modulelib->modules.find(module_name);
    if (iter == modulelib->modules.end()) {
        return "Module not found during undo: " + module_name;
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(iter->second);
    if (!mod_ptr) {
        return "Module is an external module and cannot be modified during undo: " + module_name;
    }

    for (const auto &conn : removed_conns) {
        mod_ptr->mod_pipe_connections[conn.instance].insert(conn);
    }

    project.modified_modules.insert(module_name);
    return "";
}


auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModulePdisconnOperation>(op);
};
struct Register {
    Register() {
        VulProject::registerOperation("module.pdisconn", factory);
    }
} register_instance;

} // namespace operation_module_pdisconn
