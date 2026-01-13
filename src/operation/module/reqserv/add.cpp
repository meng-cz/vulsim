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

namespace operation_module_req_add {
class ModuleReqServAddCore {
public:
    VulOperationResponse execute_core(VulProject &project, const VulOperationPackage &op, bool is_serv);
    string undo_core(VulProject &project, bool is_serv);

    ModuleName module_name;
    VulReqServ added_req;
};

class ModuleReqAddOperation : public VulProjectOperation, public ModuleReqServAddCore {
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
            "Module Request Port Add Operation:",
            "Add a new Request Port to a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module to add the request port to.",
            "  - [1] port: The request port to add.",
            "  - [2] definition: The definition JSON of the request port.",
        };
    }
};

class ModuleServAddOperation : public VulProjectOperation, public ModuleReqServAddCore {
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
            "Module Service Port Add Operation:",
            "Add a new Service Port to a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module to add the service port to.",
            "  - [1] port: The service port to add.",
            "  - [2] definition: The definition JSON of the service port.",
        };
    }
};

VulOperationResponse ModuleReqServAddCore::execute_core(VulProject &project, const VulOperationPackage &op, bool is_serv) {
    string definition_json;
    string port_name;
    auto &modulelib = project.modulelib;

    {
        auto name_arg = op.getArg("name", 0);
        if (!name_arg) {
            return EStr(EOPModCommonMissArg, "Missing argument: [0] name");
        }
        module_name = *name_arg;
        auto port_arg = op.getArg("port", 1);
        if (!port_arg) {
            return EStr(EOPModCommonMissArg, "Missing argument: [1] port");
        }
        port_name = *port_arg;
        auto def_arg = op.getArg("definition", 2);
        if (!def_arg) {
            return EStr(EOPModCommonMissArg, "Missing argument: [2] definition");
        }
        definition_json = *def_arg;
    }

    // check module existence
    auto mod_base_iter = modulelib->modules.find(module_name);
    if (mod_base_iter == modulelib->modules.end()) {
        return EStr(EOPModCommonNotFound, "Module not found: " + module_name);
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
    if (!mod_ptr) {
        return EStr(EOPModCommonImport, string("Cannot add ") + (is_serv ? "service" : "request") + " port to external module: " + module_name);
    }

    if (!isValidIdentifier(port_name)) {
        return EStr(EOPModReqAddReqNameDup, string("Invalid ") + (is_serv ? "service" : "request") + " port name in module '" + module_name + "': " + port_name);
    }
    if (project.globalNameConflictCheck(port_name)) {
        return EStr(EOPModReqAddReqNameDup, string(is_serv ? "Service" : "Request") + " port name conflict in module '" + module_name + "': " + port_name);
    }
    if (mod_ptr->localCheckNameConflict(port_name)) {
        return EStr(EOPModReqAddReqNameDup, string(is_serv ? "Service" : "Request") + " port name conflict in module '" + module_name + "': " + port_name);
    }

    serialize::parseReqServFromJSON(definition_json, added_req);
    added_req.name = port_name;

    // add reqserv to correct container
    if (is_serv) {
        mod_ptr->services[added_req.name] = added_req;
    } else {
        mod_ptr->requests[added_req.name] = added_req;
    }
    project.modified_modules.insert(module_name);
    return VulOperationResponse();
}

string ModuleReqServAddCore::undo_core(VulProject &project, bool is_serv) {
    auto &modulelib = project.modulelib;
    auto mod_base_iter = modulelib->modules.find(module_name);
    if (mod_base_iter == modulelib->modules.end()) [[unlikely]] {
        return "Module '" + module_name + "' not found in undo operation.";
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
    if (!mod_ptr) [[unlikely]] {
        return string("Cannot undo add ") + (is_serv ? "service" : "request") + " port to external module: " + module_name;
    }
    if (is_serv) {
        mod_ptr->services.erase(added_req.name);
    } else {
        mod_ptr->requests.erase(added_req.name);
    }
    project.modified_modules.insert(module_name);
    return "";
}


auto factoryReq = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleReqAddOperation>(op);
};
struct ModuleReqAddOperationRegister {
    ModuleReqAddOperationRegister() {
        VulProject::registerOperation("module.req.add", factoryReq);
    }
} moduleReqAddOperationRegisterInstance;

auto factoryServ = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleServAddOperation>(op);
};
struct ModuleServAddOperationRegister {
    ModuleServAddOperationRegister() {
        VulProject::registerOperation("module.serv.add", factoryServ);
    }
} moduleServAddOperationRegisterInstance;

} // namespace operation_module_req_add
