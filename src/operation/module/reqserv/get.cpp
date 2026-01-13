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

namespace operation_module_req_get {

VulOperationResponse getReqServCore(VulProject &project, const VulOperationPackage &op, bool is_serv);

class ModuleReqGetOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override {
        return getReqServCore(project, op, false);
    }

    virtual vector<string> help() const override {
        return {
            "Module Request Port Get Operation:",
            "Get the definition JSON of a Request Port in a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module containing the request port.",
            "  - [1] port: The name of the request port.",
            "Results:",
            "  - definition: The definition JSON of the request port.",
        };
    }
};
class ModuleServGetOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override {
        return getReqServCore(project, op, true);
    }

    virtual vector<string> help() const override {
        return {
            "Module Service Port Get Operation:",
            "Get the definition JSON of a Service Port in a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module containing the service port.",
            "  - [1] port: The name of the service port.",
            "Results:",
            "  - definition: The definition JSON of the service port.",
        };
    }
};

auto factory_ModuleReqGetOperation = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleReqGetOperation>(op);
};
struct ModuleReqGetOperationRegister {
    ModuleReqGetOperationRegister() {
        VulProject::registerOperation("module.req.get", factory_ModuleReqGetOperation);
    }
} moduleReqGetOperationRegisterInstance;

auto factory_ModuleServGetOperation = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleServGetOperation>(op);
};
struct ModuleServGetOperationRegister {
    ModuleServGetOperationRegister() {
        VulProject::registerOperation("module.serv.get", factory_ModuleServGetOperation);
    }
} moduleServGetOperationRegisterInstance;


VulOperationResponse getReqServCore(VulProject &project, const VulOperationPackage &op, bool is_serv) {
    auto &modulelib = project.modulelib;

    ModuleName module_name;
    ReqServName req_name;

    {
        auto name_arg = op.getArg("name", 0);
        if (!name_arg) {
            return EStr(EOPModCommonMissArg, "Missing argument: [0] name");
        }
        module_name = *name_arg;
        auto req_arg = op.getArg("port", 1);
        if (!req_arg) {
            return EStr(EOPModCommonMissArg, "Missing argument: [1] port");
        }
        req_name = *req_arg;
    }

    // check module existence
    auto mod_base_iter = modulelib->modules.find(module_name);
    if (mod_base_iter == modulelib->modules.end()) {
        return EStr(EOPModCommonNotFound, "Module not found: " + module_name);
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
    if (!mod_ptr) {
        return EStr(EOPModCommonNotFound, "Cannot get " + string(is_serv ? "service" : "request") + " port from external module: " + module_name);
    }

    // find reqserv
    VulReqServ reqserv;
    if (is_serv) {
        auto serv_iter = mod_ptr->services.find(req_name);
        if (serv_iter == mod_ptr->services.end()) {
            return EStr(EOPModReqGetServNotFound, "Service port not found in module '" + module_name + "': " + req_name);
        }
        reqserv = serv_iter->second;
    } else {
        auto req_iter = mod_ptr->requests.find(req_name);
        if (req_iter == mod_ptr->requests.end()) {
            return EStr(EOPModReqGetReqNotFound, "Request port not found in module '" + module_name + "': " + req_name);
        }
        reqserv = req_iter->second;
    }

    // serialize reqserv to JSON
    string definition_json = serialize::serializeReqServToJSON(reqserv);

    VulOperationResponse resp;
    resp.results["definition"] = definition_json;
    return resp;
};


} // namespace operation_module_req_get
