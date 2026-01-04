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

namespace operation_configlib_update {

class ConfigLibUpdateOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const { return true; };

    virtual vector<string> help() const override {
        return {
            "ConfigLib Update Operation:",
            "Update the value of an existing configuration item in the project's configuration library.",
            "Arguments:",
            "  - [0] name: The name of the configuration item to update.",
            "  - [1] value: The new value for the configuration item.",
        };
    }

protected:
    string previous_value;
};

VulOperationResponse ConfigLibUpdateOperation::execute(VulProject &project) {
    string name;
    string new_value;
    auto &configlib = project.configlib;

    {
        auto name_arg = op.getArg("name", 0);
        if (!name_arg) {
            return EStr(EOPConfUpdateMissArg, "Missing argument: [0] name");
        }
        name = *name_arg;
        auto value_arg = op.getArg("value", 1);
        if (!value_arg) {
            return EStr(EOPConfUpdateMissArg, "Missing argument: [1] value");
        }
        new_value = *value_arg;
    }

    auto iter = configlib->config_items.find(name);
    if (iter == configlib->config_items.end()) {
        return EStr(EOPConfUpdateNameNotFound, "Configuration item '" + name + "' does not exist.");
    }

    auto &confe = iter->second;
    auto &item = confe.item;
    previous_value = item.value;

    ConfigRealValue real_value;
    unordered_set<ConfigName> referenced;
    ErrorMsg err = configlib->calculateConfigExpression(new_value, real_value, referenced);
    if (!err.empty()) {
        return EStr(EOPConfUpdateValueInvalid, string("Invalid config value expression for '") + name + "': " + err.msg);
    }
    if (referenced.find(name) != referenced.end()) {
        return EStr(EOPConfUpdateValueInvalid, string("Invalid config value expression for '") + name + "': Self-reference is not allowed.");
    }

    for (const auto &ref_name : confe.references) {
        auto ref_item_iter = configlib->config_items.find(ref_name);
        if (ref_item_iter != configlib->config_items.end()) [[likely]] {
            ref_item_iter->second.reverse_references.erase(name);
        }
    }
    for (const auto &ref_name : referenced) {
        auto iter_ref = configlib->config_items.find(ref_name);
        if (iter_ref != configlib->config_items.end()) {
            iter_ref->second.reverse_references.insert(name);
        }
    }
    item.value = new_value;
    confe.real_value = real_value;
    confe.references = std::move(referenced);

    if (confe.group != configlib->DefaultGroupName) {
        // add into import config overrides
        auto import_iter = project.imports.find(confe.group);
        if (import_iter != project.imports.end()) {
            import_iter->second.config_overrides[name] = new_value;
        }
        project.is_modified = true;
    }

    project.is_config_modified = true;

    return VulOperationResponse(); // Success
}

string ConfigLibUpdateOperation::undo(VulProject &project) {
    VulOperationPackage op_bak = this->op;
    VulOperationPackage fake_op;
    {
        fake_op.name = "configlib.update";
        auto name_arg = op_bak.getArg("name", 0);
        auto value_arg = op_bak.getArg("value", 1);
        if (!name_arg || !value_arg) {
            return "Invalid arguments in undo operation.";
        }
        fake_op.arg_list.push_back(VulOperationArg{0, "name", *name_arg});
        fake_op.arg_list.push_back(VulOperationArg{1, "value", previous_value});
    }
    auto undo_operation = ConfigLibUpdateOperation(fake_op);
    VulOperationResponse resp = undo_operation.execute(project);
    if (resp.code != 0) {
        return "Failed to undo configlib.update operation: " + resp.msg;
    }
    return "";
}


auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ConfigLibUpdateOperation>(op);
};
struct RegisterConfigLibUpdateOperation {
    RegisterConfigLibUpdateOperation() {
        VulProject::registerOperation("configlib.update", factory);
    }
} registerConfigLibUpdateOperationInstance;


} // namespace operation_configlib_update
