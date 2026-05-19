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
        flatten_member(reg.signature.toBundleMember("value"), ctx.local_bundlelib, "value", element_width, out);
        uint32_t wrport_num = reg.ports;
        uint32_t size = (reg.dims.empty()) ? 1 : reg.dims[0];

        string ewid_str = std::to_string(element_width);
        string ewid_m1_str = std::to_string(element_width - 1);
        string wrport_num_str = std::to_string(wrport_num);
        string size_str = std::to_string(size);

        string element_type_str = reg.signature.toString();

        string rdata_type_str = "UInt<" + ewid_str + ">";
        string wdata_type_str = "UInt<" + ewid_str + ">";
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
            ctx.hls_header.push_back("operator const " + element_type_str + "&() const {\n");
            ctx.hls_header.push_back("  " + element_type_str + " value;\n");
            for (const auto &field : out) {
                ctx.hls_header.push_back("  " + field.name + " = rdata(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ");\n");
            }
            ctx.hls_header.push_back("  return value;\n");
            ctx.hls_header.push_back("}\n");

            // single element get for non-array register
            ctx.hls_header.push_back("const " + element_type_str + " &get() const {\n");
            ctx.hls_header.push_back("  " + element_type_str + " value;\n");
            for (const auto &field : out) {
                ctx.hls_header.push_back("  " + field.name + " = rdata(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ");\n");
            }
            ctx.hls_header.push_back("  return value;\n");
            ctx.hls_header.push_back("}\n");

            // single element set for non-array register
            ctx.hls_header.push_back("template <uint32_t P = 0>\n");
            ctx.hls_header.push_back("void setnext(const " + element_type_str + " &value) {\n");
            ctx.hls_header.push_back("  static_assert(P < " + wrport_num_str + ", \"Port index out of range\");\n");
            ctx.hls_header.push_back("  UInt<" + ewid_str + "> wdata_val;\n");
            for (const auto &field : out) {
                ctx.hls_header.push_back("  wdata_val(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ") = " + field.name + ";\n");
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
            ctx.hls_header.push_back("const " + element_type_str + "& operator[](const uint32_t idx) const {\n");
            ctx.hls_header.push_back("  " + element_type_str + " value;\n");
            ctx.hls_header.push_back("  UInt<" + ewid_str + "> rdata_val = rdata[idx];\n");
            for (const auto &field : out) {
                ctx.hls_header.push_back("  " + field.name + " = rdata_val(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ");\n");
            }
            ctx.hls_header.push_back("  return value;\n");
            ctx.hls_header.push_back("}\n");

            // array element set for array register
            ctx.hls_header.push_back("template <uint32_t P = 0>\n");
            ctx.hls_header.push_back("void setnext(const uint32_t idx, const " + element_type_str + " &value) {\n");
            ctx.hls_header.push_back("  static_assert(P < " + wrport_num_str + ", \"Port index out of range\");\n");
            ctx.hls_header.push_back("  UInt<" + ewid_str + "> wdata_val;\n");
            for (const auto &field : out) {
                ctx.hls_header.push_back("  wdata_val(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ") = value." + field.name + ";\n");
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
                ctx.rtl_decl.push_back("wire " + wen_wire_name + ";\n");
                ctx.rtl_decl.push_back("wire [" + ewid_m1_str + ":0] " + wdata_wire_name + ";\n");
            } else {
                ctx.rtl_decl.push_back("wire " + wen_wire_name + "[" + wrport_num_str + "];\n");
                ctx.rtl_decl.push_back("wire [" + ewid_m1_str + ":0] " + wdata_wire_name + "[" + wrport_num_str + "];\n");
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
                ctx.rtl_decl.push_back("wire " + wen_wire_name + "[" + size_str + "];\n");
                ctx.rtl_decl.push_back("wire [" + ewid_m1_str + ":0] " + wdata_wire_name + "[" + size_str + "];\n");
            } else {
                ctx.rtl_decl.push_back("wire " + wen_wire_name + "[" + size_str + "][" + wrport_num_str + "];\n");
                ctx.rtl_decl.push_back("wire [" + ewid_m1_str + ":0] " + wdata_wire_name + "[" + size_str + "][" + wrport_num_str + "];\n");
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
    uint32_t width;
    vector<FlatField> flat_fields;
};

inline ArgPort procArg(const VulStaticArg &arg, const VulStaticBundleLib &bundlelib) {
    ArgPort port;
    port.name = arg.name;
    port.width = 0;
    flatten_member(arg.type.toBundleMember(arg.name), bundlelib, arg.name, port.width, port.flat_fields);
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


void _procRequests(RTLGenContext &ctx) {

    for (const auto &req_entry : ctx.module.requests) {
        const string &req_name = req_entry.first;
        const VulStaticReqServ &req = req_entry.second;

        VulErrorContextGuard req_guard("processing request " + req_name);

        // skip if this request is connected to a child request port
        bool skip = false;
        for (const auto &c : ctx.module.req_connections) {
            if (c.serv_instance == "" && c.serv_name == req_name) {
                skip = true;
                break;
            }
        }
        if (skip) {
            continue;
        }

        vector<ArgPort> arg_ports;
        vector<ArgPort> ret_ports;

        for (const auto &arg : req.args) {
            arg_ports.push_back(procArg(arg, ctx.local_bundlelib));
        }
        for (const auto &ret : req.rets) {
            ret_ports.push_back(procArg(ret, ctx.local_bundlelib));
        }

        string rettype = req.returnType();
        string argtypes = req.signatureArgTypeOnly();
        string argnames = req.signatureArgNameList();
        string arglists = req.signatureArgOnly();
        bool has_rdy = req.has_handshake;

        // hls function ports
        string vld_port_name = reqservVldPort(req_name);
        string rdy_port_name = reqservRdyPort(req_name);
        ctx.hls_arguments.push_back("bool &" + vld_port_name);
        if (has_rdy) {
            ctx.hls_arguments.push_back("const bool " + rdy_port_name);
        }
        for (const auto &arg : arg_ports) {
            ctx.hls_arguments.push_back("UInt<" + std::to_string(arg.width) + "> &" + reqservArgPort(req_name, arg.name));
        }
        for (const auto &ret : ret_ports) {
            ctx.hls_arguments.push_back("const UInt<" + std::to_string(ret.width) + "> " + reqservArgPort(req_name, ret.name));
        }

        // generate request helper function declaration
        ctx.hls_init.push_back(vld_port_name + " = false;\n");
        ctx.hls_helpers.push_back("auto " + req_name + " = [&](" + arglists + ") -> " + rettype + "{\n");
        ctx.hls_helpers.push_back("  " + vld_port_name + " = true;\n");
        for (const auto &arg : arg_ports) {
            for (const auto &field : arg.flat_fields) {
                ctx.hls_helpers.push_back(reqservArgPort(req_name, arg.name) + "(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ") = " + field.name + ";\n");
            }
        }
        for (const auto &ret : ret_ports) {
            for (const auto &field : ret.flat_fields) {
                ctx.hls_helpers.push_back("  " + field.name + " = " + reqservArgPort(req_name, ret.name) + "(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ");\n");
            }
        }
        if (has_rdy) {
            ctx.hls_helpers.push_back("  return " + rdy_port_name + ";\n");
        }
        ctx.hls_helpers.push_back("};\n");

        // generate RTL ports
        ctx.rtl_ports.push_back("output " + vld_port_name);
        ctx.rtl_logicports.push_back("." + vld_port_name + "(" + vld_port_name + ")");
        if (has_rdy) {
            ctx.rtl_ports.push_back("input " + rdy_port_name);
            ctx.rtl_logicports.push_back("." + rdy_port_name + "(" + rdy_port_name + ")");
        }
        for (const auto &arg : arg_ports) {
            string arg_port_name = reqservArgPort(req_name, arg.name);
            ctx.rtl_ports.push_back("output [" + std::to_string(arg.width - 1) + ":0] " + arg_port_name);
            ctx.rtl_logicports.push_back("." + arg_port_name + "(" + arg_port_name + ")");
        }
        for (const auto &ret : ret_ports) {
            string ret_port_name = reqservArgPort(req_name, ret.name);
            ctx.rtl_ports.push_back("input [" + std::to_string(ret.width - 1) + ":0] " + ret_port_name);
            ctx.rtl_logicports.push_back("." + ret_port_name + "(" + ret_port_name + ")");
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

        // rtl ports
        string vld_port_name = reqservVldPort(serv_name);
        string rdy_port_name = reqservRdyPort(serv_name);
        ctx.rtl_ports.push_back("input " + vld_port_name);
        if (has_rdy) {
            ctx.rtl_ports.push_back("output " + rdy_port_name);
        }
        for (const auto &arg : arg_ports) {
            string arg_port_name = reqservArgPort(serv_name, arg.name);
            ctx.rtl_ports.push_back("input [" + std::to_string(arg.width - 1) + ":0] " + arg_port_name);
        }
        for (const auto &ret : ret_ports) {
            string ret_port_name = reqservArgPort(serv_name, ret.name);
            ctx.rtl_ports.push_back("output [" + std::to_string(ret.width - 1) + ":0] " + ret_port_name);
        }

        // a service should be:
        // (1) implemented in logic code blocks
        // (2) connected to child instance service ports

        auto iter = ctx.module.serv_logic_blocks.find(serv_name);
        if (iter == ctx.module.serv_logic_blocks.end()) {
            continue;
        }
        const auto& lb = iter->second;

        ctx.hls_arguments.push_back("const bool " + vld_port_name);
        ctx.rtl_logicports.push_back("." + vld_port_name + "(" + vld_port_name + ")");
        if (has_rdy) {
            ctx.hls_arguments.push_back("bool &" + rdy_port_name);
            ctx.rtl_logicports.push_back("." + rdy_port_name + "(" + rdy_port_name + ")");
        }
        for (const auto &arg : arg_ports) {
            string arg_port_name = reqservArgPort(serv_name, arg.name);
            ctx.hls_arguments.push_back("const UInt<" + std::to_string(arg.width) + "> " + arg_port_name);
            ctx.rtl_logicports.push_back("." + arg_port_name + "(" + arg_port_name + ")");
        }
        for (const auto &ret : ret_ports) {
            string ret_port_name = reqservArgPort(serv_name, ret.name);
            ctx.hls_arguments.push_back("UInt<" + std::to_string(ret.width) + "> &" + ret_port_name);
            ctx.rtl_logicports.push_back("." + ret_port_name + "(" + ret_port_name + ")");
        }

        ctx.hls_blocks.push_back("auto " + implFuncName(serv_name) + " = [&](" + arglists + ") -> void {\n");
        ctx.hls_blocks.insert(ctx.hls_blocks.end(), lb.codelines.begin(), lb.codelines.end());
        ctx.hls_blocks.push_back("};\n");
        if (has_rdy) {
            ctx.hls_blocks.push_back("auto " + condFuncName(serv_name) + " = [&](" + arglists + ") -> bool {\n");
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
                ctx.hls_body.push_back("{\n");
                ctx.hls_body.push_back("  bool rdy = " + condFuncName(cache.name) + "(" + cache.argnames + ");\n");
                ctx.hls_body.push_back("  if (rdy && " + reqservVldPort(cache.name) + ") " + implFuncName(cache.name) + "(" + cache.argnames + ");\n");
                ctx.hls_body.push_back("  " + reqservRdyPort(cache.name) + " = rdy;\n");
                ctx.hls_body.push_back("}\n");
            } else if (!cache.is_tick) {
                ctx.hls_body.push_back("if (" + reqservVldPort(cache.name) + ") " + implFuncName(cache.name) + "(" + cache.argnames + ");\n");
            } else {
                ctx.hls_body.push_back(cache.name + "();\n");
            }
        }
    }
}

void _procChildrenAndConnection(RTLGenContext &ctx) {

    auto conn_to_str = [](const VulReqServConnection &conn) -> string {
        return conn.serv_instance + "_" + conn.serv_name + "_" + conn.req_instance + "_" + conn.req_name;
    };

    // CR-CS 需要wire，子实例-子实例
    // CR-S 需要wire，子实例-逻辑子模块
    // CR-R 不需要wire，直接从子实例端口连到模块端口
    // S-CS 不需要wire，直接从子实例端口连到模块端口
    // L-CS 需要wire，逻辑子模块-子实例

    for (const auto child_ptr : ctx.module.children) {
        const auto &child = *child_ptr;
        string child_class_name = child.simClassName();
        string child_instance_name = child.instance_path.back();
        VulErrorContextGuard child_guard("processing child instance " + child_class_name);

        if (ctx.module.instances.find(child_instance_name) == ctx.module.instances.end()) {
            throw VulException("Instance " + child_instance_name + " not found in module instances");
        }
        const auto &inst = ctx.module.instances.at(child_instance_name);

        vector<string> child_port_lines;
        child_port_lines.push_back(".clk(clk)");
        child_port_lines.push_back(".rstn(rstn)");

        for (const auto &req_entry : child.requests) {
            const auto &req = req_entry.second;
            const string &req_name = req_entry.first;
            VulReqServConnection conn;
            bool connected = false;
            for (const auto &c : ctx.module.req_connections) {
                if (c.req_instance == child_instance_name && c.req_name == req_name) {
                    conn = c;
                    connected = true;
                    break;
                }
            }
            if (!connected) {
                throw VulException("Request " + req_name + " of instance " + child_instance_name + " is not connected");
            }
            // CR-CS 需要wire，子实例-子实例
            // CR-S 需要wire，子实例-逻辑子模块
            if (conn.serv_instance != "" || ctx.module.services.find(conn.serv_name) != ctx.module.services.end()) {
                string conn_str = conn_to_str(conn);
                ctx.rtl_decl.push_back("wire " + conn_str + "_valid;\n");
                child_port_lines.push_back(string(".") + reqservVldPort(req_name) + "(" + conn_str + "_valid)" );
                if (req.has_handshake) {
                    ctx.rtl_decl.push_back("wire " + conn_str + "_ready;\n");
                    child_port_lines.push_back(string(".") + reqservRdyPort(req_name) + "(" + conn_str + "_ready)" );
                }
                for (const auto &arg : req.args) {
                    uint32_t offset = 0;
                    std::vector<FlatField> out;
                    flatten_member(arg.type.toBundleMember(""), ctx.local_bundlelib, arg.name, offset, out);
                    ctx.rtl_decl.push_back("wire [" + std::to_string(offset - 1) + ":0] " + conn_str + "_arg_" + arg.name + ";\n");
                    child_port_lines.push_back(string(".") + reqservArgPort(req_name, arg.name) + "(" + conn_str + "_arg_" + arg.name + ")" );
                }
                for (const auto &ret : req.rets) {
                    uint32_t offset = 0;
                    std::vector<FlatField> out;
                    flatten_member(ret.type.toBundleMember(""), ctx.local_bundlelib, ret.name, offset, out);
                    ctx.rtl_decl.push_back("wire [" + std::to_string(offset - 1) + ":0] " + conn_str + "_ret_" + ret.name + ";\n");
                    child_port_lines.push_back(string(".") + reqservArgPort(req_name, ret.name) + "(" + conn_str + "_ret_" + ret.name + ")" );
                }
            } else {
                // CR-R 不需要wire，直接从子实例端口连到模块端口
                child_port_lines.push_back(string(".") + reqservVldPort(req_name) + "(" + reqservVldPort(conn.serv_name) + ")" );
                if (req.has_handshake) {
                    child_port_lines.push_back(string(".") + reqservRdyPort(req_name) + "(" + reqservRdyPort(conn.serv_name) + ")" );
                }
                for (const auto &arg : req.args) {
                    child_port_lines.push_back(string(".") + reqservArgPort(req_name, arg.name) + "(" + reqservArgPort(conn.serv_name, arg.name) + ")" );
                }
                for (const auto &ret : req.rets) {
                    child_port_lines.push_back(string(".") + reqservArgPort(req_name, ret.name) + "(" + reqservArgPort(conn.serv_name, ret.name) + ")" );
                }
            }
        }
        for (const auto &srv_entry : child.services) {
            const auto &srv = srv_entry.second;
            const string &srv_name = srv_entry.first;
            VulReqServConnection conn;
            bool connected = false;
            for (const auto &c : ctx.module.req_connections) {
                if (c.serv_instance == child_instance_name && c.serv_name == srv_name) {
                    conn = c;
                    connected = true;
                    break;
                }
            }
            bool logic_ref = (inst.referenced_services.find(srv_name) != inst.referenced_services.end());
            if (!connected && !logic_ref) {
                continue; // service port is not connected and not referenced in logic, skip
            }
            if (connected && conn.req_instance == "") {
                // S-CS 不需要wire，直接从子实例端口连到模块端口
                child_port_lines.push_back(string(".") + reqservVldPort(srv_name) + "(" + reqservVldPort(conn.req_name) + ")" );
                if (srv.has_handshake) {
                    child_port_lines.push_back(string(".") + reqservRdyPort(srv_name) + "(" + reqservRdyPort(conn.req_name) + ")" );
                }
                for (const auto &arg : srv.args) {
                    child_port_lines.push_back(string(".") + reqservArgPort(srv_name, arg.name) + "(" + reqservArgPort(conn.req_name, arg.name) + ")" );
                }
                for (const auto &ret : srv.rets) {
                    child_port_lines.push_back(string(".") + reqservArgPort(srv_name, ret.name) + "(" + reqservArgPort(conn.req_name, ret.name) + ")" );
                }
            } else if (connected) {
                // CR-CS 需要wire，子实例-子实例，但wire已经在Req侧生成，这里只连接子实例的端口
                string conn_str = conn_to_str(conn);
                child_port_lines.push_back(string(".") + reqservVldPort(srv_name) + "(" + conn_str + "_valid)" );
                if (srv.has_handshake) {
                    child_port_lines.push_back(string(".") + reqservRdyPort(srv_name) + "(" + conn_str + "_ready)" );
                }
                for (const auto &arg : srv.args) {
                    child_port_lines.push_back(string(".") + reqservArgPort(srv_name, arg.name) + "(" + conn_str + "_arg_" + arg.name + ")" );
                }
                for (const auto &ret : srv.rets) {
                    child_port_lines.push_back(string(".") + reqservArgPort(srv_name, ret.name) + "(" + conn_str + "_ret_" + ret.name + ")" );
                }
            } else {
                // L-CS 需要wire，逻辑子模块-子实例，同时在 HLS 函数里设置 helper function
                string child_serv_name = child_instance_name + "_" + srv_name;
                ctx.rtl_decl.push_back("wire " + reqservVldPort(child_serv_name) + ";\n");
                child_port_lines.push_back(string(".") + reqservVldPort(srv_name) + "(" + reqservVldPort(child_serv_name) + ")" );
                ctx.hls_arguments.push_back("bool & " + reqservVldPort(child_serv_name));
                ctx.rtl_logicports.push_back("." + reqservVldPort(child_serv_name) + "(" + reqservVldPort(child_serv_name) + ")");
                if (srv.has_handshake) {
                    ctx.rtl_decl.push_back("wire " + reqservRdyPort(child_serv_name) + ";\n");
                    child_port_lines.push_back(string(".") + reqservRdyPort(srv_name) + "(" + reqservRdyPort(child_serv_name) + ")" );
                    ctx.hls_arguments.push_back("const bool " + reqservRdyPort(child_serv_name));
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
                    ctx.rtl_decl.push_back("wire [" + std::to_string(arg.width - 1) + ":0] " + reqservArgPort(child_serv_name, arg.name) + ";\n");
                    child_port_lines.push_back(string(".") + reqservArgPort(srv_name, arg.name) + "(" + reqservArgPort(child_serv_name, arg.name) + ")" );
                    ctx.hls_arguments.push_back("UInt<" + std::to_string(arg.width) + "> & " + reqservArgPort(child_serv_name, arg.name));
                    ctx.rtl_logicports.push_back("." + reqservArgPort(child_serv_name, arg.name) + "(" + reqservArgPort(child_serv_name, arg.name) + ")");
                }
                for (const auto &ret : ret_ports) {
                    ctx.rtl_decl.push_back("wire [" + std::to_string(ret.width - 1) + ":0] " + reqservArgPort(child_serv_name, ret.name) + ";\n");
                    child_port_lines.push_back(string(".") + reqservArgPort(srv_name, ret.name) + "(" + reqservArgPort(child_serv_name, ret.name) + ")" );
                    ctx.hls_arguments.push_back("const UInt<" + std::to_string(ret.width) + "> " + reqservArgPort(child_serv_name, ret.name));
                    ctx.rtl_logicports.push_back("." + reqservArgPort(child_serv_name, ret.name) + "(" + reqservArgPort(child_serv_name, ret.name) + ")");
                }
                ctx.hls_init.push_back(reqservVldPort(child_serv_name) + " = false;\n");
                ctx.hls_helpers.push_back("auto " + child_serv_name + " = [&](" + srv.signatureArgOnly() + ") -> " + srv.returnType() + " {\n");
                ctx.hls_helpers.push_back("  " + reqservVldPort(child_serv_name) + " = true;\n");
                for (const auto &arg : arg_ports) {
                    for (const auto &field : arg.flat_fields) {
                        ctx.hls_helpers.push_back(reqservArgPort(child_serv_name, arg.name) + "(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ") = " + field.name + ";\n");
                    }
                }
                for (const auto &ret : ret_ports) {
                    for (const auto &field : ret.flat_fields) {
                        ctx.hls_helpers.push_back("  " + field.name + " = " + reqservArgPort(child_serv_name, ret.name) + "(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ");\n");
                    }
                }
                if (srv.has_handshake) {
                    ctx.hls_helpers.push_back("  return " + reqservRdyPort(child_serv_name) + ";\n");
                }
                ctx.hls_helpers.push_back("};\n");
            }
        }
        ctx.rtl_inst.push_back(child_class_name + " " + child_instance_name + " (\n");
        for (size_t i = 0; i < child_port_lines.size(); i++) {
            ctx.rtl_inst.push_back("  " + child_port_lines[i] + (i == child_port_lines.size() - 1 ? "" : ",") + "\n");
        }
        ctx.rtl_inst.push_back(");\n");
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
        flatten_member(que.type.toBundleMember("value"), ctx.local_bundlelib, "value", data_width, data_fields);
        
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
            string enqdata_type_str = "UInt<" + data_width_str + ">";
            string deqdata_type_str = "UInt<" + data_width_str + ">";

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

            ctx.hls_header.push_back("  bool enqnext(const " + data_type_str + " &value) {\n");
            ctx.hls_header.push_back("    if (!enqready()) {\n");
            ctx.hls_header.push_back("      return false;\n");
            ctx.hls_header.push_back("    }\n");
            for (const auto &field : data_fields) {
                ctx.hls_header.push_back("    " + port_enqdata + "(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ") = " + field.name + ";\n");
            }
            ctx.hls_header.push_back("    " + port_enqvalid + " = true;\n");
            ctx.hls_header.push_back("    return true;\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  const " + data_type_str + "& deqnext() {\n");
            ctx.hls_header.push_back("    " + port_deqready + " = true;\n");
            ctx.hls_header.push_back("    " + data_type_str + " value;\n");
            for (const auto &field : data_fields) {
                ctx.hls_header.push_back("    " + field.name + " = " + port_deqdata + "(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ");\n");
            }
            ctx.hls_header.push_back("    return value;\n");
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
            string enqready_type_str = "UInt<" + enq_cnt_width_str + ">";
            string deqvalid_type_str = "UInt<" + deq_cnt_width_str + ">";
            string enqvalid_type_str = "UInt<" + enq_cnt_width_str + ">";
            string deqready_type_str = "UInt<" + deq_cnt_width_str + ">";
            string enqdata_type_str = "std::array<UInt<" + data_width_str + ">, " + enq_width_str + ">";
            string deqdata_type_str = "std::array<UInt<" + data_width_str + ">, " + deq_width_str + ">";

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
            ctx.hls_header.push_back("    return " + port_enqready + ";\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  " + deqvalid_type_str + " deqvalid() const {\n");
            ctx.hls_header.push_back("    return " + port_deqvalid + ";\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  " + enqready_type_str + " enqnext(const " + enqdata_type_str + " &values, const " + enqready_type_str + " num = " + enq_width_str + ") {\n");
            ctx.hls_header.push_back("    const " + enqready_type_str + " req = (num < " + enq_width_str + ") ? num : " + enq_width_str + ";\n");
            ctx.hls_header.push_back("    const " + enqready_type_str + " rdy = enqreqdy();\n");
            ctx.hls_header.push_back("    const " + enqready_type_str + " accepted = (req < rdy) ? req : rdy;\n");
            for (uint32_t i = 0; i < (uint32_t)que.enq_width; ++i) {
                ctx.hls_header.push_back("    if (accepted > " + std::to_string(i) + ") {\n");
                ctx.hls_header.push_back("      const " + data_type_str + " &value = values[" + std::to_string(i) + "];\n");
                for (const auto &field : data_fields) {
                    ctx.hls_header.push_back("      " + port_enqdata + "[" + std::to_string(i) + "](" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ") = " + field.name + ";\n");
                }
                ctx.hls_header.push_back("    }\n");
            }
            ctx.hls_header.push_back("    " + port_enqvalid + " = accepted;\n");
            ctx.hls_header.push_back("    return accepted;\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  const " + deqdata_type_str + " & deqnext(const " + deqvalid_type_str + " num = " + deq_width_str + ") {\n");
            ctx.hls_header.push_back("    const " + deqvalid_type_str + " req = (num < " + deq_width_str + ") ? num : " + deq_width_str + ";\n");
            ctx.hls_header.push_back("    const " + deqvalid_type_str + " valid = deqvalid();\n");
            ctx.hls_header.push_back("    const " + deqvalid_type_str + " accepted = (req < valid) ? req : valid;\n");
            ctx.hls_header.push_back("    " + port_deqvalid + " = accepted;\n");
            ctx.hls_header.push_back("    " + deqdata_type_str + " deq_buf;\n");
            for (uint32_t i = 0; i < (uint32_t)que.deq_width; ++i) {
                ctx.hls_header.push_back("    if (accepted > " + std::to_string(i) + ") {\n");
                ctx.hls_header.push_back("      auto &value = deq_buf[" + std::to_string(i) + "];\n");
                for (const auto &field : data_fields) {
                    ctx.hls_header.push_back("      " + field.name + " = " + port_deqdata + "[" + std::to_string(i) + "](" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ");\n");
                }
                ctx.hls_header.push_back("    }\n");
            }
            ctx.hls_header.push_back("    return deq_buf;\n");
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
        flatten_member(bram.data_type.toBundleMember("value"), ctx.local_bundlelib, "value", data_width, read_fields);
        if (data_width == 0) {
            throw VulException("BRAM data width must be positive");
        }

        if (bram.addr_width <= 0) {
            throw VulException("BRAM addr_width must be positive");
        }
        const uint32_t addr_width = static_cast<uint32_t>(bram.addr_width);
        const bool is_1rw = (bram.read_ports == 0 || bram.write_ports == 0);

        const string bram_name = bram.name;
        const string data_type_str = bram.data_type.toString();
        const string data_width_str = std::to_string(data_width);
        const string data_width_m1_str = std::to_string(data_width - 1);
        const string addr_width_str = std::to_string(addr_width);
        const string addr_width_m1_str = std::to_string(addr_width - 1);
        const string addr_type_str = "UInt<" + addr_width_str + ">";

        if (is_1rw) {
            vector<FlatField> write_fields;
            uint32_t write_width = 0;
            flatten_member(bram.data_type.toBundleMember("write_data"), ctx.local_bundlelib, "write_data", write_width, write_fields);
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
            ctx.hls_header.push_back("  const UInt<" + data_width_str + "> " + port_s2_rdata + ";\n");
            ctx.hls_header.push_back("  bool &" + port_s1_en + ";\n");
            ctx.hls_header.push_back("  bool &" + port_s1_we + ";\n");
            ctx.hls_header.push_back("  UInt<" + addr_width_str + "> &" + port_s1_addr + ";\n");
            ctx.hls_header.push_back("  UInt<" + data_width_str + "> &" + port_s1_wdata + ";\n");
            ctx.hls_header.push_back("  mutable DataType readdata_buf;\n");
            ctx.hls_header.push_back("\n");
            ctx.hls_header.push_back("  " + proxy_class_name +
                "(const UInt<" + data_width_str + "> " + port_s2_rdata +
                ", bool &" + port_s1_en +
                ", bool &" + port_s1_we +
                ", UInt<" + addr_width_str + "> &" + port_s1_addr +
                ", UInt<" + data_width_str + "> &" + port_s1_wdata +
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
            ctx.hls_header.push_back("    UInt<" + data_width_str + "> packed;\n");
            for (const auto &field : write_fields) {
                ctx.hls_header.push_back("    packed(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ") = " + field.name + ";\n");
            }
            ctx.hls_header.push_back("    " + port_s1_wdata + " = packed;\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_header.push_back("  const DataType& readdata() const {\n");
            ctx.hls_header.push_back("    DataType value;\n");
            for (const auto &field : read_fields) {
                ctx.hls_header.push_back("    " + field.name + " = " + port_s2_rdata + "(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ");\n");
            }
            ctx.hls_header.push_back("    readdata_buf = value;\n");
            ctx.hls_header.push_back("    return readdata_buf;\n");
            ctx.hls_header.push_back("  }\n");
            ctx.hls_header.push_back("};\n");
            ctx.hls_header.push_back("\n");

            ctx.hls_arguments.push_back("const UInt<" + data_width_str + "> " + port_s2_rdata);
            ctx.hls_arguments.push_back("bool &" + port_s1_en);
            ctx.hls_arguments.push_back("bool &" + port_s1_we);
            ctx.hls_arguments.push_back("UInt<" + addr_width_str + "> &" + port_s1_addr);
            ctx.hls_arguments.push_back("UInt<" + data_width_str + "> &" + port_s1_wdata);

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
            flatten_member(bram.data_type.toBundleMember("data"), ctx.local_bundlelib, "data", write_width, write_fields);
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
            const string readaddr_type_str = "std::array<UInt<" + addr_width_str + ">, " + read_ports_str + ">";
            const string readdata_type_str = "std::array<UInt<" + data_width_str + ">, " + read_ports_str + ">";
            const string write_type_str = "std::array<bool, " + write_ports_str + ">";
            const string writeaddr_type_str = "std::array<UInt<" + addr_width_str + ">, " + write_ports_str + ">";
            const string writedata_type_str = "std::array<UInt<" + data_width_str + ">, " + write_ports_str + ">";

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
                ctx.hls_header.push_back("    " + field.name + " = " + port_s2_readdata + "[PortIndex](" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ");\n");
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
            ctx.hls_header.push_back("    UInt<" + data_width_str + "> packed;\n");
            for (const auto &field : write_fields) {
                ctx.hls_header.push_back("    packed(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ") = " + field.name + ";\n");
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
        if (rom.addr_width <= 0) {
            throw VulException("ROM addr_width must be positive");
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

        const string data_type_str = "UInt<" + data_width_str + ">";
        const string addr_type_str = "UInt<" + addr_width_str + ">";
        const string readreq_type_str = "std::array<bool, " + read_ports_str + ">";
        const string readaddr_type_str = "std::array<UInt<" + addr_width_str + ">, " + read_ports_str + ">";
        const string readdata_type_str = "std::array<UInt<" + data_width_str + ">, " + read_ports_str + ">";

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
    hls.push_back("#include <uint.hpp>\n");
    hls.push_back("\n");
    hls.push_back("using int128_t = __int128_t;\n");
    hls.push_back("using uint128_t = __uint128_t;\n");
    hls.push_back("using int8 = int8_t;\n");
    hls.push_back("using uint8 = uint8_t;\n");
    hls.push_back("using int16 = int16_t;\n");
    hls.push_back("using uint16 = uint16_t;\n");
    hls.push_back("using int32 = int32_t;\n");
    hls.push_back("using uint32 = uint32_t;\n");
    hls.push_back("using int64 = int64_t;\n");
    hls.push_back("using uint64 = uint64_t;\n");
    hls.push_back("using int128 = int128_t;\n");
    hls.push_back("using uint128 = uint128_t;\n");
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
