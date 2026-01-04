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

namespace operation_configlib_rename {

class ConfigLibRenameOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const { return true; };

    virtual vector<string> help() const override {
        return {
            "ConfigLib Rename Operation:",
            "Rename an existing configuration item in the project's configuration library.",
            "Arguments:",
            "  - [0] old_name: The current name of the configuration item to rename.",
            "  - [1] new_name: The new name for the configuration item.",
            "  - [2] (optional) update: If set to 'true', update all references to the old name to the new name.",
        };
    }

};

VulOperationResponse ConfigLibRenameOperation::execute(VulProject &project) {
    string old_name;
    string new_name;
    bool update_references = false;
    auto &configlib = project.configlib;

    {
        auto old_name_arg = op.getArg("old_name", 0);
        if (!old_name_arg) {
            return EStr(EOPConfRenameMissArg, "Missing argument: [0] old_name");
        }
        auto new_name_arg = op.getArg("new_name", 1);
        if (!new_name_arg) {
            return EStr(EOPConfRenameMissArg, "Missing argument: [1] new_name");
        }
        auto update_arg = op.getArg("update", 2);
        if (update_arg) {
            string update_str = *update_arg;
            update_references = (update_str == "true" || update_str == "True" || update_str == "TRUE" || update_str == "1");
        }
        old_name = *old_name_arg;
        new_name = *new_name_arg;
    }

    auto iter = configlib->config_items.find(old_name);
    if (iter == configlib->config_items.end()) {
        return EStr(EOPConfRenameNameNotFound, string("Config item with name '") + old_name + "' does not exist.");
    }
    if (!isValidIdentifier(new_name)) {
        return EStr(EOPConfRenameNameInvalid, string("Invalid config name: '") + new_name + "'. Must be a valid identifier.");
    }
    if (project.globalNameConflictCheck(new_name)) {
        return EStr(EOPConfRenameNameConflict, string("Name conflict: An item with name '") + new_name + "' already exists in the project.");
    }
    auto confe = iter->second;
    if (confe.group != configlib->DefaultGroupName) {
        return EStr(EOPConfRenameImport, string("Cannot rename config item '") + old_name + "' because it belongs to a imported component '" + confe.group + "'.");
    }
    if (!update_references) {
        if (confe.reverse_references.size() > 0) {
            string ref_confs;
            for (const auto &ref_by_item : confe.reverse_references) {
                if (!ref_confs.empty()) ref_confs += ", ";
                ref_confs += ref_by_item;
            }
            return EStr(EOPConfRenameReferenced, string("Config item '") + old_name + string("' is referenced by other config items: ") + ref_confs);
        }
    }
    unordered_set<BundleName> referencing_bundles;
    for (const auto &bundle_entry : project.bundlelib->bundles) {
        const auto &bundle_item = bundle_entry.second;
        if (bundle_item.references.find(old_name) != bundle_item.references.end()) {
            if (!update_references) {
                referencing_bundles.insert(bundle_entry.first);
            } else {
                return EStr(EOPConfRenameReferenced, string("Config item '") + old_name + string("' is referenced by bundle item: ") + bundle_entry.first);
            }
        }
    }

    // proceed with renaming
    configlib->config_items[new_name] = confe;
    configlib->config_items.erase(old_name);
    // update references
    for (const auto &ref_item : confe.references) {
        auto iter_ref = configlib->config_items.find(ref_item);
        if (iter_ref != configlib->config_items.end()) [[likely]] {
            auto &ref_item_entry = iter_ref->second;
            ref_item_entry.reverse_references.erase(old_name);
            ref_item_entry.reverse_references.insert(new_name);
            ref_item_entry.item.value = identifierReplace(
                ref_item_entry.item.value,
                old_name,
                new_name
            );
        }
    }
    for (const auto &ref_bundle_name : referencing_bundles) {
        auto iter_bundle = project.bundlelib->bundles.find(ref_bundle_name);
        if (iter_bundle == project.bundlelib->bundles.end()) [[unlikely]] {
            continue;
        }
        auto &entry = iter_bundle->second;
        // update confs set
        entry.confs.erase(old_name);
        entry.confs.insert(new_name);
        // update bundle definition
        for (auto &member : entry.item.members) {
            member.uint_length = identifierReplace(member.uint_length, old_name, new_name);
            for (auto &dim : member.dims) {
                dim = identifierReplace(dim, old_name, new_name);
            }
        }
        // update enum members
        for (auto &member : entry.item.enum_members) {
            member.value = identifierReplace(member.value, old_name, new_name);
        }
        project.is_bundle_modified = true;
    }
    project.is_config_modified = true;
    return VulOperationResponse(); // Success
}

string ConfigLibRenameOperation::undo(VulProject &project) {
    VulOperationPackage op_bak = this->op;
    VulOperationPackage fake_op;
    {
        fake_op.name = "configlib.rename";
        auto old_name_arg = op_bak.getArg("old_name", 0);
        auto new_name_arg = op_bak.getArg("new_name", 1);
        auto update_arg = op_bak.getArg("update", 2);
        if (!old_name_arg || !new_name_arg) [[unlikely]] {
            return "Invalid operation arguments for undoing rename.";
        }
        fake_op.arg_list.push_back({0, "old_name", *new_name_arg});
        fake_op.arg_list.push_back({1, "new_name", *old_name_arg});
        if (update_arg) {
            fake_op.arg_list.push_back({2, "update", *update_arg});
        }
    }
    this->op = fake_op;
    VulOperationResponse resp = this->execute(project);
    this->op = op_bak;
    if (resp.code != 0) {
        return resp.msg;
    }
    return "";
}

auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ConfigLibRenameOperation>(op);
};
struct RegisterConfigLibRenameOperation {
    RegisterConfigLibRenameOperation() {
        VulProject::registerOperation("configlib.rename", factory);
    }
} registerConfigLibRenameOperationInstance;


} // namespace operation_configlib_rename
