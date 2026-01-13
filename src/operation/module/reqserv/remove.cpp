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

namespace operation_module_req_remove {

class ModuleReqServRemoveCore { 
public:
    VulOperationResponse execute_core(VulProject &project, const VulOperationPackage &op, bool is_serv);
    string undo_core(VulProject &project, bool is_serv);

    ModuleName module_name;
    ReqServName reqserv_name;
    VulReqServ removed_reqserv;
};

class ModuleReqRemoveOperation : public VulProjectOperation, public ModuleReqServRemoveCore {
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
            "Module Request Port Remove Operation:",
            "Remove an existing Request Port from a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module containing the request port.",
            "  - [1] port: The request port to remove.",
            "  - [2] force (optional): Whether to force the removal even if the request port is connected. Default is false.",
        };
    }
};

class ModuleServRemoveOperation : public VulProjectOperation, public ModuleReqServRemoveCore {
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
            "Module Service Port Remove Operation:",
            "Remove an existing Service Port from a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module containing the service port.",
            "  - [1] port: The service port to remove.",
            "  - [2] force (optional): Whether to force the removal even if the service port is connected. Default is false.",
        };
    }
};

auto factory_ModuleReqRemoveOperation = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleReqRemoveOperation>(op);
};
struct ModuleReqRemoveOperationRegister {
    ModuleReqRemoveOperationRegister() {
        VulProject::registerOperation("module.req.remove", factory_ModuleReqRemoveOperation);
    }
} moduleReqRemoveOperationRegisterInstance;

auto factory_ModuleServRemoveOperation = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleServRemoveOperation>(op);
};
struct ModuleServRemoveOperationRegister {
    ModuleServRemoveOperationRegister() {
        VulProject::registerOperation("module.serv.remove", factory_ModuleServRemoveOperation);
    }
} moduleServRemoveOperationRegisterInstance;


VulOperationResponse ModuleReqServRemoveCore::execute_core(VulProject &project, const VulOperationPackage &op, bool is_serv) {
    auto &modulelib = project.modulelib;
    {
        auto arg_ptr = op.getArg("name", 0);
        if (!arg_ptr) {
            return EStr(EOPModCommonMissArg, "Missing argument 'name' for module." + string(is_serv ? "serv" : "req") + ".remove operation.");
        }
        module_name = *arg_ptr;
        auto port_ptr = op.getArg("port", 1);
        if (!port_ptr) {
            return EStr(EOPModCommonMissArg, "Missing argument 'port' for module." + string(is_serv ? "serv" : "req") + ".remove operation.");
        }
        reqserv_name = *port_ptr;
    }
    bool force_remove = op.getBoolArg("force", 2);

    auto mod_base_iter = modulelib->modules.find(module_name);
    if (mod_base_iter == modulelib->modules.end()) {
        return EStr(EOPModCommonNotFound, "Module not found: " + module_name);
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
    if (!mod_ptr) {
        return EStr(EOPModCommonImport, string("Cannot remove ") + (is_serv ? "service" : "request") + " port from external module: " + module_name);
    }

    // check req existence
    auto &reqservs = (is_serv ? mod_ptr->services : mod_ptr->requests);
    auto req_iter = reqservs.find(reqserv_name);
    if (req_iter == reqservs.end()) {
        return EStr(EOPModReqRemoveNotFound, string(is_serv ? "Service" : "Request") + " port not found in module '" + module_name + "': " + reqserv_name);
    }
    // check connections
    if (!force_remove) {
        for (const auto & conns : mod_ptr->req_connections) {
            for (const auto & conn : conns.second) {
                if ((conn.req_name == reqserv_name && conn.req_instance == VulModule::TopInterface) || 
                    (conn.serv_name == reqserv_name && conn.serv_instance == VulModule::TopInterface)) {
                    return EStr(EOPModReqRemoveConnected, string(is_serv ? "Service" : "Request") + " port '" + reqserv_name + "' in module '" + module_name + "' is still connected. Use 'force' option to remove it anyway.");
                }
            }
        }
    }

    // remove reqserv
    removed_reqserv = req_iter->second;
    reqservs.erase(req_iter);

    project.modified_modules.insert(module_name);
    return VulOperationResponse();
}

string ModuleReqServRemoveCore::undo_core(VulProject &project, bool is_serv) {
    auto &modulelib = project.modulelib;

    auto mod_base_iter = modulelib->modules.find(module_name);
    if (mod_base_iter == modulelib->modules.end()) {
        return "Module not found: " + module_name;
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
    if (!mod_ptr) {
        return "Cannot undo remove " + string(is_serv ? "service" : "request") + " port from external module: " + module_name;
    }

    // check req existence
    auto &reqservs = (is_serv ? mod_ptr->services : mod_ptr->requests);
    auto req_iter = reqservs.find(reqserv_name);
    if (req_iter != reqservs.end()) {
        return string(is_serv ? "Service" : "Request") + " port '" + reqserv_name + "' already exists in module '" + module_name + "' when undoing remove.";
    }

    // restore reqserv
    reqservs[reqserv_name] = removed_reqserv;

    project.modified_modules.insert(module_name);
    return "";
}



} // namespace operation_module_req_remove
