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

namespace operation_module_req_update {

class ModuleReqServUpdateCore {
public:
    VulOperationResponse execute_core(VulProject &project, const VulOperationPackage &op, bool is_serv);
    string undo_core(VulProject &project, bool is_serv);

    ModuleName module_name;
    ReqServName reqserv_name;
    VulReqServ old_reqserv;
    VulReqServ new_reqserv;
};

class ModuleReqUpdateOperation : public VulProjectOperation, public ModuleReqServUpdateCore {
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
            "Module Request Port Update Operation:",
            "Update an existing Request Port in a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module containing the request port.",
            "  - [1] port: The request port to update.",
            "  - [2] definition: The new definition JSON of the request port.",
            "  - [3] force (optional): Whether to force the update even if the new definition has a different signature and is already connected. Default is false.",
        };
    }
};

class ModuleServUpdateOperation : public VulProjectOperation, public ModuleReqServUpdateCore {
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
            "Module Service Port Update Operation:",
            "Update an existing Service Port in a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module containing the service port.",
            "  - [1] port: The service port to update.",
            "  - [2] definition: The new definition JSON of the service port.",
            "  - [3] force (optional): Whether to force the update even if the new definition has a different signature and is already connected. Default is false.",
        };
    }
};

auto factory_ModuleReqUpdateOperation = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleReqUpdateOperation>(op);
};
struct ModuleReqUpdateOperationRegister {
    ModuleReqUpdateOperationRegister() {
        VulProject::registerOperation("module.req.update", factory_ModuleReqUpdateOperation);
    }
} moduleReqUpdateOperationRegisterInstance;

auto factory_ModuleServUpdateOperation = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleServUpdateOperation>(op);
};
struct ModuleServUpdateOperationRegister {
    ModuleServUpdateOperationRegister() {
        VulProject::registerOperation("module.serv.update", factory_ModuleServUpdateOperation);
    }
} moduleServUpdateOperationRegisterInstance;

VulOperationResponse ModuleReqServUpdateCore::execute_core(VulProject &project, const VulOperationPackage &op, bool is_serv) {
    string definition_json;
    auto &modulelib = project.modulelib;

    {
        auto arg_ptr = op.getArg("name", 0);
        if (!arg_ptr) {
            return EStr(EOPModCommonMissArg, "Missing argument 'name' for module.req.update operation.");
        }
        module_name = *arg_ptr;
    }
    {
        auto arg_ptr = op.getArg("port", 1);
        if (!arg_ptr) {
            return EStr(EOPModCommonMissArg, "Missing argument 'port' for module.req.update operation.");
        }
        reqserv_name = *arg_ptr;
    }
    {
        auto arg_ptr = op.getArg("definition", 2);
        if (!arg_ptr) {
            return EStr(EOPModCommonMissArg, "Missing argument 'definition' for module.req.update operation.");
        }
        definition_json = *arg_ptr;
    }
    bool force_update = op.getBoolArg("force", 3);

    auto mod_base_iter = modulelib->modules.find(module_name);
    if (mod_base_iter == modulelib->modules.end()) {
        return EStr(EOPModCommonNotFound, "Module not found: " + module_name);
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
    if (!mod_ptr) {
        return EStr(EOPModCommonImport, string("Cannot update ") + (is_serv ? "service" : "request") + " port in external module: " + module_name);
    }

    // check req existence
    auto &reqservs = (is_serv ? mod_ptr->services : mod_ptr->requests);
    auto req_iter = reqservs.find(reqserv_name);
    if (req_iter == reqservs.end()) {
        return EStr(EOPModReqUpdateNotFound, string(is_serv ? "Service" : "Request") + " port not found in module '" + module_name + "': " + reqserv_name);
    }
    if (!force_update) {
        for (const auto & conns : mod_ptr->req_connections) {
            for (const auto & conn : conns.second) {
                if ((conn.req_name == reqserv_name && conn.req_instance == VulModule::TopInterface) || 
                    (conn.serv_name == reqserv_name && conn.serv_instance == VulModule::TopInterface)) {
                    return EStr(EOPModReqUpdateConnected, string(is_serv ? "Service" : "Request") + " port '" + reqserv_name + "' in module '" + module_name + "' is already connected. Use 'force' to override.");
                }
            }
        }
    }

    // save old reqserv
    old_reqserv = req_iter->second;

    // parse new reqserv
    serialize::parseReqServFromJSON(definition_json, new_reqserv);
    new_reqserv.name = reqserv_name;

    // update reqserv
    req_iter->second = new_reqserv;

    project.modified_modules.insert(module_name);
    return VulOperationResponse();
}

string ModuleReqServUpdateCore::undo_core(VulProject &project, bool is_serv) {
    auto &modulelib = project.modulelib;

    auto mod_base_iter = modulelib->modules.find(module_name);
    if (mod_base_iter == modulelib->modules.end()) {
        return "Module not found during undo: " + module_name;
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
    if (!mod_ptr) {
        return string("Cannot undo update of ") + (is_serv ? "service" : "request") + " port in external module: " + module_name;
    }

    // check req existence
    auto &reqservs = (is_serv ? mod_ptr->services : mod_ptr->requests);
    auto req_iter = reqservs.find(reqserv_name);
    if (req_iter == reqservs.end()) {
        return string(is_serv ? "Service" : "Request") + " port not found in module '" + module_name + "' during undo: " + reqserv_name;
    }

    // restore old reqserv
    req_iter->second = old_reqserv;

    project.modified_modules.insert(module_name);
    return "";
}



} // namespace operation_module_req_update
