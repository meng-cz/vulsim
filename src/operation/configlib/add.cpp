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

namespace operation_configlib_add {

class ConfigLibAddOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const { return true; };

    virtual vector<string> help() const override {
        return {
            "ConfigLib Add Operation:",
            "Add a new configuration item to the project's configuration library.",
            "Arguments:",
            "  - [0] name: The name of the configuration item to add.",
            "  - [1] value: The value of the configuration item to add.",
        };
    }

};

VulOperationResponse ConfigLibAddOperation::execute(VulProject &project) {
    string name;
    string value;
    string comment = "";
    auto &configlib = project.configlib;

    {
        auto name_arg = op.getArg("name", 0);
        if (!name_arg) {
            return EStr(EOPConfAddMissArg, "Missing argument: [0] name");
        }
        auto value_arg = op.getArg("value", 1);
        if (!value_arg) {
            return EStr(EOPConfAddMissArg, "Missing argument: [1] value");
        }
        auto comment_arg = op.getArg("comment", 2);
        if (comment_arg) {
            comment = *comment_arg;
        }
        name = *name_arg;
        value = *value_arg;
    }
    if (isValidIdentifier(name) == false) {
        return EStr(EOPConfAddNameInvalid, string("Invalid config name: '") + name + "'. Must be a valid identifier.");
    }
    if (project.globalNameConflictCheck(name)) {
        return EStr(EOPConfAddNameConflict, string("Name conflict: An item with name '") + name + "' already exists in the project.");
    }

    ConfigRealValue real_value;
    unordered_set<ConfigName> referenced;
    {
        ErrorMsg err = configlib->calculateConfigExpression(value, real_value, referenced);
        if (err) {
            return EStr(EOPConfAddValueInvalid, string("Invalid config value expression for '") + name + "': " + err.msg);
        }
    }

    for (const auto &ref_name : referenced) {
        auto iter = configlib->config_items.find(ref_name);
        if (iter == configlib->config_items.end()) {
            return EStr(EOPConfAddRefNotFound, string("Invalid config value expression for '") + name + "': Referenced config item '" + ref_name + "' does not exist.");
        } else {
            iter->second.reverse_references.insert(name);
        }
    }

    VulConfigLib::ConfigEntry confe;
    confe.item.name = name;
    confe.item.value = value;
    confe.item.comment = comment;
    confe.group = configlib->DefaultGroupName;
    confe.real_value = real_value;
    confe.references = referenced;
    confe.reverse_references = unordered_set<ConfigName>{};

    configlib->config_items[name] = std::move(confe);

    project.is_config_modified = true;

    return VulOperationResponse(); // Success
}

string ConfigLibAddOperation::undo(VulProject &project) {
    auto &configlib = project.configlib;
    auto namearg = op.getArg("name", 0);
    if (!namearg) [[unlikely]] {
        return "Name '" + string("name") + "' not found in undo operation.";
    }
    const string &name = *namearg;
    auto iter = configlib->config_items.find(name);
    if (iter == configlib->config_items.end()) [[unlikely]] {
        return "Config item '" + name + "' not found in undo operation.";
    }
    auto &entry = iter->second;
    if (!entry.reverse_references.empty()) [[unlikely]] {
        return "Config item '" + name + "' has reverse references and cannot be undone.";
    }
    // remove references
    for (const auto &ref_item : entry.references) {
        auto iter = configlib->config_items.find(ref_item);
        if (iter != configlib->config_items.end()) {
            iter->second.reverse_references.erase(name);
        }
    }
    // remove the item
    configlib->config_items.erase(iter);
    project.is_config_modified = true;
    return "";
}


auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ConfigLibAddOperation>(op);
};
struct RegisterConfigLibAddOperation {
    RegisterConfigLibAddOperation() {
        VulProject::registerOperation("configlib.add", factory);
    }
} registerConfigLibAddOperationInstance;


} // namespace operation_configlib_add
