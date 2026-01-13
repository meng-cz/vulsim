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

namespace operation_module_pipeport_add {

class ModulePipePortAddCore {
public:
    VulOperationResponse execute_core(VulProject &project, const VulOperationPackage &op, bool is_input);
    string undo_core(VulProject &project, bool is_input);

    ModuleName module_name;
    PipePortName port_name;
    VulPipePort added_port;
};

class ModulePipeInputAddOperation : public VulProjectOperation, public ModulePipePortAddCore {
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
            "Module Pipe Input Port Add Operation:",
            "Add a new Pipe Input Port to a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module to add the pipe input port to.",
            "  - [1] port: The pipe input port to add.",
            "  - [2] type: The data type of the pipe input port.",
            "  - [3] comment (optional): The comment of the pipe input port.",
        };
    }
};

class ModulePipeOutputAddOperation : public VulProjectOperation, public ModulePipePortAddCore {
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
            "Module Pipe Output Port Add Operation:",
            "Add a new Pipe Output Port to a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module to add the pipe output port to.",
            "  - [1] port: The pipe output port to add.",
            "  - [2] type: The data type of the pipe output port.",
            "  - [3] comment (optional): The comment of the pipe output port.",
        };
    }
};

auto factoryModulePipeInputAdd = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModulePipeInputAddOperation>(op);
};
struct RegisterModulePipeInputAddOperation {
    RegisterModulePipeInputAddOperation() {
        VulProject::registerOperation("module.pipein.add", factoryModulePipeInputAdd);
    }
} registerModulePipeInputAddOperationInstance;

auto factoryModulePipeOutputAdd = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModulePipeOutputAddOperation>(op);
};
struct RegisterModulePipeOutputAddOperation {
    RegisterModulePipeOutputAddOperation() {
        VulProject::registerOperation("module.pipeout.add", factoryModulePipeOutputAdd);
    }
} registerModulePipeOutputAddOperationInstance;


VulOperationResponse ModulePipePortAddCore::execute_core(VulProject &project, const VulOperationPackage &op, bool is_input) {
    string type_str;
    string comment_str = "";
    auto &modulelib = project.modulelib;

    {
        auto arg0_ptr = op.getArg("", 0);
        if (!arg0_ptr) {
            return EStr(EOPModCommonMissArg, "Missing module name argument.");
        }
        module_name = *arg0_ptr;

        auto arg1_ptr = op.getArg("", 1);
        if (!arg1_ptr) {
            return EStr(EOPModCommonMissArg, "Missing pipe port name argument.");
        }
        port_name = *arg1_ptr;

        auto arg2_ptr = op.getArg("", 2);
        if (!arg2_ptr) {
            return EStr(EOPModCommonMissArg, "Missing pipe port type argument.");
        }
        type_str = *arg2_ptr;

        auto arg3_ptr = op.getArg("", 3);
        if (arg3_ptr) {
            comment_str = *arg3_ptr;
        }
    }

    auto mod_base_iter = modulelib->modules.find(module_name);
    if (mod_base_iter == modulelib->modules.end()) {
        return EStr(EOPModCommonNotFound, "Module not found: " + module_name);
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
    if (!mod_ptr) {
        return EStr(EOPModCommonImport, string("Cannot add pipe port to external module: " + module_name));
    }

    if (!isValidIdentifier(port_name)) {
        return EStr(EOPModPipePortAddNameInvalid, "Invalid pipe port name in module '" + module_name + "': " + port_name);
    }
    if (project.globalNameConflictCheck(port_name)) {
        return EStr(EOPModPipePortAddNameDup, "Pipe port name conflict in module '" + module_name + "': " + port_name);
    }
    if (mod_ptr->localCheckNameConflict(port_name)) {
        return EStr(EOPModPipePortAddNameDup, "Pipe port name conflict in module '" + module_name + "': " + port_name);
    }

    added_port.name = port_name;
    added_port.type = type_str;
    added_port.comment = comment_str;

    if (is_input) {
        mod_ptr->pipe_inputs[added_port.name] = added_port;
    } else {
        mod_ptr->pipe_outputs[added_port.name] = added_port;
    }
    project.modified_modules.insert(module_name);
    return VulOperationResponse();
}

string ModulePipePortAddCore::undo_core(VulProject &project, bool is_input) {
    auto &modulelib = project.modulelib;

    auto mod_base_iter = modulelib->modules.find(module_name);
    if (mod_base_iter == modulelib->modules.end()) {
        return "Module not found during undo: " + module_name;
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
    if (!mod_ptr) {
        return "Cannot undo pipe port removal for external module: " + module_name;
    }

    if (is_input) {
        auto pipe_in_iter = mod_ptr->pipe_inputs.find(port_name);
        if (pipe_in_iter == mod_ptr->pipe_inputs.end()) {
            return "Pipe input port not found during undo in module '" + module_name + "': " + port_name;
        }
        mod_ptr->pipe_inputs.erase(pipe_in_iter);
    } else {
        auto pipe_out_iter = mod_ptr->pipe_outputs.find(port_name);
        if (pipe_out_iter == mod_ptr->pipe_outputs.end()) {
            return "Pipe output port not found during undo in module '" + module_name + "': " + port_name;
        }
        mod_ptr->pipe_outputs.erase(pipe_out_iter);
    }
    project.modified_modules.insert(module_name);
    return "";
}





} // namespace operation_module_pipeport_add
