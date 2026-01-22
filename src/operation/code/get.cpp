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
#include "simgen.h"
#include "serializejson.h"

namespace operation_code_get {

class CodeGetOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;

    virtual vector<string> help() const override {
        return {
            "Code Get Operation:",
            "Generate helper C++ code for a specified module in the project.",
            "",
            "Arguments:",
            "  - [0] module_name : The name of the module to generate code for.",
            "  - [1] codeblock (optional): User tick code block name to generate code for, if provided.",
            "  - [2] service (optional): Service port name to generate code for, if provided.",
            "  - [3] instance (optional): Child instance name to generate code for, if provided.",
            "  - [4] request (optional): Child instance request name to generate code for, if provided.",
            "  - [5] full_helper (optional): If set to 'true', generate full helper code including headers. Otherwise, only provide symbol table.",
            "Results:",
            "  - signature: Function signature string for the generated code.",
            "  - code_lines: List of lines of generated C++ code.",
            "  - symbols: JSON string of the valid symbols in the module.",
            "  - helper_code_lines: List of lines of full helper C++ code, if requested.",
        };
    }
};

VulOperationResponse CodeGetOperation::execute(VulProject &project) {
    auto module_name_arg = op.getArg("module_name", 0);
    if (!module_name_arg) {
        return EStr(EOPCodeGetInvalidArg, "Argument 'module_name' is required.");
    }
    ModuleName module_name = *module_name_arg;

    auto mod_iter = project.modulelib->modules.find(module_name);
    if (mod_iter == project.modulelib->modules.end()) {
        return EStr(EOPModCommonNotFound, "Module '" + module_name + "' not found in project.");
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_iter->second);
    if (!mod_ptr) {
        return EStr(EOPModCommonImport, "Module '" + module_name + "' is imported and cannot be modified.");
    }

    string codeblock_name = "";
    {
        auto arg = op.getArg("codeblock", 1);
        if (arg) {
            codeblock_name = *arg;
        }
    }
    string service_name = "";
    {
        auto arg = op.getArg("service", 2);
        if (arg) {
            service_name = *arg;
        }
    }
    string instance_name = "";
    {
        auto arg = op.getArg("instance", 3);
        if (arg) {
            instance_name = *arg;
        }
    }
    string request_name = "";
    {
        auto arg = op.getArg("request", 4);
        if (arg) {
            request_name = *arg;
        }
    }

    if (!codeblock_name.empty() && (!service_name.empty() || !instance_name.empty() || !request_name.empty())) {
        return EStr(EOPCodeGetInvalidArg, "Argument 'codeblock' cannot be used with 'service', 'instance', or 'request'.");
    }
    if (!service_name.empty() && (!instance_name.empty() || !request_name.empty())) {
        return EStr(EOPCodeGetInvalidArg, "Argument 'service' cannot be used with 'instance' or 'request'.");
    }
    if (!request_name.empty() && instance_name.empty()) {
        return EStr(EOPCodeGetInvalidArg, "Argument 'request' requires 'instance' to be specified.");
    }
    if (!instance_name.empty() && request_name.empty()) {
        return EStr(EOPCodeGetInvalidArg, "Argument 'instance' requires 'request' to be specified.");
    }

    bool full_helper = op.getBoolArg("full_helper", 5);

    VulOperationResponse response;

    if (!codeblock_name.empty()) {
        auto iter = mod_ptr->user_tick_codeblocks.find(codeblock_name);
        if (iter == mod_ptr->user_tick_codeblocks.end()) {
            return EStr(EOPCodeGetNotFound, "Code block '" + codeblock_name + "' not found in module '" + module_name + "'.");
        }
        const VulTickCodeBlock &codeblock = iter->second;
        response.results["signature"] = "void tick_" + codeblock_name + "()";
        response.list_results["code_lines"] = codeblock.codelines;
    } else if (!service_name.empty()) {
        auto serv_iter = mod_ptr->services.find(service_name);
        if (serv_iter == mod_ptr->services.end()) {
            response.results["signature"] = "void UNKNOWN_SERVICE()";
        } else {
            response.results["signature"] = serv_iter->second.signatureFull();
        }

        auto code_iter = mod_ptr->serv_codelines.find(service_name);
        if (code_iter == mod_ptr->serv_codelines.end()) {
            response.list_results["code_lines"] = vector<string>();
        } else {
            response.list_results["code_lines"] = code_iter->second;
        }
    } else {
        response.results["signature"] = "void UNKNOWN_CHILD_REQUEST()";
        do {
            auto inst_iter = mod_ptr->instances.find(instance_name);
            if (inst_iter == mod_ptr->instances.end()) {
                break;
            }
            const VulInstance &instance = inst_iter->second;
            auto child_mod_iter = project.modulelib->modules.find(instance.module_name);
            if (child_mod_iter == project.modulelib->modules.end()) {
                break;
            }
            auto req_iter = child_mod_iter->second->requests.find(request_name);
            if (req_iter == child_mod_iter->second->requests.end()) {
                break;
            }
            response.results["signature"] = req_iter->second.signatureFull(instance_name + "_");
        } while(0);

        auto code_iter = mod_ptr->req_codelines.find(instance_name);
        if (code_iter == mod_ptr->req_codelines.end()) {
            response.list_results["code_lines"] = vector<string>();
        } else {
            auto req_code_iter = code_iter->second.find(request_name);
            if (req_code_iter == code_iter->second.end()) {
                response.list_results["code_lines"] = vector<string>();
            } else {
                response.list_results["code_lines"] = req_code_iter->second;
            }
        }
    }

    auto symbols = simgen::getValidSymbols(project, module_name);
    response.results["symbols"] = serialize::serializeSymbolsToJSON(symbols);

    if (full_helper) {
        response.list_results["helper_code_lines"] = simgen::getHelperCodeLines(project, module_name);
    }

    return response;
}

auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return make_unique<CodeGetOperation>(op);
};
struct Register {
    Register() {
        VulProject::registerOperation("code.get", factory);
    }
} register_instance;

} // namespace operation_code_get
