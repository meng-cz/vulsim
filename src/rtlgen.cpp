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

#include "rtlgen.h"
#include "simgen.h"

#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <cctype>

namespace rtlgen {

struct RTLPort {
    string name;
    uint32_t width;
    bool is_input; // true for input port, false for output port
};

struct RTLGenContext {
    const VulStaticModuleInstance &module;
    const VulStaticConfigLib &local_configlib;
    const VulStaticBundleLib &local_bundlelib;

    RTLGenContext(const VulStaticModuleInstance &module, const VulStaticConfigLib &local_configlib, const VulStaticBundleLib &local_bundlelib)
        : module(module), local_configlib(local_configlib), local_bundlelib(local_bundlelib) {}

    vector<string> hls_header; // 文件开头的常量和结构体声明
    vector<string> hls_arguments; // HLS主函数的参数列表，仅包含类型和参数名字符串，没有逗号或换行
    vector<string> hls_init; // HLS主函数开头的初始化代码，主要是寄存器和线网的定义和reset赋值
    vector<string> hls_helpers; // HLS主函数中定义的辅助函数，用来将函数调用转换成对信号的操作
    vector<string> hls_blocks; // HLS主函数中每个逻辑块的代码
    vector<string> hls_body; // HLS主函数的主体代码，调用逻辑块
    vector<string> hls_final; // HLS主函数结尾的代码，主要是对输出信号的收尾赋值

    vector<string> rtl_ports;  // RTL框架模块的端口声明，每个一行，没有逗号或换行
    vector<string> rtl_decl;   // RTL框架模块的声明部分代码，主要是寄存器和线网的定义
    vector<string> rtl_logic;  // RTL框架模块的逻辑部分代码，主要是时序逻辑和组合逻辑的实现
    vector<string> rtl_inst;  // RTL框架模块的实例化代码，主要是子模块的实例化和连接
    vector<string> rtl_logicports;  // RTL框架中实例化逻辑子模块时连接的端口代码，每个一行，没有逗号或换行

    vector<string> resource_files;
};

inline string uintExtractExpr(const string &var, uint32_t high, uint32_t low) {
    return var + ".at<" + std::to_string(high) + ", " + std::to_string(low) + ">()";
}

inline string reqservArraySuffix(uint32_t idx) {
    return "__IDX" + std::to_string(idx);
}

template<typename Fn>
void forEachIndexTuple(const vector<ConfigRealValue> &dims, Fn fn) {
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

struct ResolvedReqServConnection {
    VulReqServConnection conn;
    string req_instance_name;
    vector<ConfigRealValue> req_instance_indices;
    string serv_instance_name;
    vector<ConfigRealValue> serv_instance_indices;
};

static string connEndpointBaseName(const string &raw_name, const string &base_name) {
    return base_name.empty() ? raw_name : base_name;
}

static vector<ConfigRealValue> parseConcreteChildIndices(
    const VulStaticModuleInstance &module,
    const string &base_name,
    const string &concrete_name
) {
    vector<ConfigRealValue> indices;
    auto inst_it = module.instances.find(base_name);
    if (inst_it == module.instances.end()) {
        throw VulException("Instance " + base_name + " not found while resolving RTL child connection");
    }
    const auto &decl = inst_it->second;
    if (!decl.isArrayed()) {
        if (concrete_name != base_name) {
            throw VulException("Unexpected concrete child name " + concrete_name + " for scalar instance " + base_name);
        }
        return indices;
    }
    string prefix = base_name + "__";
    if (concrete_name.rfind(prefix, 0) != 0) {
        throw VulException("Concrete child name " + concrete_name + " does not match array base " + base_name);
    }
    string rest = concrete_name.substr(prefix.size());
    size_t pos = 0;
    while (pos < rest.size()) {
        size_t next = rest.find("__", pos);
        string token = (next == string::npos) ? rest.substr(pos) : rest.substr(pos, next - pos);
        if (token.empty()) {
            throw VulException("Invalid concrete child name " + concrete_name);
        }
        indices.push_back(std::stoll(token));
        if (next == string::npos) {
            break;
        }
        pos = next + 2;
    }
    if (indices.size() != decl.array_dims.size()) {
        throw VulException("Dimension mismatch while resolving concrete child name " + concrete_name);
    }
    return indices;
}

static bool matchConcreteEndpoint(
    const vector<ConfigRealValue> &concrete_indices,
    const vector<VulConnIndexExpr> &endpoint_indices,
    const VulStaticConfigLib &config_lib,
    vector<std::optional<ConfigRealValue>> &loop_vars
) {
    if (concrete_indices.size() != endpoint_indices.size()) {
        return false;
    }
    for (size_t dim = 0; dim < endpoint_indices.size(); ++dim) {
        const auto &idx = endpoint_indices[dim];
        ConfigRealValue concrete_idx = concrete_indices[dim];
        if (idx.kind == VulConnIndexKind::Wildcard) {
            continue;
        }
        if (idx.kind == VulConnIndexKind::ConstantExpr) {
            if (calculateConstexprValue(idx.expr, config_lib) != concrete_idx) {
                return false;
            }
            continue;
        }
        if (idx.kind == VulConnIndexKind::GeneralExpr) {
            return false;
        }
        if (idx.kind == VulConnIndexKind::LoopVar) {
            if (idx.loop_dim < 0 || static_cast<size_t>(idx.loop_dim) >= loop_vars.size()) {
                return false;
            }
            ConfigRealValue loop_value = concrete_idx - idx.offset;
            auto &slot = loop_vars[idx.loop_dim];
            if (slot.has_value()) {
                if (*slot != loop_value) {
                    return false;
                }
            } else {
                slot = loop_value;
            }
            continue;
        }
    }
    return true;
}

static string replaceLoopVarsForRTLExpr(const string &expr) {
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

static ConfigRealValue evalRTLExprWithLoopVars(
    const string &expr,
    const VulStaticConfigLib &config_lib,
    const vector<std::optional<ConfigRealValue>> &loop_vars
) {
    VulStaticConfigLib eval_cfg = config_lib;
    if (loop_vars.size() > 0 && loop_vars[0].has_value()) eval_cfg["__v0"] = *loop_vars[0];
    if (loop_vars.size() > 1 && loop_vars[1].has_value()) eval_cfg["__v1"] = *loop_vars[1];
    return calculateConstexprValue(replaceLoopVarsForRTLExpr(expr), eval_cfg);
}

static bool materializeConcreteEndpoint(
    const VulStaticModuleInstance &module,
    const string &base_name,
    const vector<VulConnIndexExpr> &endpoint_indices,
    const VulStaticConfigLib &config_lib,
    const vector<std::optional<ConfigRealValue>> &loop_vars,
    string &concrete_name,
    vector<ConfigRealValue> &concrete_indices
) {
    concrete_name = base_name;
    concrete_indices.clear();
    if (base_name.empty()) {
        return true;
    }
    auto inst_it = module.instances.find(base_name);
    if (inst_it == module.instances.end()) {
        throw VulException("Instance " + base_name + " not found while materializing RTL child connection");
    }
    const auto &decl = inst_it->second;
    if (!decl.isArrayed()) {
        return endpoint_indices.empty();
    }
    if (endpoint_indices.size() != decl.array_dims.size()) {
        return false;
    }
    for (size_t dim = 0; dim < endpoint_indices.size(); ++dim) {
        const auto &idx = endpoint_indices[dim];
        if (idx.kind == VulConnIndexKind::Wildcard) {
            return false;
        }
        ConfigRealValue value = 0;
        if (idx.kind == VulConnIndexKind::ConstantExpr) {
            value = calculateConstexprValue(idx.expr, config_lib);
        } else if (idx.kind == VulConnIndexKind::GeneralExpr) {
            value = evalRTLExprWithLoopVars(idx.expr, config_lib, loop_vars);
        } else {
            if (idx.loop_dim < 0 || static_cast<size_t>(idx.loop_dim) >= loop_vars.size() || !loop_vars[idx.loop_dim].has_value()) {
                return false;
            }
            value = *loop_vars[idx.loop_dim] + idx.offset;
        }
        if (value < 0 || value >= decl.array_dims[dim]) {
            return false;
        }
        concrete_indices.push_back(value);
        concrete_name += "__" + std::to_string(value);
    }
    return true;
}

static string wildcardTopPortSuffix(
    const vector<VulConnIndexExpr> &endpoint_indices,
    const vector<ConfigRealValue> &concrete_indices
) {
    for (size_t dim = 0; dim < endpoint_indices.size(); ++dim) {
        if (endpoint_indices[dim].kind == VulConnIndexKind::Wildcard) {
            return "[" + std::to_string(concrete_indices[dim]) + "]";
        }
    }
    return "";
}

static string childServiceSignalBase(const string &instance_name, const string &service_name) {
    return instance_name + "_" + service_name;
}

static string childQuerySignalBase(const string &instance_name, const string &query_name) {
    return instance_name + "_" + query_name;
}

static const VulStaticModuleInstance &findChildTemplateByName(
    const VulStaticModuleInstance &mod,
    const string &name
) {
    for (const auto &child : mod.children) {
        if (child->instance_path.back() == name) {
            return *child;
        }
    }
    for (const auto &[inst_name, inst] : mod.instances) {
        if (inst.isArrayed() && (name == inst_name || name.starts_with(inst_name + "__"))) {
            for (const auto &child : mod.children) {
                if (child->instance_path.back() == inst_name) {
                    return *child;
                }
            }
        }
    }
    throw VulException("Child instance template not found: " + name);
}

static bool resolveConnectionForConcreteRequest(
    const VulStaticModuleInstance &module,
    const VulStaticConfigLib &config_lib,
    const string &concrete_name,
    const string &req_name,
    ResolvedReqServConnection &resolved
) {
    string base_name = concrete_name;
    auto pos = concrete_name.find("__");
    if (pos != string::npos) {
        base_name = concrete_name.substr(0, pos);
    }
    vector<ConfigRealValue> req_indices = parseConcreteChildIndices(module, base_name, concrete_name);
    for (const auto &conn : module.req_connections) {
        if (conn.req_name != req_name) continue;
        string conn_base = connEndpointBaseName(conn.req_instance, conn.req_instance_base);
        if (conn_base != base_name) continue;
        vector<std::optional<ConfigRealValue>> loop_vars(2);
        if (!matchConcreteEndpoint(req_indices, conn.req_indices, config_lib, loop_vars)) continue;
        resolved.conn = conn;
        resolved.req_instance_name = concrete_name;
        resolved.req_instance_indices = req_indices;
        if (conn.serv_instance.empty()) {
            resolved.serv_instance_name.clear();
            resolved.serv_instance_indices.clear();
            return true;
        }
        string serv_base = connEndpointBaseName(conn.serv_instance, conn.serv_instance_base);
        if (!materializeConcreteEndpoint(module, serv_base, conn.serv_indices, config_lib, loop_vars, resolved.serv_instance_name, resolved.serv_instance_indices)) {
            continue;
        }
        return true;
    }
    return false;
}

static bool resolveConnectionForConcreteService(
    const VulStaticModuleInstance &module,
    const VulStaticConfigLib &config_lib,
    const string &concrete_name,
    const string &srv_name,
    ResolvedReqServConnection &resolved
) {
    string base_name = concrete_name;
    auto pos = concrete_name.find("__");
    if (pos != string::npos) {
        base_name = concrete_name.substr(0, pos);
    }
    vector<ConfigRealValue> serv_indices = parseConcreteChildIndices(module, base_name, concrete_name);
    for (const auto &conn : module.req_connections) {
        if (conn.serv_name != srv_name) continue;
        string conn_base = connEndpointBaseName(conn.serv_instance, conn.serv_instance_base);
        if (conn_base != base_name) continue;
        vector<std::optional<ConfigRealValue>> loop_vars(2);
        if (!matchConcreteEndpoint(serv_indices, conn.serv_indices, config_lib, loop_vars)) continue;
        resolved.conn = conn;
        resolved.serv_instance_name = concrete_name;
        resolved.serv_instance_indices = serv_indices;
        if (conn.req_instance.empty()) {
            resolved.req_instance_name.clear();
            resolved.req_instance_indices.clear();
            return true;
        }
        string req_base = connEndpointBaseName(conn.req_instance, conn.req_instance_base);
        if (!materializeConcreteEndpoint(module, req_base, conn.req_indices, config_lib, loop_vars, resolved.req_instance_name, resolved.req_instance_indices)) {
            continue;
        }
        return true;
    }
    return false;
}


void _procConstAndBundle(RTLGenContext &ctx){
    VulErrorContextGuard guard("processing local configlib and bundlelib");

    for (const auto &cfg_entry : ctx.local_configlib) {
        const string &cfg_name = cfg_entry.first;
        ConfigRealValue real_value = cfg_entry.second;
        ctx.hls_header.push_back("constexpr int64_t " + cfg_name + " = " + std::to_string(real_value) + ";\n");
    }
    if (!ctx.local_configlib.empty()) {
        ctx.hls_header.push_back("\n");
    }

    for (const auto &bundle : ctx.local_bundlelib) {
        auto bundle_lines = simgen::genStaticBundle(bundle);
        ctx.hls_header.insert(ctx.hls_header.end(), bundle_lines.begin(), bundle_lines.end());
        ctx.hls_header.push_back("\n");
    }

    ctx.hls_header.insert(ctx.hls_header.end(), ctx.module.helper_codes.begin(), ctx.module.helper_codes.end());
};

void _procWires(RTLGenContext &ctx) {
    VulErrorContextGuard guard("processing wires");

    for (const auto &wire : ctx.module.wires) {
        string def = wire.signature.toString() + " " + wire.name + ";\n";
        ctx.hls_init.push_back(def);
        ctx.hls_init.push_back("{\n");
        ctx.hls_init.insert(ctx.hls_init.end(), wire.reset_codelines.begin(), wire.reset_codelines.end());
        ctx.hls_init.push_back("}\n");
    }
};

void _procRegisters(RTLGenContext &ctx) {

    for (const auto &reg : ctx.module.registers) {
        VulErrorContextGuard reg_guard("processing register " + reg.name);

        bool is_ported = (reg.ports > 1);
        bool is_array = (!reg.dims.empty());

        string proxy_class_name = "__RegProxy_" + reg.signature.type;
        if (is_ported) {
            proxy_class_name += "__P" + std::to_string(reg.ports);
        }
        if (is_array) {
            proxy_class_name += "__D" + std::to_string(reg.dims[0]);
        }
        proxy_class_name += "__" + reg.name;

        uint32_t element_width = 0;
        std::vector<FlatField> out;
        flatten_type_signature(reg.signature, ctx.local_bundlelib, "value", element_width, out);
        uint32_t wrport_num = reg.ports;
        uint32_t size = (reg.dims.empty()) ? 1 : reg.dims[0];

        string ewid_str = std::to_string(element_width);
        string ewid_m1_str = std::to_string(element_width - 1);
        string wrport_num_str = std::to_string(wrport_num);
        string size_str = std::to_string(size);

        string element_type_str = reg.signature.toString();

        string rdata_type_str = "Int<" + ewid_str + ">";
        string wdata_type_str = "Int<" + ewid_str + ">";
        string wen_type_str = "bool";
        if (is_ported) {
            wdata_type_str = "std::array<" + wdata_type_str + ", " + wrport_num_str + ">";
            wen_type_str = "std::array<bool, " + wrport_num_str + ">";
        }
        if (is_array) {
            rdata_type_str = "std::array<" + rdata_type_str + ", " + size_str + ">";
            wdata_type_str = "std::array<" + wdata_type_str + ", " + size_str + ">";
            wen_type_str = "std::array<bool, " + size_str + ">";
        }

        ctx.hls_header.push_back("struct " + proxy_class_name + " {\n");
        ctx.hls_header.push_back("const " + rdata_type_str + " &rdata;\n");
        ctx.hls_header.push_back(wen_type_str + " &wen;\n");
        ctx.hls_header.push_back(wdata_type_str + " &wdata;\n");
        ctx.hls_header.push_back("\n");

        ctx.hls_header.push_back(proxy_class_name + "(const " + rdata_type_str + " &rdata, " + wen_type_str + " &wen, " + wdata_type_str + " &wdata) : rdata(rdata), wen(wen), wdata(wdata) {}\n");
        ctx.hls_header.push_back("\n");

        if (!is_array) {
            // allow implicit conversion to element type for non-array register
            ctx.hls_header.push_back("operator " + element_type_str + "() const {\n");
            ctx.hls_header.push_back("  " + element_type_str + " value;\n");
            for (const auto &field : out) {
                ctx.hls_header.push_back("  " + field.name + " = " + uintExtractExpr("rdata", field.offset + field.width - 1, field.offset) + ";\n");
            }
            ctx.hls_header.push_back("  return value;\n");
            ctx.hls_header.push_back("}\n");

            // single element get for non-array register
            ctx.hls_header.push_back(element_type_str + " get() const {\n");
            ctx.hls_header.push_back("  " + element_type_str + " value;\n");
            for (const auto &field : out) {
                ctx.hls_header.push_back("  " + field.name + " = " + uintExtractExpr("rdata", field.offset + field.width - 1, field.offset) + ";\n");
            }
            ctx.hls_header.push_back("  return value;\n");
            ctx.hls_header.push_back("}\n");

            // single element set for non-array register
            ctx.hls_header.push_back("template <uint32_t P = 0>\n");
            ctx.hls_header.push_back("void setnext(const " + element_type_str + " &value) {\n");
            ctx.hls_header.push_back("  static_assert(P < " + wrport_num_str + ", \"Port index out of range\");\n");
            ctx.hls_header.push_back("  Int<" + ewid_str + "> wdata_val;\n");
            for (const auto &field : out) {
                ctx.hls_header.push_back("  " + uintExtractExpr("wdata_val", field.offset + field.width - 1, field.offset) + " = " + field.name + ";\n");
            }
            if (is_ported) {
                ctx.hls_header.push_back("  wdata[P] = wdata_val;\n");
                ctx.hls_header.push_back("  wen[P] = true;\n");
            } else {
                ctx.hls_header.push_back("  wdata = wdata_val;\n");
                ctx.hls_header.push_back("  wen = true;\n");
            }
            ctx.hls_header.push_back("}\n");
        } else {
            // array element get for array register
            ctx.hls_header.push_back(element_type_str + " operator[](const uint32_t idx) const {\n");
            ctx.hls_header.push_back("  " + element_type_str + " value;\n");
            ctx.hls_header.push_back("  Int<" + ewid_str + "> rdata_val = rdata[idx];\n");
            for (const auto &field : out) {
                ctx.hls_header.push_back("  " + field.name + " = " + uintExtractExpr("rdata_val", field.offset + field.width - 1, field.offset) + ";\n");
            }
            ctx.hls_header.push_back("  return value;\n");
            ctx.hls_header.push_back("}\n");

            // array element set for array register
            ctx.hls_header.push_back("template <uint32_t P = 0>\n");
            ctx.hls_header.push_back("void setnext(const uint32_t idx, const " + element_type_str + " &value) {\n");
            ctx.hls_header.push_back("  static_assert(P < " + wrport_num_str + ", \"Port index out of range\");\n");
            ctx.hls_header.push_back("  Int<" + ewid_str + "> wdata_val;\n");
            for (const auto &field : out) {
                ctx.hls_header.push_back("  " + uintExtractExpr("wdata_val", field.offset + field.width - 1, field.offset) + " = value." + field.name + ";\n");
            }
            if (is_ported) {
                ctx.hls_header.push_back("  wdata[idx][P] = wdata_val;\n");
                ctx.hls_header.push_back("  wen[idx][P] = true;\n");
            } else {
                ctx.hls_header.push_back("  wdata[idx] = wdata_val;\n");
                ctx.hls_header.push_back("  wen[idx] = true;\n");
            }
            ctx.hls_header.push_back("}\n");
        }

        ctx.hls_header.push_back("};\n");
        ctx.hls_header.push_back("\n");

        string wdata_port_name = "wdata_" + reg.name + "__";
        string wen_port_name = "wen_" + reg.name + "__";
        string rdata_port_name = "rdata_" + reg.name + "__";

        ctx.hls_arguments.push_back("const " + rdata_type_str + " &" + rdata_port_name);
        ctx.hls_arguments.push_back(wen_type_str + " &" + wen_port_name);
        ctx.hls_arguments.push_back(wdata_type_str + " &" + wdata_port_name);

        ctx.hls_init.push_back(proxy_class_name + " " + reg.name + "(" + rdata_port_name + ", " + wen_port_name + ", " + wdata_port_name + ");\n");

        // generate register instances
        string reg_decl_name = reg.name;
        string wen_wire_name = "wen_" + reg.name + "__";
        string wdata_wire_name = "wdata_" + reg.name + "__";

        if (!is_array) {
            ctx.rtl_decl.push_back("reg [" + ewid_m1_str + ":0] " + reg_decl_name + ";\n");
            if (is_ported) {
                ctx.rtl_decl.push_back("wire " + wen_wire_name + "[" + wrport_num_str + "];\n");
                ctx.rtl_decl.push_back("wire [" + ewid_m1_str + ":0] " + wdata_wire_name + "[" + wrport_num_str + "];\n");
            } else {
                ctx.rtl_decl.push_back("wire " + wen_wire_name + ";\n");
                ctx.rtl_decl.push_back("wire [" + ewid_m1_str + ":0] " + wdata_wire_name + ";\n");
            }

            // generate always block for register update
            ctx.rtl_logic.push_back("always @(posedge clk) begin\n");
            ctx.rtl_logic.push_back("  if (rstn == 0) begin\n");
            ctx.rtl_logic.push_back("    " + reg_decl_name + " <= 0;\n");
            ctx.rtl_logic.push_back("  end else begin\n");
            if (!is_ported) {
                ctx.rtl_logic.push_back("    if (" + wen_wire_name + ") begin\n");
                ctx.rtl_logic.push_back("      " + reg_decl_name + " <= " + wdata_wire_name + ";\n");
                ctx.rtl_logic.push_back("    end\n");
            } else {
                for (uint32_t i = 0; i < wrport_num; i++) {
                    ctx.rtl_logic.push_back("    " + (i==0?string(""):string("else ")) + "if (" + wen_wire_name + "[" + std::to_string(i) + "]) begin\n");
                    ctx.rtl_logic.push_back("      " + reg_decl_name + " <= " + wdata_wire_name + "[" + std::to_string(i) + "];\n");
                    ctx.rtl_logic.push_back("    end\n");
                }
            }
            ctx.rtl_logic.push_back("  end\n");
            ctx.rtl_logic.push_back("end\n");
        } else {
            ctx.rtl_decl.push_back("reg [" + ewid_m1_str + ":0] " + reg_decl_name + "[" + size_str + "];\n");
            if (is_ported) {
                ctx.rtl_decl.push_back("wire " + wen_wire_name + "[" + size_str + "][" + wrport_num_str + "];\n");
                ctx.rtl_decl.push_back("wire [" + ewid_m1_str + ":0] " + wdata_wire_name + "[" + size_str + "][" + wrport_num_str + "];\n");
            } else {
                ctx.rtl_decl.push_back("wire " + wen_wire_name + "[" + size_str + "];\n");
                ctx.rtl_decl.push_back("wire [" + ewid_m1_str + ":0] " + wdata_wire_name + "[" + size_str + "];\n");
            }

            // generate always block for register update
            ctx.rtl_logic.push_back("always @(posedge clk) begin\n");
            ctx.rtl_logic.push_back("  if (rstn == 0) begin\n");
            ctx.rtl_logic.push_back("    for (int i = 0; i < " + size_str + "; i++) begin\n");
            ctx.rtl_logic.push_back("      " + reg_decl_name + "[i] <= 0;\n");
            ctx.rtl_logic.push_back("    end\n");
            ctx.rtl_logic.push_back("  end else begin\n");
            ctx.rtl_logic.push_back("    for (int i = 0; i < " + size_str + "; i++) begin\n");
            if (!is_ported) {
                ctx.rtl_logic.push_back("      if (" + wen_wire_name + "[i]) begin\n");
                ctx.rtl_logic.push_back("        " + reg_decl_name + "[i] <= " + wdata_wire_name + "[i];\n");
                ctx.rtl_logic.push_back("      end\n");
            } else {
                for (uint32_t j = 0; j < wrport_num; j++) {
                    ctx.rtl_logic.push_back("      " + (j==0?string(""):string("else ")) + "if (" + wen_wire_name + "[i][" + std::to_string(j) + "]) begin\n");
                    ctx.rtl_logic.push_back("        " + reg_decl_name + "[i] <= " + wdata_wire_name + "[i][" + std::to_string(j) + "];\n");
                    ctx.rtl_logic.push_back("      end\n");
                }
            }
            ctx.rtl_logic.push_back("    end\n");
            ctx.rtl_logic.push_back("  end\n");
            ctx.rtl_logic.push_back("end\n");
        }

        ctx.rtl_logicports.push_back("." + rdata_port_name + "(" + reg_decl_name + ")");
        ctx.rtl_logicports.push_back("." + wen_port_name + "(" + wen_wire_name + ")");
        ctx.rtl_logicports.push_back("." + wdata_port_name + "(" + wdata_wire_name + ")");

    }
    
};

struct ArgPort {
    string name;
    VulStaticTypeSignature type;
    uint32_t width;
    vector<FlatField> flat_fields;
};

inline ArgPort procArg(const VulStaticArg &arg, const VulStaticBundleLib &bundlelib) {
    ArgPort port;
    port.name = arg.name;
    port.width = 0;
    port.type = arg.type;
    flatten_type_signature(arg.type, bundlelib, arg.name, port.width, port.flat_fields);
    return port;
}

inline string reqservVldPort(const string &reqserv_name) {
    return reqserv_name + "__vld__";
}

inline string reqservRdyPort(const string &reqserv_name) {
    return reqserv_name + "__rdy__";
}

inline string reqservArgPort(const string &reqserv_name, const string &arg_name) {
    return reqserv_name + "_" + arg_name + "__";
}

inline string queryPort(const string &query_name) {
    return query_name + "__query__";
}


void _procRequests(RTLGenContext &ctx) {

    auto request_driven_by_child_connection = [&](const string &req_name) {
        for (const auto &conn : ctx.module.req_connections) {
            if (!conn.req_instance.empty() && conn.serv_instance.empty() && conn.serv_name == req_name) {
                return true;
            }
        }
        return false;
    };

    for (const auto &req_entry : ctx.module.requests) {
        const string &req_name = req_entry.first;
        const VulStaticReqServ &req = req_entry.second;

        VulErrorContextGuard req_guard("processing request " + req_name);

        vector<ArgPort> arg_ports;
        vector<ArgPort> ret_ports;

        for (const auto &arg : req.args) {
            arg_ports.push_back(procArg(arg, ctx.local_bundlelib));
        }
        for (const auto &ret : req.rets) {
            ret_ports.push_back(procArg(ret, ctx.local_bundlelib));
        }

        string rettype = req.returnType();
        string arglists = req.signatureArgOnly();
        bool has_rdy = req.has_handshake;
        const bool is_arrayed = req.is_arrayed;
        const uint32_t array_size = static_cast<uint32_t>(req.array_size);
        const bool connected_from_child_request = request_driven_by_child_connection(req_name);

        auto arrayed_port_decl = [&](const string &base_name) -> string {
            if (is_arrayed) {
                return "std::array<" + base_name + ", " + std::to_string(array_size) + ">";
            } else {
                return base_name;
            }
        };

        // hls function ports
        string vld_port_name = reqservVldPort(req_name);
        string rdy_port_name = reqservRdyPort(req_name);
        if (!connected_from_child_request) {
            ctx.hls_arguments.push_back(arrayed_port_decl("bool") + " &" + vld_port_name);
            if (has_rdy) {
                ctx.hls_arguments.push_back("const " + arrayed_port_decl("bool") + " " + rdy_port_name);
            }
            for (const auto &arg : arg_ports) {
                ctx.hls_arguments.push_back(arrayed_port_decl("Int<" + std::to_string(arg.width) + ">") + " &" + reqservArgPort(req_name, arg.name));
            }
            for (const auto &ret : ret_ports) {
                ctx.hls_arguments.push_back("const " + arrayed_port_decl("Int<" + std::to_string(ret.width) + ">") + " " + reqservArgPort(req_name, ret.name));
            }

            if (is_arrayed) {
                for (uint32_t idx = 0; idx < array_size; ++idx) {
                    ctx.hls_init.push_back(vld_port_name + "[" + std::to_string(idx) + "] = false;\n");
                }
            }

            // generate request helper function declaration (supports req<IDX>(...))
            const string helper_struct_name = "__ReqHelper__" + req_name;
            const string helper_var_name = "__req_helper__" + req_name;
            const string array_size_str = std::to_string(array_size);

            ctx.hls_header.push_back("struct " + helper_struct_name + " {\n");
            ctx.hls_header.push_back("  " + arrayed_port_decl("bool") + " & vld_ports;\n");
            if (has_rdy) {
                ctx.hls_header.push_back("  const " + arrayed_port_decl("bool") + " & rdy_ports;\n");
            }
            for (const auto &arg : arg_ports) {
                ctx.hls_header.push_back("  " + arrayed_port_decl("Int<" + std::to_string(arg.width) + ">") + " & arg_" + arg.name + ";\n");
            }
            for (const auto &ret : ret_ports) {
                ctx.hls_header.push_back("  const " + arrayed_port_decl("Int<" + std::to_string(ret.width) + ">") + " & ret_" + ret.name + ";\n");
            }

            ctx.hls_header.push_back("\n");
            ctx.hls_header.push_back("  template <uint32_t IDX = 0>\n");
            ctx.hls_header.push_back("  " + rettype + " call(" + arglists + ") {\n");
            ctx.hls_header.push_back("    static_assert(IDX < " + array_size_str + ", \"Request port index out of range\");\n");
            string sel_str = is_arrayed ? ("[IDX]") : "";
            ctx.hls_header.push_back("    vld_ports" + sel_str + " = true;\n");
            for (const auto &arg : arg_ports) {
                for (const auto &field : arg.flat_fields) {
                    ctx.hls_header.push_back("    " + uintExtractExpr("arg_" + arg.name + sel_str, field.offset + field.width - 1, field.offset) + " = " + field.name + ";\n");
                }
            }
            for (const auto &ret : ret_ports) {
                for (const auto &field : ret.flat_fields) {
                    ctx.hls_header.push_back("    " + field.name + " = " + uintExtractExpr("ret_" + ret.name + sel_str, field.offset + field.width - 1, field.offset) + ";\n");
                }
            }
            if (has_rdy) {
                ctx.hls_header.push_back("    return rdy_ports" + sel_str + ";\n");
            }
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("};\n");

            ctx.hls_helpers.push_back(helper_struct_name + " " + helper_var_name + "{\n");
            ctx.hls_helpers.push_back("  " + vld_port_name + ",\n");
            if (has_rdy) {
                ctx.hls_helpers.push_back("  " + rdy_port_name + ",\n");
            }
            for (size_t i = 0; i < arg_ports.size(); ++i) {
                const auto &arg = arg_ports[i];
                ctx.hls_helpers.push_back("  " + reqservArgPort(req_name, arg.name) + ",\n");
            }
            for (size_t i = 0; i < ret_ports.size(); ++i) {
                const auto &ret = ret_ports[i];
                ctx.hls_helpers.push_back("  " + reqservArgPort(req_name, ret.name) + ",\n");
            }
            ctx.hls_helpers.push_back("};\n");
            ctx.hls_helpers.push_back("#define " + req_name + " " + helper_var_name + ".call\n");
            ctx.hls_final.push_back("#undef " + req_name + "\n");
        }

        // generate RTL ports
        string sv_array_str = is_arrayed ? ("[" + std::to_string(array_size) + "]") : "";
        ctx.rtl_ports.push_back("output " + vld_port_name + sv_array_str);
        if (!connected_from_child_request) {
            ctx.rtl_logicports.push_back("." + vld_port_name + "(" + vld_port_name + ")");
        }
        if (has_rdy) {
            ctx.rtl_ports.push_back("input " + rdy_port_name + sv_array_str);
            if (!connected_from_child_request) {
                ctx.rtl_logicports.push_back("." + rdy_port_name + "(" + rdy_port_name + ")");
            }
        }
        for (const auto &arg : arg_ports) {
            string arg_port_name = reqservArgPort(req_name, arg.name);
            ctx.rtl_ports.push_back("output [" + std::to_string(arg.width - 1) + ":0] " + arg_port_name + sv_array_str);
            if (!connected_from_child_request) {
                ctx.rtl_logicports.push_back("." + arg_port_name + "(" + arg_port_name + ")");
            }
        }
        for (const auto &ret : ret_ports) {
            string ret_port_name = reqservArgPort(req_name, ret.name);
            ctx.rtl_ports.push_back("input [" + std::to_string(ret.width - 1) + ":0] " + ret_port_name + sv_array_str);
            if (!connected_from_child_request) {
                ctx.rtl_logicports.push_back("." + ret_port_name + "(" + ret_port_name + ")");
            }
        }
    }
}

void _procServicesAndTicks(RTLGenContext &ctx) {

    struct ServiceCache {
        ReqServName name;
        string rettype;
        string argtypes;
        string argnames;
        string arglists;
        bool has_rdy;
        bool is_arrayed = false;
        uint32_t array_size = 1;
        vector<ArgPort> arg_ports;
        vector<ArgPort> ret_ports;
        bool is_tick = false;
    };

    auto condFuncName = [](const string &serv_name) {
        return serv_name + "_cond__";
    };
    auto implFuncName = [](const string &serv_name) {
        return serv_name + "_impl__";
    };

    map<int32_t, vector<ServiceCache>> service_cache; // priority -> service cache

    auto append_call_name = [](string &call_names, const string &name) {
        if (!call_names.empty()) {
            call_names += ", ";
        }
        call_names += name;
    };

    auto emit_unpack_arg = [&](const ArgPort &arg, const string &portname, string &call_names) {
        string typestr = arg.type.toString();
        ctx.hls_body.push_back("  " + typestr + " " + arg.name + ";\n");
        for (const auto &field : arg.flat_fields) {
            ctx.hls_body.push_back("  " + field.name + " = " + uintExtractExpr(portname, field.offset + field.width - 1, field.offset) + ";\n");
        }
        append_call_name(call_names, arg.name);
    };

    auto emit_construct_ret = [&](const ArgPort &ret, string &call_names) {
        string typestr = ret.type.toString();
        ctx.hls_body.push_back("  " + typestr + " " + ret.name + ";\n");
        append_call_name(call_names, ret.name);
    };

    auto emit_pack_ret = [&](const ArgPort &ret, const string &portname) {
        string temp_var_name = ret.name + "_packed";
        ctx.hls_body.push_back("    Int<" + std::to_string(ret.width) + "> " + ret.name + "_packed;\n");
        for (const auto &field : ret.flat_fields) {
            ctx.hls_body.push_back("    " + uintExtractExpr(temp_var_name, field.offset + field.width - 1, field.offset) + " = " + field.name + ";\n");
        }
        ctx.hls_body.push_back("    " + portname + " = " + temp_var_name + ";\n");
    };

    for (const auto &entry : ctx.module.services) {
        const string &serv_name = entry.first;
        const VulStaticReqServ &serv = entry.second;

        VulErrorContextGuard serv_guard("processing service " + serv_name);

        vector<ArgPort> arg_ports;
        vector<ArgPort> ret_ports;

        for (const auto &arg : serv.args) {
            arg_ports.push_back(procArg(arg, ctx.local_bundlelib));
        }
        for (const auto &ret : serv.rets) {
            ret_ports.push_back(procArg(ret, ctx.local_bundlelib));
        }

        string rettype = serv.returnType();
        string argtypes = serv.signatureArgTypeOnly();
        string argnames = serv.signatureArgNameList();
        string arglists = serv.signatureArgOnly();
        bool has_rdy = serv.has_handshake;
        const bool is_arrayed = serv.is_arrayed;
        const uint32_t array_size = static_cast<uint32_t>(serv.array_size);
        string sv_array_str = is_arrayed ? ("[" + std::to_string(array_size) + "]") : "";

        auto arrayed_port_decl = [&](const string &base_name) -> string {
            if (is_arrayed) {
                return "std::array<" + base_name + ", " + std::to_string(array_size) + ">";
            } else {
                return base_name;
            }
        };

        // rtl ports
        string vld_port_name = reqservVldPort(serv_name);
        string rdy_port_name = reqservRdyPort(serv_name);
        ctx.rtl_ports.push_back("input " + vld_port_name + sv_array_str);
        if (has_rdy) {
            ctx.rtl_ports.push_back("output " + rdy_port_name + sv_array_str);
        }
        for (const auto &arg : arg_ports) {
            string arg_port_name = reqservArgPort(serv_name, arg.name);
            ctx.rtl_ports.push_back("input [" + std::to_string(arg.width - 1) + ":0] " + arg_port_name + sv_array_str);
        }
        for (const auto &ret : ret_ports) {
            string ret_port_name = reqservArgPort(serv_name, ret.name);
            ctx.rtl_ports.push_back("output [" + std::to_string(ret.width - 1) + ":0] " + ret_port_name + sv_array_str);
        }

        // a service should be:
        // (1) implemented in logic code blocks
        // (2) connected to child instance service ports

        auto iter = ctx.module.serv_logic_blocks.find(serv_name);
        if (iter == ctx.module.serv_logic_blocks.end()) {
            continue;
        }
        const auto& lb = iter->second;

        ctx.hls_arguments.push_back("const " + arrayed_port_decl("bool") + " " + vld_port_name);
        ctx.rtl_logicports.push_back("." + vld_port_name + "(" + vld_port_name + ")");
        if (has_rdy) {
            ctx.hls_arguments.push_back(arrayed_port_decl("bool") + " & " + rdy_port_name);
            ctx.rtl_logicports.push_back("." + rdy_port_name + "(" + rdy_port_name + ")");
        }
        for (const auto &arg : arg_ports) {
            string arg_port_name = reqservArgPort(serv_name, arg.name);
            ctx.hls_arguments.push_back("const " + arrayed_port_decl("Int<" + std::to_string(arg.width) + ">") + " " + arg_port_name);
            ctx.rtl_logicports.push_back("." + arg_port_name + "(" + arg_port_name + ")");
        }
        for (const auto &ret : ret_ports) {
            string ret_port_name = reqservArgPort(serv_name, ret.name);
            ctx.hls_arguments.push_back(arrayed_port_decl("Int<" + std::to_string(ret.width) + ">") + " &" + ret_port_name);
            ctx.rtl_logicports.push_back("." + ret_port_name + "(" + ret_port_name + ")");
        }

        if (is_arrayed) {
            ctx.hls_blocks.push_back("auto " + implFuncName(serv_name) + " = [&]<uint32_t IDX = 0>(" + arglists + ") -> void {\n");
        } else {
            ctx.hls_blocks.push_back("auto " + implFuncName(serv_name) + " = [&](" + arglists + ") -> void {\n");
        }
        ctx.hls_blocks.insert(ctx.hls_blocks.end(), lb.codelines.begin(), lb.codelines.end());
        ctx.hls_blocks.push_back("};\n");
        if (has_rdy) {
            if (is_arrayed) {
                ctx.hls_blocks.push_back("auto " + condFuncName(serv_name) + " = [&]<uint32_t IDX = 0>(" + arglists + ") -> bool {\n");
            } else {
                ctx.hls_blocks.push_back("auto " + condFuncName(serv_name) + " = [&](" + arglists + ") -> bool {\n");
            }
            ctx.hls_blocks.insert(ctx.hls_blocks.end(), lb.cond_codelines.begin(), lb.cond_codelines.end());
            ctx.hls_blocks.push_back("};\n");
        }

        // cache service info for tick generation
        ServiceCache cache;
        cache.name = serv_name;
        cache.rettype = rettype;
        cache.argtypes = argtypes;
        cache.argnames = argnames;
        cache.arglists = arglists;
        cache.has_rdy = has_rdy;
        cache.is_arrayed = is_arrayed;
        cache.array_size = array_size;
        cache.arg_ports = arg_ports;
        cache.ret_ports = ret_ports;
        cache.is_tick = false;
        service_cache[lb.with_priority?lb.priority:0].push_back(cache);
    }
    // ticks
    uint32_t tick_count = 0;
    for (const auto &tb : ctx.module.tick_blocks) {
        string name = "tick" + std::to_string(tick_count++) + "__";
        ctx.hls_blocks.push_back("auto " + name + " = [&]() {\n");
        ctx.hls_blocks.insert(ctx.hls_blocks.end(), tb.codelines.begin(), tb.codelines.end());
        ctx.hls_blocks.push_back("};\n");

        // cache tick info
        ServiceCache cache;
        cache.name = name;
        cache.rettype = "void";
        cache.argtypes = "";
        cache.argnames = "";
        cache.arglists = "";
        cache.has_rdy = false;
        cache.is_tick = true;
        service_cache[0].push_back(cache);
    }
    // generate cycle logic
    VulErrorContextGuard cycle_guard("generating cycle logic for services and ticks");
    for (auto it = service_cache.rbegin(); it != service_cache.rend(); ++it) {
        int32_t key = it->first;
        const std::vector<ServiceCache>& vec = it->second;
        for (const ServiceCache& cache : vec) {
            if (cache.has_rdy) {
                if (cache.is_arrayed) {
                    for (uint32_t idx = 0; idx < cache.array_size; ++idx) {
                        ctx.hls_body.push_back("{\n");
                        string call_names = "";
                        for (uint32_t i = 0; i < cache.arg_ports.size(); ++i) {
                            const auto &arg = cache.arg_ports[i];
                            string portname = (reqservArgPort(cache.name, arg.name) + "[" + std::to_string(idx) + "]");
                            emit_unpack_arg(arg, portname, call_names);
                        }
                        for (const auto &ret : cache.ret_ports) {
                            emit_construct_ret(ret, call_names);
                        }
                        ctx.hls_body.push_back("  bool rdy = " + condFuncName(cache.name) + ".template operator()<" + std::to_string(idx) + ">(" + call_names + ");\n");
                        ctx.hls_body.push_back("  if (rdy && " + reqservVldPort(cache.name) + "[" + std::to_string(idx) + "]) {\n");
                        ctx.hls_body.push_back("    " + implFuncName(cache.name) + ".template operator()<" + std::to_string(idx) + ">(" + call_names + ");\n");
                        for (const auto &ret : cache.ret_ports) {
                            string portname = reqservArgPort(cache.name, ret.name) + "[" + std::to_string(idx) + "]";
                            emit_pack_ret(ret, portname);
                        }
                        ctx.hls_body.push_back("  }\n");
                        ctx.hls_body.push_back("  " + reqservRdyPort(cache.name) + "[" + std::to_string(idx) + "] = rdy;\n");
                        ctx.hls_body.push_back("}\n");
                    }
                } else {
                    ctx.hls_body.push_back("{\n");
                    string call_names = "";
                    for (uint32_t i = 0; i < cache.arg_ports.size(); ++i) {
                        const auto &arg = cache.arg_ports[i];
                        string portname = (reqservArgPort(cache.name, arg.name));
                        emit_unpack_arg(arg, portname, call_names);
                    }
                    for (const auto &ret : cache.ret_ports) {
                        emit_construct_ret(ret, call_names);
                    }
                    ctx.hls_body.push_back("  bool rdy = " + condFuncName(cache.name) + "(" + call_names + ");\n");
                    ctx.hls_body.push_back("  if (rdy && " + reqservVldPort(cache.name) + ") {\n");
                    ctx.hls_body.push_back("    " + implFuncName(cache.name) + "(" + call_names + ");\n");
                    for (const auto &ret : cache.ret_ports) {
                        string portname = reqservArgPort(cache.name, ret.name);
                        emit_pack_ret(ret, portname);
                    }
                    ctx.hls_body.push_back("  }\n");
                    ctx.hls_body.push_back("  " + reqservRdyPort(cache.name) + " = rdy;\n");
                    ctx.hls_body.push_back("}\n");
                }
            } else if (!cache.is_tick) {
                if (cache.is_arrayed) {
                    for (uint32_t idx = 0; idx < cache.array_size; ++idx) {
                        ctx.hls_body.push_back("{\n");
                        string call_names = "";
                        for (uint32_t i = 0; i < cache.arg_ports.size(); ++i) {
                            const auto &arg = cache.arg_ports[i];
                            string portname = reqservArgPort(cache.name, arg.name) + "[" + std::to_string(idx) + "]";
                            emit_unpack_arg(arg, portname, call_names);
                        }
                        for (const auto &ret : cache.ret_ports) {
                            emit_construct_ret(ret, call_names);
                        }
                        ctx.hls_body.push_back("  if (" + reqservVldPort(cache.name) + "[" + std::to_string(idx) + "]) {\n");
                        ctx.hls_body.push_back("    " + implFuncName(cache.name) + ".template operator()<" + std::to_string(idx) + ">(" + call_names + ");\n");
                        for (const auto &ret : cache.ret_ports) {
                            string portname = reqservArgPort(cache.name, ret.name) + "[" + std::to_string(idx) + "]";
                            emit_pack_ret(ret, portname);
                        }
                        ctx.hls_body.push_back("  }\n");
                        ctx.hls_body.push_back("}\n");
                    }
                } else {
                    ctx.hls_body.push_back("{\n");
                    string call_names = "";
                    for (uint32_t i = 0; i < cache.arg_ports.size(); ++i) {
                        const auto &arg = cache.arg_ports[i];
                        string portname = reqservArgPort(cache.name, arg.name);
                        emit_unpack_arg(arg, portname, call_names);
                    }
                    for (const auto &ret : cache.ret_ports) {
                        emit_construct_ret(ret, call_names);
                    }
                    ctx.hls_body.push_back("  if (" + reqservVldPort(cache.name) + ") {\n");
                    ctx.hls_body.push_back("    " + implFuncName(cache.name) + "(" + call_names + ");\n");
                    for (const auto &ret : cache.ret_ports) {
                        string portname = reqservArgPort(cache.name, ret.name);
                        emit_pack_ret(ret, portname);
                    }
                    ctx.hls_body.push_back("  }\n");
                    ctx.hls_body.push_back("}\n");
                }
            } else {
                ctx.hls_body.push_back(cache.name + "();\n");
            }
        }
    }
}

void _procQueries(RTLGenContext &ctx) {
    for (const auto &entry : ctx.module.queries) {
        const string &query_name = entry.first;
        const auto &query = entry.second;

        auto iter = ctx.module.query_logic_blocks.find(query_name);
        if (iter == ctx.module.query_logic_blocks.end()) {
            throw VulException("Missing logic block for query " + query_name);
        }

        uint32_t width = 0;
        vector<FlatField> flat_fields;
        flatten_type_signature(query.ret_type, ctx.local_bundlelib, "value", width, flat_fields);

        const string port_name = queryPort(query_name);
        const string ret_type_str = query.ret_type.toString();

        ctx.hls_arguments.push_back("Int<" + std::to_string(width) + "> &" + port_name);
        ctx.rtl_ports.push_back("output [" + std::to_string(width - 1) + ":0] " + port_name);
        ctx.rtl_logicports.push_back("." + port_name + "(" + port_name + ")");

        ctx.hls_blocks.push_back("auto " + query_name + " = [&]() -> " + ret_type_str + " {\n");
        ctx.hls_blocks.insert(ctx.hls_blocks.end(), iter->second.codelines.begin(), iter->second.codelines.end());
        ctx.hls_blocks.push_back("};\n");

        ctx.hls_body.push_back("{\n");
        ctx.hls_body.push_back("  " + ret_type_str + " value = " + query_name + "();\n");
        ctx.hls_body.push_back("  Int<" + std::to_string(width) + "> packed;\n");
        for (const auto &field : flat_fields) {
            ctx.hls_body.push_back("  " + uintExtractExpr("packed", field.offset + field.width - 1, field.offset) + " = " + field.name + ";\n");
        }
        ctx.hls_body.push_back("  " + port_name + " = packed;\n");
        ctx.hls_body.push_back("}\n");
    }
}

void _procChildrenAndConnection(RTLGenContext &ctx) {

    auto conn_to_str = [](const ResolvedReqServConnection &conn) -> string {
        return conn.serv_instance_name + "_" + conn.conn.serv_name + "_" + conn.req_instance_name + "_" + conn.conn.req_name;
    };

    // CR-CS 需要wire，子实例-子实例
    // CR-S 需要wire，子实例-逻辑子模块
    // CR-R 不需要wire，直接从子实例端口连到模块端口
    // S-CS 不需要wire，直接从子实例端口连到模块端口
    // L-CS 需要wire，逻辑子模块-子实例

    for (const auto child_ptr : ctx.module.children) {
        const auto &child = *child_ptr;
        string child_class_name = child.simClassName();
        string child_decl_name = child.instance_path.back();
        VulErrorContextGuard child_guard("processing child instance " + child_class_name);

        if (ctx.module.instances.find(child_decl_name) == ctx.module.instances.end()) {
            throw VulException("Instance " + child_decl_name + " not found in module instances");
        }
        const auto &inst = ctx.module.instances.at(child_decl_name);

        auto emit_child_instance = [&](const string &child_instance_name) {
            vector<string> child_port_lines;
            child_port_lines.push_back(".clk(clk)");
            child_port_lines.push_back(".rstn(rstn)");

            for (const auto &req_entry : child.requests) {
                const auto &req = req_entry.second;
                const string &req_name = req_entry.first;
                string sv_array_str = req.is_arrayed ? ("[" + std::to_string(req.array_size) + "]") : "";
                ResolvedReqServConnection conn;
                bool connected = false;
                connected = resolveConnectionForConcreteRequest(ctx.module, ctx.local_configlib, child_instance_name, req_name, conn);
                if (!connected) {
                    throw VulException("Request " + req_name + " of instance " + child_instance_name + " is not connected");
                }
                if (conn.conn.serv_instance != "" || ctx.module.services.find(conn.conn.serv_name) != ctx.module.services.end()) {
                    string conn_str = conn_to_str(conn);
                    ctx.rtl_decl.push_back("wire " + conn_str + "_valid" + sv_array_str + ";\n");
                    child_port_lines.push_back(string(".") + reqservVldPort(req_name) + "(" + conn_str + "_valid)");
                    if (req.has_handshake) {
                        ctx.rtl_decl.push_back("wire " + conn_str + "_ready" + sv_array_str + ";\n");
                        child_port_lines.push_back(string(".") + reqservRdyPort(req_name) + "(" + conn_str + "_ready)");
                    }
                    for (const auto &arg : req.args) {
                        uint32_t offset = 0;
                        std::vector<FlatField> out;
                        flatten_type_signature(arg.type, ctx.local_bundlelib, arg.name, offset, out);
                        ctx.rtl_decl.push_back("wire [" + std::to_string(offset - 1) + ":0] " + conn_str + "_arg_" + arg.name + sv_array_str + ";\n");
                        child_port_lines.push_back(string(".") + reqservArgPort(req_name, arg.name) + "(" + conn_str + "_arg_" + arg.name + ")");
                    }
                    for (const auto &ret : req.rets) {
                        uint32_t offset = 0;
                        std::vector<FlatField> out;
                        flatten_type_signature(ret.type, ctx.local_bundlelib, ret.name, offset, out);
                        ctx.rtl_decl.push_back("wire [" + std::to_string(offset - 1) + ":0] " + conn_str + "_ret_" + ret.name + sv_array_str + ";\n");
                        child_port_lines.push_back(string(".") + reqservArgPort(req_name, ret.name) + "(" + conn_str + "_ret_" + ret.name + ")");
                    }
                } else {
                    string top_suffix = wildcardTopPortSuffix(conn.conn.req_indices, conn.req_instance_indices);
                    child_port_lines.push_back(string(".") + reqservVldPort(req_name) + "(" + reqservVldPort(conn.conn.serv_name) + top_suffix + ")");
                    if (req.has_handshake) {
                        child_port_lines.push_back(string(".") + reqservRdyPort(req_name) + "(" + reqservRdyPort(conn.conn.serv_name) + top_suffix + ")");
                    }
                    for (const auto &arg : req.args) {
                        child_port_lines.push_back(string(".") + reqservArgPort(req_name, arg.name) + "(" + reqservArgPort(conn.conn.serv_name, arg.name) + top_suffix + ")");
                    }
                    for (const auto &ret : req.rets) {
                        child_port_lines.push_back(string(".") + reqservArgPort(req_name, ret.name) + "(" + reqservArgPort(conn.conn.serv_name, ret.name) + top_suffix + ")");
                    }
                }
            }
            for (const auto &srv_entry : child.services) {
                const auto &srv = srv_entry.second;
                const string &srv_name = srv_entry.first;
                ResolvedReqServConnection conn;
                bool connected = false;
                connected = resolveConnectionForConcreteService(ctx.module, ctx.local_configlib, child_instance_name, srv_name, conn);
                bool logic_ref = false;
                for (const auto &use : ctx.module.child_service_uses) {
                    if (use.instance_name == child_instance_name && use.service_name == srv_name) {
                        logic_ref = true;
                        break;
                    }
                }
                if (!connected && !logic_ref) {
                    continue;
                }
                if (connected && conn.conn.req_instance == "") {
                    string top_suffix = wildcardTopPortSuffix(conn.conn.serv_indices, conn.serv_instance_indices);
                    child_port_lines.push_back(string(".") + reqservVldPort(srv_name) + "(" + reqservVldPort(conn.conn.req_name) + top_suffix + ")");
                    if (srv.has_handshake) {
                        child_port_lines.push_back(string(".") + reqservRdyPort(srv_name) + "(" + reqservRdyPort(conn.conn.req_name) + top_suffix + ")");
                    }
                    for (const auto &arg : srv.args) {
                        child_port_lines.push_back(string(".") + reqservArgPort(srv_name, arg.name) + "(" + reqservArgPort(conn.conn.req_name, arg.name) + top_suffix + ")");
                    }
                    for (const auto &ret : srv.rets) {
                        child_port_lines.push_back(string(".") + reqservArgPort(srv_name, ret.name) + "(" + reqservArgPort(conn.conn.req_name, ret.name) + top_suffix + ")");
                    }
                } else if (connected) {
                    string conn_str = conn_to_str(conn);
                    child_port_lines.push_back(string(".") + reqservVldPort(srv_name) + "(" + conn_str + "_valid)");
                    if (srv.has_handshake) {
                        child_port_lines.push_back(string(".") + reqservRdyPort(srv_name) + "(" + conn_str + "_ready)");
                    }
                    for (const auto &arg : srv.args) {
                        child_port_lines.push_back(string(".") + reqservArgPort(srv_name, arg.name) + "(" + conn_str + "_arg_" + arg.name + ")");
                    }
                    for (const auto &ret : srv.rets) {
                        child_port_lines.push_back(string(".") + reqservArgPort(srv_name, ret.name) + "(" + conn_str + "_ret_" + ret.name + ")");
                    }
                } else {
                    string sv_array_str = srv.is_arrayed ? ("[" + std::to_string(srv.array_size) + "]") : "";
                    auto arrayed_port_decl = [&](const string &base_name) -> string {
                        if (srv.is_arrayed) {
                            return "std::array<" + base_name + ", " + std::to_string(srv.array_size) + ">";
                        } else {
                            return base_name;
                        }
                    };
                    string child_serv_name = childServiceSignalBase(child_instance_name, srv_name);
                    ctx.rtl_decl.push_back("wire " + reqservVldPort(child_serv_name) + sv_array_str + ";\n");
                    child_port_lines.push_back(string(".") + reqservVldPort(srv_name) + "(" + reqservVldPort(child_serv_name) + ")");
                    ctx.hls_arguments.push_back(arrayed_port_decl("bool") + " & " + reqservVldPort(child_serv_name));
                    ctx.rtl_logicports.push_back("." + reqservVldPort(child_serv_name) + "(" + reqservVldPort(child_serv_name) + ")");
                    if (srv.has_handshake) {
                        ctx.rtl_decl.push_back("wire " + reqservRdyPort(child_serv_name) + sv_array_str + ";\n");
                        child_port_lines.push_back(string(".") + reqservRdyPort(srv_name) + "(" + reqservRdyPort(child_serv_name) + ")");
                        ctx.hls_arguments.push_back("const " + arrayed_port_decl("bool") + " & " + reqservRdyPort(child_serv_name));
                        ctx.rtl_logicports.push_back("." + reqservRdyPort(child_serv_name) + "(" + reqservRdyPort(child_serv_name) + ")");
                    }
                    vector<ArgPort> arg_ports;
                    vector<ArgPort> ret_ports;
                    for (const auto &arg : srv.args) {
                        arg_ports.push_back(procArg(arg, ctx.local_bundlelib));
                    }
                    for (const auto &ret : srv.rets) {
                        ret_ports.push_back(procArg(ret, ctx.local_bundlelib));
                    }
                    for (const auto &arg : arg_ports) {
                        ctx.rtl_decl.push_back("wire [" + std::to_string(arg.width - 1) + ":0] " + reqservArgPort(child_serv_name, arg.name) + sv_array_str + ";\n");
                        child_port_lines.push_back(string(".") + reqservArgPort(srv_name, arg.name) + "(" + reqservArgPort(child_serv_name, arg.name) + ")");
                        ctx.hls_arguments.push_back(arrayed_port_decl("Int<" + std::to_string(arg.width) + ">") + " & " + reqservArgPort(child_serv_name, arg.name));
                        ctx.rtl_logicports.push_back("." + reqservArgPort(child_serv_name, arg.name) + "(" + reqservArgPort(child_serv_name, arg.name) + ")");
                    }
                    for (const auto &ret : ret_ports) {
                        ctx.rtl_decl.push_back("wire [" + std::to_string(ret.width - 1) + ":0] " + reqservArgPort(child_serv_name, ret.name) + sv_array_str + ";\n");
                        child_port_lines.push_back(string(".") + reqservArgPort(srv_name, ret.name) + "(" + reqservArgPort(child_serv_name, ret.name) + ")");
                        ctx.hls_arguments.push_back("const " + arrayed_port_decl("Int<" + std::to_string(ret.width) + ">") + " & " + reqservArgPort(child_serv_name, ret.name));
                        ctx.rtl_logicports.push_back("." + reqservArgPort(child_serv_name, ret.name) + "(" + reqservArgPort(child_serv_name, ret.name) + ")");
                    }
                    if (srv.is_arrayed) {
                        for (uint32_t idx = 0; idx < srv.array_size; ++idx) {
                            ctx.hls_init.push_back(reqservVldPort(child_serv_name) + "[" + std::to_string(idx) + "] = false;\n");
                        }
                    } else {
                        ctx.hls_init.push_back(reqservVldPort(child_serv_name) + " = false;\n");
                    }
                }
            }
            for (const auto &query_entry : child.queries) {
                const string &query_name = query_entry.first;
                const auto &query = query_entry.second;
                const string signal_base = childQuerySignalBase(child_instance_name, query_name);
                const string port_name = queryPort(signal_base);
                const string child_port_name = queryPort(query_name);

                uint32_t width = 0;
                vector<FlatField> flat_fields;
                flatten_type_signature(query.ret_type, ctx.local_bundlelib, "value", width, flat_fields);
                ctx.rtl_decl.push_back("wire [" + std::to_string(width - 1) + ":0] " + port_name + ";\n");
                child_port_lines.push_back(string(".") + child_port_name + "(" + port_name + ")");

                bool logic_ref = false;
                for (const auto &use : ctx.module.child_query_uses) {
                    if (use.instance_name == child_instance_name && use.query_name == query_name) {
                        logic_ref = true;
                        break;
                    }
                }
                if (logic_ref) {
                    ctx.hls_arguments.push_back("const Int<" + std::to_string(width) + "> " + port_name);
                    ctx.rtl_logicports.push_back("." + port_name + "(" + port_name + ")");
                }
            }
            ctx.rtl_inst.push_back(child_class_name + " " + child_instance_name + " (\n");
            for (size_t i = 0; i < child_port_lines.size(); i++) {
                ctx.rtl_inst.push_back("  " + child_port_lines[i] + (i == child_port_lines.size() - 1 ? "" : ",") + "\n");
            }
            ctx.rtl_inst.push_back(");\n");
        };

        if (inst.isArrayed()) {
            forEachIndexTuple(inst.array_dims, [&](const vector<ConfigRealValue> &indices) {
                string concrete_name = child_decl_name;
                for (ConfigRealValue idx : indices) {
                    concrete_name += "__" + std::to_string(idx);
                }
                emit_child_instance(concrete_name);
            });
        } else {
            emit_child_instance(child_decl_name);
        }
    }

    std::map<string, vector<VulStaticChildServiceUse>> child_service_groups;
    for (const auto &use : ctx.module.child_service_uses) {
        child_service_groups[use.alias_name].push_back(use);
    }
    for (const auto &group_entry : child_service_groups) {
        const string &alias_name = group_entry.first;
        const auto &group = group_entry.second;
        const auto &target_child = findChildTemplateByName(ctx.module, group.front().instance_name);
        auto serv_iter = target_child.services.find(group.front().service_name);
        if (serv_iter == target_child.services.end()) {
            throw VulException("Service " + group.front().service_name + " not found in child " + target_child.module_name);
        }
        const auto &serv = serv_iter->second;
        vector<ArgPort> arg_ports;
        vector<ArgPort> ret_ports;
        for (const auto &arg : serv.args) {
            arg_ports.push_back(procArg(arg, ctx.local_bundlelib));
        }
        for (const auto &ret : serv.rets) {
            ret_ports.push_back(procArg(ret, ctx.local_bundlelib));
        }

        const string helper_struct_name = "__ChildServiceHelper__" + alias_name;
        const string helper_var_name = "__child_service_helper__" + alias_name;
        const bool is_alias_arrayed = group.front().alias_indexed;

        ctx.hls_header.push_back("struct " + helper_struct_name + " {\n");
        for (size_t i = 0; i < group.size(); ++i) {
            ctx.hls_header.push_back("  bool & vld_ports_" + std::to_string(i) + ";\n");
            if (serv.has_handshake) {
                ctx.hls_header.push_back("  const bool & rdy_ports_" + std::to_string(i) + ";\n");
            }
            for (const auto &arg : arg_ports) {
                ctx.hls_header.push_back("  Int<" + std::to_string(arg.width) + "> & arg_" + arg.name + "_" + std::to_string(i) + ";\n");
            }
            for (const auto &ret : ret_ports) {
                ctx.hls_header.push_back("  const Int<" + std::to_string(ret.width) + "> & ret_" + ret.name + "_" + std::to_string(i) + ";\n");
            }
        }
        ctx.hls_header.push_back("\n");
        ctx.hls_header.push_back("  template <uint32_t IDX = 0>\n");
        ctx.hls_header.push_back("  " + serv.returnType() + " call(" + serv.signatureArgOnly() + ") {\n");
        ctx.hls_header.push_back("    static_assert(IDX < " + std::to_string(is_alias_arrayed ? group.size() : 1) + ", \"Implicit request index out of range\");\n");
        for (size_t i = 0; i < group.size(); ++i) {
            string branch = (i == 0 ? "if constexpr" : "else if constexpr");
            ctx.hls_header.push_back("    " + branch + " (IDX == " + std::to_string(group[i].alias_index) + ") {\n");
            ctx.hls_header.push_back("      vld_ports_" + std::to_string(i) + " = true;\n");
            for (const auto &arg : arg_ports) {
                for (const auto &field : arg.flat_fields) {
                    ctx.hls_header.push_back("      " + uintExtractExpr("arg_" + arg.name + "_" + std::to_string(i), field.offset + field.width - 1, field.offset) + " = " + field.name + ";\n");
                }
            }
            for (const auto &ret : ret_ports) {
                for (const auto &field : ret.flat_fields) {
                    ctx.hls_header.push_back("      " + field.name + " = " + uintExtractExpr("ret_" + ret.name + "_" + std::to_string(i), field.offset + field.width - 1, field.offset) + ";\n");
                }
            }
            if (serv.has_handshake) {
                ctx.hls_header.push_back("      return rdy_ports_" + std::to_string(i) + ";\n");
            } else if (serv.returnType() == "void") {
                ctx.hls_header.push_back("      return;\n");
            }
            ctx.hls_header.push_back("    }\n");
        }
        ctx.hls_header.push_back("  }\n");
        ctx.hls_header.push_back("};\n");

        ctx.hls_helpers.push_back(helper_struct_name + " " + helper_var_name + "{\n");
        for (size_t i = 0; i < group.size(); ++i) {
            const string signal_base = childServiceSignalBase(group[i].instance_name, group[i].service_name);
            ctx.hls_helpers.push_back("  " + reqservVldPort(signal_base) + ",\n");
            if (serv.has_handshake) {
                ctx.hls_helpers.push_back("  " + reqservRdyPort(signal_base) + ",\n");
            }
            for (const auto &arg : arg_ports) {
                ctx.hls_helpers.push_back("  " + reqservArgPort(signal_base, arg.name) + ",\n");
            }
            for (const auto &ret : ret_ports) {
                ctx.hls_helpers.push_back("  " + reqservArgPort(signal_base, ret.name) + ",\n");
            }
        }
        ctx.hls_helpers.push_back("};\n");
        ctx.hls_helpers.push_back("#define " + alias_name + " " + helper_var_name + ".call\n");
        ctx.hls_final.push_back("#undef " + alias_name + "\n");
    }

    std::map<string, vector<VulStaticChildQueryUse>> child_query_groups;
    for (const auto &use : ctx.module.child_query_uses) {
        child_query_groups[use.alias_name].push_back(use);
    }
    for (const auto &group_entry : child_query_groups) {
        const string &alias_name = group_entry.first;
        const auto &group = group_entry.second;
        const auto &target_child = findChildTemplateByName(ctx.module, group.front().instance_name);
        auto query_iter = target_child.queries.find(group.front().query_name);
        if (query_iter == target_child.queries.end()) {
            throw VulException("Query " + group.front().query_name + " not found in child " + target_child.module_name);
        }
        if (!(group.front().ret_type == query_iter->second.ret_type)) {
            throw VulException("USE_CHILD_QUERY return type mismatch for alias " + alias_name);
        }

        uint32_t width = 0;
        vector<FlatField> flat_fields;
        flatten_type_signature(query_iter->second.ret_type, ctx.local_bundlelib, "value", width, flat_fields);

        const string helper_struct_name = "__ChildQueryHelper__" + alias_name;
        const string helper_var_name = "__child_query_helper__" + alias_name;
        const bool is_alias_arrayed = group.front().alias_indexed;
        const string ret_type_str = query_iter->second.ret_type.toString();

        ctx.hls_header.push_back("struct " + helper_struct_name + " {\n");
        for (size_t i = 0; i < group.size(); ++i) {
            ctx.hls_header.push_back("  const Int<" + std::to_string(width) + "> & value_" + std::to_string(i) + ";\n");
        }
        ctx.hls_header.push_back("\n");
        if (is_alias_arrayed) {
            ctx.hls_header.push_back("  template <uint32_t IDX = 0>\n");
            ctx.hls_header.push_back("  " + ret_type_str + " call() const {\n");
            ctx.hls_header.push_back("    static_assert(IDX < " + std::to_string(group.size()) + ", \"Child query index out of range\");\n");
            for (size_t i = 0; i < group.size(); ++i) {
                string branch = (i == 0 ? "if constexpr" : "else if constexpr");
                ctx.hls_header.push_back("    " + branch + " (IDX == " + std::to_string(group[i].alias_index) + ") {\n");
                ctx.hls_header.push_back("      " + ret_type_str + " value;\n");
                for (const auto &field : flat_fields) {
                    ctx.hls_header.push_back("      " + field.name + " = " + uintExtractExpr("value_" + std::to_string(i), field.offset + field.width - 1, field.offset) + ";\n");
                }
                ctx.hls_header.push_back("      return value;\n");
                ctx.hls_header.push_back("    }\n");
            }
            ctx.hls_header.push_back("  }\n");
        } else {
            ctx.hls_header.push_back("  " + ret_type_str + " call() const {\n");
            ctx.hls_header.push_back("    " + ret_type_str + " value;\n");
            for (const auto &field : flat_fields) {
                ctx.hls_header.push_back("    " + field.name + " = " + uintExtractExpr("value_0", field.offset + field.width - 1, field.offset) + ";\n");
            }
            ctx.hls_header.push_back("    return value;\n");
            ctx.hls_header.push_back("  }\n");
        }
        ctx.hls_header.push_back("};\n");

        ctx.hls_helpers.push_back(helper_struct_name + " " + helper_var_name + "{\n");
        for (size_t i = 0; i < group.size(); ++i) {
            const string signal_base = childQuerySignalBase(group[i].instance_name, group[i].query_name);
            ctx.hls_helpers.push_back("  " + queryPort(signal_base) + ",\n");
        }
        ctx.hls_helpers.push_back("};\n");
        ctx.hls_helpers.push_back("#define " + alias_name + " " + helper_var_name + ".call\n");
        ctx.hls_final.push_back("#undef " + alias_name + "\n");
    }
}

void _procQueues(RTLGenContext &ctx) {

    auto clog2 = [](uint32_t x) -> uint32_t {
        uint32_t res = 0;
        while ((1U << res) < x) {
            res++;
        }
        return res;
    };

    for (const auto &que : ctx.module.queues) {

        VulErrorContextGuard que_guard("processing queue " + que.name);

        string que_name = que.name;

        bool is_mp = (que.deq_width > 1) || (que.enq_width > 1);

        string port_enqready = que_name + "__enqready__";
        string port_deqready = que_name + "__deqready__";
        string port_enqvalid = que_name + "__enqvalid__";
        string port_deqvalid = que_name + "__deqvalid__";
        string port_enqdata = que_name + "__enqdata__";
        string port_deqdata = que_name + "__deqdata__";
        string port_clrnext = que_name + "__clrnext__";

        uint32_t data_width = 0;
        vector<FlatField> data_fields;
        flatten_type_signature(que.type, ctx.local_bundlelib, "value", data_width, data_fields);
        
        string proxy_class_name = "QueueProxy_" + que_name + "__";
        string data_width_str = std::to_string(data_width);
        string data_width_m1_str = std::to_string(data_width - 1);
        string depth_str = std::to_string(que.depth);
        string enq_width_str = std::to_string(que.enq_width);
        string deq_width_str = std::to_string(que.deq_width);
        string enq_cnt_width_str = std::to_string(clog2(que.enq_width + 1));
        string deq_cnt_width_str = std::to_string(clog2(que.deq_width + 1));
        string data_type_str = que.type.toString();

        if (que.depth == 0) {
            throw VulException("Queue must have positive depth");
        }
        if (que.enq_width == 0 || que.deq_width == 0) {
            throw VulException("Queue must have positive enq_width and deq_width");
        }

        if (!is_mp) {
            string enqready_type_str = "bool";
            string deqvalid_type_str = "bool";
            string enqvalid_type_str = "bool";
            string deqready_type_str = "bool";
            string enqdata_type_str = "Int<" + data_width_str + ">";
            string deqdata_type_str = "Int<" + data_width_str + ">";

            ctx.hls_header.push_back("struct " + proxy_class_name + " {\n");
            ctx.hls_header.push_back("  const " + enqready_type_str + " " + port_enqready + ";\n");
            ctx.hls_header.push_back("  const " + deqvalid_type_str + " " + port_deqvalid + ";\n");
            ctx.hls_header.push_back("  " + enqvalid_type_str + " &" + port_enqvalid + ";\n");
            ctx.hls_header.push_back("  " + deqready_type_str + " &" + port_deqready + ";\n");
            ctx.hls_header.push_back("  " + enqdata_type_str + " &" + port_enqdata + ";\n");
            ctx.hls_header.push_back("  const " + deqdata_type_str + " " + port_deqdata + ";\n");
            ctx.hls_header.push_back("  bool &" + port_clrnext + ";\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  " + 
                proxy_class_name + 
                "(const " + enqready_type_str + " " + port_enqready + 
                ", const " + deqvalid_type_str + " " + port_deqvalid + 
                ", " + enqvalid_type_str + " &" + port_enqvalid + 
                ", " + deqready_type_str + " &" + port_deqready + 
                ", " + enqdata_type_str + " &" + port_enqdata + 
                ", const " + deqdata_type_str + " " + port_deqdata + 
                ", bool &" + port_clrnext + 
                ") : " + 
                port_enqready + "(" + port_enqready + "), " + 
                port_deqvalid + "(" + port_deqvalid + "), " + 
                port_enqvalid + "(" + port_enqvalid + "), " + 
                port_deqready + "(" + port_deqready + "), " + 
                port_enqdata + "(" + port_enqdata + "), " + 
                port_deqdata + "(" + port_deqdata + "), " + 
                port_clrnext + "(" + port_clrnext + ") {}\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  bool enqready() const {\n");
            ctx.hls_header.push_back("    return " + port_enqready + ";\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  bool deqvalid() const {\n");
            ctx.hls_header.push_back("    return " + port_deqvalid + ";\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  void enqnext(const " + data_type_str + " &value) {\n");
            ctx.hls_header.push_back("    assert(enqready());\n");
            for (const auto &field : data_fields) {
                ctx.hls_header.push_back("    " + uintExtractExpr(port_enqdata, field.offset + field.width - 1, field.offset) + " = " + field.name + ";\n");
            }
            ctx.hls_header.push_back("    " + port_enqvalid + " = true;\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  " + data_type_str + " front() const {\n");
            ctx.hls_header.push_back("    " + data_type_str + " value;\n");
            for (const auto &field : data_fields) {
                ctx.hls_header.push_back("    " + field.name + " = " + uintExtractExpr(port_deqdata, field.offset + field.width - 1, field.offset) + ";\n");
            }
            ctx.hls_header.push_back("    return value;\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  void deqnext() {\n");
            ctx.hls_header.push_back("    assert(deqvalid());\n");
            ctx.hls_header.push_back("    " + port_deqready + " = true;\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  void clrnext() {\n");
            ctx.hls_header.push_back("    " + port_clrnext + " = true;\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("};\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_arguments.push_back("const " + enqready_type_str + " " + port_enqready);
            ctx.hls_arguments.push_back("const " + deqvalid_type_str + " " + port_deqvalid);
            ctx.hls_arguments.push_back(enqvalid_type_str + " &" + port_enqvalid);
            ctx.hls_arguments.push_back(deqready_type_str + " &" + port_deqready);
            ctx.hls_arguments.push_back(enqdata_type_str + " &" + port_enqdata);
            ctx.hls_arguments.push_back("const " + deqdata_type_str + " " + port_deqdata);
            ctx.hls_arguments.push_back("bool &" + port_clrnext);

            ctx.hls_init.push_back(port_enqvalid + " = false;\n");
            ctx.hls_init.push_back(port_deqready + " = false;\n");
            ctx.hls_init.push_back(port_clrnext + " = false;\n");
            ctx.hls_init.push_back(proxy_class_name + " " + que_name + "(" + port_enqready + ", " + port_deqvalid + ", " + port_enqvalid + ", " + port_deqready + ", " + port_enqdata + ", " + port_deqdata + ", " + port_clrnext + ");\n");

            ctx.rtl_decl.push_back("wire " + port_enqready + ";\n");
            ctx.rtl_decl.push_back("wire " + port_deqvalid + ";\n");
            ctx.rtl_decl.push_back("wire " + port_enqvalid + ";\n");
            ctx.rtl_decl.push_back("wire " + port_deqready + ";\n");
            ctx.rtl_decl.push_back("wire [" + data_width_m1_str + ":0] " + port_enqdata + ";\n");
            ctx.rtl_decl.push_back("wire [" + data_width_m1_str + ":0] " + port_deqdata + ";\n");
            ctx.rtl_decl.push_back("wire " + port_clrnext + ";\n");

            ctx.rtl_logicports.push_back("." + port_enqready + "(" + port_enqready + ")");
            ctx.rtl_logicports.push_back("." + port_deqvalid + "(" + port_deqvalid + ")");
            ctx.rtl_logicports.push_back("." + port_enqvalid + "(" + port_enqvalid + ")");
            ctx.rtl_logicports.push_back("." + port_deqready + "(" + port_deqready + ")");
            ctx.rtl_logicports.push_back("." + port_enqdata + "(" + port_enqdata + ")");
            ctx.rtl_logicports.push_back("." + port_deqdata + "(" + port_deqdata + ")");
            ctx.rtl_logicports.push_back("." + port_clrnext + "(" + port_clrnext + ")");

            ctx.rtl_inst.push_back("VulQueue #(\n");
            ctx.rtl_inst.push_back("  .Width(" + data_width_str + "),\n");
            ctx.rtl_inst.push_back("  .Depth(" + depth_str + ")\n");
            ctx.rtl_inst.push_back(") " + que_name + "__inst (\n");
            ctx.rtl_inst.push_back("  .clk(clk),\n");
            ctx.rtl_inst.push_back("  .rstn(rstn),\n");
            ctx.rtl_inst.push_back("  .enqready(" + port_enqready + "),\n");
            ctx.rtl_inst.push_back("  .deqready(" + port_deqvalid + "),\n");
            ctx.rtl_inst.push_back("  .enqnext_vld(" + port_enqvalid + "),\n");
            ctx.rtl_inst.push_back("  .enqnext_data(" + port_enqdata + "),\n");
            ctx.rtl_inst.push_back("  .deqnext_vld(" + port_deqready + "),\n");
            ctx.rtl_inst.push_back("  .deqnext_data(" + port_deqdata + "),\n");
            ctx.rtl_inst.push_back("  .clrnext(" + port_clrnext + ")\n");
            ctx.rtl_inst.push_back(");\n");
        } else {
            string enqready_type_str = "uint32_t";
            string deqvalid_type_str = "uint32_t";
            string enqvalid_type_str = "Int<" + enq_cnt_width_str + ">";
            string deqready_type_str = "Int<" + deq_cnt_width_str + ">";
            string enqdata_type_str = "std::array<Int<" + data_width_str + ">, " + enq_width_str + ">";
            string deqdata_type_str = "std::array<Int<" + data_width_str + ">, " + deq_width_str + ">";

            ctx.hls_header.push_back("struct " + proxy_class_name + " {\n");
            ctx.hls_header.push_back("  const " + enqready_type_str + " " + port_enqready + ";\n");
            ctx.hls_header.push_back("  const " + deqvalid_type_str + " " + port_deqvalid + ";\n");
            ctx.hls_header.push_back("  " + enqvalid_type_str + " &" + port_enqvalid + ";\n");
            ctx.hls_header.push_back("  " + deqready_type_str + " &" + port_deqready + ";\n");
            ctx.hls_header.push_back("  " + enqdata_type_str + " &" + port_enqdata + ";\n");
            ctx.hls_header.push_back("  const " + deqdata_type_str + " &" + port_deqdata + ";\n");
            ctx.hls_header.push_back("  bool &" + port_clrnext + ";\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  " + proxy_class_name + 
                "(const " + enqready_type_str + " " + port_enqready + 
                ", const " + deqvalid_type_str + " " + port_deqvalid + 
                ", " + enqvalid_type_str + " &" + port_enqvalid + 
                ", " + deqready_type_str + " &" + port_deqready + 
                ", " + enqdata_type_str + " &" + port_enqdata + 
                ", const " + deqdata_type_str + 
                " &" + port_deqdata + 
                ", bool &" + port_clrnext + 
                ") : " + 
                port_enqready + "(" + port_enqready + "), " + 
                port_deqvalid + "(" + port_deqvalid + "), " + 
                port_enqvalid + "(" + port_enqvalid + "), " + 
                port_deqready + "(" + port_deqready + "), " + 
                port_enqdata + "(" + port_enqdata + "), " + 
                port_deqdata + "(" + port_deqdata + "), " + 
                port_clrnext + "(" + port_clrnext + ") {}\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  " + enqready_type_str + " enqreqdy() const {\n");
            ctx.hls_header.push_back("    return " + port_enqready + ".template to<uint32_t>();\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  " + deqvalid_type_str + " deqvalid() const {\n");
            ctx.hls_header.push_back("    return " + port_deqvalid + ".template to<uint32_t>();\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  void enqnext(const " + enqdata_type_str + " &values, const " + enqready_type_str + " num = " + enq_width_str + ") {\n");
            ctx.hls_header.push_back("    const " + enqready_type_str + " req = (num < " + enq_width_str + ") ? num : " + enq_width_str + ";\n");
            ctx.hls_header.push_back("    const " + enqready_type_str + " rdy = enqreqdy();\n");
            ctx.hls_header.push_back("    assert(req <= rdy);\n");
            for (uint32_t i = 0; i < (uint32_t)que.enq_width; ++i) {
                ctx.hls_header.push_back("    if (req > " + std::to_string(i) + ") {\n");
                ctx.hls_header.push_back("      const " + data_type_str + " &value = values[" + std::to_string(i) + "];\n");
                for (const auto &field : data_fields) {
                    ctx.hls_header.push_back("      " + uintExtractExpr(port_enqdata + "[" + std::to_string(i) + "]", field.offset + field.width - 1, field.offset) + " = " + field.name + ";\n");
                }
                ctx.hls_header.push_back("    }\n");
            }
            ctx.hls_header.push_back("    " + port_enqvalid + " = static_cast<uint32_t>(req);\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  " + deqdata_type_str + " front(const " + deqvalid_type_str + " num = " + deq_width_str + ") const {\n");
            ctx.hls_header.push_back("    (void)num;\n");
            ctx.hls_header.push_back("    " + deqdata_type_str + " deq_buf;\n");
            for (uint32_t i = 0; i < (uint32_t)que.deq_width; ++i) {
                ctx.hls_header.push_back("    if (deqvalid() > " + std::to_string(i) + ") {\n");
                ctx.hls_header.push_back("      auto &value = deq_buf[" + std::to_string(i) + "];\n");
                for (const auto &field : data_fields) {
                    ctx.hls_header.push_back("      " + field.name + " = " + uintExtractExpr(port_deqdata + "[" + std::to_string(i) + "]", field.offset + field.width - 1, field.offset) + ";\n");
                }
                ctx.hls_header.push_back("    }\n");
            }
            ctx.hls_header.push_back("    return deq_buf;\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  void deqnext(const " + deqvalid_type_str + " num = " + deq_width_str + ") {\n");
            ctx.hls_header.push_back("    const " + deqvalid_type_str + " req = (num < " + deq_width_str + ") ? num : " + deq_width_str + ";\n");
            ctx.hls_header.push_back("    const " + deqvalid_type_str + " valid = deqvalid();\n");
            ctx.hls_header.push_back("    assert(req <= valid);\n");
            ctx.hls_header.push_back("    " + port_deqready + " = static_cast<uint32_t>(req);\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  void clrnext() {\n");
            ctx.hls_header.push_back("    " + port_clrnext + " = true;\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("};\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_arguments.push_back("const " + enqready_type_str + " " + port_enqready);
            ctx.hls_arguments.push_back("const " + deqvalid_type_str + " " + port_deqvalid);
            ctx.hls_arguments.push_back(enqvalid_type_str + " &" + port_enqvalid);
            ctx.hls_arguments.push_back(deqready_type_str + " &" + port_deqready);
            ctx.hls_arguments.push_back(enqdata_type_str + " &" + port_enqdata);
            ctx.hls_arguments.push_back("const " + deqdata_type_str + " &" + port_deqdata);
            ctx.hls_arguments.push_back("bool &" + port_clrnext);

            ctx.hls_init.push_back(port_enqvalid + " = 0;\n");
            ctx.hls_init.push_back(port_deqready + " = 0;\n");
            ctx.hls_init.push_back(port_clrnext + " = false;\n");
            ctx.hls_init.push_back(proxy_class_name + " " + que_name + "(" + port_enqready + ", " + port_deqvalid + ", " + port_enqvalid + ", " + port_deqready + ", " + port_enqdata + ", " + port_deqdata + ", " + port_clrnext + ");\n");

            ctx.rtl_decl.push_back("wire [" + enq_cnt_width_str + "-1:0] " + port_enqready + ";\n");
            ctx.rtl_decl.push_back("wire [" + deq_cnt_width_str + "-1:0] " + port_deqvalid + ";\n");
            ctx.rtl_decl.push_back("wire [" + enq_cnt_width_str + "-1:0] " + port_enqvalid + ";\n");
            ctx.rtl_decl.push_back("wire [" + deq_cnt_width_str + "-1:0] " + port_deqready + ";\n");
            ctx.rtl_decl.push_back("wire [" + data_width_m1_str + ":0] " + port_enqdata + "[" + enq_width_str + "];\n");
            ctx.rtl_decl.push_back("wire [" + data_width_m1_str + ":0] " + port_deqdata + "[" + deq_width_str + "];\n");
            ctx.rtl_decl.push_back("wire " + port_clrnext + ";\n");

            ctx.rtl_logicports.push_back("." + port_enqready + "(" + port_enqready + ")");
            ctx.rtl_logicports.push_back("." + port_deqvalid + "(" + port_deqvalid + ")");
            ctx.rtl_logicports.push_back("." + port_enqvalid + "(" + port_enqvalid + ")");
            ctx.rtl_logicports.push_back("." + port_deqready + "(" + port_deqready + ")");
            ctx.rtl_logicports.push_back("." + port_enqdata + "(" + port_enqdata + ")");
            ctx.rtl_logicports.push_back("." + port_deqdata + "(" + port_deqdata + ")");
            ctx.rtl_logicports.push_back("." + port_clrnext + "(" + port_clrnext + ")");

            ctx.rtl_inst.push_back("VulQueueMP #(\n");
            ctx.rtl_inst.push_back("  .Width(" + data_width_str + "),\n");
            ctx.rtl_inst.push_back("  .Depth(" + depth_str + "),\n");
            ctx.rtl_inst.push_back("  .EnqWidth(" + enq_width_str + "),\n");
            ctx.rtl_inst.push_back("  .DeqWidth(" + deq_width_str + ")\n");
            ctx.rtl_inst.push_back(") " + que_name + "__inst (\n");
            ctx.rtl_inst.push_back("  .clk(clk),\n");
            ctx.rtl_inst.push_back("  .rstn(rstn),\n");
            ctx.rtl_inst.push_back("  .enqready(" + port_enqready + "),\n");
            ctx.rtl_inst.push_back("  .deqready(" + port_deqvalid + "),\n");
            ctx.rtl_inst.push_back("  .enqnext_vld(" + port_enqvalid + "),\n");
            ctx.rtl_inst.push_back("  .enqnext_data(" + port_enqdata + "),\n");
            ctx.rtl_inst.push_back("  .deqnext_vld(" + port_deqready + "),\n");
            ctx.rtl_inst.push_back("  .deqnext_data(" + port_deqdata + "),\n");
            ctx.rtl_inst.push_back("  .clrnext(" + port_clrnext + ")\n");
            ctx.rtl_inst.push_back(");\n");
        }
    }
}

void _procBRAMAndROM(RTLGenContext &ctx) {
    auto escape_verilog_string = [](const string &s) -> string {
        string out;
        out.reserve(s.size());
        for (char c : s) {
            if (c == '\\' || c == '"') {
                out.push_back('\\');
            }
            out.push_back(c);
        }
        return out;
    };

    for (const auto &bram : ctx.module.brams) {
        VulErrorContextGuard bram_guard("processing bram " + bram.name);

        uint32_t data_width = 0;
        vector<FlatField> read_fields;
        flatten_type_signature(bram.data_type, ctx.local_bundlelib, "value", data_width, read_fields);
        if (data_width == 0) {
            throw VulException("BRAM data width must be positive");
        }

        if (bram.addr_size <= 1) {
            throw VulException("BRAM addr_size must be greater than 1");
        }
        if (bram.addr_width < 0) {
            throw VulException("BRAM addr_width must be non-negative");
        }
        const uint32_t addr_width = static_cast<uint32_t>(bram.addr_width);
        const bool is_1rw = (bram.read_ports == 0 || bram.write_ports == 0);

        const string bram_name = bram.name;
        const string data_type_str = bram.data_type.toString();
        const string data_width_str = std::to_string(data_width);
        const string data_width_m1_str = std::to_string(data_width - 1);
        const string addr_width_str = std::to_string(addr_width);
        const string addr_width_m1_str = std::to_string(addr_width - 1);
        const string addr_type_str = "Int<" + addr_width_str + ">";

        if (is_1rw) {
            vector<FlatField> write_fields;
            uint32_t write_width = 0;
            flatten_type_signature(bram.data_type, ctx.local_bundlelib, "write_data", write_width, write_fields);
            if (write_width != data_width) {
                throw VulException("BRAM data width mismatch while generating proxy");
            }

            const string proxy_class_name = "__BRAM1RWProxy__" + bram_name;
            const string port_s1_en = "bram_" + bram_name + "__s1_en";
            const string port_s1_we = "bram_" + bram_name + "__s1_we";
            const string port_s1_addr = "bram_" + bram_name + "__s1_addr";
            const string port_s1_wdata = "bram_" + bram_name + "__s1_wdata";
            const string port_s2_rdata = "bram_" + bram_name + "__s2_rdata";

            ctx.hls_header.push_back("struct " + proxy_class_name + " {\n");
            ctx.hls_header.push_back("  using DataType = " + data_type_str + ";\n");
            ctx.hls_header.push_back("  using AddrType = " + addr_type_str + ";\n");
            ctx.hls_header.push_back("  const Int<" + data_width_str + "> " + port_s2_rdata + ";\n");
            ctx.hls_header.push_back("  bool &" + port_s1_en + ";\n");
            ctx.hls_header.push_back("  bool &" + port_s1_we + ";\n");
            ctx.hls_header.push_back("  Int<" + addr_width_str + "> &" + port_s1_addr + ";\n");
            ctx.hls_header.push_back("  Int<" + data_width_str + "> &" + port_s1_wdata + ";\n");
            ctx.hls_header.push_back("  mutable DataType readdata_buf;\n");
            ctx.hls_header.push_back("\n");
            ctx.hls_header.push_back("  " + proxy_class_name +
                "(const Int<" + data_width_str + "> " + port_s2_rdata +
                ", bool &" + port_s1_en +
                ", bool &" + port_s1_we +
                ", Int<" + addr_width_str + "> &" + port_s1_addr +
                ", Int<" + data_width_str + "> &" + port_s1_wdata +
                ") : " +
                port_s2_rdata + "(" + port_s2_rdata + "), " +
                port_s1_en + "(" + port_s1_en + "), " +
                port_s1_we + "(" + port_s1_we + "), " +
                port_s1_addr + "(" + port_s1_addr + "), " +
                port_s1_wdata + "(" + port_s1_wdata + ") {}\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  void req(const AddrType &addr, const DataType &write_data, bool write_en) {\n");
            ctx.hls_header.push_back("    " + port_s1_en + " = true;\n");
            ctx.hls_header.push_back("    " + port_s1_we + " = write_en;\n");
            ctx.hls_header.push_back("    " + port_s1_addr + " = addr;\n");
            ctx.hls_header.push_back("    Int<" + data_width_str + "> packed;\n");
            for (const auto &field : write_fields) {
                ctx.hls_header.push_back("    " + uintExtractExpr("packed", field.offset + field.width - 1, field.offset) + " = " + field.name + ";\n");
            }
            ctx.hls_header.push_back("    " + port_s1_wdata + " = packed;\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  const DataType& readdata() const {\n");
            ctx.hls_header.push_back("    DataType value;\n");
            for (const auto &field : read_fields) {
                ctx.hls_header.push_back("    " + field.name + " = " + uintExtractExpr(port_s2_rdata, field.offset + field.width - 1, field.offset) + ";\n");
            }
            ctx.hls_header.push_back("    readdata_buf = value;\n");
            ctx.hls_header.push_back("    return readdata_buf;\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("};\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_arguments.push_back("const Int<" + data_width_str + "> " + port_s2_rdata);
            ctx.hls_arguments.push_back("bool &" + port_s1_en);
            ctx.hls_arguments.push_back("bool &" + port_s1_we);
            ctx.hls_arguments.push_back("Int<" + addr_width_str + "> &" + port_s1_addr);
            ctx.hls_arguments.push_back("Int<" + data_width_str + "> &" + port_s1_wdata);

            ctx.hls_init.push_back(port_s1_en + " = false;\n");
            ctx.hls_init.push_back(port_s1_we + " = false;\n");
            ctx.hls_init.push_back(port_s1_addr + " = 0;\n");
            ctx.hls_init.push_back(port_s1_wdata + " = 0;\n");
            ctx.hls_init.push_back(proxy_class_name + " " + bram_name + "(" + port_s2_rdata + ", " + port_s1_en + ", " + port_s1_we + ", " + port_s1_addr + ", " + port_s1_wdata + ");\n");

            ctx.rtl_decl.push_back("wire " + port_s1_en + ";\n");
            ctx.rtl_decl.push_back("wire " + port_s1_we + ";\n");
            ctx.rtl_decl.push_back("wire [" + addr_width_m1_str + ":0] " + port_s1_addr + ";\n");
            ctx.rtl_decl.push_back("wire [" + data_width_m1_str + ":0] " + port_s1_wdata + ";\n");
            ctx.rtl_decl.push_back("wire [" + data_width_m1_str + ":0] " + port_s2_rdata + ";\n");

            ctx.rtl_logicports.push_back("." + port_s1_en + "(" + port_s1_en + ")");
            ctx.rtl_logicports.push_back("." + port_s1_we + "(" + port_s1_we + ")");
            ctx.rtl_logicports.push_back("." + port_s1_addr + "(" + port_s1_addr + ")");
            ctx.rtl_logicports.push_back("." + port_s1_wdata + "(" + port_s1_wdata + ")");
            ctx.rtl_logicports.push_back("." + port_s2_rdata + "(" + port_s2_rdata + ")");

            ctx.rtl_inst.push_back("VulBRAM1RW #(\n");
            ctx.rtl_inst.push_back("  .DataWidth(" + data_width_str + "),\n");
            ctx.rtl_inst.push_back("  .AddrWidth(" + addr_width_str + ")\n");
            ctx.rtl_inst.push_back(") " + bram_name + "__inst (\n");
            ctx.rtl_inst.push_back("  .clk(clk),\n");
            ctx.rtl_inst.push_back("  .rstn(rstn),\n");
            ctx.rtl_inst.push_back("  .s1_en(" + port_s1_en + "),\n");
            ctx.rtl_inst.push_back("  .s1_we(" + port_s1_we + "),\n");
            ctx.rtl_inst.push_back("  .s1_addr(" + port_s1_addr + "),\n");
            ctx.rtl_inst.push_back("  .s1_wdata(" + port_s1_wdata + "),\n");
            ctx.rtl_inst.push_back("  .s2_rdata(" + port_s2_rdata + ")\n");
            ctx.rtl_inst.push_back(");\n");
        } else {
            if (bram.read_ports <= 0 || bram.write_ports <= 0) {
                throw VulException("BRAM read_ports and write_ports must be positive");
            }
            const uint32_t read_ports = static_cast<uint32_t>(bram.read_ports);
            const uint32_t write_ports = static_cast<uint32_t>(bram.write_ports);
            const string read_ports_str = std::to_string(read_ports);
            const string write_ports_str = std::to_string(write_ports);

            vector<FlatField> write_fields;
            uint32_t write_width = 0;
            flatten_type_signature(bram.data_type, ctx.local_bundlelib, "data", write_width, write_fields);
            if (write_width != data_width) {
                throw VulException("BRAM data width mismatch while generating proxy");
            }

            const string proxy_class_name = "__BRAMProxy__" + bram_name;
            const string port_s1_readreq = "bram_" + bram_name + "__s1_readreq";
            const string port_s1_readaddr = "bram_" + bram_name + "__s1_readaddr";
            const string port_s2_readdata = "bram_" + bram_name + "__s2_readdata";
            const string port_s1_write = "bram_" + bram_name + "__s1_write";
            const string port_s1_writeaddr = "bram_" + bram_name + "__s1_writeaddr";
            const string port_s1_writedata = "bram_" + bram_name + "__s1_writedata";

            const string readreq_type_str = "std::array<bool, " + read_ports_str + ">";
            const string readaddr_type_str = "std::array<Int<" + addr_width_str + ">, " + read_ports_str + ">";
            const string readdata_type_str = "std::array<Int<" + data_width_str + ">, " + read_ports_str + ">";
            const string write_type_str = "std::array<bool, " + write_ports_str + ">";
            const string writeaddr_type_str = "std::array<Int<" + addr_width_str + ">, " + write_ports_str + ">";
            const string writedata_type_str = "std::array<Int<" + data_width_str + ">, " + write_ports_str + ">";

            ctx.hls_header.push_back("struct " + proxy_class_name + " {\n");
            ctx.hls_header.push_back("  using DataType = " + data_type_str + ";\n");
            ctx.hls_header.push_back("  using AddrType = " + addr_type_str + ";\n");
            ctx.hls_header.push_back("  " + readreq_type_str + " &" + port_s1_readreq + ";\n");
            ctx.hls_header.push_back("  " + readaddr_type_str + " &" + port_s1_readaddr + ";\n");
            ctx.hls_header.push_back("  const " + readdata_type_str + " " + port_s2_readdata + ";\n");
            ctx.hls_header.push_back("  " + write_type_str + " &" + port_s1_write + ";\n");
            ctx.hls_header.push_back("  " + writeaddr_type_str + " &" + port_s1_writeaddr + ";\n");
            ctx.hls_header.push_back("  " + writedata_type_str + " &" + port_s1_writedata + ";\n");
            ctx.hls_header.push_back("  mutable std::array<DataType, " + read_ports_str + "> readdata_buf;\n");
            ctx.hls_header.push_back("\n");
            ctx.hls_header.push_back("  " + proxy_class_name +
                "(" + readreq_type_str + " &" + port_s1_readreq +
                ", " + readaddr_type_str + " &" + port_s1_readaddr +
                ", const " + readdata_type_str + " " + port_s2_readdata +
                ", " + write_type_str + " &" + port_s1_write +
                ", " + writeaddr_type_str + " &" + port_s1_writeaddr +
                ", " + writedata_type_str + " &" + port_s1_writedata +
                ") : " +
                port_s1_readreq + "(" + port_s1_readreq + "), " +
                port_s1_readaddr + "(" + port_s1_readaddr + "), " +
                port_s2_readdata + "(" + port_s2_readdata + "), " +
                port_s1_write + "(" + port_s1_write + "), " +
                port_s1_writeaddr + "(" + port_s1_writeaddr + "), " +
                port_s1_writedata + "(" + port_s1_writedata + ") {}\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  template <uint32_t PortIndex>\n");
            ctx.hls_header.push_back("  void readreq(const AddrType &addr) {\n");
            ctx.hls_header.push_back("    static_assert(PortIndex < " + read_ports_str + ", \"Read port index out of range\");\n");
            ctx.hls_header.push_back("    " + port_s1_readreq + "[PortIndex] = true;\n");
            ctx.hls_header.push_back("    " + port_s1_readaddr + "[PortIndex] = addr;\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  template <uint32_t PortIndex>\n");
            ctx.hls_header.push_back("  const DataType& readdata() const {\n");
            ctx.hls_header.push_back("    static_assert(PortIndex < " + read_ports_str + ", \"Read port index out of range\");\n");
            ctx.hls_header.push_back("    DataType value;\n");
            for (const auto &field : read_fields) {
                ctx.hls_header.push_back("    " + field.name + " = " + uintExtractExpr(port_s2_readdata + "[PortIndex]", field.offset + field.width - 1, field.offset) + ";\n");
            }
            ctx.hls_header.push_back("    readdata_buf[PortIndex] = value;\n");
            ctx.hls_header.push_back("    return readdata_buf[PortIndex];\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  template <uint32_t PortIndex>\n");
            ctx.hls_header.push_back("  void write(const AddrType &addr, const DataType &data) {\n");
            ctx.hls_header.push_back("    static_assert(PortIndex < " + write_ports_str + ", \"Write port index out of range\");\n");
            ctx.hls_header.push_back("    " + port_s1_write + "[PortIndex] = true;\n");
            ctx.hls_header.push_back("    " + port_s1_writeaddr + "[PortIndex] = addr;\n");
            ctx.hls_header.push_back("    Int<" + data_width_str + "> packed;\n");
            for (const auto &field : write_fields) {
                ctx.hls_header.push_back("    " + uintExtractExpr("packed", field.offset + field.width - 1, field.offset) + " = " + field.name + ";\n");
            }
            ctx.hls_header.push_back("    " + port_s1_writedata + "[PortIndex] = packed;\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("};\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_arguments.push_back(readreq_type_str + " &" + port_s1_readreq);
            ctx.hls_arguments.push_back(readaddr_type_str + " &" + port_s1_readaddr);
            ctx.hls_arguments.push_back("const " + readdata_type_str + " " + port_s2_readdata);
            ctx.hls_arguments.push_back(write_type_str + " &" + port_s1_write);
            ctx.hls_arguments.push_back(writeaddr_type_str + " &" + port_s1_writeaddr);
            ctx.hls_arguments.push_back(writedata_type_str + " &" + port_s1_writedata);

            for (uint32_t i = 0; i < read_ports; ++i) {
                ctx.hls_init.push_back(port_s1_readreq + "[" + std::to_string(i) + "] = false;\n");
                ctx.hls_init.push_back(port_s1_readaddr + "[" + std::to_string(i) + "] = 0;\n");
            }
            for (uint32_t i = 0; i < write_ports; ++i) {
                ctx.hls_init.push_back(port_s1_write + "[" + std::to_string(i) + "] = false;\n");
                ctx.hls_init.push_back(port_s1_writeaddr + "[" + std::to_string(i) + "] = 0;\n");
                ctx.hls_init.push_back(port_s1_writedata + "[" + std::to_string(i) + "] = 0;\n");
            }
            ctx.hls_init.push_back(proxy_class_name + " " + bram_name + "(" + port_s1_readreq + ", " + port_s1_readaddr + ", " + port_s2_readdata + ", " + port_s1_write + ", " + port_s1_writeaddr + ", " + port_s1_writedata + ");\n");

            ctx.rtl_decl.push_back("wire " + port_s1_readreq + "[" + read_ports_str + "];\n");
            ctx.rtl_decl.push_back("wire [" + addr_width_m1_str + ":0] " + port_s1_readaddr + "[" + read_ports_str + "];\n");
            ctx.rtl_decl.push_back("wire [" + data_width_m1_str + ":0] " + port_s2_readdata + "[" + read_ports_str + "];\n");
            ctx.rtl_decl.push_back("wire " + port_s1_write + "[" + write_ports_str + "];\n");
            ctx.rtl_decl.push_back("wire [" + addr_width_m1_str + ":0] " + port_s1_writeaddr + "[" + write_ports_str + "];\n");
            ctx.rtl_decl.push_back("wire [" + data_width_m1_str + ":0] " + port_s1_writedata + "[" + write_ports_str + "];\n");

            ctx.rtl_logicports.push_back("." + port_s1_readreq + "(" + port_s1_readreq + ")");
            ctx.rtl_logicports.push_back("." + port_s1_readaddr + "(" + port_s1_readaddr + ")");
            ctx.rtl_logicports.push_back("." + port_s2_readdata + "(" + port_s2_readdata + ")");
            ctx.rtl_logicports.push_back("." + port_s1_write + "(" + port_s1_write + ")");
            ctx.rtl_logicports.push_back("." + port_s1_writeaddr + "(" + port_s1_writeaddr + ")");
            ctx.rtl_logicports.push_back("." + port_s1_writedata + "(" + port_s1_writedata + ")");

            ctx.rtl_inst.push_back("VulBRAM #(\n");
            ctx.rtl_inst.push_back("  .DataWidth(" + data_width_str + "),\n");
            ctx.rtl_inst.push_back("  .AddrWidth(" + addr_width_str + "),\n");
            ctx.rtl_inst.push_back("  .ReadPorts(" + read_ports_str + "),\n");
            ctx.rtl_inst.push_back("  .WritePorts(" + write_ports_str + ")\n");
            ctx.rtl_inst.push_back(") " + bram_name + "__inst (\n");
            ctx.rtl_inst.push_back("  .clk(clk),\n");
            ctx.rtl_inst.push_back("  .rstn(rstn),\n");
            ctx.rtl_inst.push_back("  .s1_readreq(" + port_s1_readreq + "),\n");
            ctx.rtl_inst.push_back("  .s1_readaddr(" + port_s1_readaddr + "),\n");
            ctx.rtl_inst.push_back("  .s2_readdata(" + port_s2_readdata + "),\n");
            ctx.rtl_inst.push_back("  .s1_write(" + port_s1_write + "),\n");
            ctx.rtl_inst.push_back("  .s1_writeaddr(" + port_s1_writeaddr + "),\n");
            ctx.rtl_inst.push_back("  .s1_writedata(" + port_s1_writedata + ")\n");
            ctx.rtl_inst.push_back(");\n");
        }
    }

    for (const auto &rom : ctx.module.roms) {
        VulErrorContextGuard rom_guard("processing rom " + rom.name);

        if (rom.data_width <= 0) {
            throw VulException("ROM data_width must be positive");
        }
        if (rom.addr_size <= 1) {
            throw VulException("ROM addr_size must be greater than 1");
        }
        if (rom.addr_width < 0) {
            throw VulException("ROM addr_width must be non-negative");
        }
        if (rom.read_ports <= 0) {
            throw VulException("ROM read_ports must be positive");
        }

        ctx.resource_files.push_back(rom.init_path);

        const uint32_t data_width = static_cast<uint32_t>(rom.data_width);
        const uint32_t addr_width = static_cast<uint32_t>(rom.addr_width);
        const uint32_t read_ports = static_cast<uint32_t>(rom.read_ports);

        const string rom_name = rom.name;
        const string data_width_str = std::to_string(data_width);
        const string data_width_m1_str = std::to_string(data_width - 1);
        const string addr_width_str = std::to_string(addr_width);
        const string addr_width_m1_str = std::to_string(addr_width - 1);
        const string read_ports_str = std::to_string(read_ports);
        const string init_path_escaped = escape_verilog_string(rom.init_path);

        const string proxy_class_name = "__ROMProxy__" + rom_name;
        const string port_s1_readreq = "rom_" + rom_name + "__s1_readreq";
        const string port_s1_readaddr = "rom_" + rom_name + "__s1_readaddr";
        const string port_s2_readdata = "rom_" + rom_name + "__s2_readdata";

        const string data_type_str = "Int<" + data_width_str + ">";
        const string addr_type_str = "Int<" + addr_width_str + ">";
        const string readreq_type_str = "std::array<bool, " + read_ports_str + ">";
        const string readaddr_type_str = "std::array<Int<" + addr_width_str + ">, " + read_ports_str + ">";
        const string readdata_type_str = "std::array<Int<" + data_width_str + ">, " + read_ports_str + ">";

        ctx.hls_header.push_back("struct " + proxy_class_name + " {\n");
        ctx.hls_header.push_back("  using DataType = " + data_type_str + ";\n");
        ctx.hls_header.push_back("  using AddrType = " + addr_type_str + ";\n");
        ctx.hls_header.push_back("  " + readreq_type_str + " &" + port_s1_readreq + ";\n");
        ctx.hls_header.push_back("  " + readaddr_type_str + " &" + port_s1_readaddr + ";\n");
        ctx.hls_header.push_back("  const " + readdata_type_str + " " + port_s2_readdata + ";\n");
        ctx.hls_header.push_back("\n");
        ctx.hls_header.push_back("  " + proxy_class_name +
            "(" + readreq_type_str + " &" + port_s1_readreq +
            ", " + readaddr_type_str + " &" + port_s1_readaddr +
            ", const " + readdata_type_str + " " + port_s2_readdata +
            ") : " +
            port_s1_readreq + "(" + port_s1_readreq + "), " +
            port_s1_readaddr + "(" + port_s1_readaddr + "), " +
            port_s2_readdata + "(" + port_s2_readdata + ") {}\n");
        ctx.hls_header.push_back("\n");

        ctx.hls_header.push_back("  template <uint32_t PortIndex>\n");
        ctx.hls_header.push_back("  void readreq(const AddrType &addr) {\n");
        ctx.hls_header.push_back("    static_assert(PortIndex < " + read_ports_str + ", \"Read port index out of range\");\n");
        ctx.hls_header.push_back("    " + port_s1_readreq + "[PortIndex] = true;\n");
        ctx.hls_header.push_back("    " + port_s1_readaddr + "[PortIndex] = addr;\n");
        ctx.hls_header.push_back("  }\n");
        ctx.hls_header.push_back("\n");

        ctx.hls_header.push_back("  template <uint32_t PortIndex>\n");
        ctx.hls_header.push_back("  const DataType readdata() const {\n");
        ctx.hls_header.push_back("    static_assert(PortIndex < " + read_ports_str + ", \"Read port index out of range\");\n");
        ctx.hls_header.push_back("    return " + port_s2_readdata + "[PortIndex];\n");
        ctx.hls_header.push_back("  }\n");
        ctx.hls_header.push_back("};\n");
        ctx.hls_header.push_back("\n");

        ctx.hls_arguments.push_back(readreq_type_str + " &" + port_s1_readreq);
        ctx.hls_arguments.push_back(readaddr_type_str + " &" + port_s1_readaddr);
        ctx.hls_arguments.push_back("const " + readdata_type_str + " " + port_s2_readdata);

        for (uint32_t i = 0; i < read_ports; ++i) {
            ctx.hls_init.push_back(port_s1_readreq + "[" + std::to_string(i) + "] = false;\n");
            ctx.hls_init.push_back(port_s1_readaddr + "[" + std::to_string(i) + "] = 0;\n");
        }
        ctx.hls_init.push_back(proxy_class_name + " " + rom_name + "(" + port_s1_readreq + ", " + port_s1_readaddr + ", " + port_s2_readdata + ");\n");

        ctx.rtl_decl.push_back("wire " + port_s1_readreq + "[" + read_ports_str + "];\n");
        ctx.rtl_decl.push_back("wire [" + addr_width_m1_str + ":0] " + port_s1_readaddr + "[" + read_ports_str + "];\n");
        ctx.rtl_decl.push_back("wire [" + data_width_m1_str + ":0] " + port_s2_readdata + "[" + read_ports_str + "];\n");

        ctx.rtl_logicports.push_back("." + port_s1_readreq + "(" + port_s1_readreq + ")");
        ctx.rtl_logicports.push_back("." + port_s1_readaddr + "(" + port_s1_readaddr + ")");
        ctx.rtl_logicports.push_back("." + port_s2_readdata + "(" + port_s2_readdata + ")");

        ctx.rtl_inst.push_back("VulROM #(\n");
        ctx.rtl_inst.push_back("  .DataWidth(" + data_width_str + "),\n");
        ctx.rtl_inst.push_back("  .AddrWidth(" + addr_width_str + "),\n");
        ctx.rtl_inst.push_back("  .ReadPorts(" + read_ports_str + "),\n");
        ctx.rtl_inst.push_back("  .ReadMemHPath(\"" + init_path_escaped + "\")\n");
        ctx.rtl_inst.push_back(") " + rom_name + "__inst (\n");
        ctx.rtl_inst.push_back("  .clk(clk),\n");
        ctx.rtl_inst.push_back("  .rstn(rstn),\n");
        ctx.rtl_inst.push_back("  .s1_readreq(" + port_s1_readreq + "),\n");
        ctx.rtl_inst.push_back("  .s1_readaddr(" + port_s1_readaddr + "),\n");
        ctx.rtl_inst.push_back("  .s2_readdata(" + port_s2_readdata + ")\n");
        ctx.rtl_inst.push_back(");\n");
    }

}

RTLGenResult genModuleRTL(
    const VulStaticModuleInstance &module,
    const VulStaticConfigLib &configlib,
    const VulStaticBundleLib &bundlelib
) {
    VulStaticConfigLib local_configlib = configlib;
    for (const auto &entry : module.local_parameters) {
        local_configlib[entry.first] = entry.second;
    }
    for (const auto &entry : module.local_consts) {
        local_configlib[entry.first] = entry.second;
    }

    VulStaticBundleLib local_bundlelib = bundlelib;
    for (const auto &local_bundle : module.local_bundles) {
        bool replaced = false;
        for (auto &bundle : local_bundlelib) {
            if (bundle.name == local_bundle.name) {
                bundle = local_bundle;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            local_bundlelib.push_back(local_bundle);
        }
    }

    RTLGenContext ctx(module, local_configlib, local_bundlelib);

    _procConstAndBundle(ctx);
    _procWires(ctx);
    _procRegisters(ctx);
    _procRequests(ctx);
    _procQueries(ctx);
    _procServicesAndTicks(ctx);
    _procChildrenAndConnection(ctx);
    _procQueues(ctx);
    _procBRAMAndROM(ctx);

    RTLGenResult result;

    const string module_name = module.simClassName();
    const string logic_module_name = LogicSubModuleName(module_name);

    auto append_lines = [](vector<string> &dst, const vector<string> &src) {
        dst.insert(dst.end(), src.begin(), src.end());
    };

    auto &hls = result.logic_hls_codes;
    hls.push_back("#include <array>\n");
    hls.push_back("#include <cstdint>\n");
    hls.push_back("\n");
    hls.push_back("using std::array;\n");
    hls.push_back("\n");
    hls.push_back("using int8 = int8_t;\n");
    hls.push_back("using uint8 = uint8_t;\n");
    hls.push_back("using int16 = int16_t;\n");
    hls.push_back("using uint16 = uint16_t;\n");
    hls.push_back("using int32 = int32_t;\n");
    hls.push_back("using uint32 = uint32_t;\n");
    hls.push_back("using int64 = int64_t;\n");
    hls.push_back("using uint64 = uint64_t;\n");
    hls.push_back("\n");
    hls.push_back("#define assert(...) ((void)0)\n");
    hls.push_back("\n");

    append_lines(hls, ctx.hls_header);
    if (!ctx.hls_header.empty()) {
        hls.push_back("\n");
    }

    hls.push_back("void " + logic_module_name + "(\n");
    for (size_t i = 0; i < ctx.hls_arguments.size(); ++i) {
        hls.push_back("  " + ctx.hls_arguments[i] + (i + 1 == ctx.hls_arguments.size() ? "" : ",") + "\n");
    }
    hls.push_back(") {\n");

    append_lines(hls, ctx.hls_init);
    if (!ctx.hls_init.empty()) {
        hls.push_back("\n");
    }
    append_lines(hls, ctx.hls_helpers);
    if (!ctx.hls_helpers.empty()) {
        hls.push_back("\n");
    }
    append_lines(hls, ctx.hls_blocks);
    if (!ctx.hls_blocks.empty()) {
        hls.push_back("\n");
    }
    append_lines(hls, ctx.hls_body);
    if (!ctx.hls_body.empty()) {
        hls.push_back("\n");
    }
    append_lines(hls, ctx.hls_final);
    hls.push_back("}\n");
    hls.push_back("\n");

    auto &rtl = result.rtl_skeleten_codes;
    vector<string> ports;
    ports.reserve(2 + ctx.rtl_ports.size());
    ports.push_back("input clk");
    ports.push_back("input rstn");
    ports.insert(ports.end(), ctx.rtl_ports.begin(), ctx.rtl_ports.end());

    rtl.push_back("module " + module_name + "(\n");
    for (size_t i = 0; i < ports.size(); ++i) {
        rtl.push_back("  " + ports[i] + (i + 1 == ports.size() ? "" : ",") + "\n");
    }
    rtl.push_back(");\n");
    rtl.push_back("\n");

    append_lines(rtl, ctx.rtl_decl);
    if (!ctx.rtl_decl.empty()) {
        rtl.push_back("\n");
    }
    append_lines(rtl, ctx.rtl_logic);
    if (!ctx.rtl_logic.empty()) {
        rtl.push_back("\n");
    }

    rtl.push_back(logic_module_name + " u_logic (\n");
    for (size_t i = 0; i < ctx.rtl_logicports.size(); ++i) {
        rtl.push_back("  " + ctx.rtl_logicports[i] + (i + 1 == ctx.rtl_logicports.size() ? "" : ",") + "\n");
    }
    rtl.push_back(");\n");
    rtl.push_back("\n");

    append_lines(rtl, ctx.rtl_inst);
    if (!ctx.rtl_inst.empty()) {
        rtl.push_back("\n");
    }
    rtl.push_back("endmodule\n");
    rtl.push_back("\n");

    result.resource_files = std::move(ctx.resource_files);

    return result;
}




}
