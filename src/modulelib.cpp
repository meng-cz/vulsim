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

#include "module.h"
#include "toposort.hpp"

#include <algorithm>
#include <functional>

using std::make_shared;
using std::function;

/**
 * @brief Build the module tree from the module library.
 * Must be called after 'updateDynamicReferences' are called for all modules.
 * @param root_modules Output parameter to hold the root modules of each tree.
 */
void VulModuleLib::buildModuleTree(vector<shared_ptr<ModuleTreeNode>> &root_modules) const {
    root_modules.clear();
    auto &module_lib = modules;

    // Collect all module names
    unordered_set<ModuleName> all_modules;
    unordered_map<ModuleName, uint32_t> indeg;
    for (const auto &entry : module_lib) {
        all_modules.insert(entry.first);
        indeg[entry.first] = 0;
    }

    // Compute in-degree for referenced modules
    for (const auto &entry : module_lib) {
        const auto &mod = entry.second;
        for (const auto &ref : mod->_dyn_referenced_modules) {
            if (all_modules.count(ref)) {
                indeg[ref] += 1;
            }
        }
    }

    // Determine roots: modules with in-degree == 0
    vector<ModuleName> roots_candidates;
    for (const auto &mn : all_modules) {
        if (indeg[mn] == 0) {
            roots_candidates.push_back(mn);
        }
    }
    // If no in-degree-zero nodes (cycle-heavy graph), fall back to all modules
    if (roots_candidates.empty()) {
        for (const auto &mn : all_modules) roots_candidates.push_back(mn);
    }

    // Helper: build a subtree for a module by DFS, avoiding cycles per path
    function<shared_ptr<ModuleTreeNode>(const ModuleName&, unordered_set<ModuleName>&)> buildSubtree;
    buildSubtree = [&](const ModuleName &mn, unordered_set<ModuleName> &pathSeen) -> shared_ptr<ModuleTreeNode> {
        auto it = module_lib.find(mn);
        if (it == module_lib.end()) {
            return nullptr;
        }
        if (pathSeen.count(mn)) {
            // Cycle detected on this path; stop expanding further to avoid infinite recursion
            // Still create a leaf node for visibility
            auto cyc = make_shared<ModuleTreeNode>();
            cyc->name = mn;
            cyc->comment = it->second->comment;
            cyc->is_external = it->second->isExternalModule();
            return cyc;
        }
        pathSeen.insert(mn);
        auto node = make_shared<ModuleTreeNode>();
        node->name = mn;
        node->comment = it->second->comment;
        node->is_external = it->second->isExternalModule();
        for (const auto &ref : it->second->_dyn_referenced_modules) {
            if (!all_modules.count(ref)) {
                // referenced external or missing module: still show as leaf if present in lib, else skip
                auto jt = module_lib.find(ref);
                if (jt != module_lib.end()) {
                    auto childSeen = pathSeen; // allow shared path but safe
                    auto child = buildSubtree(ref, childSeen);
                    if (child) node->submodules.push_back(child);
                }
                continue;
            }
            auto childSeen = pathSeen; // per-branch copy to track cycles
            auto child = buildSubtree(ref, childSeen);
            if (child) node->submodules.push_back(child);
        }
        return node;
    };

    typedef struct {
        size_t size;
        shared_ptr<ModuleTreeNode> node;
    } SizeNodePair;

    vector<SizeNodePair> sized_roots;

    // Helper to compute subtree size
    function<size_t(const shared_ptr<ModuleTreeNode>&)> subtreeSize = [&](const shared_ptr<ModuleTreeNode> &node) -> size_t {
        if (!node) [[unlikely]] return 0;
        size_t s = 1;
        for (const auto &child : node->submodules) {
            s += subtreeSize(child);
        }
        return s;
    };

    // Build trees for each root candidate
    for (const auto &rootName : roots_candidates) {
        unordered_set<ModuleName> seen;
        auto root = buildSubtree(rootName, seen);
        if (root) [[likely]] {
            size_t sz = subtreeSize(root);
            sized_roots.push_back({sz, root});
        }
    }

    // Sort roots by subtree size descending
    std::sort(sized_roots.begin(), sized_roots.end(), [&](const SizeNodePair &a, const SizeNodePair &b) {
        return a.size > b.size;
    });

    root_modules.clear();
    for (const auto &pair : sized_roots) {
        root_modules.push_back(pair.node);
    }
}

/**
 * @brief Build a single module reference tree (bidirectional) from the module library.
 * Must be called after 'updateDynamicReferences' are called for all modules.
 * @param root_module_name The name of the root module.
 * @param out_root_node Output parameter to hold the root node of the tree. Nullptr if the module does not exist.
 */
void VulModuleLib::buildSingleModuleReferenceTree(const ModuleName &root_module_name, shared_ptr<ModuleTreeBidirectionalNode> &out_root_node) const {
    out_root_node.reset();
    auto &module_lib = modules;

    // Root must exist in library
    auto it_root = module_lib.find(root_module_name);
    if (it_root == module_lib.end()) {
        out_root_node = nullptr;
        return;
    }

    // Build adjacency (down: dependencies) and reverse adjacency (up: dependents)
    unordered_map<ModuleName, vector<ModuleName>> down_adj; // M -> deps(M)
    unordered_map<ModuleName, vector<ModuleName>> up_adj;   // M -> parents/dependents(M)

    for (const auto &kv : module_lib) {
        const ModuleName &mn = kv.first;
        const auto &mod = kv.second;
        for (const auto &dep : mod->_dyn_referenced_modules) {
            // only keep edges within library
            if (module_lib.find(dep) == module_lib.end()) continue;
            down_adj[mn].push_back(dep);
            up_adj[dep].push_back(mn);
        }
    }

    // Create root node
    auto root_node = make_shared<ModuleTreeBidirectionalNode>();
    root_node->name = it_root->second->name;
    root_node->comment = it_root->second->comment;
    root_node->is_external = it_root->second->isExternalModule();

    // Downward expansion: fill children-only recursively, keep parents empty for all non-root downward nodes
    function<void(const ModuleName&, const shared_ptr<ModuleTreeBidirectionalNode>&, unordered_set<ModuleName>&)> buildDown;
    buildDown = [&](const ModuleName &mn, const shared_ptr<ModuleTreeBidirectionalNode> &node, unordered_set<ModuleName> &path) {
        auto it = down_adj.find(mn);
        if (it == down_adj.end()) return;
        for (const auto &dep : it->second) {
            if (path.count(dep)) {
                // cycle on this path: create a leaf and stop
                auto cyc_node = make_shared<ModuleTreeBidirectionalNode>();
                auto jt = module_lib.find(dep);
                if (jt != module_lib.end()) {
                    cyc_node->name = jt->second->name;
                    cyc_node->comment = jt->second->comment;
                    cyc_node->is_external = jt->second->isExternalModule();
                } else {
                    cyc_node->name = dep;
                }
                node->children.push_back(cyc_node);
                continue;
            }
            auto jt = module_lib.find(dep);
            if (jt == module_lib.end()) continue;
            auto child = make_shared<ModuleTreeBidirectionalNode>();
            child->name = jt->second->name;
            child->comment = jt->second->comment;
            child->is_external = jt->second->isExternalModule();
            // Do NOT set child->parents for downward subtree as required
            node->children.push_back(child);

            auto path_next = path; // per-branch path tracking to avoid cycles
            path_next.insert(dep);
            buildDown(dep, child, path_next);
        }
    };

    // Upward expansion: fill parents-only recursively, keep children empty for all upward nodes
    function<void(const ModuleName&, const shared_ptr<ModuleTreeBidirectionalNode> &, unordered_set<ModuleName>&)> buildUp;
    buildUp = [&](const ModuleName &mn, const shared_ptr<ModuleTreeBidirectionalNode> &node, unordered_set<ModuleName> &path) {
        auto it = up_adj.find(mn);
        if (it == up_adj.end()) return;
        for (const auto &par : it->second) {
            if (path.count(par)) {
                // cycle on this path: create a leaf and stop
                auto cyc_node = make_shared<ModuleTreeBidirectionalNode>();
                auto jt = module_lib.find(par);
                if (jt != module_lib.end()) {
                    cyc_node->name = jt->second->name;
                    cyc_node->comment = jt->second->comment;
                    cyc_node->is_external = jt->second->isExternalModule();
                } else {
                    cyc_node->name = par;
                }
                node->parents.push_back(cyc_node);
                continue;
            }
            auto jt = module_lib.find(par);
            if (jt == module_lib.end()) continue;
            auto parent = make_shared<ModuleTreeBidirectionalNode>();
            parent->name = jt->second->name;
            parent->comment = jt->second->comment;
            parent->is_external = jt->second->isExternalModule();
            // Do NOT set parent->children for upward subtree as required
            node->parents.push_back(parent);

            auto path_next = path; // per-branch path tracking to avoid cycles
            path_next.insert(par);
            buildUp(par, parent, path_next);
        }
    };

    // Build both directions from root
    {
        unordered_set<ModuleName> down_path; down_path.insert(root_node->name);
        buildDown(root_node->name, root_node, down_path);
    }
    {
        unordered_set<ModuleName> up_path; up_path.insert(root_node->name);
        buildUp(root_node->name, root_node, up_path);
    }

    out_root_node = root_node;
}

/**
 * @brief Notify the module library that a config item has been renamed.
 * Update any module definitions that reference the old config name.
 * Assumes that the dynamic references have been updated.
 * @param old_name The old config item name.
 * @param new_name The new config item name.
 */
void VulModuleLib::externalRenameConfig(const ConfigName &old_name, const ConfigName &new_name) {

    for (auto &kv : modules) {
        auto &mod = kv.second;
        if (mod->_dyn_referenced_configs.count(old_name)) {
            mod->_dyn_referenced_configs.erase(old_name);
            mod->_dyn_referenced_configs.insert(new_name);

            // Local configs
            for (auto &lc_kv : mod->local_configs) {
                lc_kv.second.value = identifierReplace(lc_kv.second.value, old_name, new_name);
            }
            // Local bundles
            for (auto &lb_kv : mod->local_bundles) {
                auto &bundle_item = lb_kv.second;
                // members
                for (auto &member : bundle_item.members) {
                    member.uint_length = identifierReplace(member.uint_length, old_name, new_name);
                    for (auto &dim : member.dims) {
                        dim = identifierReplace(dim, old_name, new_name);
                    }
                    member.value = identifierReplace(member.value, old_name, new_name);
                }
                // enum members
                for (auto &enum_member : bundle_item.enum_members) {
                    enum_member.value = identifierReplace(enum_member.value, old_name, new_name);
                }
            }
            if (!mod->isExternalModule()) {
                VulModule *concrete_mod = static_cast<VulModule*>(mod.get());
                // Instances
                for (auto &inst_kv : concrete_mod->instances) {
                    auto &inst = inst_kv.second;
                    for (auto &lcov_kv : inst.local_config_overrides) {
                        lcov_kv.second = identifierReplace(lcov_kv.second, old_name, new_name);
                    }
                }
                // Pipe instances
                for (auto &pipe_kv : concrete_mod->pipe_instances) {
                    auto &pipe_inst = pipe_kv.second;
                    pipe_inst.input_size = identifierReplace(pipe_inst.input_size, old_name, new_name);
                    pipe_inst.output_size = identifierReplace(pipe_inst.output_size, old_name, new_name);
                    pipe_inst.buffer_size = identifierReplace(pipe_inst.buffer_size, old_name, new_name);
                    pipe_inst.latency = identifierReplace(pipe_inst.latency, old_name, new_name);
                }
            }
        }
    }

}

/**
 * @brief Notify the module library that a bundle item has been renamed.
 * Update any module definitions that reference the old bundle name.
 * Assumes that the dynamic references have been updated.
 * @param old_name The old bundle item name.
 * @param new_name The new bundle item name.
 */
void VulModuleLib::externalRenameBundle(const BundleName &old_name, const BundleName &new_name) {

    for (auto &kv : modules) {
        auto &mod = kv.second;
        if (mod->_dyn_referenced_bundles.count(old_name)) {
            mod->_dyn_referenced_bundles.erase(old_name);
            mod->_dyn_referenced_bundles.insert(new_name);

            // Local bundles
            for (auto &lb_kv : mod->local_bundles) {
                auto &bundle_item = lb_kv.second;
                // members
                for (auto &member : bundle_item.members) {
                    member.type = identifierReplace(member.type, old_name, new_name);
                }
            }
            // Requests and Services
            auto renameBundleInReqServ = [&](VulReqServ &reqserv) {
                for (auto &arg : reqserv.args) {
                    arg.type = identifierReplace(arg.type, old_name, new_name);
                }
                for (auto &ret : reqserv.rets) {
                    ret.type = identifierReplace(ret.type, old_name, new_name);
                }
            };
            for (auto &req_kv : mod->requests) {
                renameBundleInReqServ(req_kv.second);
            }
            for (auto &serv_kv : mod->services) {
                renameBundleInReqServ(serv_kv.second);
            }
            // Pipe ports
            for (auto &pipein_kv : mod->pipe_inputs) {
                pipein_kv.second.type = identifierReplace(pipein_kv.second.type, old_name, new_name);
            }
            for (auto &pipeout_kv : mod->pipe_outputs) {
                pipeout_kv.second.type = identifierReplace(pipeout_kv.second.type, old_name, new_name);
            }

            if (!mod->isExternalModule()) {
                VulModule *concrete_mod = static_cast<VulModule*>(mod.get());
                // Pipe instances
                for (auto &pipe_kv : concrete_mod->pipe_instances) {
                    auto &pipe_inst = pipe_kv.second;
                    pipe_inst.type = identifierReplace(pipe_inst.type, old_name, new_name);
                }
                // Storages
                for (auto &stor_kv : concrete_mod->storages) {
                    auto &stor = stor_kv.second;
                    stor.type = identifierReplace(stor.type, old_name, new_name);
                }
                for (auto &stor_kv : concrete_mod->storagenexts) {
                    auto &stor = stor_kv.second;
                    stor.type = identifierReplace(stor.type, old_name, new_name);
                }
                for (auto &stor_kv : concrete_mod->storagetmp) {
                    auto &stor = stor_kv.second;
                    stor.type = identifierReplace(stor.type, old_name, new_name);
                }
            }
        }
    }
}
