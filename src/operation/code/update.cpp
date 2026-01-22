
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

namespace operation_code_update {

class CodeUpdateOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_undoable() const override { return true; };
    virtual bool is_modify() const override { return true; };

    virtual vector<string> help() const override {
        return {
            "Code Update Operation:",
            "Update the code lines for a specified code block in a module.",
            "",
            "Arguments:",
            "  - [0] module_name : The name of the module to update code for.",
            "  - [1] codeblock (optional): User tick code block name to generate code for, if provided.",
            "  - [2] service (optional): Service port name to generate code for, if provided.",
            "  - [3] instance (optional): Child instance name to generate code for, if provided.",
            "  - [4] request (optional): Child instance request name to generate code for, if provided.",
            "  - [5] code_lines : List of lines of C++ code to update, separated by newline characters.",
            "Results:",
            "  - (none)",
        };
    }

protected:
    ModuleName        module_name;
    string            codeblock_name;
    string            service_name;
    string            instance_name;
    string            request_name;
    vector<string>    old_code_lines;
};

VulOperationResponse CodeUpdateOperation::execute(VulProject &project) {
    auto module_name_arg = op.getArg("module_name", 0);
    if (!module_name_arg) {
        return EStr(EOPCodeGetInvalidArg, "Argument 'module_name' is required.");
    }
    module_name = *module_name_arg;

    auto mod_iter = project.modulelib->modules.find(module_name);
    if (mod_iter == project.modulelib->modules.end()) {
        return EStr(EOPModCommonNotFound, "Module '" + module_name + "' not found in project.");
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_iter->second);
    if (!mod_ptr) {
        return EStr(EOPModCommonImport, "Module '" + module_name + "' is imported and cannot be modified.");
    }

    {
        auto arg = op.getArg("codeblock", 1);
        if (arg) {
            codeblock_name = *arg;
        }
    }
    {
        auto arg = op.getArg("service", 2);
        if (arg) {
            service_name = *arg;
        }
    }
    {
        auto arg = op.getArg("instance", 3);
        if (arg) {
            instance_name = *arg;
        }
    }
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

    vector<string> new_code_lines;
    auto code_lines_arg = op.getArg("code_lines", 5);
    if (code_lines_arg && !code_lines_arg->empty()) {
        size_t pos = 0;
        size_t prev_pos = 0;
        while ((pos = code_lines_arg->find('\n', prev_pos)) != string::npos) {
            new_code_lines.push_back(code_lines_arg->substr(prev_pos, pos - prev_pos));
            prev_pos = pos + 1;
        }
        // last line
        new_code_lines.push_back(code_lines_arg->substr(prev_pos));
    }

    if (!codeblock_name.empty()) {
        auto iter = mod_ptr->user_tick_codeblocks.find(codeblock_name);
        if (iter == mod_ptr->user_tick_codeblocks.end()) {
            mod_ptr->user_tick_codeblocks[codeblock_name] = VulTickCodeBlock{codeblock_name, "", {}};
        } else {
            VulTickCodeBlock &codeblock = iter->second;
            old_code_lines = codeblock.codelines;
            codeblock.codelines = new_code_lines;
            if (new_code_lines.empty()) {
                // remove empty codeblock
                mod_ptr->user_tick_codeblocks.erase(iter);
            }
        }
    } else if (!service_name.empty()) {
        auto iter = mod_ptr->serv_codelines.find(service_name);
        if (iter == mod_ptr->serv_codelines.end()) {
            mod_ptr->serv_codelines[service_name] = new_code_lines;
        } else {
            old_code_lines = iter->second;
            iter->second = new_code_lines;
            if (new_code_lines.empty()) {
                // remove empty code lines
                mod_ptr->serv_codelines.erase(iter);
            }
        }
    } else {
        auto &instance_req_map = mod_ptr->req_codelines[instance_name];
        auto &code_lines_ref = instance_req_map[request_name];
        old_code_lines = code_lines_ref;
        code_lines_ref = new_code_lines;
        if (new_code_lines.empty()) {
            // remove empty code lines
            instance_req_map.erase(request_name);
            if (instance_req_map.empty()) {
                mod_ptr->req_codelines.erase(instance_name);
            }
        }
    }

    return VulOperationResponse();
}

string CodeUpdateOperation::undo(VulProject &project) {
    auto mod_iter = project.modulelib->modules.find(module_name);
    if (mod_iter == project.modulelib->modules.end()) {
        return "Module '" + module_name + "' not found in project.";
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_iter->second);
    if (!mod_ptr) {
        return "Module '" + module_name + "' is imported and cannot be modified.";
    }

    if (!codeblock_name.empty()) {
        if (old_code_lines.empty()) {
            // remove codeblock
            mod_ptr->user_tick_codeblocks.erase(codeblock_name);
        } else {
            mod_ptr->user_tick_codeblocks[codeblock_name] = VulTickCodeBlock{codeblock_name, "", old_code_lines};
        }
    } else if (!service_name.empty()) {
        if (old_code_lines.empty()) {
            // remove code lines
            mod_ptr->serv_codelines.erase(service_name);
        } else {
            mod_ptr->serv_codelines[service_name] = old_code_lines;
        }
    } else {
        auto &instance_req_map = mod_ptr->req_codelines[instance_name];
        if (old_code_lines.empty()) {
            // remove code lines
            instance_req_map.erase(request_name);
            if (instance_req_map.empty()) {
                mod_ptr->req_codelines.erase(instance_name);
            }
        } else {
            instance_req_map[request_name] = old_code_lines;
        }
    }

    return "";
}

auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return make_unique<CodeUpdateOperation>(op);
};
struct Register {
    Register() {
        VulProject::registerOperation("code.update", factory);
    }
} register_instance;

} // namespace operation_code_update
