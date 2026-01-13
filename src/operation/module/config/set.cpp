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

namespace operation_module_localconf {

class ModuleLocalConfigOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const override { return true; };

    virtual vector<string> help() const override {
        return {
            "Module Local Configuration Set Operation:",
            "Add/Set/Rename/Remove a local configuration value in a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module to set the local configuration in.",
            "  - [1] oldname: (optional) The name of the local configuration to modify.",
            "  - [2] newname: (optional) The new name of the local configuration.",
            "  - [3] value: (optional) The new value of the local configuration.",
            "  - [4] comment: (optional) The comment of the local configuration.",
        };
    }

    ModuleName module_name;
    bool has_old = false;
    VulLocalConfigItem old_config;
    bool has_new = false;
    VulLocalConfigItem new_config;
};

VulOperationResponse ModuleLocalConfigOperation::execute(VulProject &project) {
    {
        auto name_arg = op.getArg("name", 0);
        if (!name_arg) {
            return EStr(EOPModCommonMissArg, "Missing argument: [0] name");
        }
        module_name = *name_arg;
    }

    // check module existence
    auto &modulelib = project.modulelib;
    auto mod_base_iter = modulelib->modules.find(module_name);
    if (mod_base_iter == modulelib->modules.end()) {
        return EStr(EOPModCommonNotFound, "Module not found: " + module_name);
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
    if (!mod_ptr) {
        return EStr(EOPModCommonImport, "Cannot set local configuration of external module: " + module_name);
    }

    {
        auto oldname_arg = op.getArg("oldname", 1);
        if (oldname_arg) {
            has_old = true;
            old_config.name = *oldname_arg;
            auto iter = mod_ptr->local_configs.find(old_config.name);
            if (iter == mod_ptr->local_configs.end()) {
                return EStr(EOPModConfNotFound, "Local configuration not found in module '" + module_name + "': " + old_config.name);
            }
            old_config.value = iter->second.value;
            old_config.comment = iter->second.comment;
        }
    }
    {
        new_config = old_config; // start from old config
        auto newname_arg = op.getArg("newname", 2);
        if (newname_arg) {
            has_new = true;
            new_config.name = *newname_arg;
            if (!isValidIdentifier(new_config.name)) {
                return EStr(EOPModConfNameInvalid, "Invalid local configuration name in module '" + module_name + "': " + new_config.name);
            }
            if (new_config.name != old_config.name && mod_ptr->localCheckNameConflict(new_config.name)) {
                return EStr(EOPModConfNameDup, "Local configuration name conflict in module '" + module_name + "': " + new_config.name);
            }
            if (new_config.name != old_config.name && project.globalNameConflictCheck(new_config.name)) {
                return EStr(EOPModConfNameDup, "Local configuration name conflict in module '" + module_name + "': " + new_config.name);
            }
        }
        auto value_arg = op.getArg("value", 3);
        if (value_arg) {
            has_new = true;
            new_config.value = *value_arg;
            ConfigRealValue dummy_real_value;
            unordered_set<ConfigName> dummy_seen_configs;
            ErrorMsg err = project.configlib->calculateConfigExpression(new_config.value, dummy_real_value, dummy_seen_configs);
            if (err.error()) {
                return EStr(EOPModConfValueInvalid, "Invalid local configuration value expression in module '" + module_name + "': " + new_config.value + " (" + err.msg + ")");
            }
        }
        auto comment_arg = op.getArg("comment", 4);
        if (comment_arg) {
            has_new = true;
            new_config.comment = *comment_arg;
        }
    }

    // apply changes
    if (has_old) mod_ptr->local_configs.erase(old_config.name);
    if (has_new) mod_ptr->local_configs[new_config.name] = new_config;

    project.modified_modules.insert(module_name);

    return VulOperationResponse();
}

string ModuleLocalConfigOperation::undo(VulProject &project) {
    // revert changes
    auto &modulelib = project.modulelib;
    auto mod_base_iter = modulelib->modules.find(module_name);
    if (mod_base_iter == modulelib->modules.end()) {
        return "Module not found during undo: " + module_name;
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
    if (!mod_ptr) {
        return "Cannot undo local configuration change of external module: " + module_name;
    }

    if (has_new) mod_ptr->local_configs.erase(new_config.name);
    if (has_old) mod_ptr->local_configs[old_config.name] = old_config;

    project.modified_modules.insert(module_name);

    return "";
}

auto factory_module_localconf = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleLocalConfigOperation>(op);
};
struct RegisterModuleLocalConfigOperation {
    RegisterModuleLocalConfigOperation() {
        VulProject::registerOperation("module.config.set", factory_module_localconf);
    }
} register_module_localconf_operation;

} // namespace operation_module_localconf
