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

namespace operation_module_pipeport_update {

class ModulePipePortUpdateCore {
public:
    VulOperationResponse execute_core(VulProject &project, const VulOperationPackage &op, bool is_input);
    string undo_core(VulProject &project, bool is_input);

    ModuleName module_name;
    PipePortName port_name;
    VulPipePort old_port;
    VulPipePort new_port;
};

class ModulePipeInputUpdateOperation : public VulProjectOperation, public ModulePipePortUpdateCore {
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
            "Module Pipe Input Port Update Operation:",
            "Update an existing Pipe Input Port in a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module containing the pipe input port to update.",
            "  - [1] port: The name of the pipe input port to update.",
            "  - [2] type: The new data type of the pipe input port.",
            "  - [3] comment (optional): The new comment of the pipe input port.",
            "  - [4] force (optional): Whether to force the update when the port is already connected (true/false, default: false).",
        };
    }
};

class ModulePipeOutputUpdateOperation : public VulProjectOperation, public ModulePipePortUpdateCore {
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
            "Module Pipe Output Port Update Operation:",
            "Update an existing Pipe Output Port in a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module containing the pipe output port to update.",
            "  - [1] port: The name of the pipe output port to update.",
            "  - [2] type: The new data type of the pipe output port.",
            "  - [3] comment (optional): The new comment of the pipe output port.",
            "  - [4] force (optional): Whether to force the update when the port is already connected (true/false, default: false).",
        };
    }
};

auto factoryModulePipeInputUpdate = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModulePipeInputUpdateOperation>(op);
};
struct RegisterModulePipeInputUpdateOperation {
    RegisterModulePipeInputUpdateOperation() {
        VulProject::registerOperation("module.pipein.update", factoryModulePipeInputUpdate);
    }
} registerModulePipeInputUpdateOperationInstance;

auto factoryModulePipeOutputUpdate = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModulePipeOutputUpdateOperation>(op);
};
struct RegisterModulePipeOutputUpdateOperation {
    RegisterModulePipeOutputUpdateOperation() {
        VulProject::registerOperation("module.pipeout.update", factoryModulePipeOutputUpdate);
    }
} registerModulePipeOutputUpdateOperationInstance;


VulOperationResponse ModulePipePortUpdateCore::execute_core(VulProject &project, const VulOperationPackage &op, bool is_input) {
    string type_str;
    string comment_str = "";
    bool force_update = false;
    auto &modulelib = project.modulelib;

    {
        auto arg0_ptr = op.getArg("name", 0);
        if (!arg0_ptr) {
            return EStr(EOPModCommonMissArg, "Missing module name argument.");
        }
        module_name = *arg0_ptr;

        auto arg1_ptr = op.getArg("port", 1);
        if (!arg1_ptr) {
            return EStr(EOPModCommonMissArg, "Missing pipe port name argument.");
        }
        port_name = *arg1_ptr;

        auto arg2_ptr = op.getArg("type", 2);
        if (!arg2_ptr) {
            return EStr(EOPModCommonMissArg, "Missing pipe port type argument.");
        }
        type_str = *arg2_ptr;

        auto arg3_ptr = op.getArg("comment", 3);
        if (arg3_ptr) {
            comment_str = *arg3_ptr;
        }

        force_update = op.getBoolArg("force", 4);
    }

    auto mod_base_iter = modulelib->modules.find(module_name);
    if (mod_base_iter == modulelib->modules.end()) {
        return EStr(EOPModCommonNotFound, "Module not found: " + module_name);
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
    if (!mod_ptr) {
        return EStr(EOPModCommonImport, string("Cannot update pipe port in external module: " + module_name));
    }

    auto &pipe_ports = (is_input ? mod_ptr->pipe_inputs : mod_ptr->pipe_outputs);
    auto port_iter = pipe_ports.find(port_name);
    if (port_iter == pipe_ports.end()) {
        return EStr(EOPModPipePortUpdateNotFound, "Pipe port not found in module '" + module_name + "': " + port_name);
    }

    // check existing connections
    if (!force_update) {
        for (const auto &pipe_conn_pair : mod_ptr->mod_pipe_connections) {
            for (const auto &pipe_conn : pipe_conn_pair.second) {
                if (pipe_conn.top_pipe_port == port_name && pipe_conn.pipe_instance == VulModule::TopInterface) {
                    return EStr(EOPModPipePortUpdateConnected, "Pipe port '" + port_name + "' in module '" + module_name + "' is already connected. Use force option to override.");
                }
            }
        }
    }

    old_port = port_iter->second;
    new_port.name = port_name;
    new_port.type = type_str;
    new_port.comment = comment_str;
    port_iter->second = new_port;

    project.modified_modules.insert(module_name);
    return VulOperationResponse();
}

string ModulePipePortUpdateCore::undo_core(VulProject &project, bool is_input) {
    auto &modulelib = project.modulelib;

    auto mod_base_iter = modulelib->modules.find(module_name);
    if (mod_base_iter == modulelib->modules.end()) {
        return "Module not found during undo: " + module_name;
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
    if (!mod_ptr) {
        return "Cannot undo pipe port update in external module: " + module_name;
    }

    auto &pipe_ports = (is_input ? mod_ptr->pipe_inputs : mod_ptr->pipe_outputs);
    auto port_iter = pipe_ports.find(port_name);
    if (port_iter == pipe_ports.end()) {
        return "Pipe port not found during undo in module '" + module_name + "': " + port_name;
    }

    port_iter->second = old_port;

    project.modified_modules.insert(module_name);
    return "";
}


} // namespace operation_module_pipeport_update
