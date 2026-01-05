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

namespace operation_bundlelib_update {

class BundleLibUpdateOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const override { return true; };

    virtual vector<string> help() const override {
        return {
            "BundleLib Update Operation:",
            "Update an existing bundle in the project's bundle library.",
            "Arguments:",
            "  - [0] name: The name of the bundle to update.",
            "  - [1] definition: The new bundle definition in JSON format.",
        };
    }

    VulBundleItem previous_item;
    VulBundleItem new_item;
};

VulOperationResponse BundleLibUpdateOperation::execute(VulProject &project) {
    string bundle_name;
    string definition_json;
    auto &bundlelib = project.bundlelib;

    {
        auto name_arg = op.getArg("name", 0);
        if (!name_arg) {
            return EStr(EOPBundUpdateMissArg, "Missing argument: [0] name");
        }
        bundle_name = *name_arg;
        auto def_arg = op.getArg("definition", 0);
        if (!def_arg) {
            return EStr(EOPBundUpdateMissArg, "Missing argument: [1] definition");
        }
        definition_json = *def_arg;
    }

    auto it = bundlelib->bundles.find(bundle_name);
    if (it == bundlelib->bundles.end()) {
        return EStr(EOPBundUpdateNameNotFound, "Bundle does not exist: " + bundle_name);
    }
    auto &bunde = it->second;
    if (bunde.tags.size() != 1 || bunde.tags.find(bundlelib->DefaultTag) == bunde.tags.end()) {
        return EStr(EOPBundUpdateImport, "Only bundles with the default tag can be updated: " + bundle_name);
    }

    previous_item = bunde.item;
    new_item.name = bundle_name;
    new_item.comment = previous_item.comment;
    new_item.fromMemberJson(definition_json);

    VulBundleLib::BundleEntry new_entry;
    string err = new_item.checkAndExtractReferences(new_entry.references, new_entry.confs);
    if (!err.empty()) {
        return EStr(EOPBundUpdateDefinitionInvalid, "Invalid bundle definition '" + bundle_name + "': " + err);
    }

    {
        // find loop
        struct DFSEntry {
            BundleName name;
            vector<BundleName> childs;
        };
        vector<DFSEntry> dfs_stack;
        unordered_set<BundleName> visited;
        dfs_stack.push_back({bundle_name, vector<BundleName>(new_entry.references.begin(), new_entry.references.end())});
        visited.insert(bundle_name);
        while (!dfs_stack.empty()) {
            auto &current = dfs_stack.back();
            if (current.childs.empty()) {
                dfs_stack.pop_back();
                continue;
            }
            BundleName next_name = current.childs.back();
            current.childs.pop_back();
            if (visited.find(next_name) != visited.end()) {
                string loop_path;
                for (const auto &entry : dfs_stack) {
                    loop_path += entry.name + " -> ";
                }
                loop_path += next_name;
                return EStr(EOPBundUpdateRefLoop, "Invalid bundle definition '" + bundle_name + "': introduces circular reference: " + loop_path);
            }
            visited.insert(next_name);
            auto next_iter = bundlelib->bundles.find(next_name);
            if (next_iter != bundlelib->bundles.end()) {
                dfs_stack.push_back({next_name, vector<BundleName>(next_iter->second.references.begin(), next_iter->second.references.end())});
            }
        }
    }

    // all checks passed, update bundle
    for (const auto &ref_name : bunde.references) {
        auto ref_iter = bundlelib->bundles.find(ref_name);
        if (ref_iter != bundlelib->bundles.end()) [[likely]] {
            ref_iter->second.reverse_references.erase(bundle_name);
        }
    }
    for (const auto &ref_name : new_entry.references) {
        auto ref_iter = bundlelib->bundles.find(ref_name);
        if (ref_iter != bundlelib->bundles.end()) [[likely]] {
            ref_iter->second.reverse_references.insert(bundle_name);
        }
    }
    bunde.item = new_item;
    bunde.references = new_entry.references;
    bunde.confs = new_entry.confs;
    project.is_bundle_modified = true;
    return VulOperationResponse();
}

string BundleLibUpdateOperation::undo(VulProject &project) {
    auto &bundlelib = project.bundlelib;

    auto it = bundlelib->bundles.find(new_item.name);
    if (it == bundlelib->bundles.end()) {
        return string("Undo failed: updated bundle '") + new_item.name + "' not found.";
    }
    auto &bunde = it->second;

    // revert update
    for (const auto &ref_name : bunde.references) {
        auto ref_iter = bundlelib->bundles.find(ref_name);
        if (ref_iter != bundlelib->bundles.end()) [[likely]] {
            ref_iter->second.reverse_references.erase(new_item.name);
        }
    }
    unordered_set<BundleName> previous_refs;
    unordered_set<ConfigName> previous_confs;
    string err = previous_item.checkAndExtractReferences(previous_refs, previous_confs);
    if (!err.empty()) {
        return string("Undo failed: previous bundle definition '") + new_item.name + "' is invalid: " + err;
    }
    for (const auto &ref_name : previous_refs) {
        auto ref_iter = bundlelib->bundles.find(ref_name);
        if (ref_iter != bundlelib->bundles.end()) [[likely]] {
            ref_iter->second.reverse_references.insert(previous_item.name);
        }
    }
    bunde.item = previous_item;
    bunde.references = previous_refs;
    bunde.confs = previous_confs;
    project.is_bundle_modified = true;
    return "";
}


auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return make_unique<BundleLibUpdateOperation>(op);
};

} // namespace operation_bundlelib_update
