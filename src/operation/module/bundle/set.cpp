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
#include "toposort.hpp"

namespace operation_module_localbund {

class ModuleLocalBundleOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const override { return true; };

    virtual vector<string> help() const override {
        return {
            "Module Local Bundle Set Operation:",
            "Add/Set/Rename/Remove a local bundle in a module in the project's module library.",
            "Arguments:",
            "  - [0] name: The name of the module to set the local bundle in.",
            "  - [1] oldname: (optional) The name of the local bundle to modify.",
            "  - [2] newname: (optional) The new name of the local bundle.",
            "  - [3] definition: (optional) The new definition JSON of the local bundle.",
        };
    }

    ModuleName module_name;
    bool has_old = false;
    VulBundleItem old_bundle;
    bool has_new = false;
    VulBundleItem new_bundle;
};

VulOperationResponse ModuleLocalBundleOperation::execute(VulProject &project) {
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
        return EStr(EOPModCommonImport, "Cannot modify local bundle of external module: " + module_name);
    }

    {
        auto oldname_arg = op.getArg("oldname", 1);
        if (oldname_arg) {
            has_old = true;
            old_bundle.name = *oldname_arg;
            auto iter = mod_ptr->local_bundles.find(old_bundle.name);
            if (iter == mod_ptr->local_bundles.end()) {
                return EStr(EOPModBundNotFound, "Local bundle not found in module '" + module_name + "': " + old_bundle.name);
            }
            old_bundle = iter->second;
            new_bundle = old_bundle;
        }
    }
    {
        auto newname_arg = op.getArg("newname", 2);
        if (newname_arg) {
            has_new = true;
            new_bundle.name = *newname_arg;
            if (!isValidIdentifier(new_bundle.name)) {
                return EStr(EOPModBundNameInvalid, "Invalid local bundle name in module '" + module_name + "': " + new_bundle.name);
            }
            if (new_bundle.name != old_bundle.name && mod_ptr->localCheckNameConflict(new_bundle.name)) {
                return EStr(EOPModBundNameDup, "Local bundle name conflict in module '" + module_name + "': " + new_bundle.name);
            }
            if (new_bundle.name != old_bundle.name && project.globalNameConflictCheck(new_bundle.name)) {
                return EStr(EOPModBundNameDup, "Local bundle name conflict in module '" + module_name + "': " + new_bundle.name);
            }
        }
        auto definition_arg = op.getArg("definition", 3);
        if (definition_arg) {
            has_new = true;
            // parse definition JSON
            serialize::parseBundleItemFromJSON(*definition_arg, new_bundle);
        }
    }

    // check new bundle validity and looped references
    {
        unordered_set<BundleName> local_bundle_names;
        unordered_map<BundleName, unordered_set<BundleName>> bundle_ref_graph;
        for (const auto &pair : mod_ptr->local_bundles) {
            local_bundle_names.insert(pair.first);

            unordered_set<BundleName> refs;
            unordered_set<ConfigName> confs;
            auto errstr = pair.second.checkAndExtractReferences(refs, confs);
            if (!errstr.empty()) {
                continue;
            }
            bundle_ref_graph[pair.first] = refs;
        }
        if (has_old) {
            local_bundle_names.erase(old_bundle.name);
            bundle_ref_graph.erase(old_bundle.name);
        }
        if (has_new) {
            local_bundle_names.insert(new_bundle.name);

            unordered_set<BundleName> refs;
            unordered_set<ConfigName> confs;
            auto errstr = new_bundle.checkAndExtractReferences(refs, confs);
            if (!errstr.empty()) {
                return EStr(EOPModBundDefinitionInvalid, "Invalid local bundle definition in module '" + module_name + "': " + errstr);
            }
            bundle_ref_graph[new_bundle.name] = refs;
        }
        vector<BundleName> looped_bundles;
        auto err = topologicalSort(local_bundle_names, bundle_ref_graph, looped_bundles);
        if (!err) {
            string looped_str;
            for (const auto &bname : looped_bundles) {
                if (!looped_str.empty()) looped_str += ", ";
                looped_str += bname;
            }
            return EStr(EOPModBundRefLoop, "Looped local bundle references in module '" + module_name + "': " + looped_str);
        }
    }

    // apply changes
    if (has_old) mod_ptr->local_bundles.erase(old_bundle.name);
    if (has_new) mod_ptr->local_bundles[new_bundle.name] = new_bundle;
    project.modified_modules.insert(module_name);
    return VulOperationResponse();
};

string ModuleLocalBundleOperation::undo(VulProject &project) {
    // check module existence
    auto &modulelib = project.modulelib;
    auto mod_base_iter = modulelib->modules.find(module_name);
    if (mod_base_iter == modulelib->modules.end()) {
        return "Cannot undo local bundle change of non-existing module: " + module_name;
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_base_iter->second);
    if (!mod_ptr) {
        return "Cannot undo local bundle change of external module: " + module_name;
    }

    // apply changes
    if (has_new) mod_ptr->local_bundles.erase(new_bundle.name);
    if (has_old) mod_ptr->local_bundles[old_bundle.name] = old_bundle;
    project.modified_modules.insert(module_name);
    return "";
};


auto factory_module_localbund = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ModuleLocalBundleOperation>(op);
};
struct RegisterModuleLocalBundleOperation {
    RegisterModuleLocalBundleOperation() {
        VulProject::registerOperation("module.bundle.set", factory_module_localbund);
    }
} register_module_localbund_operation;

} // namespace operation_module_localbund
