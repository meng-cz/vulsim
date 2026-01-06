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
#include "serializejson.h"

namespace operation_bundlelib_add {

class BundleLibAddOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const { return true; };

    virtual vector<string> help() const override {
        return {
            "BundleLib Add Operation:",
            "Add a new bundle to the project's bundle library.",
            "Arguments:",
            "  - [0] name: The name of the bundle to add.",
            "  - [1] comment (optional): The comment for the bundle.",
            "  - [2] definition (optional) : The JSON definition of the bundle.",
        };
    }

    VulBundleLib::BundleEntry bunde;
};

VulOperationResponse BundleLibAddOperation::execute(VulProject &project) {
    string name;
    string comment = "";
    string definition = "";
    {
        auto arg0 = op.getArg("name", 0);
        if (!arg0) {
            return EStr(EOPBundAddMissArg, "Missing argument: name");
        }
        name = *arg0;
        auto arg1 = op.getArg("comment", 0);
        if (arg1) {
            comment = *arg1;
        }
        auto arg2 = op.getArg("definition", 0);
        if (arg2) {
            definition = *arg2;
        }
    }
    if (!isValidIdentifier(name)) {
        return EStr(EOPBundAddNameInvalid, "Invalid bundle name: " + name);
    }
    if (project.globalNameConflictCheck(name)) {
        return EStr(EOPBundAddNameConflict, "Bundle name conflicts with existing config, bundle, or module: " + name);
    }

    bunde = VulBundleLib::BundleEntry();

    VulBundleItem added_bundle;
    added_bundle.name = name;
    added_bundle.comment = comment;
    if (definition.empty()) {
        // default empty bundle
        bunde.item = added_bundle;
        bunde.tags.insert(project.bundlelib->DefaultTag);
        project.bundlelib->bundles[name] = bunde;
        project.is_bundle_modified = true;
        return VulOperationResponse();
    }

    if (!definition.empty()) {
        try {
            serialize::parseBundleItemFromJSON(definition, added_bundle);
        } catch (const std::exception &e) {
            return EStr(EOPBundAddDefinitionInvalid, string("Invalid bundle definition JSON: ") + e.what());
        }
    }

    unordered_set<BundleName> bundle_refs;
    unordered_set<ConfigName> config_refs;
    string err = added_bundle.checkAndExtractReferences(bundle_refs, config_refs);
    if (!err.empty()) {
        return EStr(EOPBundAddDefinitionInvalid, string("Invalid bundle definition: ") + err);
    }

    for (const auto &ref_name : bundle_refs) {
        if (!project.bundlelib->checkNameConflict(ref_name)) {
            return EStr(EOPBundAddDefinitionInvalid, string("Referenced bundle '") + ref_name + string("' in definition does not exist in bundle library"));
        }
    }
    for (const auto &conf_name : config_refs) {
        if (!project.configlib->checkNameConflict(conf_name)) {
            return EStr(EOPBundAddDefinitionInvalid, string("Referenced config item '") + conf_name + string("' in definition does not exist in config library"));
        }
    }

    // all checks passed, insert bundle

    for (const auto &ref_name : bundle_refs) {
        auto ref_iter = project.bundlelib->bundles.find(ref_name);
        if (ref_iter != project.bundlelib->bundles.end()) [[likely]] {
            ref_iter->second.reverse_references.insert(added_bundle.name);
        }
    }

    VulBundleLib::BundleEntry bunde;
    bunde.item = added_bundle;
    bunde.tags.insert(project.bundlelib->DefaultTag);
    bunde.references = std::move(bundle_refs);
    bunde.confs = std::move(config_refs);
    project.bundlelib->bundles[added_bundle.name] = bunde;
    project.is_bundle_modified = true;
    return VulOperationResponse();
}

string BundleLibAddOperation::undo(VulProject &project) {
    auto iter = project.bundlelib->bundles.find(bunde.item.name);
    if (iter != project.bundlelib->bundles.end()) {
        // remove reverse references
        for (const auto &ref_name : bunde.references) {
            auto ref_iter = project.bundlelib->bundles.find(ref_name);
            if (ref_iter != project.bundlelib->bundles.end()) [[likely]] {
                ref_iter->second.reverse_references.erase(bunde.item.name);
            }
        }
        project.bundlelib->bundles.erase(iter);
    }
    return "";
}

auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return make_unique<BundleLibAddOperation>(op);
};
struct Register {
    Register() {
        VulProject::registerOperation("bundlelib.add", factory);
    }
} register_instance;

} // namespace operation_bundlelib_add
