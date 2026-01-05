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

namespace operation_bundlelib_rename {

class BundleLibRenameOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const override { return true; };

    virtual vector<string> help() const override {
        return {
            "BundleLib Rename Operation:",
            "Rename an existing bundle in the project's bundle library.",
            "Arguments:",
            "  - [0] old_name: The current name of the bundle to rename.",
            "  - [1] new_name: The new name for the bundle.",
            "  - [2] (optional) update: If set to 'true', update all references to the old name to the new name.",
        };
    }

    string old_name_saved;
    string new_name_saved;
    bool update_references = false;
};

VulOperationResponse BundleLibRenameOperation::execute(VulProject &project) {
    string old_name;
    string new_name;
    bool update_references = false;
    auto &bundlelib = project.bundlelib;

    {
        auto old_name_arg = op.getArg("old_name", 0);
        if (!old_name_arg) {
            return EStr(EOPBundRenameMissArg, "Missing argument: [0] old_name");
        }
        old_name = *old_name_arg;
        auto new_name_arg = op.getArg("new_name", 1);
        if (!new_name_arg) {
            return EStr(EOPBundRenameMissArg, "Missing argument: [1] new_name");
        }
        new_name = *new_name_arg;
    }
    update_references = op.getBoolArg("update", 2);

    auto iter = bundlelib->bundles.find(old_name);
    if (iter == bundlelib->bundles.end()) {
        return EStr(EOPBundRenameNameNotFound, "Bundle name does not exist: " + old_name);
    }
    auto &bunde = iter->second;
    if (bunde.tags.size() > 1 || bunde.tags.find(bundlelib->DefaultTag) == bunde.tags.end()) {
        return EStr(EOPBundRenameImport, "Cannot rename imported bundle with non-default tag: " + old_name);
    }
    if (bunde.reverse_references.size() > 0 && !update_references) {
        return EStr(EOPBundRenameReferenced, "Bundle '" + old_name + "' is referenced by other bundles.");
    }
    if (bundlelib->checkNameConflict(new_name)) {
        return EStr(EOPBundRenameNameConflict, "Bundle name already exists: " + new_name);
    }

    // save for undo
    old_name_saved = old_name;
    new_name_saved = new_name;
    this->update_references = update_references;

    // rename bundle
    for (const auto &refed_name : bunde.reverse_references) {
        auto refed_iter = bundlelib->bundles.find(refed_name);
        if (refed_iter != bundlelib->bundles.end()) {
            refed_iter->second.references.erase(old_name);
            refed_iter->second.references.insert(new_name);
            auto &ref_item = refed_iter->second.item;
            for (auto &member : ref_item.members) {
                if (member.type == old_name) {
                    member.type = new_name;
                }
            }
        }
    }
    for (const auto &ref_name : bunde.references) {
        auto ref_iter = bundlelib->bundles.find(ref_name);
        if (ref_iter != bundlelib->bundles.end()) {
            ref_iter->second.reverse_references.erase(old_name);
            ref_iter->second.reverse_references.insert(new_name);
        }
    }
    bundlelib->bundles[new_name] = bunde;
    bundlelib->bundles.erase(old_name);

    project.is_bundle_modified = true;

    return VulOperationResponse();
}

string BundleLibRenameOperation::undo(VulProject &project) {
    auto &bundlelib = project.bundlelib;

    auto iter = bundlelib->bundles.find(new_name_saved);
    if (iter == bundlelib->bundles.end()) {
        return string("Undo failed: renamed bundle '") + new_name_saved + "' not found.";
    }
    auto &bunde = iter->second;

    // rename back
    for (const auto &refed_name : bunde.reverse_references) {
        auto refed_iter = bundlelib->bundles.find(refed_name);
        if (refed_iter != bundlelib->bundles.end()) {
            refed_iter->second.references.erase(new_name_saved);
            refed_iter->second.references.insert(old_name_saved);
            auto &ref_item = refed_iter->second.item;
            for (auto &member : ref_item.members) {
                if (member.type == new_name_saved) {
                    member.type = old_name_saved;
                }
            }
        }
    }
    for (const auto &ref_name : bunde.references) {
        auto ref_iter = bundlelib->bundles.find(ref_name);
        if (ref_iter != bundlelib->bundles.end()) {
            ref_iter->second.reverse_references.erase(new_name_saved);
            ref_iter->second.reverse_references.insert(old_name_saved);
        }
    }
    bundlelib->bundles[old_name_saved] = bunde;
    bundlelib->bundles.erase(new_name_saved);

    project.is_bundle_modified = true;
    
    return string();
}


auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return make_unique<BundleLibRenameOperation>(op);
};

struct BundleLibRenameOperationRegister {
    BundleLibRenameOperationRegister() {
        VulProject::registerOperation("bundlelib.rename", factory);
    }
} bundlelib_rename_operation_register_instance;

} // namespace operation_bundlelib_rename
