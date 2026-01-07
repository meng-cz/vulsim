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

namespace operation_module_conn {

class ModuleConnOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const { return true; };

    virtual vector<string> help() const override {
        return {
            "Module Connect Operation:",
            "Connect a request-service pair between two module instances (or __top__ instance).",
            "Arguments:",
            "  - [0] module: The name of the module to add the connection to.",
            "  - [1] src_instance: The instance name of the requestor module (or __top__ instance).",
            "  - [2] src_port: The name of the request to connect.",
            "  - [3] dst_instance: The instance name of the service provider module (or __top__ instance).",
            "  - [4] dst_port: The name of the service to connect.",
        };
    }
    VulReqServConnection added_connection;
};

VulOperationResponse ModuleConnOperation::execute(VulProject &project) {
    auto &modulelib = project.modulelib;
    ModuleName module_name;
    {
        auto arg0 = op.getArg("module", 0);
        if (!arg0) {
            return EStr(EOPModConnMissArg, "Missing argument: [0] module");
        }
        module_name = *arg0;
    }
    {
        auto arg0 = op.getArg("src_instance", 1);
        if (!arg0) {
            return EStr(EOPModConnMissArg, "Missing argument: [1] src_instance");
        }
        added_connection.req_instance = *arg0;
        auto arg1 = op.getArg("src_port", 2);
        if (!arg1) {
            return EStr(EOPModConnMissArg, "Missing argument: [2] src_port");
        }
        added_connection.req_name = *arg1;
        auto arg2 = op.getArg("dst_instance", 3);
        if (!arg2) {
            return EStr(EOPModConnMissArg, "Missing argument: [3] dst_instance");
        }
        added_connection.serv_instance = *arg2;
        auto arg3 = op.getArg("dst_port", 4);
        if (!arg3) {
            return EStr(EOPModConnMissArg, "Missing argument: [4] dst_port");
        }
        added_connection.serv_name = *arg3;
    }

    auto mod_iter = modulelib->modules.find(module_name);
    if (mod_iter == modulelib->modules.end()) {
        return EStr(EOPModCommonNotFound, "Module not found: '" + module_name + "'");
    }
    auto &module_base_ptr = mod_iter->second;
    shared_ptr<VulModule> module_ptr = std::dynamic_pointer_cast<VulModule>(module_base_ptr);
    if (!module_ptr) {
        return EStr(EOPModCommonImport, "Module is imported and cannot be modified: '" + module_name + "'");
    }

    bool is_src_top = (added_connection.req_instance == module_ptr->TopInterface);
    bool is_dst_top = (added_connection.serv_instance == module_ptr->TopInterface);
    VulReqServ src, dst;

    if (is_src_top) {
        auto req_iter = module_ptr->services.find(added_connection.req_name);
        if (req_iter == module_ptr->services.end()) {
            return EStr(EOPModConnSrcPortNotFound, "Service not found in __top__ instance: '" + added_connection.req_name + "'");
        }
        src = req_iter->second;
    } else {
        auto inst_iter = module_ptr->instances.find(added_connection.req_instance);
        if (inst_iter == module_ptr->instances.end()) {
            return EStr(EOPModConnSrcInstNotFound, "Instance not found: '" + added_connection.req_instance + "'");
        }
        auto &inst = inst_iter->second;
        auto mod_inst_iter = modulelib->modules.find(inst.module_name);
        if (mod_inst_iter == modulelib->modules.end()) {
            return EStr(EOPModConnSrcInstNotFound, "Module of instance not found: '" + inst.module_name + "'");
        }
        auto mod_inst_ptr = mod_inst_iter->second;
        auto req_iter = mod_inst_ptr->requests.find(added_connection.req_name);
        if (req_iter == mod_inst_ptr->requests.end()) {
            return EStr(EOPModConnSrcPortNotFound, "Request not found in instance '" + added_connection.req_instance + "': '" + added_connection.req_name + "'");
        }
        src = req_iter->second;
    }
    if (is_dst_top) {
        auto serv_iter = module_ptr->requests.find(added_connection.serv_name);
        if (serv_iter == module_ptr->requests.end()) {
            return EStr(EOPModConnDstPortNotFound, "Request not found in __top__ instance: '" + added_connection.serv_name + "'");
        }
        dst = serv_iter->second;
    } else {
        auto inst_iter = module_ptr->instances.find(added_connection.serv_instance);
        if (inst_iter == module_ptr->instances.end()) {
            return EStr(EOPModConnDstInstNotFound, "Instance not found: '" + added_connection.serv_instance + "'");
        }
        auto &inst = inst_iter->second;
        auto mod_inst_iter = modulelib->modules.find(inst.module_name);
        if (mod_inst_iter == modulelib->modules.end()) {
            return EStr(EOPModConnDstInstNotFound, "Module of instance not found: '" + inst.module_name + "'");
        }
        auto mod_inst_ptr = mod_inst_iter->second;
        auto serv_iter = mod_inst_ptr->services.find(added_connection.serv_name);
        if (serv_iter == mod_inst_ptr->services.end()) {
            return EStr(EOPModConnDstPortNotFound, "Service not found in instance '" + added_connection.serv_instance + "': '" + added_connection.serv_name + "'");
        }
        dst = serv_iter->second;
    }

    if (!src.match(dst)) {
        return EStr(EOPModConnMismatch, "Request-Service signature mismatch between '" + added_connection.req_instance + "'.'" + added_connection.req_name + "' and '" + added_connection.serv_instance + "'.'" + added_connection.serv_name + "'");
    }

    bool allow_multi = src.allowMultiConnect();
    {
        auto conns_iter = module_ptr->req_connections.find(added_connection.req_instance);
        if (conns_iter != module_ptr->req_connections.end()) {
            auto &conn_set = conns_iter->second;
            if (conn_set.find(added_connection) != conn_set.end()) {
                return EStr(EOPModConnAlreadyExists, "Connection already exists: '" + added_connection.toString() + "'");
            }
            for (const auto &conn : conn_set) {
                if (conn.req_name == added_connection.req_name && !allow_multi) {
                    return EStr(EOPModConnMultiConn, "Source already connected: '" + added_connection.toString() + "'");
                }
            }
        }
        if (is_src_top) {
            // also check user code implements
            auto user_req_iter = module_ptr->serv_codelines.find(added_connection.req_name);
            if (user_req_iter != module_ptr->serv_codelines.end() && !isCodeLineEmpty(user_req_iter->second) && !allow_multi) {
                return EStr(EOPModConnMultiConn, "Source already connected (user code implements): '" + added_connection.toString() + "'");
            }
        }
    }

    // add connection
    module_ptr->req_connections[added_connection.req_instance].insert(added_connection);
    project.modified_modules.insert(module_name);
    return VulOperationResponse();
}

string ModuleConnOperation::undo(VulProject &project) {
    auto &modulelib = project.modulelib;
    auto mod_iter = modulelib->modules.find(added_connection.req_instance);
    if (mod_iter == modulelib->modules.end()) {
        return "Module not found during undo: '" + added_connection.req_instance + "'";
    }
    auto &module_base_ptr = mod_iter->second;
    shared_ptr<VulModule> module_ptr = std::dynamic_pointer_cast<VulModule>(module_base_ptr);
    if (!module_ptr) {
        return "Module is imported and cannot be modified during undo: '" + added_connection.req_instance + "'";
    }

    auto conns_iter = module_ptr->req_connections.find(added_connection.req_instance);
    if (conns_iter == module_ptr->req_connections.end()) {
        return "No connections found during undo in instance: '" + added_connection.req_instance + "'";
    }
    auto &conn_set = conns_iter->second;
    auto conn_iter = conn_set.find(added_connection);
    if (conn_iter == conn_set.end()) {
        return "Connection not found during undo: '" + added_connection.toString() + "'";
    }
    conn_set.erase(conn_iter);
    project.modified_modules.insert(added_connection.req_instance);
    return "";
}


auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleConnOperation>(op);
};
struct ModuleConnOperationRegister {
    ModuleConnOperationRegister() {
        VulProject::registerOperation("module.connect", factory);
    }
} moduleConnOperationRegisterInstance;


} // namespace operation_module_conn
