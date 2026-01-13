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

namespace operation_module_req_rename {

class ModuleReqServRenameCore {
public:
    
    VulOperationResponse execute_core(VulProject &project, const VulOperationPackage &op, bool is_serv);
    string undo_core(VulProject &project, bool is_serv);
    
    ModuleName module_name;
    ReqServName old_req_name;
    ReqServName new_req_name;
};

class ModuleReqRenameOperation : public VulProjectOperation, public ModuleReqServRenameCore {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override {
        return execute_core(project, op, false);
    }
    virtual string undo(VulProject &project) override {
        return undo_core(project, false);
    }

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const override { return true; };

    virtual vector<string> help() const override {
        return {
            "Module Request Port Rename Operation:",
            "Rename a Request Port in a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module containing the request port.",
            "  - [1] old_port: The current name of the request port.",
            "  - [2] new_port: The new name for the request port.",
            "  - [3] update_connections: (optional) Whether to update request-service connections in the module and its instances in other modules.",
        };
    }
};

class ModuleServRenameOperation : public VulProjectOperation, public ModuleReqServRenameCore {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override {
        return execute_core(project, op, true);
    }
    virtual string undo(VulProject &project) override {
        return undo_core(project, true);
    }

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const override { return true; };

    virtual vector<string> help() const override {
        return {
            "Module Service Port Rename Operation:",
            "Rename a Service Port in a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module containing the service port.",
            "  - [1] old_port: The current name of the service port.",
            "  - [2] new_port: The new name for the service port.",
            "  - [3] update_connections: (optional) Whether to update request-service connections in the module and its instances in other modules.",
        };
    }
};

VulOperationResponse ModuleReqServRenameCore::execute_core(VulProject &project, const VulOperationPackage &op, bool is_serv) {
    auto &modulelib = project.modulelib;

    {
        auto name_arg = op.getArg("name", 0);
        if (!name_arg) {
            return EStr(EOPModCommonMissArg, "Missing argument: [0] name");
        }
        module_name = *name_arg;
        auto old_port_arg = op.getArg("old_port", 1);
        if (!old_port_arg) {
            return EStr(EOPModCommonMissArg, "Missing argument: [1] old_port");
        }
        old_req_name = *old_port_arg;
        auto new_port_arg = op.getArg("new_port", 2);
        if (!new_port_arg) {
            return EStr(EOPModCommonMissArg, "Missing argument: [2] new_port");
        }
        new_req_name = *new_port_arg;
    }
    bool update_connections = op.getBoolArg("update_connections", 3);

    // check module existence
    auto mod_base_iter = modulelib->modules.find(module_name);
    if (mod_base_iter == modulelib->modules.end()) {
        return EStr(EOPModCommonNotFound, "Module not found: " + module_name);
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
    if (!mod_ptr) {
        return EStr(EOPModCommonImport, "Cannot rename " + string(is_serv ? "service" : "request") + " port in external module: " + module_name);
    }

    // check req existence
    auto &reqservs = (is_serv ? mod_ptr->services : mod_ptr->requests);
    auto req_iter = reqservs.find(old_req_name);
    if (req_iter == reqservs.end()) {
        return EStr(EOPModReqRenameReqNotFound, string(is_serv ? "Service" : "Request") + string(" port not found in module '") + module_name + "': " + old_req_name);
    }

    if (!isValidIdentifier(new_req_name)) {
        return EStr(EOPModReqRenameReqNameDup, "Invalid " + string(is_serv ? "service" : "request") + " port name in module '" + module_name + "': " + new_req_name);
    }
    if (project.globalNameConflictCheck(new_req_name) || mod_ptr->localCheckNameConflict(new_req_name)) {
        return EStr(EOPModReqRenameReqNameDup, string(is_serv ? "Service" : "Request") + string(" port name conflict in module '") + module_name + "': " + new_req_name);
    }

    vector<VulReqServConnection> affected_connections;
    for (auto &[inst_name, conn_set] : mod_ptr->req_connections) {
        for (const auto &conn : conn_set) {
            if ((conn.req_instance == VulModule::TopInterface && conn.req_name == old_req_name) ||
                (conn.serv_instance == VulModule::TopInterface && conn.serv_name == old_req_name)) {
                affected_connections.push_back(conn);
            }
        }
    }
    if (!update_connections && !affected_connections.empty()) {
        return EStr(EOPModReqRenameConnected, string(is_serv ? "Service" : "Request") + string(" port '") + old_req_name + "' in module '" + module_name + "' has existing connections. Set 'update_connections' to true to update them automatically.");
    }

    // rename reqserv
    req_iter->second.name = new_req_name;
    reqservs[new_req_name] = req_iter->second;
    reqservs.erase(old_req_name);
    // update connections
    for (const auto &conn : affected_connections) {
        mod_ptr->req_connections[conn.req_instance].erase(conn);
        VulReqServConnection new_conn = conn;
        if (conn.req_instance == VulModule::TopInterface && conn.req_name == old_req_name) {
            new_conn.req_name = new_req_name;
        }
        if (conn.serv_instance == VulModule::TopInterface && conn.serv_name == old_req_name) {
            new_conn.serv_name = new_req_name;
        }
        mod_ptr->req_connections[new_conn.req_instance].insert(new_conn);
    }

    project.modified_modules.insert(module_name);
    return VulOperationResponse();
}

string ModuleReqServRenameCore::undo_core(VulProject &project, bool is_serv) {
    auto &modulelib = project.modulelib;
    auto mod_base_iter = modulelib->modules.find(module_name);
    if (mod_base_iter == modulelib->modules.end()) [[unlikely]] {
        return "Module '" + module_name + "' not found in undo operation.";
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
    if (!mod_ptr) [[unlikely]] {
        return "Cannot undo rename request port in external module: " + module_name;
    }

    // check req existence
    auto &reqservs = (is_serv ? mod_ptr->services : mod_ptr->requests);
    auto req_iter = reqservs.find(new_req_name);
    if (req_iter == reqservs.end()) [[unlikely]] {
        return "Request port not found in module '" + module_name + "' in undo operation: " + new_req_name;
    }

    vector<VulReqServConnection> affected_connections;
    for (auto &[inst_name, conn_set] : mod_ptr->req_connections) {
        for (const auto &conn : conn_set) {
            if ((conn.req_instance == VulModule::TopInterface && conn.req_name == new_req_name) ||
                (conn.serv_instance == VulModule::TopInterface && conn.serv_name == new_req_name)) {
                affected_connections.push_back(conn);
            }
        }
    }

    // rename reqserv back
    req_iter->second.name = old_req_name;
    reqservs[old_req_name] = req_iter->second;
    reqservs.erase(new_req_name);
    // update connections back
    for (auto &entry : mod_ptr->req_connections) {
        auto &conn_set = entry.second;
        vector<VulReqServConnection> to_update;
        for (const auto &conn : conn_set) {
            if ((conn.req_instance == VulModule::TopInterface && conn.req_name == new_req_name) ||
                (conn.serv_instance == VulModule::TopInterface && conn.serv_name == new_req_name)) {
                to_update.push_back(conn);
            }
        }
        for (const auto &conn : to_update) {
            conn_set.erase(conn);
            VulReqServConnection new_conn = conn;
            if (conn.req_instance == VulModule::TopInterface && conn.req_name == new_req_name) {
                new_conn.req_name = old_req_name;
            }
            if (conn.serv_instance == VulModule::TopInterface && conn.serv_name == new_req_name) {
                new_conn.serv_name = old_req_name;
            }
            conn_set.insert(new_conn);
        }
    }

    project.modified_modules.insert(module_name);
    return "";
}


auto factoryReq = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleReqRenameOperation>(op);
};
struct ModuleReqRenameOperationRegister {
    ModuleReqRenameOperationRegister() {
        VulProject::registerOperation("module.req.rename", factoryReq);
    }
} moduleReqRenameOperationRegisterInstance;

auto factoryServ = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleServRenameOperation>(op);
};
struct ModuleServRenameOperationRegister {
    ModuleServRenameOperationRegister() {
        VulProject::registerOperation("module.serv.rename", factoryServ);
    }
} moduleServRenameOperationRegisterInstance;

} // namespace operation_module_req_rename
