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

namespace operation_bundlelib_remove {

class BundleLibRemoveOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const override { return true; };

    virtual vector<string> help() const override {
        return {
            "BundleLib Remove Operation:",
            "Remove an existing bundle from the project's bundle library.",
            "Arguments:",
            "  - [0] name: The name of the bundle to remove.",
        };
    }

    VulBundleLib::BundleEntry removed_entry;
};

VulOperationResponse BundleLibRemoveOperation::execute(VulProject &project) {
    string bundle_name;
    auto &bundlelib = project.bundlelib;

    {
        auto name_arg = op.getArg("name", 0);
        if (!name_arg) {
            return EStr(EOPBundRemoveMissArg, "Missing argument: [0] name");
        }
        bundle_name = *name_arg;
    }

    auto it = bundlelib->bundles.find(bundle_name);
    if (it == bundlelib->bundles.end()) {
        return EStr(EOPBundRemoveNameNotFound, "Bundle does not exist: " + bundle_name);
    }
    auto &bunde = it->second;
    if (bunde.reverse_references.size() > 0) {
        return EStr(EOPBundRemoveReferenced, "Bundle is still referenced by other bundles, cannot remove: " + bundle_name);
    }
    if (bunde.tags.size() > 1 || bunde.tags.find(bundlelib->DefaultTag) == bunde.tags.end()) {
        return EStr(EOPBundRemoveReferenced, "Bundle is imported from external module, cannot remove: " + bundle_name);
    }

    removed_entry = bunde;

    for (const auto &ref_name : bunde.references) {
        auto ref_iter = bundlelib->bundles.find(ref_name);
        if (ref_iter != bundlelib->bundles.end()) {
            ref_iter->second.reverse_references.erase(bundle_name);
        }
    }
    bundlelib->bundles.erase(it);
    project.is_bundle_modified = true;
    return VulOperationResponse(0, "Bundle removed successfully: " + bundle_name);
}

string BundleLibRemoveOperation::undo(VulProject &project) {
    auto &bundlelib = project.bundlelib;

    bundlelib->bundles[removed_entry.item.name] = removed_entry;
    for (const auto &ref_name : removed_entry.references) {
        auto ref_iter = bundlelib->bundles.find(ref_name);
        if (ref_iter != bundlelib->bundles.end()) {
            ref_iter->second.reverse_references.insert(removed_entry.item.name);
        }
    }

    project.is_bundle_modified = true;
    return string();
}


auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return make_unique<BundleLibRemoveOperation>(op);
};
struct BundleLibRemoveOperationFactoryRegister {
    BundleLibRemoveOperationFactoryRegister() {
        VulProject::registerOperation("bundlelib.remove", factory);
    }
} bundlelib_remove_operation_factory_register_instance;


} // namespace operation_bundlelib_remove
