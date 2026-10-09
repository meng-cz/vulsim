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

#include "module.h"
#include "configexpr.hpp"
#include "toposort.hpp"
#include "cppparse.hpp"
#include "stringop.hpp"
#include <cassert>
#include <functional>
#include <iostream>
#include <optional>
#include <regex>
#include <set>
#include "instance_graph.h"

using std::make_shared;

namespace {

struct ParsedInstanceExpr {
    string base_name;
    vector<string> index_exprs;
};

static bool isIdentStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

static bool isIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

static ParsedInstanceExpr parseInstanceExpr(const string &expr_raw) {
    const string expr = stringop::trim(expr_raw);
    if (expr.empty()) {
        return {};
    }

    size_t pos = 0;
    if (!isIdentStart(expr[pos])) {
        throw VulException("Invalid instance expression: '" + expr + "'");
    }
    while (pos < expr.size() && isIdentChar(expr[pos])) {
        ++pos;
    }

    ParsedInstanceExpr out;
    out.base_name = expr.substr(0, pos);

    while (pos < expr.size()) {
        while (pos < expr.size() && std::isspace(static_cast<unsigned char>(expr[pos]))) {
            ++pos;
        }
        if (pos >= expr.size()) {
            break;
        }
        if (expr[pos] != '[') {
            throw VulException("Invalid instance expression: '" + expr + "'");
        }
        const size_t begin = ++pos;
        int depth = 1;
        bool in_string = false;
        bool in_char = false;
        bool escape = false;
        for (; pos < expr.size(); ++pos) {
            const char c = expr[pos];
            if (in_string || in_char) {
                if (escape) {
                    escape = false;
                } else if (c == '\\') {
                    escape = true;
                } else if (in_string && c == '"') {
                    in_string = false;
                } else if (in_char && c == '\'') {
                    in_char = false;
                }
                continue;
            }
            if (c == '"') {
                in_string = true;
                continue;
            }
            if (c == '\'') {
                in_char = true;
                continue;
            }
            if (c == '[') {
                ++depth;
            } else if (c == ']') {
                --depth;
                if (depth == 0) {
                    break;
                }
            }
        }
        if (pos >= expr.size() || expr[pos] != ']') {
            throw VulException("Unmatched '[' in instance expression: '" + expr + "'");
        }
        out.index_exprs.push_back(stringop::trim(expr.substr(begin, pos - begin)));
        ++pos;
    }
    return out;
}

static string arrayInstanceName(const string &base_name, const vector<ConfigRealValue> &indices) {
    return concreteInstanceName(base_name, indices);
}

static string replaceLoopVars(const string &expr, const vector<ConfigRealValue> &loop_vars) {
    string out;
    out.reserve(expr.size() * 2);
    for (char c : expr) {
        if (c == '$') {
            out += "__v0";
        } else if (c == '?') {
            out += "__v1";
        } else {
            out.push_back(c);
        }
    }
    return out;
}

static ConfigRealValue evalExprWithLoopVars(
    const string &expr,
    const VulStaticConfigLib &config_lib,
    const vector<ConfigRealValue> &loop_vars
) {
    VulStaticConfigLib eval_cfg = config_lib;
    if (loop_vars.size() > 0) {
        eval_cfg["__v0"] = loop_vars[0];
    }
    if (loop_vars.size() > 1) {
        eval_cfg["__v1"] = loop_vars[1];
    }
    return calculateConstexprValue(replaceLoopVars(expr, loop_vars), eval_cfg);
}

static const VulStaticInstanceDecl &requireInstanceDecl(
    const unordered_map<InstanceName, VulStaticInstanceDecl> &instances,
    const string &name,
    const string &ctx
) {
    auto it = instances.find(name);
    if (it == instances.end()) {
        throw VulException("Instance '" + name + "' not found while " + ctx);
    }
    return it->second;
}

template<typename Fn>
static void forEachIndexTuple(const vector<ConfigRealValue> &dims, Fn fn) {
    vector<ConfigRealValue> indices(dims.size(), 0);
    std::function<void(size_t)> dfs = [&](size_t dim) {
        if (dim == dims.size()) {
            fn(indices);
            return;
        }
        for (ConfigRealValue idx = 0; idx < dims[dim]; ++idx) {
            indices[dim] = idx;
            dfs(dim + 1);
        }
    };
    if (dims.empty()) {
        fn(indices);
    } else {
        dfs(0);
    }
}

static bool hasLoopVar(const string &expr) {
    return expr.find('$') != string::npos || expr.find('?') != string::npos;
}

static bool hasWildcard(const string &expr) {
    return expr.find('*') != string::npos;
}

static VulConnIndexExpr parseConnIndexExpr(const string &expr_raw) {
    const string expr = stringop::trim(expr_raw);
    if (expr.empty()) {
        throw VulException("Empty array index expression");
    }
    if (expr == "*") {
        VulConnIndexExpr out;
        out.kind = VulConnIndexKind::Wildcard;
        out.expr = expr;
        return out;
    }

    auto parse_loop_var = [&](char symbol, int32_t loop_dim) -> std::optional<VulConnIndexExpr> {
        const size_t pos = expr.find(symbol);
        if (pos == string::npos) return std::nullopt;
        if (expr.find(symbol, pos + 1) != string::npos) {
            return std::nullopt;
        }
        if (expr.find(symbol == '$' ? '?' : '$') != string::npos) {
            return std::nullopt;
        }
        string lhs = stringop::trim(expr.substr(0, pos));
        string rhs = stringop::trim(expr.substr(pos + 1));
        int64_t offset = 0;
        if (!lhs.empty()) {
            return std::nullopt;
        }
        if (!rhs.empty()) {
            if (rhs[0] == '+') {
                offset = std::stoll(stringop::trim(rhs.substr(1)));
            } else if (rhs[0] == '-') {
                offset = -std::stoll(stringop::trim(rhs.substr(1)));
            } else {
                return std::nullopt;
            }
        }
        VulConnIndexExpr out;
        out.kind = VulConnIndexKind::LoopVar;
        out.loop_dim = loop_dim;
        out.offset = offset;
        out.expr = expr;
        return out;
    };

    if (auto parsed = parse_loop_var('$', 0)) return *parsed;
    if (auto parsed = parse_loop_var('?', 1)) return *parsed;

    if (hasLoopVar(expr)) {
        VulConnIndexExpr out;
        out.kind = VulConnIndexKind::GeneralExpr;
        out.expr = expr;
        return out;
    }

    VulConnIndexExpr out;
    out.kind = VulConnIndexKind::ConstantExpr;
    out.expr = expr;
    return out;
}

static vector<VulConnIndexExpr> parseConnIndices(const ParsedInstanceExpr &parsed) {
    vector<VulConnIndexExpr> out;
    out.reserve(parsed.index_exprs.size());
    for (const auto &expr : parsed.index_exprs) {
        out.push_back(parseConnIndexExpr(expr));
    }
    return out;
}

static bool tryResolveConcreteInstance(
    const ParsedInstanceExpr &parsed,
    const VulStaticInstanceDecl &decl,
    const VulStaticConfigLib &config_lib,
    const vector<ConfigRealValue> &loop_vars,
    string &concrete_name
) {
    if (parsed.index_exprs.size() != decl.array_dims.size()) {
        throw VulException("Dimension mismatch when resolving instance '" + parsed.base_name + "'");
    }
    vector<ConfigRealValue> indices;
    indices.reserve(parsed.index_exprs.size());
    for (size_t i = 0; i < parsed.index_exprs.size(); ++i) {
        const string &expr = parsed.index_exprs[i];
        if (hasWildcard(expr)) {
            throw VulException("Wildcard '*' is only allowed in module boundary connections");
        }
        ConfigRealValue idx = evalExprWithLoopVars(expr, config_lib, loop_vars);
        if (idx < 0 || idx >= decl.array_dims[i]) {
            return false;
        }
        indices.push_back(idx);
    }
    concrete_name = decl.isArrayed() ? arrayInstanceName(parsed.base_name, indices) : parsed.base_name;
    return true;
}

static vector<size_t> collectWildcardDims(const ParsedInstanceExpr &parsed) {
    vector<size_t> dims;
    for (size_t i = 0; i < parsed.index_exprs.size(); ++i) {
        if (stringop::trim(parsed.index_exprs[i]) == "*") {
            dims.push_back(i);
        }
    }
    return dims;
}

static bool sameServiceDeclarationSignature(
    const VulStaticReqServ &decl,
    const VulStaticReqServ &impl,
    string &reason
) {
    if (decl.has_handshake != impl.has_handshake) {
        reason = "handshake attribute differs";
        return false;
    }
    if (decl.is_arrayed != impl.is_arrayed) {
        reason = "array attribute presence differs";
        return false;
    }
    if (decl.array_size != impl.array_size) {
        reason = "array size differs";
        return false;
    }
    if (decl.args.size() != impl.args.size()) {
        reason = "ARG count differs";
        return false;
    }
    if (decl.rets.size() != impl.rets.size()) {
        reason = "RESP count differs";
        return false;
    }
    if (decl.param_order != impl.param_order) {
        reason = "ARG/RESP declaration order differs";
        return false;
    }
    for (size_t i = 0; i < decl.args.size(); ++i) {
        if (decl.args[i].name != impl.args[i].name) {
            reason = "ARG #" + std::to_string(i) + " name differs";
            return false;
        }
        if (decl.args[i].type != impl.args[i].type) {
            reason = "ARG '" + decl.args[i].name + "' type differs";
            return false;
        }
    }
    for (size_t i = 0; i < decl.rets.size(); ++i) {
        if (decl.rets[i].name != impl.rets[i].name) {
            reason = "RESP #" + std::to_string(i) + " name differs";
            return false;
        }
        if (decl.rets[i].type != impl.rets[i].type) {
            reason = "RESP '" + decl.rets[i].name + "' type differs";
            return false;
        }
    }
    return true;
}

} // namespace

VulStaticReqServ staticalizeReqServ(const VulTempReqServBase &item, const VulStaticConfigLib &config_lib) {
    VulStaticReqServ static_req;
    static_req.name = item.name;
    static_req.is_arrayed = false;
    static_req.array_size = 1;
    static_req.has_handshake = item.has_handshake;
    static_req.param_order = item.param_order;
    if (!item.array_size.empty()) {
        ConfigRealValue array_size = calculateConstexprValue(item.array_size, config_lib);
        if (array_size <= 0) {
            throw VulException("ARRAY() must be positive for request/service '" + item.name + "'");
        }
        static_req.is_arrayed = true;
        static_req.array_size = array_size;
    }
    for (const auto &arg : item.args) {
        VulStaticArg static_arg;
        static_arg.name = arg.second;
        static_arg.type = parseTypeSignature(arg.first, config_lib);
        static_req.args.push_back(static_arg);
    }
    for (const auto &ret : item.rets) {
        VulStaticArg static_arg;
        static_arg.name = ret.second;
        static_arg.type = parseTypeSignature(ret.first, config_lib);
        static_req.rets.push_back(static_arg);
    }
    return static_req;
}

static VulStaticQuery staticalizeQuery(const VulTempQuery &item, const VulStaticConfigLib &config_lib) {
    VulStaticQuery query;
    query.name = item.name;
    query.ret_type = parseTypeSignature(item.ret_type, config_lib);
    return query;
}

void instantiateModule(
    VulStaticModuleInstance &instance,
    const VulTempModule &temp,
    const VulStaticConfigLib &param_overrides,
    const VulStaticConfigLib &global_config,
    const VulStaticBundleLib &global_bundles
) {
    auto reject_internal = [](const vector<string> &lines) {
        const std::regex reserved(R"(\b__vul_(?:idx_|array_idx_)\w*)");
        for (const auto &line : cppparse::stripComments(lines).lines)
            if (std::regex_search(line, reserved)) throw VulException("Internal array coordinate names are not a user API");
    };
    reject_internal(temp.helper_codes);
    for (const auto &reg : temp.registers) reject_internal(reg.reset_codelines);
    for (const auto &wire : temp.wires) reject_internal(wire.reset_codelines);
    for (const auto &block : temp.tick_blocks) reject_internal(block);
    for (const auto &service : temp.services) reject_internal(service.codelines);
    for (const auto &query : temp.queries) reject_internal(query.codelines);
    instance.filepath = temp.filepath;
    instance.module_name = temp.name;

    VulStaticConfigLib local_config_lib = global_config;
    
    for (const auto &conf : temp.configs) {
        VulErrorContextGuard _err{"Processing config '" + conf.name + "'"};
        ConfigRealValue value = calculateConstexprValue(conf.value, local_config_lib);
        local_config_lib[conf.name] = value;
        instance.local_consts[conf.name] = value;
    }
    for (const auto &param : temp.params) {
        VulErrorContextGuard _err{"Processing parameter '" + param.name + "'"};
        ConfigRealValue value;
        auto override_iter = param_overrides.find(param.name);
        if (override_iter != param_overrides.end()) {
            value = override_iter->second;
        } else {
            value = calculateConstexprValue(param.value, local_config_lib);
        }
        local_config_lib[param.name] = value;
        instance.local_parameters[param.name] = value;
    }

    for (const auto &bundle : temp.bundles) {
        VulErrorContextGuard _err{"Processing bundle '" + bundle.name + "'"};
        VulStaticBundle sb = staticalizeBundle(bundle, local_config_lib);
        instance.local_bundles.push_back(sb);
    }
    VulStaticBundleLib local_config_library = mergeStaticBundleLibs(global_bundles, instance.local_bundles);

    // Requests
    for (const auto &req : temp.requests) {
        VulErrorContextGuard _err{"Processing request '" + req.name + "'"};
        instance.requests[req.name] = staticalizeReqServ(req, local_config_lib);
    }

    // services
    struct ServiceDeclarationInfo {
        VulStaticReqServ signature;
        std::optional<ConfigRealValue> priority;
        bool implemented = false;
    };
    unordered_map<string, ServiceDeclarationInfo> service_declarations;
    VulLogicBlockID next_logic_block_id = 1;
    for (const auto &serv : temp.services) {
        const string &serv_name = serv.name;
        VulErrorContextGuard _err{
            string("Processing service ") + (serv.is_declaration ? "declaration" : "implementation") +
            " '" + serv_name + "'"
        };
        VulStaticReqServ static_serv = staticalizeReqServ(serv, local_config_lib);
        std::optional<ConfigRealValue> priority_value;
        if (!serv.priority.empty()) {
            priority_value = calculateConstexprValue(serv.priority, local_config_lib);
        }
        if (serv.is_declaration) {
            if (service_declarations.find(serv_name) != service_declarations.end()) {
                throw VulException("Duplicate SERVICE declaration for '" + serv_name + "'");
            }
            ServiceDeclarationInfo info;
            info.signature = std::move(static_serv);
            info.priority = priority_value;
            service_declarations[serv_name] = std::move(info);
            continue;
        }

        auto decl_iter = service_declarations.find(serv_name);
        if (decl_iter != service_declarations.end()) {
            string reason;
            if (!sameServiceDeclarationSignature(decl_iter->second.signature, static_serv, reason)) {
                throw VulException("SERVICE implementation for '" + serv_name +
                                   "' does not match its forward declaration: " + reason);
            }
            if (decl_iter->second.priority.has_value()) {
                if (!priority_value.has_value()) {
                    throw VulException("SERVICE implementation for '" + serv_name +
                                       "' does not match its forward declaration: priority attribute is missing");
                }
                if (*decl_iter->second.priority != *priority_value) {
                    throw VulException("SERVICE implementation for '" + serv_name +
                                       "' does not match its forward declaration: priority value differs");
                }
            }
            decl_iter->second.implemented = true;
        }

        instance.services[serv_name] = static_serv;
        VulLogicBlock lb;
        lb.block_id = next_logic_block_id++;
        lb.with_priority = !serv.priority.empty();
        if (priority_value.has_value()) {
            lb.priority = *priority_value;
        } else {
            lb.priority = 0;
        }
        lb.codelines = serv.codelines;
        lb.codelines_debug = serv.codelines_debug;
        if (serv.has_handshake) {
            lb.cond_codelines.push_back("return (" + serv.cond + ");\n");
            lb.cond_codelines_debug.push_back(serv.cond_debug);
        }
        instance.serv_logic_blocks[serv_name] = lb;
    }
    for (const auto &entry : service_declarations) {
        if (!entry.second.implemented) {
            const bool forwarded = std::any_of(temp.req_connections.begin(), temp.req_connections.end(),
                [&](const auto &conn) {
                    return conn.req_instance.empty() && conn.req_name == entry.first && !conn.serv_instance.empty();
                });
            if (!forwarded) {
                throw VulException("SERVICE forward declaration for '" + entry.first + "' has no matching implementation");
            }
            // A boundary service can be implemented by CONNECT_S_CS instead of a body.
            instance.services[entry.first] = entry.second.signature;
        }
    }

    // queries
    for (const auto &query : temp.queries) {
        VulErrorContextGuard _err{"Processing query '" + query.name + "'"};
        instance.queries[query.name] = staticalizeQuery(query, local_config_lib);
        VulLogicBlock lb;
        lb.block_id = next_logic_block_id++;
        lb.with_priority = false;
        lb.priority = 0;
        lb.codelines = query.codelines;
        lb.codelines_debug = query.codelines_debug;
        instance.query_logic_blocks[query.name] = lb;
    }

    // registers
    for (const auto &reg : temp.registers) {
        VulErrorContextGuard _err{"Processing register '" + reg.name + "'"};
        VulStaticRegister static_reg;
        static_reg.name = reg.name;
        static_reg.signature = parseTypeSignature(reg.type, local_config_lib);
        static_reg.ports = 1;
        if (!reg.portnum.empty()) {
            ConfigRealValue portnum_value = calculateConstexprValue(reg.portnum, local_config_lib);
            static_reg.ports = (portnum_value > 1) ? portnum_value : 1;
        }
        static_reg.reset_codelines = reg.reset_codelines;
        static_reg.reset_codelines_debug = reg.reset_codelines_debug;
        for (const auto &dim : reg.dims) {
            ConfigRealValue dim_value = calculateConstexprValue(dim, local_config_lib);
            static_reg.dims.push_back(dim_value);
        }
        instance.registers.push_back(std::move(static_reg));
    }

    // wires
    for (const auto &wire : temp.wires) {
        VulErrorContextGuard _err{"Processing wire '" + wire.name + "'"};
        VulStaticWire static_wire;
        static_wire.name = wire.name;
        static_wire.signature = parseTypeSignature(wire.type, local_config_lib);
        static_wire.reset_codelines = wire.reset_codelines;
        static_wire.reset_codelines_debug = wire.reset_codelines_debug;
        instance.wires.push_back(std::move(static_wire));
    }

    auto log2ceil = [](uint64_t x) -> uint64_t {
        if (x == 0) return 0;
        uint64_t power = 1;
        uint64_t exp = 0;
        while (power < x) {
            power <<= 1;
            ++exp;
        }
        return exp;
    };

    // brams
    for (const auto &bram : temp.brams) {
        VulErrorContextGuard _err{"Processing BRAM '" + bram.name + "'"};
        VulStaticBRAM static_bram;
        static_bram.name = bram.name;
        static_bram.data_type = parseTypeSignature(bram.data_type, local_config_lib);
        static_bram.addr_size = calculateConstexprValue(bram.addr_size, local_config_lib);
        if (static_bram.addr_size <= 1) {
            throw VulException("BRAM addr_size must be greater than 1");
        }
        static_bram.addr_width = static_cast<ConfigRealValue>(log2ceil(static_cast<uint64_t>(static_bram.addr_size)));
        if (bram.read_ports.empty() || bram.write_ports.empty()) {
            static_bram.read_ports = 0;
            static_bram.write_ports = 0;
        } else {
            static_bram.read_ports = calculateConstexprValue(bram.read_ports, local_config_lib);
            static_bram.write_ports = calculateConstexprValue(bram.write_ports, local_config_lib);
        }
        instance.brams.push_back(std::move(static_bram));
    }

    // digital ROMs
    for (const auto &rom : temp.roms) {
        VulErrorContextGuard _err{"Processing ROM '" + rom.name + "'"};
        VulStaticDigitalROM static_rom;
        static_rom.name = rom.name;
        static_rom.data_width = calculateConstexprValue(rom.data_width, local_config_lib);
        static_rom.addr_size = calculateConstexprValue(rom.addr_size, local_config_lib);
        if (static_rom.addr_size <= 1) {
            throw VulException("ROM addr_size must be greater than 1");
        }
        static_rom.addr_width = static_cast<ConfigRealValue>(log2ceil(static_cast<uint64_t>(static_rom.addr_size)));
        static_rom.read_ports = calculateConstexprValue(rom.read_ports, local_config_lib);
        static_rom.init_path = rom.init_path;
        instance.roms.push_back(std::move(static_rom));
    }

    // queues
    for (const auto &queue : temp.queues) {
        VulErrorContextGuard _err{"Processing queue '" + queue.name + "'"};
        VulStaticQueue static_queue;
        static_queue.name = queue.name;
        static_queue.type = parseTypeSignature(queue.type, local_config_lib);
        static_queue.depth = calculateConstexprValue(queue.depth, local_config_lib);
        static_queue.enq_width = calculateConstexprValue(queue.enq_width, local_config_lib);
        static_queue.deq_width = calculateConstexprValue(queue.deq_width, local_config_lib);
        instance.queues.push_back(std::move(static_queue));
    }

    // instances
    for (const auto &inst : temp.instances) {
        VulErrorContextGuard _err{"Processing instance '" + inst.name + "'"};
        VulStaticInstanceDecl static_inst;
        static_inst.name = inst.name;
        static_inst.module_name = inst.module_name;
        for (const auto &dim_expr : inst.array_dims) {
            ConfigRealValue dim_value = calculateConstexprValue(dim_expr, local_config_lib);
            if (dim_value <= 0) {
                throw VulException("Child instance array dimension must be positive for '" + inst.name + "'");
            }
            static_inst.array_dims.push_back(dim_value);
        }
        std::unordered_set<string> bound_names;
        for (const auto &[dim_expr, name] : inst.coordinate_bindings) {
            const auto dim = calculateConstexprValue(dim_expr, local_config_lib);
            if (dim < 0 || dim >= static_cast<ConfigRealValue>(static_inst.array_dims.size()))
                throw VulException("COORD dimension out of range for '" + inst.name + "'");
            if (!static_inst.coordinate_bindings.emplace(static_cast<uint32_t>(dim), name).second || !bound_names.insert(name).second)
                throw VulException("Duplicate COORD binding for '" + inst.name + "'");
        }
        for (const auto &param_override : inst.parameter_overrides) {
            const string &param_name = param_override.first;
            const string &param_value_str = param_override.second;
            if (bound_names.contains(param_name))
                throw VulException("COORD-bound parameter cannot be overridden: " + param_name);
            static_inst.parameter_expressions[param_name] = param_value_str;
            ConfigRealValue param_value = calculateConstexprValue(param_value_str, local_config_lib);
            static_inst.parameter_overrides[param_name] = param_value;
        }
        instance.instances[inst.name] = static_inst;
    }

    // tick blocks
    for (size_t tb_idx = 0; tb_idx < temp.tick_blocks.size(); ++tb_idx) {
        const auto &tb = temp.tick_blocks[tb_idx];
        VulErrorContextGuard _err{"Processing tick block"};
        VulTickBlock tick_block;
        tick_block.codelines = tb;
        if (tb_idx < temp.tick_blocks_debug.size()) {
            tick_block.codelines_debug = temp.tick_blocks_debug[tb_idx];
        }
        instance.tick_blocks.push_back(std::move(tick_block));
    }

    instance.connection_config = local_config_lib;

    // req-serv connections
    instance.req_connections.clear();
    for (const auto &temp_conn : temp.req_connections) {
        VulReqServConnection conn = temp_conn;
        const ParsedInstanceExpr req_expr = parseInstanceExpr(temp_conn.req_instance);
        const ParsedInstanceExpr serv_expr = parseInstanceExpr(temp_conn.serv_instance);
        const bool req_is_top = temp_conn.req_instance.empty();
        const bool serv_is_top = temp_conn.serv_instance.empty();

        conn.req_instance_base = req_expr.base_name;
        conn.req_indices = parseConnIndices(req_expr);
        conn.serv_instance_base = serv_expr.base_name;
        conn.serv_indices = parseConnIndices(serv_expr);

        auto validate_child_ref = [&](const ParsedInstanceExpr &expr, const vector<VulConnIndexExpr> &indices, const string &ctx, bool allow_wildcard) {
            if (expr.base_name.empty()) return;
            const auto &decl = requireInstanceDecl(instance.instances, expr.base_name, ctx);
            if (expr.index_exprs.size() != decl.array_dims.size()) {
                throw VulException("Dimension mismatch for array instance '" + expr.base_name + "'");
            }
            size_t wildcard_count = 0;
            for (const auto &idx : indices) {
                if (idx.kind == VulConnIndexKind::Wildcard) {
                    ++wildcard_count;
                    if (!allow_wildcard) {
                        throw VulException("Wildcard '*' is only allowed in module boundary connections");
                    }
                }
            }
            if (wildcard_count > 1) {
                throw VulException("Only one '*' wildcard is supported in a module boundary connection");
            }
        };

        if (!req_is_top) {
            validate_child_ref(req_expr, conn.req_indices, "processing request side connection", serv_is_top);
        }
        if (!serv_is_top) {
            validate_child_ref(serv_expr, conn.serv_indices, "processing service side connection", req_is_top);
        }

        if (!req_is_top && !serv_is_top) {
            const auto &req_decl = requireInstanceDecl(instance.instances, req_expr.base_name, "checking request side");
            const auto &serv_decl = requireInstanceDecl(instance.instances, serv_expr.base_name, "checking service side");
            vector<ConfigRealValue> loop_dims(2, -1);
            vector<int32_t> source_loop_dim_pos(2, -1);
            for (size_t i = 0; i < conn.req_indices.size(); ++i) {
                const auto &idx = conn.req_indices[i];
                if (idx.kind == VulConnIndexKind::GeneralExpr) {
                    throw VulException(
                        "Loop variables on source instance '" + req_expr.base_name +
                        "' must appear alone in one dimension, such as [$] or [?]"
                    );
                }
                if (idx.kind != VulConnIndexKind::LoopVar) continue;
                if (idx.offset != 0) {
                    throw VulException(
                        "Loop variables on source instance '" + req_expr.base_name +
                        "' cannot use offsets; use [$] or [?] instead of '" + idx.expr + "'"
                    );
                }
                const int32_t loop_dim = idx.loop_dim;
                if (loop_dim < 0 || static_cast<size_t>(loop_dim) >= loop_dims.size()) {
                    throw VulException("Loop variable index out of range in '" + req_expr.base_name + "'");
                }
                if (source_loop_dim_pos[loop_dim] >= 0) {
                    throw VulException("Loop variable appears multiple times on source instance '" + req_expr.base_name + "'");
                }
                source_loop_dim_pos[loop_dim] = static_cast<int32_t>(i);
                loop_dims[loop_dim] = req_decl.array_dims[i];
            }

            auto validate_loop_var_uses = [&](const vector<VulConnIndexExpr> &indices, const string &instance_name) {
                for (const auto &idx : indices) {
                    if (idx.kind == VulConnIndexKind::ConstantExpr || idx.kind == VulConnIndexKind::Wildcard) {
                        continue;
                    }
                    if (idx.kind == VulConnIndexKind::LoopVar) {
                        if (idx.loop_dim < 0 || static_cast<size_t>(idx.loop_dim) >= loop_dims.size() || source_loop_dim_pos[idx.loop_dim] < 0) {
                            throw VulException("Loop variable used in destination instance '" + instance_name + "' is not defined on the source side");
                        }
                        continue;
                    }
                    if (idx.kind == VulConnIndexKind::GeneralExpr) {
                        if (idx.expr.find('$') != string::npos && source_loop_dim_pos[0] < 0) {
                            throw VulException("Destination expression '" + idx.expr + "' uses '$' without a matching source-side [$]");
                        }
                        if (idx.expr.find('?') != string::npos && source_loop_dim_pos[1] < 0) {
                            throw VulException("Destination expression '" + idx.expr + "' uses '?' without a matching source-side [?]");
                        }
                    }
                }
            };
            validate_loop_var_uses(conn.serv_indices, serv_expr.base_name);

            vector<ConfigRealValue> loop_vars(2, 0);
            std::function<void(size_t)> validate_dest_eval = [&](size_t var_id) {
                if (var_id == loop_dims.size()) {
                    for (size_t dim = 0; dim < conn.serv_indices.size(); ++dim) {
                        const auto &idx = conn.serv_indices[dim];
                        if (idx.kind == VulConnIndexKind::Wildcard) {
                            throw VulException("Wildcard '*' is only allowed in module boundary connections");
                        }
                        if (idx.kind == VulConnIndexKind::ConstantExpr || idx.kind == VulConnIndexKind::GeneralExpr) {
                            (void)evalExprWithLoopVars(idx.expr, local_config_lib, loop_vars);
                        }
                    }
                    return;
                }
                if (source_loop_dim_pos[var_id] < 0) {
                    validate_dest_eval(var_id + 1);
                    return;
                }
                for (ConfigRealValue idx = 0; idx < loop_dims[var_id]; ++idx) {
                    loop_vars[var_id] = idx;
                    validate_dest_eval(var_id + 1);
                }
            };
            validate_dest_eval(0);
        } else {
            const ParsedInstanceExpr &child_expr = req_is_top ? serv_expr : req_expr;
            const vector<VulConnIndexExpr> &child_indices = req_is_top ? conn.serv_indices : conn.req_indices;
            if (!child_expr.base_name.empty()) {
                const auto &child_decl = requireInstanceDecl(instance.instances, child_expr.base_name, "checking module boundary connection");
                if (child_decl.isArrayed() && child_indices.empty()) {
                    throw VulException("Array child instance '" + child_expr.base_name + "' requires explicit indices in connection");
                }
                for (const auto &idx : child_indices) {
                    if (idx.kind == VulConnIndexKind::LoopVar || idx.kind == VulConnIndexKind::GeneralExpr) {
                        throw VulException("Loop variables are only allowed in internal array connection rules");
                    }
                }
            }
        }

        instance.req_connections.push_back(std::move(conn));
    }

    // child service uses
    auto check_child_alias_array_size = [&](const string &macro_name,
                                            const string &alias_name,
                                            const string &array_expr,
                                            ConfigRealValue expected_size) {
        if (array_expr.empty()) {
            return;
        }
        ConfigRealValue explicit_size = calculateConstexprValue(array_expr, local_config_lib);
        if (explicit_size <= 0) {
            throw VulException(macro_name + " array size for alias '" + alias_name + "' must be positive");
        }
        if (explicit_size != expected_size) {
            throw VulException(macro_name + " array size for alias '" + alias_name +
                               "' does not match wildcard instance dimension");
        }
    };
    auto reject_child_alias_array_size = [&](const string &macro_name,
                                             const string &alias_name,
                                             const string &array_expr) {
        if (!array_expr.empty()) {
            throw VulException(macro_name + " array=<N> for alias '" + alias_name +
                               "' requires exactly one '*' wildcard in instance expression");
        }
    };

    instance.child_service_uses.clear();
    for (const auto &temp_use : temp.child_service_uses) {
        const ParsedInstanceExpr child_expr = parseInstanceExpr(temp_use.instance_expr);
        const auto &child_decl = requireInstanceDecl(instance.instances, child_expr.base_name, "processing USE_CHILD_SERVICE_PORT");
        if (child_expr.index_exprs.empty()) {
            if (child_decl.isArrayed()) {
                throw VulException("Array child instance '" + child_expr.base_name + "' requires explicit indices in USE_CHILD_SERVICE_PORT");
            }
            reject_child_alias_array_size("USE_CHILD_SERVICE", temp_use.alias_name, temp_use.array_size);
            VulStaticChildServiceUse use;
            use.alias_name = temp_use.alias_name;
            use.instance_name = child_expr.base_name;
            use.service_name = temp_use.service_name;
            instance.child_service_uses.push_back(std::move(use));
            continue;
        }
        if (child_expr.index_exprs.size() != child_decl.array_dims.size()) {
            throw VulException("Dimension mismatch for child instance '" + child_expr.base_name + "' in USE_CHILD_SERVICE_PORT");
        }
        vector<size_t> wildcard_dims = collectWildcardDims(child_expr);
        if (wildcard_dims.size() > 1) {
            throw VulException("Only one '*' wildcard is supported in USE_CHILD_SERVICE_PORT");
        }
        if (wildcard_dims.empty()) {
            string concrete_name;
            if (!tryResolveConcreteInstance(child_expr, child_decl, local_config_lib, {}, concrete_name)) {
                throw VulException("Indexed child instance out of range in USE_CHILD_SERVICE_PORT");
            }
            reject_child_alias_array_size("USE_CHILD_SERVICE", temp_use.alias_name, temp_use.array_size);
            VulStaticChildServiceUse use;
            use.alias_name = temp_use.alias_name;
            use.instance_name = concrete_name;
            use.service_name = temp_use.service_name;
            instance.child_service_uses.push_back(std::move(use));
            continue;
        }

        const size_t wildcard_dim = wildcard_dims[0];
        check_child_alias_array_size("USE_CHILD_SERVICE", temp_use.alias_name,
                                     temp_use.array_size, child_decl.array_dims[wildcard_dim]);
        for (ConfigRealValue wildcard_idx = 0; wildcard_idx < child_decl.array_dims[wildcard_dim]; ++wildcard_idx) {
            vector<ConfigRealValue> concrete_indices;
            concrete_indices.reserve(child_expr.index_exprs.size());
            bool valid = true;
            for (size_t dim = 0; dim < child_expr.index_exprs.size(); ++dim) {
                if (dim == wildcard_dim && stringop::trim(child_expr.index_exprs[dim]) == "*") {
                    concrete_indices.push_back(wildcard_idx);
                    continue;
                }
                ConfigRealValue idx = calculateConstexprValue(child_expr.index_exprs[dim], local_config_lib);
                if (idx < 0 || idx >= child_decl.array_dims[dim]) {
                    valid = false;
                    break;
                }
                concrete_indices.push_back(idx);
            }
            if (!valid) {
                continue;
            }
            VulStaticChildServiceUse use;
            use.alias_name = temp_use.alias_name;
            use.instance_name = arrayInstanceName(child_expr.base_name, concrete_indices);
            use.service_name = temp_use.service_name;
            use.alias_indexed = true;
            use.alias_index = wildcard_idx;
            instance.child_service_uses.push_back(std::move(use));
        }
    }

    // child query uses
    instance.child_query_uses.clear();
    for (const auto &temp_use : temp.child_query_uses) {
        const ParsedInstanceExpr child_expr = parseInstanceExpr(temp_use.instance_expr);
        const auto &child_decl = requireInstanceDecl(instance.instances, child_expr.base_name, "processing USE_CHILD_QUERY");
        const VulStaticTypeSignature declared_type = parseTypeSignature(temp_use.ret_type, local_config_lib);
        auto materialize_use = [&](const string &concrete_name, ConfigRealValue alias_index, bool alias_indexed) {
            VulStaticChildQueryUse use;
            use.alias_name = temp_use.alias_name;
            use.instance_name = concrete_name;
            use.query_name = temp_use.query_name;
            use.ret_type = declared_type;
            use.alias_indexed = alias_indexed;
            use.alias_index = alias_index;
            instance.child_query_uses.push_back(std::move(use));
        };

        if (child_expr.index_exprs.empty()) {
            if (child_decl.isArrayed()) {
                throw VulException("Array child instance '" + child_expr.base_name + "' requires explicit indices in USE_CHILD_QUERY");
            }
            reject_child_alias_array_size("USE_CHILD_QUERY", temp_use.alias_name, temp_use.array_size);
            materialize_use(child_expr.base_name, 0, false);
            continue;
        }
        if (child_expr.index_exprs.size() != child_decl.array_dims.size()) {
            throw VulException("Dimension mismatch for child instance '" + child_expr.base_name + "' in USE_CHILD_QUERY");
        }
        vector<size_t> wildcard_dims = collectWildcardDims(child_expr);
        if (wildcard_dims.size() > 1) {
            throw VulException("Only one '*' wildcard is supported in USE_CHILD_QUERY");
        }
        if (wildcard_dims.empty()) {
            string concrete_name;
            if (!tryResolveConcreteInstance(child_expr, child_decl, local_config_lib, {}, concrete_name)) {
                throw VulException("Indexed child instance out of range in USE_CHILD_QUERY");
            }
            reject_child_alias_array_size("USE_CHILD_QUERY", temp_use.alias_name, temp_use.array_size);
            materialize_use(concrete_name, 0, false);
            continue;
        }

        const size_t wildcard_dim = wildcard_dims[0];
        check_child_alias_array_size("USE_CHILD_QUERY", temp_use.alias_name,
                                     temp_use.array_size, child_decl.array_dims[wildcard_dim]);
        for (ConfigRealValue wildcard_idx = 0; wildcard_idx < child_decl.array_dims[wildcard_dim]; ++wildcard_idx) {
            vector<ConfigRealValue> concrete_indices;
            concrete_indices.reserve(child_expr.index_exprs.size());
            bool valid = true;
            for (size_t dim = 0; dim < child_expr.index_exprs.size(); ++dim) {
                if (dim == wildcard_dim && stringop::trim(child_expr.index_exprs[dim]) == "*") {
                    concrete_indices.push_back(wildcard_idx);
                    continue;
                }
                ConfigRealValue idx = calculateConstexprValue(child_expr.index_exprs[dim], local_config_lib);
                if (idx < 0 || idx >= child_decl.array_dims[dim]) {
                    valid = false;
                    break;
                }
                concrete_indices.push_back(idx);
            }
            if (!valid) {
                continue;
            }
            string concrete_name = arrayInstanceName(child_expr.base_name, concrete_indices);
            materialize_use(concrete_name, wildcard_idx, true);
        }
    }

    // helper codes
    instance.helper_codes = temp.helper_codes;
    instance.helper_codes_debug = temp.helper_codes_debug;
}

void detectRequestCallInLogicBlocks(VulStaticModuleInstance &module_instance) {
    vector<std::pair<string, LogicBlockCall>> valid_function_names;
    for (const auto &req_entry : module_instance.requests) {
        LogicBlockCall call;
        call.instance = "";
        call.port = req_entry.first;
        valid_function_names.push_back({req_entry.first, call});
    }
    for (const auto &use : module_instance.child_service_uses) {
        LogicBlockCall call;
        call.instance = use.instance_name;
        call.port = use.service_name;
        valid_function_names.push_back({use.alias_name, call});
    }
    for (auto &serv_entry: module_instance.serv_logic_blocks) {
        auto &serv_lb = serv_entry.second;
        for (auto &func_entry : valid_function_names) {
            if (cppparse::codeblockContainsFunctionCall(serv_lb.codelines, func_entry.first)) {
                serv_lb.call_requests.push_back(func_entry.second);
            }
        }
    }
    for (auto &tb: module_instance.tick_blocks) {
        for (auto &func_entry : valid_function_names) {
            if (cppparse::codeblockContainsFunctionCall(tb.codelines, func_entry.first)) {
                tb.call_requests.push_back(func_entry.second);
            }
        }
    }
}

namespace {
string joinCode(const vector<string> &lines) {
    string out;
    const auto stripped = cppparse::stripComments(lines);
    uint32_t line_number = 1;
    for (size_t i = 0; i < stripped.lines.size(); ++i) {
        while (line_number < stripped.mapping[i]) { out += "\n"; ++line_number; }
        out += stripped.lines[i] + "\n"; ++line_number;
    }
    return out;
}
size_t balancedEnd(const string &text, size_t start, char open, char close) {
    int depth = 0;
    bool quoted = false; char quote = 0;
    for (size_t i = start; i < text.size(); ++i) {
        if (quoted) { if (text[i] == '\\') ++i; else if (text[i] == quote) quoted = false; continue; }
        if (text[i] == '"' || text[i] == '\'') { quoted = true; quote = text[i]; continue; }
        if (text[i] == open) ++depth;
        if (text[i] == close && --depth == 0) return i;
    }
    throw VulException("Unbalanced constexpr conditional in transaction analysis");
}
string maskQuotedText(string text) {
    // Ignore names in literal text; retain offsets for source diagnostics.
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '"' && text[i] != '\'') continue;
        const char quote = text[i]; text[i++] = ' ';
        for (; i < text.size(); ++i) {
            const char c = text[i]; text[i] = c == '\n' ? '\n' : ' ';
            if (c == '\\') { if (++i < text.size()) text[i] = ' '; }
            else if (c == quote) break;
        }
    }
    return text;
}
vector<std::pair<size_t, string>> transactionCallIndices(string text, const string &name) {
    text = maskQuotedText(std::move(text));
    vector<std::pair<size_t, string>> out;
    const std::regex identifier("\\b" + name + "\\b");
    for (std::sregex_iterator it(text.begin(), text.end(), identifier), end; it != end; ++it) {
        size_t position = it->position() + it->length();
        position = text.find_first_not_of(" \t\r\n", position);
        if (position == string::npos) continue;
        string expression;
        if (text[position] == '<') {
            const size_t begin = ++position;
            int parens = 0;
            for (; position < text.size(); ++position) {
                if (text[position] == '(') ++parens;
                else if (text[position] == ')') --parens;
                else if (text[position] == '>' && parens == 0) break;
            }
            if (position == text.size()) continue;
            expression = text.substr(begin, position - begin);
            position = text.find_first_not_of(" \t\r\n", position + 1);
        }
        if (position < text.size() && text[position] == '(') out.push_back({it->position(), expression});
    }
    return out;
}

bool keywordAt(const string &text, size_t position, const string &keyword) {
    return text.compare(position, keyword.size(), keyword) == 0 &&
        (position + keyword.size() == text.size() || !isIdentChar(text[position + keyword.size()]));
}
size_t statementEnd(const string &text, size_t position) {
    position = text.find_first_not_of(" \t\r\n", position);
    if (position == string::npos) throw VulException("Missing constexpr statement body");
    if (text[position] == '{') return balancedEnd(text, position, '{', '}') + 1;
    if (keywordAt(text, position, "if")) {
        const size_t open = text.find('(', position + 2);
        if (open == string::npos) throw VulException("Invalid conditional statement");
        size_t end = statementEnd(text, balancedEnd(text, open, '(', ')') + 1);
        size_t next = text.find_first_not_of(" \t\r\n", end);
        if (next != string::npos && keywordAt(text, next, "else")) end = statementEnd(text, next + 4);
        return end;
    }
    if (keywordAt(text, position, "for") || keywordAt(text, position, "while")) {
        const size_t open = text.find('(', position);
        return statementEnd(text, balancedEnd(text, open, '(', ')') + 1);
    }
    for (size_t i = position; i < text.size(); ++i) {
        if (text[i] == '(') i = balancedEnd(text, i, '(', ')');
        else if (text[i] == '{') i = balancedEnd(text, i, '{', '}');
        else if (text[i] == '[') i = balancedEnd(text, i, '[', ']');
        else if (text[i] == ';') return i + 1;
    }
    throw VulException("Unterminated constexpr statement");
}
string activeConstexprCode(string text, const VulStaticConfigLib &config) {
    const std::regex pattern(R"(\bif\s+constexpr\s*\()");
    const string searchable = maskQuotedText(text);
    std::smatch match;
    size_t search = 0;
    while (search < text.size()) {
        const string tail = searchable.substr(search);
        if (!std::regex_search(tail, match, pattern)) break;
        const size_t begin = search + match.position();
        const size_t paren = begin + match.length() - 1;
        const size_t endparen = balancedEnd(text, paren, '(', ')');
        const size_t body = text.find_first_not_of(" \t\r\n", endparen + 1);
        const size_t body_end = statementEnd(text, body);
        size_t end = body_end;
        size_t other = end;
        size_t next = text.find_first_not_of(" \t\r\n", end);
        if (next != string::npos && keywordAt(text, next, "else")) {
            other = text.find_first_not_of(" \t\r\n", next + 4);
            end = statementEnd(text, other);
        }
        const bool enabled = calculateConstexprValue(text.substr(paren + 1, endparen - paren - 1), config) != 0;
        size_t selected = enabled ? body : other;
        size_t selected_end = enabled ? body_end : end;
        if (selected < selected_end && text[selected] == '{') { ++selected; --selected_end; }
        string chosen = activeConstexprCode(text.substr(selected, selected_end - selected), config);
        string replacement = text.substr(begin, end - begin);
        for (auto &c : replacement) if (c != '\n') c = ' ';
        replacement.replace(selected - begin, chosen.size(), chosen);
        text.replace(begin, end - begin, replacement);
        search = end;
    }
    return text;
}
}

void setupUpdateSequence(shared_ptr<VulStaticModuleInstance> &top) {
    VulErrorContextGuard top_guard("Setting up update sequence for instance '" + top->simClassName() + "'");
    unordered_map<VulInstanceID, shared_ptr<VulStaticModuleInstance>> instance_id_map;
    std::deque<shared_ptr<VulStaticModuleInstance>> bfs_queue{top};
    struct ServiceNode { string port; uint32_t index; const VulLogicBlock *logic; };
    unordered_map<uint64_t, ServiceNode> service_nodes;
    unordered_map<uint64_t, string> node_names;
    std::map<std::tuple<VulInstanceID, string, uint32_t>, uint64_t> endpoint_ids;
    while (!bfs_queue.empty()) {
        auto inst = bfs_queue.front(); bfs_queue.pop_front();
        instance_id_map[inst->instance_id] = inst;
        for (const auto &child : inst->children) bfs_queue.push_back(child);
        const string path = inst->concatInstancePath("::", true);
        node_names[uint64_t(inst->instance_id) << 32] = path + ".<tick>";
        uint32_t block = 1;
        for (const auto &[name, logic] : inst->serv_logic_blocks) {
            auto decl = inst->services.find(name);
            const uint32_t count = decl == inst->services.end() ? 1 : decl->second.array_size;
            for (uint32_t idx = 0; idx < count; ++idx) {
                const uint64_t id = (uint64_t(inst->instance_id) << 32) | block++;
                endpoint_ids[{inst->instance_id, name, idx}] = id;
                service_nodes[id] = {name, idx, &logic};
                node_names[id] = path + "." + name + (count > 1 ? "[" + std::to_string(idx) + "]" : "");
            }
        }
    }
    auto debug_lb_name_by_id = [&](uint64_t id) { return node_names.at(id); };
    auto childByName = [](const shared_ptr<VulStaticModuleInstance> &parent, const string &name) {
        for (const auto &child : parent->children) if (child->instance_path.back() == name) return child;
        throw VulException("Concrete child instance not found: " + name);
    };
    std::function<vector<uint64_t>(shared_ptr<VulStaticModuleInstance>, string, uint32_t, bool, std::set<string>)> resolve;
    resolve = [&](auto inst, string port, uint32_t index, bool service, std::set<string> path) -> vector<uint64_t> {
        const string key = std::to_string(inst->instance_id) + ":" + port + ":" + std::to_string(index) + (service ? ":S" : ":R");
        if (!path.insert(key).second) throw VulException("Cyclic transaction forwarding at " + inst->concatInstancePath("::", true) + "." + port);
        const auto &ports = service ? inst->services : inst->requests;
        auto decl = ports.find(port);
        if (decl == ports.end() || index >= decl->second.array_size)
            throw VulException("Invalid transaction endpoint: " + inst->concatInstancePath("::", true) + "." + port + "[" + std::to_string(index) + "]");
        if (service) {
            auto found = endpoint_ids.find({inst->instance_id, port, index});
            if (found != endpoint_ids.end()) return {found->second};
        }
        auto scope = service ? inst : inst->parent;
        if (!scope) throw VulException("Unconnected root request: " + port);
        vector<uint64_t> out;
        for (const auto &conn : scope->concrete_connections) {
            if (conn.req_instance != (service ? "" : inst->instance_path.back()) || conn.req_name != port) continue;
            if (conn.req_port_index >= 0 && uint32_t(conn.req_port_index) != index) continue;
            auto destination = conn.serv_instance.empty() ? scope : childByName(scope, conn.serv_instance);
            bool next_service = !conn.serv_instance.empty() || !scope->requests.contains(conn.serv_name);
            const auto &dstports = next_service ? destination->services : destination->requests;
            auto dstdecl = dstports.find(conn.serv_name);
            if (dstdecl == dstports.end()) throw VulException("Missing destination transaction: " + conn.serv_name);
            const uint32_t dstindex = conn.serv_port_index >= 0 ? conn.serv_port_index : (dstdecl->second.is_arrayed ? index : 0);
            auto targets = resolve(destination, conn.serv_name, dstindex, next_service, path);
            out.insert(out.end(), targets.begin(), targets.end());
        }
        if (out.empty()) throw VulException("No connected service found for " + inst->concatInstancePath("::", true) + "." + port);
        return out;
    };
    unordered_map<uint64_t, vector<uint64_t>> logic_block_call_graph;
    auto scan = [&](const shared_ptr<VulStaticModuleInstance> &inst, uint64_t source, const vector<string> &lines, uint32_t slot, const VulDebugLocs &locations) {
        auto config = inst->connection_config;
        config["IDX"] = slot;
        const string text = activeConstexprCode(joinCode(lines), config);
        vector<string> names;
        for (const auto &[name, _] : inst->requests) names.push_back(name);
        for (const auto &use : inst->child_service_uses)
            if (std::find(names.begin(), names.end(), use.alias_name) == names.end()) names.push_back(use.alias_name);
        for (const auto &name : names) {
            for (const auto &[offset, expression] : transactionCallIndices(text, name)) {
                LogicBlockCall call;
                call.port = name;
                call.index_expression = expression;
                const size_t line_index = std::count(text.begin(), text.begin() + offset, '\n');
                if (line_index < locations.size()) call.source_location = locations[line_index];
                const string location = call.source_location.valid() ? " at " + call.source_location.file + ":" + std::to_string(call.source_location.line) : "";
                VulErrorContextGuard call_guard("Analyzing transaction call '" + name + "' in " + inst->concatInstancePath("::", true) + location);
                uint32_t idx = 0;
                if (!call.index_expression.empty()) {
                    auto value = calculateConstexprValue(call.index_expression, config);
                    if (value < 0 || value > UINT32_MAX) throw VulException("Transaction index out of range: " + name);
                    idx = value;
                }
                vector<uint64_t> targets;
                if (inst->requests.contains(name)) targets = resolve(inst, name, idx, false, {});
                else {
                    bool found = false;
                    for (const auto &use : inst->child_service_uses) {
                        if (use.alias_name != name || (use.alias_indexed && use.alias_index != idx)) continue;
                        auto child = childByName(inst, use.instance_name);
                        auto result = resolve(child, use.service_name, use.alias_indexed ? 0 : idx, true, {});
                        targets.insert(targets.end(), result.begin(), result.end()); found = true;
                    }
                    if (!found) throw VulException("Child service alias index out of range: " + name);
                }
                auto &edges = logic_block_call_graph[source];
                edges.insert(edges.end(), targets.begin(), targets.end());
            }
        }
    };
    for (const auto &[id, inst] : instance_id_map) {
        for (const auto &tick : inst->tick_blocks) {
            const uint64_t source = uint64_t(id) << 32;
            scan(inst, source, tick.codelines, 0, tick.codelines_debug);
            // TestMain's externally callable services have synthetic callers.
            if (inst->module_name == "TestMain") for (const auto &call : tick.call_requests) {
                for (uint32_t idx = 0; idx < inst->requests.at(call.port).array_size; ++idx) {
                    auto result = resolve(inst, call.port, idx, false, {});
                    logic_block_call_graph[source].insert(logic_block_call_graph[source].end(), result.begin(), result.end());
                }
            }
        }
    }
    for (const auto &[id, node] : service_nodes) scan(instance_id_map.at(id >> 32), id, node.logic->codelines, node.index, node.logic->codelines_debug);
    unordered_map<VulInstanceID, vector<uint64_t>> instance_tick_to_lb_call_graph;
    unordered_map<uint64_t, VulInstanceID> logic_block_id_to_instance_id;
    for (const auto &[instid, inst] : instance_id_map) {
        std::set<uint64_t> active, visited;
        vector<uint64_t> path;
        std::function<void(uint64_t)> visit = [&](uint64_t id) {
            path.push_back(id);
            if (active.contains(id) || visited.contains(id)) {
                string diagnostic;
                for (auto p : path) { if (!diagnostic.empty()) diagnostic += " -> "; diagnostic += debug_lb_name_by_id(p); }
                throw VulException(string(active.contains(id) ? "Cyclic call" : "Repeated call") + " detected in logic block call graph: " + diagnostic);
            }
            active.insert(id); visited.insert(id);
            if ((id & 0xFFFFFFFF) != 0) {
                instance_tick_to_lb_call_graph[instid].push_back(id);
                auto [previous, inserted] = logic_block_id_to_instance_id.emplace(id, instid);
                if (!inserted && previous->second != instid)
                    throw VulException("Logic block '" + debug_lb_name_by_id(id) + "' is called by multiple instances: '" + debug_lb_name_by_id(uint64_t(previous->second) << 32) + "' and '" + debug_lb_name_by_id(uint64_t(instid) << 32) + "'.");
            }
            for (auto target : logic_block_call_graph[id]) visit(target);
            active.erase(id); path.pop_back();
        };
        visit(uint64_t(instid) << 32);
    }

    VulErrorContextGuard order_guard("Determining instance update order");

    unordered_map<VulInstanceID, unordered_set<VulInstanceID>> instance_order_graph;
    bfs_queue.clear();
    bfs_queue.push_back(top);
    while (!bfs_queue.empty()) {
        auto cur_inst = bfs_queue.front();
        bfs_queue.pop_front();
        VulInstanceID cur_inst_id = cur_inst->instance_id;
        for (const auto &child : cur_inst->children) bfs_queue.push_back(child);
        map<int32_t, vector<VulInstanceID>> priority_to_callee_instances;
        priority_to_callee_instances[0].push_back(cur_inst_id); // tick block has default priority 0

        VulErrorContextGuard inst_guard("Processing instance '" + cur_inst->concatInstancePath("::", true) + "' (IID: " + std::to_string(cur_inst_id) + ") for update order");

        for (const auto &[lb_id, node] : service_nodes) {
            if ((lb_id >> 32) != cur_inst_id) continue;
            const auto &serv_lb = *node.logic;
            if (!serv_lb.with_priority) continue;
            auto caller = logic_block_id_to_instance_id.find(lb_id);
            if (caller == logic_block_id_to_instance_id.end()) continue;
            const auto caller_id = caller->second;
            if (caller_id == cur_inst_id)
                throw VulException("Logic block '" + debug_lb_name_by_id(lb_id) + "' is called by its own tick block.");
            priority_to_callee_instances[serv_lb.priority].push_back(caller_id);
        }
        
        unordered_set<VulInstanceID> higher_priority_instance_set;
        vector<VulInstanceID> higher_priority_instances;
        for (auto prio_it = priority_to_callee_instances.rbegin(); prio_it != priority_to_callee_instances.rend(); ++prio_it) {
            auto &same_priority_instances = prio_it->second;
            if (same_priority_instances.empty()) {
                continue;
            }
            std::sort(same_priority_instances.begin(), same_priority_instances.end());
            same_priority_instances.erase(std::unique(same_priority_instances.begin(), same_priority_instances.end()), same_priority_instances.end());

            if (!higher_priority_instances.empty()) {
                for (VulInstanceID from_inst_id : higher_priority_instances) {
                    auto &out_edges = instance_order_graph[from_inst_id];
                    for (VulInstanceID to_inst_id : same_priority_instances) {
                        if (from_inst_id != to_inst_id) {
                            out_edges.insert(to_inst_id);
                        }
                    }
                }
            }

            for (VulInstanceID inst_id : same_priority_instances) {
                if (higher_priority_instance_set.insert(inst_id).second) {
                    higher_priority_instances.push_back(inst_id);
                }
            }
        }
    }

    unordered_map<VulInstanceID, unordered_map<VulInstanceID, unordered_set<VulInstanceID>>> child_instance_order_graph; // instance_id -> child_instance_id -> set of child_instance_id that should be updated after this child_instance_id
    for (const auto &entry : instance_order_graph) {
        const auto &former_inst_id = entry.first;
        const auto &latter_inst_ids = entry.second;
        auto former_inst_iter = instance_id_map.find(former_inst_id);
        if (former_inst_iter == instance_id_map.end()) {
            throw VulException("Instance ID " + std::to_string(former_inst_id) + " not found in instance_id_map");
        }
        shared_ptr<VulStaticModuleInstance> former_inst_ptr = former_inst_iter->second;
        for (const auto &latter_inst_id : latter_inst_ids) {
            auto latter_inst_iter = instance_id_map.find(latter_inst_id);
            if (latter_inst_iter == instance_id_map.end()) {
                throw VulException("Instance ID " + std::to_string(latter_inst_id) + " not found in instance_id_map");
            }
            shared_ptr<VulStaticModuleInstance> latter_inst_ptr = latter_inst_iter->second;
            
            size_t former_depth = former_inst_ptr->instance_path.size();
            size_t latter_depth = latter_inst_ptr->instance_path.size();
            shared_ptr<VulStaticModuleInstance> former_ancestor = former_inst_ptr;
            shared_ptr<VulStaticModuleInstance> latter_ancestor = latter_inst_ptr;
            VulInstanceID former_last_id = former_inst_id;
            VulInstanceID latter_last_id = latter_inst_id;

            while (former_depth > latter_depth) {
                former_last_id = former_ancestor->instance_id;
                former_ancestor = former_ancestor->parent;
                --former_depth;
            }
            while (latter_depth > former_depth) {
                latter_last_id = latter_ancestor->instance_id;
                latter_ancestor = latter_ancestor->parent;
                --latter_depth;
            }
            while (former_ancestor.get() != latter_ancestor.get()) {
                if (former_ancestor == nullptr || latter_ancestor == nullptr) {
                    throw VulException("Cannot find common ancestor for instance IDs " + std::to_string(former_inst_id) + " and " + std::to_string(latter_inst_id));
                }
                former_last_id = former_ancestor->instance_id;
                latter_last_id = latter_ancestor->instance_id;
                former_ancestor = former_ancestor->parent;
                latter_ancestor = latter_ancestor->parent;
            }
            if (former_ancestor == nullptr) {
                throw VulException("Cannot find common ancestor for instance IDs " + std::to_string(former_inst_id) + " and " + std::to_string(latter_inst_id));
            }

            shared_ptr<VulStaticModuleInstance> common_ancestor = former_ancestor;
            // An endpoint equal to the LCA denotes that ancestor's local tick.
            // Otherwise it denotes the direct child subtree containing the endpoint.
            auto update_node = [&](shared_ptr<VulStaticModuleInstance> endpoint) {
                if (endpoint == common_ancestor) return endpoint->instance_id;
                while (endpoint->parent != common_ancestor) endpoint = endpoint->parent;
                return endpoint->instance_id;
            };
            former_last_id = update_node(former_inst_ptr);
            latter_last_id = update_node(latter_inst_ptr);
            if (former_last_id != latter_last_id) {
                child_instance_order_graph[common_ancestor->instance_id][former_last_id].insert(latter_last_id);
            }
        }
    }

    bfs_queue.clear();
    bfs_queue.push_back(top);
    while (!bfs_queue.empty()) {
        auto cur_inst = bfs_queue.front();
        bfs_queue.pop_front();

        const VulInstanceID cur_inst_id = cur_inst->instance_id;
        vector<VulInstanceID> update_nodes;
        update_nodes.reserve(cur_inst->children.size() + 1);
        update_nodes.push_back(cur_inst_id); // self ID represents local tick
        for (const auto &child : cur_inst->children) {
            update_nodes.push_back(child->instance_id);
            bfs_queue.push_back(child);
        }

        unordered_map<VulInstanceID, int32_t> indegree;
        indegree.reserve(update_nodes.size() * 2);
        for (VulInstanceID node_id : update_nodes) {
            indegree.emplace(node_id, 0);
        }

        unordered_map<VulInstanceID, vector<VulInstanceID>> adj;
        auto order_it = child_instance_order_graph.find(cur_inst_id);
        if (order_it != child_instance_order_graph.end()) {
            adj.reserve(order_it->second.size());
            for (const auto &from_entry : order_it->second) {
                VulInstanceID from_id = from_entry.first;
                if (indegree.find(from_id) == indegree.end()) {
                    continue;
                }
                auto &out_edges = adj[from_id];
                out_edges.reserve(from_entry.second.size());
                for (VulInstanceID to_id : from_entry.second) {
                    auto indeg_it = indegree.find(to_id);
                    if (indeg_it == indegree.end() || to_id == from_id) {
                        continue;
                    }
                    out_edges.push_back(to_id);
                    indeg_it->second += 1;
                }
            }
        }

        std::priority_queue<VulInstanceID, vector<VulInstanceID>, std::greater<VulInstanceID>> ready;
        for (const auto &entry : indegree) {
            if (entry.second == 0) {
                ready.push(entry.first);
            }
        }

        cur_inst->update_seq.clear();
        cur_inst->update_seq.reserve(update_nodes.size());

        while (!ready.empty()) {
            VulInstanceID from_id = ready.top();
            ready.pop();
            cur_inst->update_seq.push_back(from_id);

            auto adj_it = adj.find(from_id);
            if (adj_it == adj.end()) {
                continue;
            }
            for (VulInstanceID to_id : adj_it->second) {
                auto indeg_it = indegree.find(to_id);
                if (indeg_it == indegree.end()) {
                    continue;
                }
                indeg_it->second -= 1;
                if (indeg_it->second == 0) {
                    ready.push(to_id);
                }
            }
        }

        if (cur_inst->update_seq.size() != update_nodes.size()) {
            std::set<VulInstanceID> blocked;
            string diagnostic = "Cyclic update constraints found in instance update order graph at " + cur_inst->concatInstancePath("::", true) + ":";
            for (auto id : update_nodes) if (indegree.at(id) > 0) {
                blocked.insert(id);
                diagnostic += " " + instance_id_map.at(id)->concatInstancePath("::", true);
            }
            vector<string> calls;
            for (const auto &[id, node] : service_nodes) {
                auto caller = logic_block_id_to_instance_id.find(id);
                if (!node.logic->with_priority || caller == logic_block_id_to_instance_id.end()) continue;
                auto projected = instance_id_map.at(caller->second);
                while (projected && projected != cur_inst && projected->parent != cur_inst) projected = projected->parent;
                if (!projected || !blocked.contains(projected->instance_id)) continue;
                calls.push_back(debug_lb_name_by_id(uint64_t(caller->second) << 32) + " -> " + debug_lb_name_by_id(id)
                    + " (priority=" + std::to_string(node.logic->priority) + ")");
            }
            std::sort(calls.begin(), calls.end());
            for (const auto &call : calls) diagnostic += "\n  " + call;
            throw VulException(diagnostic);
        }
    }

}
