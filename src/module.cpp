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
#include "configexpr.hpp"
#include "toposort.hpp"
#include "cppparse.hpp"
#include "stringop.hpp"
#include <cassert>
#include <functional>
#include <iostream>
#include <optional>

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
    string out = base_name;
    for (ConfigRealValue idx : indices) {
        out += "__" + std::to_string(idx);
    }
    return out;
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

} // namespace

VulStaticReqServ staticalizeReqServ(const VulTempReqServBase &item, const VulStaticConfigLib &config_lib) {
    VulStaticReqServ static_req;
    static_req.name = item.name;
    static_req.is_arrayed = false;
    static_req.array_size = 1;
    static_req.has_handshake = item.has_handshake;
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
    VulLogicBlockID next_logic_block_id = 1;
    for (const auto &serv : temp.services) {
        const string &serv_name = serv.name;
        VulErrorContextGuard _err{"Processing service '" + serv_name + "'"};
        instance.services[serv_name] = staticalizeReqServ(serv, local_config_lib);
        VulLogicBlock lb;
        lb.block_id = next_logic_block_id++;
        lb.with_priority = !serv.priority.empty();
        if (lb.with_priority) {
            ConfigRealValue priority_value = calculateConstexprValue(serv.priority, local_config_lib);
            lb.priority = priority_value;
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
        for (const auto &param_override : inst.parameter_overrides) {
            const string &param_name = param_override.first;
            const string &param_value_str = param_override.second;
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
    instance.child_service_uses.clear();
    for (const auto &temp_use : temp.child_service_uses) {
        const ParsedInstanceExpr child_expr = parseInstanceExpr(temp_use.instance_expr);
        const auto &child_decl = requireInstanceDecl(instance.instances, child_expr.base_name, "processing USE_CHILD_SERVICE_PORT");
        if (child_expr.index_exprs.empty()) {
            if (child_decl.isArrayed()) {
                throw VulException("Array child instance '" + child_expr.base_name + "' requires explicit indices in USE_CHILD_SERVICE_PORT");
            }
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
            VulStaticChildServiceUse use;
            use.alias_name = temp_use.alias_name;
            use.instance_name = concrete_name;
            use.service_name = temp_use.service_name;
            instance.child_service_uses.push_back(std::move(use));
            continue;
        }

        const size_t wildcard_dim = wildcard_dims[0];
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
            materialize_use(concrete_name, 0, false);
            continue;
        }

        const size_t wildcard_dim = wildcard_dims[0];
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

uint64_t findConnectedLogicBlockID(shared_ptr<VulStaticModuleInstance> instance, const LogicBlockCall &call) {
    // 顺着指针关系和req_connections成员找到这个call最终连接到的serv_logic_block的ID，返回这个ID（高32位为instance_id，低32位为block_id）。由于连接的单一性，一个call最多只能连接到一个serv_logic_block，如果找不到连接的serv_logic_block，抛出异常并退出

    auto find_child_ptr_by_name = [](shared_ptr<VulStaticModuleInstance> &mod, const InstanceName &child_name) -> shared_ptr<VulStaticModuleInstance> {
        for (auto &child : mod->children) {
            if (!child->instance_path.empty() && child->instance_path.back() == child_name) {
                return child;
            }
        }
        for (const auto &[inst_name, inst] : mod->instances) {
            if (!inst.isArrayed()) {
                continue;
            }
            if (child_name == inst_name || child_name.rfind(inst_name + "__", 0) == 0) {
                for (auto &child : mod->children) {
                    if (!child->instance_path.empty() && child->instance_path.back() == inst_name) {
                        return child;
                    }
                }
            }
        }
        return nullptr;
    };
    auto child_req_name_match = [](const shared_ptr<VulStaticModuleInstance> &child, const InstanceName &conn_req_instance) -> bool {
        if (conn_req_instance == child->instance_path.back()) {
            return true;
        }
        if (child->parent) {
            auto inst_it = child->parent->instances.find(child->instance_path.back());
            if (inst_it != child->parent->instances.end() && inst_it->second.isArrayed()) {
                return conn_req_instance.rfind(child->instance_path.back() + "__", 0) == 0;
            }
        }
        return false;
    };
    auto pack_lb_id = [](uint32_t instance_id, uint32_t block_id) -> uint64_t {
        return ((uint64_t)instance_id << 32) | block_id;
    };

    shared_ptr<VulStaticModuleInstance> cur = instance;
    ReqServName port;
    bool is_serv_call = false;

    if (call.instance.empty()) {
        // it's a request call
        is_serv_call = false;
        port = call.port;
    } else {
        // it's a service call
        is_serv_call = true;
        port = call.port;
        cur = find_child_ptr_by_name(cur, call.instance);
        if (cur == nullptr) {
            throw VulException("Invalid LogicBlockCall: instance '" + call.instance + "' not found in instance '" + instance->instance_path.back() + "'");
        }
    }

    for (uint32_t hop = 0; hop < 4096; hop ++) {
        VulErrorContextGuard hop_guard("Entering instance '" + cur->simClassName() + "' with port '" + port + "'");
        if (is_serv_call) {
            // impl as code block here, or connected to a child service
            auto lb_iter = cur->serv_logic_blocks.find(port);
            if (lb_iter != cur->serv_logic_blocks.end()) {
                return pack_lb_id(cur->instance_id, lb_iter->second.block_id);
            } else {
                // find connected child service
                bool found_conn = false;
                for (const auto &conn : cur->req_connections) {
                    if (conn.req_instance == "" && conn.req_name == port) {
                        const string &target_name = conn.serv_instance_base.empty() ? conn.serv_instance : conn.serv_instance_base;
                        cur = find_child_ptr_by_name(cur, target_name);
                        if (cur == nullptr) {
                            throw VulException("Invalid Req-Serv connection: instance '" + target_name + "' not found in instance '" + instance->instance_path.back() + "'");
                        }
                        port = conn.serv_name;
                        found_conn = true;
                        break;
                    }
                }
                if (!found_conn) {
                    throw VulException("No connected service found for LogicBlockCall to service '" + port + "' in instance '" + instance->instance_path.back() + "'");
                }
            }
            continue;
        }
        // request call, should be connected at parent level
        auto parent = cur->parent;
        // CR-R: connected to parent's request
        // CR-S: connected to parent's service
        // CR-CS: connected to another child's service
        bool found_conn = false;
        for (const auto &conn : parent->req_connections) {
            const string &req_name_match = conn.req_instance_base.empty() ? conn.req_instance : conn.req_instance_base;
            if (child_req_name_match(cur, req_name_match) && conn.req_name == port) {
                if (conn.serv_instance == "") {
                    // connected to parent's service/request
                    cur = parent;
                    port = conn.serv_name;
                    is_serv_call = (parent->requests.find(conn.serv_name) == parent->requests.end());
                } else {
                    // connected to another child's service
                    const string &target_name = conn.serv_instance_base.empty() ? conn.serv_instance : conn.serv_instance_base;
                    cur = find_child_ptr_by_name(parent, target_name);
                    if (cur == nullptr) {
                        throw VulException("Invalid Req-Serv connection: instance '" + target_name + "' not found in instance '" + parent->instance_path.back() + "'");
                    }
                    port = conn.serv_name;
                    is_serv_call = true;
                }
                found_conn = true;
                break;
            }
        }
        if (!found_conn) {
            throw VulException("No connected service found for LogicBlockCall to request '" + port + "' in instance '" + instance->instance_path.back() + "'");
        }
    }
    throw VulException("Exceeded maximum hop count while finding connected logic block for LogicBlockCall to '" + port + "' in instance '" + instance->instance_path.back() + "'. Possible cyclic connections.");
}

void setupUpdateSequence(shared_ptr<VulStaticModuleInstance> &top) {

    VulErrorContextGuard top_guard("Setting up update sequence for instance '" + top->simClassName() + "'");

    unordered_set<uint64_t> all_logic_block_ids;
    unordered_map<uint64_t, unordered_set<uint64_t>> logic_block_call_graph;
    unordered_map<VulInstanceID, shared_ptr<VulStaticModuleInstance>> instance_id_map;

    std::deque<shared_ptr<VulStaticModuleInstance>> bfs_queue;
    bfs_queue.push_back(top);
    while (!bfs_queue.empty()) {
        auto cur_inst = bfs_queue.front();
        bfs_queue.pop_front();

        instance_id_map[cur_inst->instance_id] = cur_inst;

        VulErrorContextGuard inst_guard("Parsing connection from instance '" + cur_inst->simClassName() + "' (IID: " + std::to_string(cur_inst->instance_id) + ")");

        for (const auto &serv_lb_entry : cur_inst->serv_logic_blocks) {
            const auto &serv_lb = serv_lb_entry.second;
            VulErrorContextGuard lb_guard("Parsing service logic block '" + serv_lb_entry.first + "' (BID: " + std::to_string(serv_lb.block_id) + ")");
            uint64_t lb_id = ((uint64_t)cur_inst->instance_id << 32) | serv_lb.block_id;
            all_logic_block_ids.insert(lb_id);
            for (const auto &req_call : serv_lb.call_requests) {
                uint64_t called_lb_id = findConnectedLogicBlockID(cur_inst, req_call);
                logic_block_call_graph[lb_id].insert(called_lb_id);
            }
        }
        for (const auto &tick_lb : cur_inst->tick_blocks) {
            VulErrorContextGuard lb_guard("Parsing tick logic block (BID: 0)");
            uint64_t lb_id = ((uint64_t)cur_inst->instance_id << 32);
            all_logic_block_ids.insert(lb_id);
            for (const auto &req_call : tick_lb.call_requests) {
                uint64_t called_lb_id = findConnectedLogicBlockID(cur_inst, req_call);
                logic_block_call_graph[lb_id].insert(called_lb_id);
            }
        }
    }

    auto debug_lb_name_by_id = [&](uint64_t lb_id) -> string {
        VulInstanceID inst_id = lb_id >> 32;
        uint32_t block_id = lb_id & 0xFFFFFFFF;
        auto inst_iter = instance_id_map.find(inst_id);
        if (inst_iter == instance_id_map.end()) {
            return "<unknown instance " + std::to_string(inst_id) + ">";
        }
        auto &inst_ptr = inst_iter->second;
        string instance_path_str;
        for (const auto &path_elem : inst_ptr->instance_path) {
            if (!instance_path_str.empty()) instance_path_str += "::";
            instance_path_str += path_elem;
        }
        if (block_id == 0) {
            return instance_path_str + ".<tick>";
        }
        for (auto &serv_lb_entry : inst_ptr->serv_logic_blocks) {
            auto &serv_lb = serv_lb_entry.second;
            if (serv_lb.block_id == block_id) {
                return instance_path_str + "." + serv_lb_entry.first;
            }
        }
        return instance_path_str + ".<unknown>";
    };

    VulErrorContextGuard graph_guard("Checking logic block call graph");
    unordered_map<VulInstanceID, vector<uint64_t>> instance_tick_to_lb_call_graph;
    // logic_block_call_graph 是 {tick_code_blocks, serv_logic_blocks} 之间的调用关系图，instance_tick_to_lb_call_graph 是从实例的 tick_code_block 到被所有可能被递归调用的 logic_block 的调用关系图
    for (const auto &inst_entry : instance_id_map) {
        const auto &inst_id = inst_entry.first;
        const auto &inst_ptr = inst_entry.second;
        uint64_t tick_lb_id = ((uint64_t)inst_id << 32);
        unordered_set<uint64_t> visited;
        struct CallPathNode {
            uint64_t lb_id;
            vector<uint64_t> call_path; // for debugging
        };
        std::deque<CallPathNode> bfs_queue;
        bfs_queue.push_back({tick_lb_id, {tick_lb_id}});
        while (!bfs_queue.empty()) {
            auto [cur_lb_id, call_path] = bfs_queue.front();
            bfs_queue.pop_front();
            if (visited.find(cur_lb_id) != visited.end()) {
                string loop_str;
                for (const auto &lb_id : call_path) {
                    if (!loop_str.empty()) loop_str += " -> ";
                    loop_str += debug_lb_name_by_id(lb_id);
                }
                throw VulException("Cyclic call or repeated call detected in logic block call graph: " + loop_str);
            }
            visited.insert(cur_lb_id);
            const auto &called_lbs_set = logic_block_call_graph.find(cur_lb_id);
            if (called_lbs_set != logic_block_call_graph.end()) {
                for (const auto &called_lb_id : called_lbs_set->second) {
                    vector<uint64_t> new_call_path = call_path;
                    new_call_path.push_back(called_lb_id);
                    bfs_queue.push_back({called_lb_id, new_call_path});
                }
            }
        }
    }

    unordered_map<uint64_t, VulInstanceID> logic_block_id_to_instance_id;
    // 反向映射一下，同时保证每一个serv codeblock仅会被最多一个tick codeblock调用，否则这个重复调用行为在硬件上是无法实现的
    for (const auto &entry : instance_tick_to_lb_call_graph) {
        const auto &inst_id = entry.first;
        const auto &called_lb_ids = entry.second;
        for (const auto &lb_id : called_lb_ids) {
            auto iter = logic_block_id_to_instance_id.find(lb_id);
            if (iter != logic_block_id_to_instance_id.end() && iter->second != inst_id) {
                string called_inst1 = debug_lb_name_by_id(static_cast<uint64_t>(iter->second) << 32);
                string called_inst2 = debug_lb_name_by_id(static_cast<uint64_t>(inst_id) << 32);
                string lb_name = debug_lb_name_by_id(lb_id);
                throw VulException("Logic block '" + lb_name + "' is called by multiple instances: '" + called_inst1 + "' and '" + called_inst2 + "'. This is not allowed because it cannot be implemented in hardware.");
            }
            logic_block_id_to_instance_id[lb_id] = inst_id;
        }
    }

    VulErrorContextGuard order_guard("Determining instance update order");

    unordered_map<VulInstanceID, unordered_set<VulInstanceID>> instance_order_graph;
    bfs_queue.clear();
    bfs_queue.push_back(top);
    while (!bfs_queue.empty()) {
        auto cur_inst = bfs_queue.front();
        bfs_queue.pop_front();
        VulInstanceID cur_inst_id = cur_inst->instance_id;
        map<int32_t, vector<VulInstanceID>> priority_to_callee_instances;
        priority_to_callee_instances[0].push_back(cur_inst_id); // tick block has default priority 0

        VulErrorContextGuard inst_guard("Processing instance '" + cur_inst->simClassName() + "' (IID: " + std::to_string(cur_inst_id) + ") for update order");

        for (const auto &serv_lb_entry : cur_inst->serv_logic_blocks) {
            const auto &serv_lb = serv_lb_entry.second;
            if (!serv_lb.with_priority) {
                continue;
            }
            uint64_t lb_id = ((uint64_t)cur_inst->instance_id << 32) | serv_lb.block_id;
            auto callee_inst_iter = logic_block_id_to_instance_id.find(lb_id);
            if (callee_inst_iter == logic_block_id_to_instance_id.end()) {
                // not called by any tick block, skip
                continue;
            }
            VulInstanceID callee_inst_id = callee_inst_iter->second;
            if (callee_inst_id == cur_inst_id) {
                string lb_name = debug_lb_name_by_id(lb_id);
                throw VulException("Logic block '" + lb_name + "' is called by its own tick block.");
            }
            priority_to_callee_instances[serv_lb.priority].push_back(callee_inst_id);
        }
        
        unordered_set<VulInstanceID> lower_priority_instance_set;
        vector<VulInstanceID> lower_priority_instances;
        for (auto prio_it = priority_to_callee_instances.rbegin(); prio_it != priority_to_callee_instances.rend(); ++prio_it) {
            auto &same_priority_instances = prio_it->second;
            if (same_priority_instances.empty()) {
                continue;
            }
            std::sort(same_priority_instances.begin(), same_priority_instances.end());
            same_priority_instances.erase(std::unique(same_priority_instances.begin(), same_priority_instances.end()), same_priority_instances.end());

            if (!lower_priority_instances.empty()) {
                for (VulInstanceID from_inst_id : same_priority_instances) {
                    auto &out_edges = instance_order_graph[from_inst_id];
                    for (VulInstanceID to_inst_id : lower_priority_instances) {
                        if (from_inst_id != to_inst_id) {
                            out_edges.insert(to_inst_id);
                        }
                    }
                }
            }

            for (VulInstanceID inst_id : same_priority_instances) {
                if (lower_priority_instance_set.insert(inst_id).second) {
                    lower_priority_instances.push_back(inst_id);
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
            throw VulException("Cyclic update constraints found in instance update order graph for instance ID " + std::to_string(cur_inst_id));
        }
    }

}
