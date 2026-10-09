#include "instance_graph.h"
#include "configexpr.hpp"
#include "stringop.hpp"
#include <functional>

namespace {
struct Match {
    shared_ptr<VulStaticModuleInstance> node;
    vector<ConfigRealValue> loops = {0, 0};
    int32_t wildcard = -1;
};
string substitute(string expression, const vector<ConfigRealValue> &loops) {
    string out;
    for (char c : expression) {
        if (c == '$') out += std::to_string(loops[0]);
        else if (c == '?') out += std::to_string(loops[1]);
        else out += c;
    }
    return out;
}
vector<Match> matchChildren(const VulStaticModuleInstance &parent, const string &base,
        const vector<VulConnIndexExpr> &indices, const VulStaticConfigLib &config,
        const vector<ConfigRealValue> &initial, bool source) {
    vector<Match> out;
    for (const auto &child : parent.children) {
        if (child->instance_decl_name != base && child->instance_path.back() != base) continue;
        if (indices.size() != child->instance_array_indices.size()) continue;
        Match match{child, initial, -1};
        // Bind source loop variables before evaluating any expressions.
        if (source) for (size_t dim = 0; dim < indices.size(); ++dim)
            if (indices[dim].kind == VulConnIndexKind::LoopVar)
                match.loops.at(indices[dim].loop_dim) = child->instance_array_indices[dim];
        bool valid = true;
        for (size_t dim = 0; dim < indices.size(); ++dim) {
            const auto &index = indices[dim];
            const auto actual = child->instance_array_indices[dim];
            if (index.kind == VulConnIndexKind::Wildcard) { match.wildcard = actual; continue; }
            ConfigRealValue wanted;
            if (index.kind == VulConnIndexKind::LoopVar) wanted = match.loops.at(index.loop_dim) + index.offset;
            else wanted = calculateConstexprValue(substitute(index.expr, match.loops), config);
            if (wanted != actual) { valid = false; break; }
        }
        if (valid) out.push_back(std::move(match));
    }
    return out;
}
}

void materializeConcreteConnections(const shared_ptr<VulStaticModuleInstance> &root) {
    auto config = root->local_consts;
    config.insert(root->local_parameters.begin(), root->local_parameters.end());
    // Global constants have already been folded in connection dimensions but may occur in expressions.
    for (const auto &entry : root->connection_config) config[entry.first] = entry.second;
    root->concrete_connections.clear();
    for (const auto &rule : root->req_connections) {
        vector<Match> sources = rule.req_instance.empty()
            ? vector<Match>{{nullptr, {0, 0}, -1}}
            : matchChildren(*root, rule.req_instance_base.empty() ? rule.req_instance : rule.req_instance_base,
                            rule.req_indices, config, {0, 0}, true);
        for (const auto &source : sources) {
            vector<Match> destinations = rule.serv_instance.empty()
                ? vector<Match>{{nullptr, source.loops, -1}}
                : matchChildren(*root, rule.serv_instance_base.empty() ? rule.serv_instance : rule.serv_instance_base,
                                rule.serv_indices, config, source.loops, false);
            for (const auto &destination : destinations) {
                auto conn = rule;
                conn.req_instance = source.node ? source.node->instance_path.back() : "";
                conn.serv_instance = destination.node ? destination.node->instance_path.back() : "";
                conn.req_instance_base = conn.req_instance;
                conn.serv_instance_base = conn.serv_instance;
                conn.req_indices.clear(); conn.serv_indices.clear();
                if (!source.node && destination.wildcard >= 0) conn.req_port_index = destination.wildcard;
                if (!destination.node && source.wildcard >= 0) conn.serv_port_index = source.wildcard;
                root->concrete_connections.push_back(std::move(conn));
            }
        }
    }
    for (const auto &child : root->children) materializeConcreteConnections(child);
}

VulStaticModuleInstance concreteGenerationView(const VulStaticModuleInstance &module) {
    auto view = module;
    view.req_connections = module.concrete_connections;
    view.instances.clear();
    for (const auto &child : module.children) {
        VulStaticInstanceDecl decl;
        decl.name = child->instance_path.back();
        decl.module_name = child->module_name;
        view.instances.emplace(decl.name, std::move(decl));
    }
    return view;
}
