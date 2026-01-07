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

namespace operation_module_disconn {

class ModuleDisconnOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const { return true; };

    virtual vector<string> help() const override {
        return {
            "Module Disconnect Operation:",
            "Disconnect request-service connection(s) that match the specified arguments.",
            "Arguments:",
            "  - [0] module: The name of the module to remove the connection from.",
            "  - [1] src_instance (optional): The instance name of the requestor module (or __top__ instance).",
            "  - [2] src_port (optional): The name of the request to disconnect.",
            "  - [3] dst_instance (optional): The instance name of the service provider module (or __top__ instance).",
            "  - [4] dst_port (optional): The name of the service to disconnect.",
        };
    }

    ModuleName module_name;
    vector<VulReqServConnection> removed_connections;
};

VulOperationResponse ModuleDisconnOperation::execute(VulProject &project) {
    auto &modulelib = project.modulelib;
    ModuleName module_name;
    {
        auto arg0 = op.getArg("module", 0);
        if (!arg0) {
            return EStr(EOPModCommonMissArg, "Missing argument: [0] module");
        }
        module_name = *arg0;
    }
    InstanceName src_instance = "", dst_instance = "";
    ReqServName src_port = "", dst_port = "";
    {
        auto arg1 = op.getArg("src_instance", 1);
        if (arg1) {
            src_instance = *arg1;
        }
        auto arg2 = op.getArg("src_port", 2);
        if (arg2) {
            src_port = *arg2;
        }
        auto arg3 = op.getArg("dst_instance", 3);
        if (arg3) {
            dst_instance = *arg3;
        }
        auto arg4 = op.getArg("dst_port", 4);
        if (arg4) {
            dst_port = *arg4;
        }
    }
    if (src_instance.empty()) {
        src_port = "";
    }
    if (dst_instance.empty()) {
        dst_port = "";
    }

    auto mod_iter = modulelib->modules.find(module_name);
    if (mod_iter == modulelib->modules.end()) {
        return EStr(EOPModCommonNotFound, "Module not found: '" + module_name + "'");
    }
    auto mod_base_ptr = mod_iter->second;
    shared_ptr<VulModule> module_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_ptr);
    if (!module_ptr) {
        return EStr(EOPModCommonImport, "Module is imported and cannot be modified: '" + module_name + "'");
    }

    for (const auto &conn_pair : module_ptr->req_connections) {
        const auto &instance_name = conn_pair.first;
        const auto &conn_set = conn_pair.second;
        for (auto iter = conn_set.begin(); iter != conn_set.end(); ) {
            const auto &conn = *iter;
            bool match = true;
            if (!src_instance.empty() && conn.req_instance != src_instance) {
                match = false;
            }
            if (!src_port.empty() && conn.req_name != src_port) {
                match = false;
            }
            if (!dst_instance.empty() && conn.serv_instance != dst_instance) {
                match = false;
            }
            if (!dst_port.empty() && conn.serv_name != dst_port) {
                match = false;
            }
            if (match) {
                removed_connections.push_back(conn);
                iter = module_ptr->req_connections[instance_name].erase(iter);
            } else {
                ++iter;
            }
        }
    }

    project.modified_modules.insert(module_name);

    return VulOperationResponse();
}

string ModuleDisconnOperation::undo(VulProject &project) {
    auto &modulelib = project.modulelib;

    auto mod_iter = modulelib->modules.find(module_name);
    if (mod_iter == modulelib->modules.end()) {
        return "Module not found during undo: '" + module_name + "'";
    }
    auto mod_base_ptr = mod_iter->second;
    shared_ptr<VulModule> module_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_ptr);
    if (!module_ptr) {
        return "Module is imported and cannot be modified during undo: '" + module_name + "'";
    }

    for (const auto &conn : removed_connections) {
        module_ptr->req_connections[conn.req_instance].insert(conn);
    }

    project.modified_modules.insert(module_name);

    return "";
}

auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleDisconnOperation>(op);
};
struct ModuleDisconnOperationRegister {
    ModuleDisconnOperationRegister() {
        VulProject::registerOperation("module.disconn", factory);
    }
} moduleDisconnOperationRegisterInstance;

} // namespace operation_module_disconn
