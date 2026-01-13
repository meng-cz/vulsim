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

namespace operation_module_pipeport_rename {

class PipePortRenameCore {
public:
    VulOperationResponse execute_core(VulProject &project, const VulOperationPackage &op, bool is_input);
    string undo_core(VulProject &project, bool is_input);

    ModuleName module_name;
    PipePortName old_port_name;
    PipePortName new_port_name;
};

class ModulePipeInputRenameOperation : public VulProjectOperation, public PipePortRenameCore {
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
            "Module Pipe Input Port Rename Operation:",
            "Rename an existing Pipe Input Port in a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module containing the pipe input port to rename.",
            "  - [1] old_port: The current name of the pipe input port.",
            "  - [2] new_port: The new name for the pipe input port.",
            "  - [3] update_connections (optional): Whether to update connections using this port (true/false, default: false).",
        };
    }
};

class ModulePipeOutputRenameOperation : public VulProjectOperation, public PipePortRenameCore {
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
            "Module Pipe Output Port Rename Operation:",
            "Rename an existing Pipe Output Port in a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module containing the pipe output port to rename.",
            "  - [1] old_port: The current name of the pipe output port.",
            "  - [2] new_port: The new name for the pipe output port.",
            "  - [3] update_connections (optional): Whether to update connections using this port (true/false, default: false).",
        };
    }
};

auto factoryModulePipeInputRename = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModulePipeInputRenameOperation>(op);
};
struct RegisterModulePipeInputRenameOperation {
    RegisterModulePipeInputRenameOperation() {
        VulProject::registerOperation("module.pipein.rename", factoryModulePipeInputRename);
    }
} registerModulePipeInputRenameOperationInstance;

auto factoryModulePipeOutputRename = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModulePipeOutputRenameOperation>(op);
};
struct RegisterModulePipeOutputRenameOperation {
    RegisterModulePipeOutputRenameOperation() {
        VulProject::registerOperation("module.pipeout.rename", factoryModulePipeOutputRename);
    }
} registerModulePipeOutputRenameOperationInstance;


VulOperationResponse PipePortRenameCore::execute_core(VulProject &project, const VulOperationPackage &op, bool is_input) {
    bool update_connections = false;
    auto &modulelib = project.modulelib;

    {
        auto arg0_ptr = op.getArg("name", 0);
        if (!arg0_ptr) {
            return EStr(EOPModCommonMissArg, "Missing module name argument.");
        }
        module_name = *arg0_ptr;
        auto arg1_ptr = op.getArg("old_port", 1);
        if (!arg1_ptr) {
            return EStr(EOPModCommonMissArg, "Missing old pipe port name argument.");
        }
        old_port_name = *arg1_ptr;
        auto arg2_ptr = op.getArg("new_port", 2);
        if (!arg2_ptr) {
            return EStr(EOPModCommonMissArg, "Missing new pipe port name argument.");
        }
        new_port_name = *arg2_ptr;
    }
    update_connections = op.getBoolArg("update_connections", 3);

    auto mod_base_iter = modulelib->modules.find(module_name);
    if (mod_base_iter == modulelib->modules.end()) {
        return EStr(EOPModCommonNotFound, "Module not found: " + module_name);
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
    if (!mod_ptr) {
        return EStr(EOPModCommonImport, string("Cannot add pipe port to external module: " + module_name));
    }

    auto &pipe_ports = (is_input ? mod_ptr->pipe_inputs : mod_ptr->pipe_outputs);
    auto port_iter = pipe_ports.find(old_port_name);
    if (port_iter == pipe_ports.end()) {
        return EStr(EOPModPipePortRenameNotFound, "Pipe port not found in module '" + module_name + "': " + old_port_name);
    }

    if (!isValidIdentifier(new_port_name)) {
        return EStr(EOPModPipePortRenameNameInvalid, "Invalid pipe port name in module '" + module_name + "': " + new_port_name);
    }
    if (project.globalNameConflictCheck(new_port_name)) {
        return EStr(EOPModPipePortRenameNameDup, "Pipe port name conflict in module '" + module_name + "': " + new_port_name);
    }
    if (mod_ptr->localCheckNameConflict(new_port_name)) {
        return EStr(EOPModPipePortRenameNameDup, "Pipe port name conflict in module '" + module_name + "': " + new_port_name);
    }

    if (!update_connections) {
        // check if there are existing connections using this port
        for (const auto &pipe_conn_pair : mod_ptr->mod_pipe_connections) {
            for (const auto &pipe_conn : pipe_conn_pair.second) {
                if (pipe_conn.top_pipe_port == old_port_name && pipe_conn.pipe_instance == VulModule::TopInterface) {
                    return EStr(EOPModPipePortRenameNameDup, "Pipe port '" + old_port_name + "' in module '" + module_name + "' is used in pipe connections. Set 'update_connections' to true to rename connections automatically.");
                }
            }
        }
    }

    // rename the port
    VulPipePort port_def = port_iter->second;
    pipe_ports.erase(port_iter);
    port_def.name = new_port_name;
    pipe_ports[new_port_name] = port_def;

    // update connections
    for (auto &pipe_conn_pair : mod_ptr->mod_pipe_connections) {
        for (auto it = pipe_conn_pair.second.begin(); it != pipe_conn_pair.second.end(); ) {
            if (it->top_pipe_port == old_port_name && it->pipe_instance == VulModule::TopInterface) {
                auto nh = pipe_conn_pair.second.extract(it++);
                nh.value().top_pipe_port = new_port_name;
                pipe_conn_pair.second.insert(std::move(nh));
            } else {
                ++it;
            }
        }
        // vector<VulModulePipeConnection> updated_conns;
        // for (auto it = pipe_conn_pair.second.begin(); it != pipe_conn_pair.second.end(); ) {
        //     if (it->top_pipe_port == old_port_name && it->pipe_instance == VulModule::TopInterface) {
        //         VulModulePipeConnection new_conn = *it;
        //         new_conn.top_pipe_port = new_port_name;
        //         updated_conns.push_back(new_conn);
        //         it = pipe_conn_pair.second.erase(it);
        //     } else {
        //         ++it;
        //     }
        // }
        // for (const auto &new_conn : updated_conns) {
        //     pipe_conn_pair.second.insert(new_conn);
        // }
    }
    project.modified_modules.insert(module_name);
    return VulOperationResponse();
}

string PipePortRenameCore::undo_core(VulProject &project, bool is_input) {
    auto &modulelib = project.modulelib;

    auto mod_base_iter = modulelib->modules.find(module_name);
    if (mod_base_iter == modulelib->modules.end()) {
        return "Module not found during undo: " + module_name;
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
    if (!mod_ptr) {
        return "Cannot undo pipe port rename for external module: " + module_name;
    }

    auto &pipe_ports = (is_input ? mod_ptr->pipe_inputs : mod_ptr->pipe_outputs);
    auto port_iter = pipe_ports.find(new_port_name);
    if (port_iter == pipe_ports.end()) {
        return "Pipe port not found during undo in module '" + module_name + "': " + new_port_name;
    }

    // rename the port back
    VulPipePort port_def = port_iter->second;
    pipe_ports.erase(port_iter);
    port_def.name = old_port_name;
    pipe_ports[old_port_name] = port_def;

    // update connections
    for (auto &pipe_conn_pair : mod_ptr->mod_pipe_connections) {
        for (auto it = pipe_conn_pair.second.begin(); it != pipe_conn_pair.second.end(); ) {
            if (it->top_pipe_port == new_port_name && it->pipe_instance == VulModule::TopInterface) {
                auto nh = pipe_conn_pair.second.extract(it++);
                nh.value().top_pipe_port = old_port_name;
                pipe_conn_pair.second.insert(std::move(nh));
            } else {
                ++it;
            }
        }
        // vector<VulModulePipeConnection> updated_conns;
        // for (auto it = pipe_conn_pair.second.begin(); it != pipe_conn_pair.second.end(); ) {
        //     if (it->top_pipe_port == new_port_name && it->pipe_instance == VulModule::TopInterface) {
        //         VulModulePipeConnection new_conn = *it;
        //         new_conn.top_pipe_port = old_port_name;
        //         updated_conns.push_back(new_conn);
        //         it = pipe_conn_pair.second.erase(it);
        //     } else {
        //         ++it;
        //     }
        // }
        // for (const auto &new_conn : updated_conns) {
        //     pipe_conn_pair.second.insert(new_conn);
        // }
    }
    project.modified_modules.insert(module_name);
    return "";
}


} // namespace operation_module_pipeport_rename
