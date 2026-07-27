// Copyright (c) 2025 Meng Chengzhen, in Shandong University
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "validate.hpp"

#include "errormsg.hpp"

#include <deque>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace {

using SymbolTable = std::unordered_map<std::string, std::string>;

std::string instanceLabel(const VulStaticModuleInstance &module) {
    return module.concatInstancePath("::", true) + " [" + module.module_name + "]";
}

void declareSymbol(
    SymbolTable &table,
    const std::string &name,
    const std::string &kind,
    const std::string &scope
) {
    VulErrorContextGuard guard(
        "declaring " + kind + " symbol '" +
        (name.empty() ? std::string("<empty>") : name) +
        "' in " + scope
    );
    if (name.empty()) {
        throw VulException("Empty " + kind + " name in " + scope);
    }
    auto it = table.find(name);
    if (it != table.end()) {
        throw VulException(
            "Name redefinition in " + scope + ": '" + name +
            "' was already defined as " + it->second +
            ", cannot redefine as " + kind
        );
    }
    table[name] = kind;
}

SymbolTable buildGlobalSymbols(const VulStaticProject &project) {
    VulErrorContextGuard guard("building global symbol table");

    SymbolTable symbols;
    if (!project.global_names.empty()) {
        for (const auto &[name, kind] : project.global_names) {
            declareSymbol(symbols, name, kind, "global scope");
        }
        return symbols;
    }

    for (const auto &[name, _] : project.global_configlib) {
        declareSymbol(symbols, name, "CONFIG", "global scope");
    }
    for (const auto &bundle : project.global_bundlelib) {
        declareSymbol(symbols, bundle.name, "BUNDLE", "global scope");
    }
    return symbols;
}

void checkModuleSymbols(
    const VulStaticModuleInstance &module,
    const SymbolTable &global_symbols
) {
    SymbolTable local;
    const std::string scope = "module scope " + instanceLabel(module);
    VulErrorContextGuard guard("checking symbol names in " + scope);

    auto declare_local = [&](const std::string &name, const std::string &kind) {
        declareSymbol(local, name, kind, scope);
        auto global_it = global_symbols.find(name);
        if (global_it != global_symbols.end()) {
            throw VulException(
                "Module scope name conflicts with global header name in " + scope +
                ": '" + name + "' was already defined globally as " +
                global_it->second + ", cannot define as " + kind
            );
        }
    };

    for (const auto &[name, _] : module.local_consts) declare_local(name, "CONFIG");
    for (const auto &[name, _] : module.local_parameters) declare_local(name, "PARAMETER");
    for (const auto &bundle : module.local_bundles) declare_local(bundle.name, "BUNDLE");
    for (const auto &[name, _] : module.requests) declare_local(name, "REQUEST");
    for (const auto &[name, _] : module.services) declare_local(name, "SERVICE");
    for (const auto &[name, _] : module.queries) declare_local(name, "QUERY");
    for (const auto &reg : module.registers) declare_local(reg.name, "REGISTER");
    for (const auto &wire : module.wires) declare_local(wire.name, "WIRE");
    for (const auto &bram : module.brams) declare_local(bram.name, "BRAM");
    for (const auto &rom : module.roms) declare_local(rom.name, "ROM");
    for (const auto &queue : module.queues) declare_local(queue.name, "QUEUE");
    for (const auto &[name, _] : module.instances) declare_local(name, "CHILD_INSTANCE");
    std::unordered_set<std::string> child_service_aliases;
    for (const auto &use : module.child_service_uses) {
        if (child_service_aliases.insert(use.alias_name).second) {
            declare_local(use.alias_name, "USE_CHILD_SERVICE_PORT");
        }
    }
    std::unordered_set<std::string> child_query_aliases;
    for (const auto &use : module.child_query_uses) {
        if (child_query_aliases.insert(use.alias_name).second) {
            declare_local(use.alias_name, "USE_CHILD_QUERY");
        }
    }
}

bool samePortShapeIgnoringArray(
    const VulStaticReqServ &a,
    const VulStaticReqServ &b
) {
    if (a.has_handshake != b.has_handshake) return false;
    if (a.args.size() != b.args.size()) return false;
    if (a.rets.size() != b.rets.size()) return false;
    for (size_t i = 0; i < a.args.size(); ++i) {
        if (a.args[i].type != b.args[i].type) return false;
    }
    for (size_t i = 0; i < a.rets.size(); ++i) {
        if (a.rets[i].type != b.rets[i].type) return false;
    }
    return true;
}

bool hasWildcard(const std::vector<VulConnIndexExpr> &indices) {
    for (const auto &idx : indices) {
        if (idx.kind == VulConnIndexKind::Wildcard) return true;
    }
    return false;
}

bool compatibleConnection(
    const VulStaticReqServ &src,
    const VulStaticReqServ &dst,
    bool wildcard_connection
) {
    if (src.match(dst)) return true;
    if (!wildcard_connection) return false;
    if (!samePortShapeIgnoringArray(src, dst)) return false;
    return src.is_arrayed || dst.is_arrayed;
}

bool isBroadcastSource(const VulStaticReqServ &port) {
    return !port.has_handshake && port.rets.empty();
}

std::string endpointKey(
    const std::string &instance_name,
    const std::vector<VulConnIndexExpr> &indices,
    const std::string &port_name
) {
    std::ostringstream os;
    os << instance_name << ".";
    for (const auto &idx : indices) {
        os << "[" << idx.toString() << "]";
    }
    os << port_name;
    return os.str();
}

std::string endpointText(const std::string &instance_name, const std::string &port_name) {
    if (instance_name.empty()) return "<self>." + port_name;
    return instance_name + "." + port_name;
}

const VulStaticModuleInstance *findChildModule(
    const VulStaticModuleInstance &module,
    const std::string &child_name
) {
    for (const auto &child : module.children) {
        if (!child || child->instance_path.empty()) continue;
        if (child->instance_path.back() == child_name) return child.get();
        if (!child->instance_decl_name.empty() && child->instance_decl_name == child_name) {
            return child.get();
        }
    }
    return nullptr;
}

const VulStaticModuleInstance &requireChildModule(
    const VulStaticModuleInstance &module,
    const std::string &child_name,
    const std::string &context
) {
    auto inst_it = module.instances.find(child_name);
    if (inst_it == module.instances.end()) {
        throw VulException(context + ": child instance '" + child_name + "' does not exist");
    }
    const VulStaticModuleInstance *child = findChildModule(module, child_name);
    if (child == nullptr) {
        throw VulException(context + ": parsed child module for instance '" + child_name + "' does not exist");
    }
    return *child;
}

const VulStaticReqServ &requirePort(
    const std::unordered_map<ReqServName, VulStaticReqServ> &ports,
    const std::string &name,
    const std::string &kind,
    const std::string &context
) {
    auto it = ports.find(name);
    if (it == ports.end()) {
        throw VulException(context + ": " + kind + " port '" + name + "' does not exist");
    }
    return it->second;
}

bool hasServiceImplementation(const VulStaticModuleInstance &module, const std::string &service_name) {
    return module.serv_logic_blocks.find(service_name) != module.serv_logic_blocks.end();
}

void validateModuleConnections(const VulStaticModuleInstance &module) {
    const std::string scope = "in module " + instanceLabel(module);
    VulErrorContextGuard guard("validating transaction connectivity " + scope);

    std::unordered_map<std::string, size_t> source_connection_counts;
    std::unordered_map<std::string, const VulStaticReqServ *> source_ports;
    std::unordered_set<std::string> forwarded_services;

    for (const auto &conn : module.req_connections) {
        VulErrorContextGuard conn_guard("validating transaction connection " + conn.toString() + " " + scope);

        const VulStaticReqServ *src = nullptr;
        const VulStaticReqServ *dst = nullptr;
        bool wildcard_connection = hasWildcard(conn.req_indices) || hasWildcard(conn.serv_indices);
        std::string source_key;

        if (conn.req_instance.empty()) {
            src = &requirePort(module.services, conn.req_name, "source service", scope);
            source_key = endpointKey("", {}, conn.req_name);
            forwarded_services.insert(conn.req_name);
        } else {
            const std::string req_base = conn.req_instance_base.empty() ? conn.req_instance : conn.req_instance_base;
            const auto &child = requireChildModule(module, req_base, scope);
            src = &requirePort(child.requests, conn.req_name, "source child request", scope);
            source_key = endpointKey(req_base, conn.req_indices, conn.req_name);
        }

        if (conn.serv_instance.empty()) {
            auto req_it = module.requests.find(conn.serv_name);
            auto serv_it = module.services.find(conn.serv_name);
            if (req_it == module.requests.end() && serv_it == module.services.end()) {
                throw VulException(
                    scope + ": destination self port '" + conn.serv_name +
                    "' does not exist as either REQUEST or SERVICE"
                );
            }
            if (req_it != module.requests.end() && serv_it != module.services.end()) {
                throw VulException(
                    scope + ": destination self port '" + conn.serv_name +
                    "' is ambiguous because both REQUEST and SERVICE exist"
                );
            }
            dst = (req_it != module.requests.end()) ? &req_it->second : &serv_it->second;
        } else {
            const std::string serv_base = conn.serv_instance_base.empty() ? conn.serv_instance : conn.serv_instance_base;
            const auto &child = requireChildModule(module, serv_base, scope);
            dst = &requirePort(child.services, conn.serv_name, "destination child service", scope);
        }

        if (!compatibleConnection(*src, *dst, wildcard_connection)) {
            throw VulException(
                scope + ": transaction port signature mismatch between " +
                endpointText(conn.req_instance, conn.req_name) + " and " +
                endpointText(conn.serv_instance, conn.serv_name)
            );
        }

        source_connection_counts[source_key] += 1;
        source_ports[source_key] = src;
    }

    for (const auto &[key, count] : source_connection_counts) {
        const VulStaticReqServ *src = source_ports.at(key);
        if (count > 1 && !isBroadcastSource(*src)) {
            throw VulException(
                scope + ": non-broadcast transaction source '" + key +
                "' is connected multiple times"
            );
        }
    }

    for (const auto &[name, service] : module.services) {
        VulErrorContextGuard service_guard("validating service port '" + name + "' " + scope);
        bool implemented = hasServiceImplementation(module, name);
        bool forwarded = forwarded_services.find(name) != forwarded_services.end();
        if (implemented && forwarded) {
            throw VulException(
                scope + ": service '" + name +
                "' has both local implementation and child-service forwarding connection"
            );
        }
        if (!implemented && !forwarded) {
            throw VulException(
                scope + ": service '" + name +
                "' is neither locally implemented nor connected to a child service"
            );
        }
    }

    for (const auto &[child_name, _] : module.instances) {
        VulErrorContextGuard child_guard("validating child request ports for '" + child_name + "' " + scope);
        const auto &child = requireChildModule(module, child_name, scope);
        for (const auto &[req_name, _req] : child.requests) {
            VulErrorContextGuard req_guard("validating child request port '" + child_name + "." + req_name + "' " + scope);
            bool connected = false;
            for (const auto &conn : module.req_connections) {
                const std::string req_base = conn.req_instance_base.empty() ? conn.req_instance : conn.req_instance_base;
                if (req_base == child_name && conn.req_name == req_name) {
                    connected = true;
                    break;
                }
            }
            if (!connected) {
                throw VulException(
                    scope + ": child request port '" + child_name + "." + req_name +
                    "' is not connected"
                );
            }
        }
    }
}

} // namespace

void validateStaticProject(const VulStaticProject &project) {
    VulErrorContextGuard guard("validating static project");

    const SymbolTable global_symbols = buildGlobalSymbols(project);
    if (!project.top_module_instance) {
        throw VulException("Static project has no top module instance");
    }

    std::deque<const VulStaticModuleInstance *> queue;
    queue.push_back(project.top_module_instance.get());
    while (!queue.empty()) {
        const VulStaticModuleInstance *module = queue.front();
        queue.pop_front();
        if (module == nullptr) continue;

        VulErrorContextGuard module_guard("validating module " + instanceLabel(*module));
        checkModuleSymbols(*module, global_symbols);
        validateModuleConnections(*module);

        for (const auto &child : module->children) {
            queue.push_back(child.get());
        }
    }
}
