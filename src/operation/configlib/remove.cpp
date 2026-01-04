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

namespace operation_configlib_remove {

class ConfigLibRemoveOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const { return true; };

    virtual vector<string> help() const override {
        return {
            "ConfigLib Remove Operation:",
            "Remove an existing configuration item from the project's configuration library.",
            "Arguments:",
            "  - [0] name: The name of the configuration item to remove.",
        };
    }

protected:
    VulConfigLib::ConfigEntry removed_confe;
};

VulOperationResponse ConfigLibRemoveOperation::execute(VulProject &project) {
    string name;
    auto &configlib = project.configlib;

    {
        auto name_arg = op.getArg("name", 0);
        if (!name_arg) {
            return EStr(EOPConfRemoveMissArg, "Missing argument: [0] name");
        }
        name = *name_arg;
    }

    auto iter = configlib->config_items.find(name);
    if (iter == configlib->config_items.end()) {
        return EStr(EOPConfRemoveNameNotFound, "Configuration item '" + name + "' does not exist.");
    }

    auto &confe = iter->second;
    auto &item = confe.item;

    if (confe.group != configlib->DefaultGroupName) {
        return EStr(EOPConfRemoveImport, "Configuration item '" + name + "' is imported and cannot be removed.");
    }
    if (!confe.reverse_references.empty()) {
        return EStr(EOPConfRemoveReferenced, "Configuration item '" + name + "' is referenced by other configuration item(s) '" + *(confe.reverse_references.begin()) + "' and cannot be removed.");
    }
    for (const auto &bunde : project.bundlelib->bundles) {
        const auto &bundlee = bunde.second;
        if (bundlee.confs.find(name) != bundlee.confs.end()) {
            return EStr(EOPConfRemoveNameNotFound, "Configuration item '" + name + "' is used in bundle '" + bunde.first + "' and cannot be removed.");
        }
    }

    this->removed_confe = confe;
    for (const auto &ref_name : confe.references) {
        auto ref_iter = configlib->config_items.find(ref_name);
        if (ref_iter != configlib->config_items.end()) {
            ref_iter->second.reverse_references.erase(name);
        }
    }
    configlib->config_items.erase(name);
    project.is_config_modified = true;

    return VulOperationResponse(); // Success
}

string ConfigLibRemoveOperation::undo(VulProject &project) {
    auto &configlib = project.configlib;
    const auto &namearg = op.getArg("name", 0);
    if (!namearg) {
        return "Undo failed: Missing argument: [0] name";
    }
    string name = *namearg;

    if (project.globalNameConflictCheck(name)) {
        return "Undo failed: Name conflict: An item with name '" + name + "' already exists in the project.";
    }

    configlib->config_items[name] = this->removed_confe;
    for (const auto &ref_name : this->removed_confe.references) {
        auto ref_iter = configlib->config_items.find(ref_name);
        if (ref_iter != configlib->config_items.end()) {
            ref_iter->second.reverse_references.insert(name);
        }
    }
    project.is_config_modified = true;

    return ""; // Success
}

auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ConfigLibRemoveOperation>(op);
};
struct RegisterConfigLibRemoveOperation {
    RegisterConfigLibRemoveOperation() {
        VulProject::registerOperation("configlib.remove", factory);
    }
} registerConfigLibRemoveOperationInstance;


} // namespace operation_configlib_remove
