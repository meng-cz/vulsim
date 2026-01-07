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

namespace operation_module_pconn {

class ModulePconnOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const { return true; };

    virtual vector<string> help() const override {
        return {
            "Instance-Pipe Connect Operation:",
            "Connect a pipe port between instances and pipe instances (or __top__ instance).",
            "Arguments:",
            "  - [0] module: The name of the module to add the pipe connection to.",
            "  - [1] instance: The source instance name.",
            "  - [2] port: The name of the pipe port to connect.",
            "  - [3] pipe: The pipe instance name (or __top__ instance).",
            "  - [4] pipe_port (optional): The name of the pipe port to connect to. Valid when pipe is __top__.",
        };
    }

    ModuleName module_name;
    VulModulePipeConnection added_conn;
};

VulOperationResponse ModulePconnOperation::execute(VulProject &project) {
    auto &modulelib = project.modulelib;
    {
        auto arg0 = op.getArg("module", 0);
        if (!arg0) {
            return EStr(EOPModPConnMissArg, "Missing argument: [0] module");
        }
        module_name = *arg0;
        auto arg1 = op.getArg("instance", 1);
        if (!arg1) {
            return EStr(EOPModPConnMissArg, "Missing argument: [1] instance");
        }
        added_conn.instance = *arg1;
        auto arg2 = op.getArg("port", 2);
        if (!arg2) {
            return EStr(EOPModPConnMissArg, "Missing argument: [2] port");
        }
        added_conn.instance_pipe_port = *arg2;
        auto arg3 = op.getArg("pipe", 3);
        if (!arg3) {
            return EStr(EOPModPConnMissArg, "Missing argument: [3] pipe");
        }
        added_conn.pipe_instance = *arg3;
        if (added_conn.pipe_instance == VulModule::TopInterface) {
            auto arg4 = op.getArg("pipe_port", 4);
            if (!arg4) {
                return EStr(EOPModPConnMissArg, "Missing argument: [4] pipe_port");
            }
            added_conn.top_pipe_port = *arg4;
        } else {
            added_conn.top_pipe_port = "";
        }
    }

    auto iter = modulelib->modules.find(module_name);
    if (iter == modulelib->modules.end()) {
        return EStr(EOPModCommonNotFound, string("Module not found: ") + module_name);
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(iter->second);
    if (!mod_ptr) {
        return EStr(EOPModCommonImport, string("Module is an external module and cannot be modified: ") + module_name);
    }

    DataType src_type = "", dst_type = "";
    bool src_is_output = false;
    bool dst_is_output = false;
    bool dst_top = (added_conn.pipe_instance == VulModule::TopInterface);
    {
        auto inst_iter = mod_ptr->instances.find(added_conn.instance);
        if (inst_iter == mod_ptr->instances.end()) {
            return EStr(EOPModPConnSrcInstNotFound, "Source instance not found: '" + added_conn.instance + "'");
        }
        auto &src_instance = inst_iter->second;
        auto src_mod_iter = modulelib->modules.find(src_instance.module_name);
        if (src_mod_iter == modulelib->modules.end()) {
            return EStr(EOPModCommonNotFound, "Module not found for source instance: '" + src_instance.module_name + "'");
        }
        auto &src_module_base_ptr = src_mod_iter->second;
        auto src_iport_iter = src_module_base_ptr->pipe_inputs.find(added_conn.instance_pipe_port);
        if (src_iport_iter != src_module_base_ptr->pipe_inputs.end()) {
            src_type = src_iport_iter->second.type;
            src_is_output = false;
        } else {
            auto src_oport_iter = src_module_base_ptr->pipe_outputs.find(added_conn.instance_pipe_port);
            if (src_oport_iter != src_module_base_ptr->pipe_outputs.end()) {
                src_type = src_oport_iter->second.type;
                src_is_output = true;
            } else {
                return EStr(EOPModPConnSrcPortNotFound, "Source pipe port not found: '" + added_conn.instance_pipe_port + "' in instance '" + added_conn.instance + "'");
            }
        }
    }
    {
        if (dst_top) {
            auto iport_iter = mod_ptr->pipe_inputs.find(added_conn.top_pipe_port);
            if (iport_iter != mod_ptr->pipe_inputs.end()) {
                dst_type = iport_iter->second.type;
                dst_is_output = false;
            } else {
                auto oport_iter = mod_ptr->pipe_outputs.find(added_conn.top_pipe_port);
                if (oport_iter != mod_ptr->pipe_outputs.end()) {
                    dst_type = oport_iter->second.type;
                    dst_is_output = true;
                } else {
                    return EStr(EOPModPConnDstPortNotFound, "Destination pipe port not found: '" + added_conn.top_pipe_port + "' in __top__ instance");
                }
            }
        } else {
            auto pipe_inst_iter = mod_ptr->pipe_instances.find(added_conn.pipe_instance);
            if (pipe_inst_iter == mod_ptr->pipe_instances.end()) {
                return EStr(EOPModPConnDstPipeNotFound, "Destination pipe instance not found: '" + added_conn.pipe_instance + "'");
            }
            auto &pipe_instance = pipe_inst_iter->second;
            dst_type = pipe_instance.type;
        }
    }

    if (dst_top && (src_is_output != dst_is_output)) {
        return EStr(EOPModPConnMismatch, "Cannot bypass ports with different direction in __top__ instance: '" + added_conn.toString() + "'");
    }
    if (src_type != dst_type) {
        return EStr(EOPModPConnMismatch, "Data type mismatch in pipe connection: '" + added_conn.toString() + "'");
    }

    auto &conn_set = mod_ptr->mod_pipe_connections[added_conn.instance];
    for (const auto &conn : conn_set) {
        if (conn.instance_pipe_port == added_conn.instance_pipe_port) {
            return EStr(EOPModPConnMultiConn, "Pipe connection already exists for source port: '" + added_conn.toString() + "'");
        }
    }
    if (conn_set.find(added_conn) != conn_set.end()) {
        return EStr(EOPModPConnAlreadyExists, "Pipe connection already exists: '" + added_conn.toString() + "'");
    }
    
    conn_set.insert(added_conn);
    project.modified_modules.insert(module_name);

    return VulOperationResponse();
}

string ModulePconnOperation::undo(VulProject &project) {
    auto &modulelib = project.modulelib;

    auto iter = modulelib->modules.find(module_name);
    if (iter == modulelib->modules.end()) {
        return "Module not found during undo: " + module_name;
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(iter->second);
    if (!mod_ptr) {
        return "Module is an external module and cannot be modified during undo: " + module_name;
    }

    mod_ptr->mod_pipe_connections[added_conn.instance].erase(added_conn);

    project.modified_modules.insert(added_conn.instance);

    return "";
}

auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModulePconnOperation>(op);
};
struct Register {
    Register() {
        VulProject::registerOperation("module.pconn", factory);
    }
} register_instance;

} // namespace operation_module_pconn
