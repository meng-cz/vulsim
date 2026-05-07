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
#include "toposort.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <cctype>

namespace rtlgen {

void flatten_reqserv_arg(
    const VulArg& arg,
    const BundleTable& bundle_table,
    const std::string& prefix,
    uint32_t& offset,
    std::vector<FlatField>& out
) {
    VulBundleMember member;
    member.name = arg.name;
    member.type = arg.type;
    member.uint_length = ""; // TODO: support uint<N>
    member.dims = {};
    flatten_member(member, bundle_table, prefix, offset, out);
}

struct FlattenedPort {
    string port_name;
    uint32_t bitlen;
    std::vector<FlatField> fields; // 展平后的字段列表，包含访问路径和位宽信息
};

struct TransactionPorts {
    string trans_name;
    std::vector<FlattenedPort> arg_ports;
    std::vector<FlattenedPort> resp_ports;
    bool with_ready = false;
};

TransactionPorts extractTransactionPorts(
    const VulReqServ &reqserv,
    const BundleTable &bundle_table
) {
    TransactionPorts trans;
    trans.trans_name = reqserv.name;
    trans.with_ready = reqserv.has_handshake;
    for (const auto &arg : reqserv.args) {
        FlattenedPort port;
        port.port_name = arg.name;
        uint32_t offset = 0;
        vector<FlatField> fields;
        flatten_reqserv_arg(arg, bundle_table, arg.name, offset, fields);
        port.bitlen = offset;
        port.fields = fields;
        trans.arg_ports.push_back(port);
    }
    for (const auto &ret : reqserv.rets) {
        FlattenedPort port;
        port.port_name = ret.name;
        uint32_t offset = 0;
        vector<FlatField> fields;
        flatten_reqserv_arg(ret, bundle_table, ret.name, offset, fields);
        port.bitlen = offset;
        port.fields = fields;
        trans.resp_ports.push_back(port);
    }
    return trans;
}

struct RegisterPorts {
    string reg_name;
    FlattenedPort port;
    uint32_t wrport_num = 1; // 寄存器写端口数量，默认为1
};

using simgen::replaceLog2CeilChar;

bool is_loop_start(const string& line) {
    for (size_t i = 0; i + 2 < line.size(); i++) {
        if (line[i]=='f'&&line[i+1]=='o'&&line[i+2]=='r') {
            bool l = (i==0)||!isalnum(line[i-1]);
            bool r = (i+3>=line.size())||!isalnum(line[i+3]);
            if (l && r) return true;
        }
    }
    for (size_t i = 0; i + 4 < line.size(); i++) {
        if (line[i]=='w'&&line[i+1]=='h'&&line[i+2]=='i'&&line[i+3]=='l'&&line[i+4]=='e') {
            bool l = (i==0)||!isalnum(line[i-1]);
            bool r = (i+5>=line.size())||!isalnum(line[i+5]);
            if (l && r) return true;
        }
    }
    return false;
}
bool is_blank(const string& s) {
    for (char c : s) if (!isspace(c)) return false;
    return true;
}
vector<string> normalize_loops(const vector<string>& code) {
    vector<string> result;

    for (size_t i = 0; i < code.size(); i++) {
        string line = code[i];

        if (!is_loop_start(line)) {
            result.push_back(line);
            continue;
        }

        // 开始拼接
        string merged = line;

        // 如果这一行没有 ')', 继续往下拼
        while (merged.find(')') == string::npos && i + 1 < code.size()) {
            i++;
            string next = code[i];

            // 去掉前导空白
            size_t pos = 0;
            while (pos < next.size() && isspace(next[pos])) pos++;

            if (!merged.empty() && merged.back() != ' ')
                merged += " ";

            merged += next.substr(pos);
        }

        result.push_back(merged);
    }

    return result;
}
vector<string> insert_unroll(const vector<string>& code) {
    vector<string> result;

    for (size_t i = 0; i < code.size(); i++) {
        const string& line = code[i];

        result.push_back(line);

        if (is_loop_start(line)) {
            result.push_back("#pragma HLS UNROLL");
        }
    }

    return result;
}
vector<string> insert_hls_unroll(const vector<string>& code) {
    vector<string> normalized = normalize_loops(code);
    return insert_unroll(normalized);
}

const string CodeTab = "    ";

ErrorMsg genHLSMainFunc(
    const VulModule &module,
    const BundleTable &bundle_table,
    const unordered_map<ConfigName, ConfigValue> &config_overrides,
    const unordered_map<ConfigName, ConfigRealValue> &calculated_configlib,
    const VulProject &project,
    vector<string> &out_lines
) {
    vector<string> argument_lines;
    vector<string> init_field;
    vector<string> helper_field;
    vector<string> body_field;
    vector<string> final_field;

    vector<string> input_ports;
    vector<string> output_ports;

    // constexpr config 定义
    for (const auto &conf_entry : calculated_configlib) {
        const string &conf_name = conf_entry.first;
        ConfigRealValue real_value = conf_entry.second;
        if (config_overrides.find(conf_name) != config_overrides.end()) {
            continue; // 已被覆盖的 config 不生成 constexpr 定义
        }
        init_field.push_back("constexpr int64_t " + conf_name + " = " + std::to_string(real_value) + ";\n");
    }
    // local_configs
    for (const auto &lconf_entry : module.local_configs) {
        const string &conf_name = lconf_entry.first;
        ConfigValue value = lconf_entry.second.value;
        auto iter = config_overrides.find(conf_name);
        if (iter != config_overrides.end()) {
            value = iter->second;
        }
        ConfigRealValue real_value = 0;
        unordered_set<ConfigName> seen_configs;
        ErrorMsg err = project.configlib->calculateConfigExpression(value, real_value, seen_configs);
        if (!err.empty()) {
            return ErrorMsg(ERTL, "Failed to calculate config expression for local config " + conf_name + ": " + err.msg);
        }
        init_field.push_back("constexpr int64_t " + conf_name + " = " + std::to_string(real_value) + ";\n");
    }
    // bundle 定义
    for (const auto &bundle_entry : bundle_table) {
        const VulBundleItem &item = bundle_entry.second;
        const BundleName &bundle_name = bundle_entry.first;
        if (item.is_alias) {
            if (item.members.empty()) {
                return ErrorMsg(ERTL, "Alias bundle " + bundle_name + " has no members");
            }
            VulBundleMember member = item.members[0];
            string base_type = member.type;
            if (!member.uint_length.empty()) {
                base_type = "UInt<" + member.uint_length + ">";
            }
            if (!member.dims.empty()) {
                // generate array in struct definition
                size_t N = member.dims.size();
                init_field.push_back("struct " + bundle_name + " {\n");
                // 生成内部结构（从内到外）
                for (int i = N - 1; i >= 1; --i) {
                    string structName = bundle_name + "D" + std::to_string(i);

                    init_field.push_back("  struct " + structName + " {\n");

                    if (i == (int)N - 1) {
                        // 最内层：直接是基础类型数组
                        init_field.push_back("    " + base_type + " d[" + (member.dims[i]) + "];\n");
                        init_field.push_back("    " + base_type + " & operator[](uint64_t idx) { return d[idx]; }\n");
                        init_field.push_back("    const " + base_type + " & operator[](uint64_t idx) const { return d[idx]; }\n");
                    } else {
                        // 中间层：是下一级 struct
                        string child = bundle_name + "D" + std::to_string(i + 1);
                        init_field.push_back("    " + child + " d[" + (member.dims[i]) + "];\n");
                        init_field.push_back("    " + child + " & operator[](uint64_t idx) { return d[idx]; }\n");
                        init_field.push_back("    const " + child + " & operator[](uint64_t idx) const { return d[idx]; }\n");
                    }
                    init_field.push_back("  };\n");
                }
                // 最外层
                if (N == 1) {
                    // 一维数组特殊处理
                    init_field.push_back("  " + base_type + " d[" + (member.dims[0]) + "];\n");
                    init_field.push_back("  " + base_type + " & operator[](uint64_t idx) { return d[idx]; }\n");
                    init_field.push_back("  const " + base_type + " & operator[](uint64_t idx) const { return d[idx]; }\n");
                } else {
                    string child = bundle_name + "D1";
                    init_field.push_back("  " + child + " d[" + (member.dims[0]) + "];\n");
                    init_field.push_back("  " + child + " & operator[](uint64_t idx) { return d[idx]; }\n");
                    init_field.push_back("  const " + child + " & operator[](uint64_t idx) const { return d[idx]; }\n");
                }
                init_field.push_back("};\n");
            } else {
                // generate using alias
                init_field.push_back("using " + bundle_name + " = " + base_type + ";\n");
            }
        } else if (!item.enum_members.empty()) {
            init_field.push_back("enum class " + bundle_name + " {\n");
            for (const auto &em : item.enum_members) {
                string member_str = "    " + em.name;
                if (!em.value.empty()) {
                    member_str += " = " + em.value;
                }
                member_str += ",";
                init_field.push_back(member_str + "\n");
            }
            init_field.push_back("};\n");
        } else {
            init_field.push_back("struct " + bundle_name + " {\n");
            for (const auto &member : item.members) {
                string base_type = member.type;
                if (!member.uint_length.empty()) {
                    base_type = "UInt<" + member.uint_length + ">";
                }
                string def = "    " + base_type + " " + member.name;
                for (const auto &d : member.dims) {
                    def += "[" + d + "]";
                }
                if (!member.value.empty()) {
                    def += " = " + (member.value);
                }
                init_field.push_back(def + ";\n");
            }
            init_field.push_back("};\n");
        }
    }

    // wire/reg 定义
    for (const auto &sto_entry : module.storagetmp) {
        const VulStorage &sto = sto_entry.second;
        if (sto.dims.size() > 0) {
            return ErrorMsg(ERTL, "storagetmp does not support array type");
        }
        string def = sto.typeString() + " " + sto_entry.first;
        if (!sto.value.empty()) {
            def += " = " + replaceLog2CeilChar(sto.value);
        }
        def += ";\n";
        init_field.push_back(def);
    }
    for (const auto &sto_entry : module.storages) {
        const VulStorage &sto = sto_entry.second;
        if (sto.dims.size() > 0) {
            return ErrorMsg(ERTL, "storage does not support array type");
        }
        string def = sto.typeString() + " " + sto_entry.first;
        if (!sto.value.empty()) {
            def += " = " + replaceLog2CeilChar(sto.value);
        }
        def += ";\n";
        init_field.push_back(def);
        uint32_t offset = 0;
        std::vector<FlatField> out;
        flatten_member(sto, bundle_table, sto_entry.first, offset, out);
        argument_lines.push_back("const UInt<" + std::to_string(offset) + "> & varin_" + sto_entry.first);
        argument_lines.push_back("UInt<" + std::to_string(offset) + "> & varout_" + sto_entry.first);
        input_ports.push_back("varin_" + sto_entry.first);
        output_ports.push_back("varout_" + sto_entry.first);
        final_field.push_back("UInt<" + std::to_string(offset) + "> varoutbuf_" + sto_entry.first + ";\n");
        for (const auto &field : out) {
            init_field.push_back(field.name + " = varin_" + sto_entry.first + "(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ");\n");
            final_field.push_back("varoutbuf_" + sto_entry.first + "(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ") = " + field.name + ";\n");
        }
        final_field.push_back("varout_" + sto_entry.first + " = varoutbuf_" + sto_entry.first + ";\n");
    }
    for (const auto &reg_entry : module.storagenexts) {
        const VulStorage &sto = reg_entry.second;
        if (sto.dims.size() > 0) {
            return ErrorMsg(ERTL, "storagenext does not support array type");
        }
        string def = sto.typeString() + " " + reg_entry.first;
        if (!sto.value.empty()) {
            def += " = " + replaceLog2CeilChar(sto.value);
        }
        def += ";\n";
        init_field.push_back(def);
        uint32_t offset = 0;
        std::vector<FlatField> out;
        flatten_member(sto, bundle_table, reg_entry.first, offset, out);
        argument_lines.push_back("const UInt<" + std::to_string(offset) + "> & regin_" + reg_entry.first);
        input_ports.push_back("regin_" + reg_entry.first);
        for (const auto &field : out) {
            init_field.push_back(field.name + " = regin_" + reg_entry.first + "(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ");\n");
        }
        uint32_t port_num = 1;
        auto port_num_iter = module.storagenext_ports.find(reg_entry.first);
        if (port_num_iter != module.storagenext_ports.end()) {
            port_num = port_num_iter->second;
        }
        for (uint32_t i = 0; i < port_num; i++) {
            argument_lines.push_back("bool & regwen_" + reg_entry.first + "_" + std::to_string(i));
            argument_lines.push_back("UInt<" + std::to_string(offset) + "> & regout_" + reg_entry.first + "_" + std::to_string(i));
            output_ports.push_back("regwen_" + reg_entry.first + "_" + std::to_string(i));
            output_ports.push_back("regout_" + reg_entry.first + "_" + std::to_string(i));
            init_field.push_back("bool regwenbuf_" + reg_entry.first + "_" + std::to_string(i) + " = false;\n");
            init_field.push_back("UInt<" + std::to_string(offset) + "> regoutbuf_" + reg_entry.first + "_" + std::to_string(i) + " = 0;\n");
            final_field.push_back("regwen_" + reg_entry.first + "_" + std::to_string(i) + " = regwenbuf_" + reg_entry.first + "_" + std::to_string(i) + ";\n");
            final_field.push_back("regout_" + reg_entry.first + "_" + std::to_string(i) + " = regoutbuf_" + reg_entry.first + "_" + std::to_string(i) + ";\n");
        }
        // generate xxx_setnext helper function as lambda
        helper_field.push_back("auto " + reg_entry.first + "_setnext = [&](const " + sto.typeString() + " & " + reg_entry.first + ", uint32_t prio = 0) {\n");
        helper_field.push_back(CodeTab + "UInt<" + std::to_string(offset) + "> refbuf = 0;\n");
        for (const auto &field : out) {
            helper_field.push_back(CodeTab + "refbuf(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ") = " + field.name + ";\n");
        }
        for (uint32_t i = 0; i < port_num; i++) {
            helper_field.push_back(CodeTab + (i == 0 ? "" : "else ") + "if (prio == " + std::to_string(i) + ") {\n");
            helper_field.push_back(CodeTab + CodeTab + "regwenbuf_" + reg_entry.first + "_" + std::to_string(i) + " = true;\n");
            helper_field.push_back(CodeTab + CodeTab + "regoutbuf_" + reg_entry.first + "_" + std::to_string(i) + " = refbuf;\n");
            helper_field.push_back(CodeTab + "}\n");
        }
        helper_field.push_back("};\n");
    }

    // requests
    for (const auto &req_entry : module.requests) {
        const VulReqServ &req = req_entry.second;
        const string &req_name = req_entry.first;

        // pass if connected
        bool connected = false;
        for (const auto &conne : module.req_connections) {
            for (const auto &conn : conne.second) {
                if (conn.serv_instance == module.TopInterface && conn.serv_name == req_name) {
                    connected = true;
                    break;
                }
            }
        }
        if (connected) continue;

        argument_lines.push_back("bool & reqvalid_" + req_name);
        output_ports.push_back("reqvalid_" + req_name);
        init_field.push_back("bool reqvalidbuf_" + req_name + " = false;\n");
        final_field.push_back("reqvalid_" + req_name + " = reqvalidbuf_" + req_name + ";\n");
        if (req.has_handshake) {
            argument_lines.push_back("const bool & reqready_" + req_name);
            input_ports.push_back("reqready_" + req_name);
        }
        for (const auto &arg : req.args) {
            uint32_t offset = 0;
            std::vector<FlatField> out;
            flatten_reqserv_arg(arg, bundle_table, req_name + "_" + arg.name, offset, out);
            argument_lines.push_back("UInt<" + std::to_string(offset) + "> & reqarg_" + req_name + "_" + arg.name);
            output_ports.push_back("reqarg_" + req_name + "_" + arg.name);
            init_field.push_back(arg.type + " " + req_name + "_" + arg.name + ";\n");
            final_field.push_back("UInt<" + std::to_string(offset) + "> reqargbuf_" + req_name + "_" + arg.name + " = 0;\n");
            for (const auto &field : out) {
                final_field.push_back("reqargbuf_" + req_name + "_" + arg.name + "(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ") = " + field.name + ";\n");
            }
            final_field.push_back("reqarg_" + req_name + "_" + arg.name + " = reqargbuf_" + req_name + "_" + arg.name + ";\n");
        }
        for (const auto &ret : req.rets) {
            uint32_t offset = 0;
            std::vector<FlatField> out;
            flatten_reqserv_arg(ret, bundle_table, req_name + "_" + ret.name, offset, out);
            argument_lines.push_back("const UInt<" + std::to_string(offset) + "> & reqret_" + req_name + "_" + ret.name);
            input_ports.push_back("reqret_" + req_name + "_" + ret.name);
            init_field.push_back(ret.type + " " + req_name + "_" + ret.name + ";\n");
            for (const auto &field : out) {
                init_field.push_back(field.name + " = reqret_" + req_name + "_" + ret.name + "(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ");\n");
            }
        }
        string rettype = req.returnType();
        string argtypes = req.signatureArgTypeOnly();
        string argnames = req.signatureArgNameList();
        string arglists = req.signatureArgOnly();
        helper_field.push_back("auto " + req_name + " = [&](" + arglists + ") -> " + rettype + " {\n");
        helper_field.push_back(CodeTab + "reqvalidbuf_" + req_name + " = true;\n");
        for (const auto &arg : req.args) {
            helper_field.push_back(CodeTab + req_name + "_" + arg.name + " = " + arg.name + ";\n");
        }
        for (const auto &ret : req.rets) {
            helper_field.push_back(CodeTab + ret.name + " = " + req_name + "_" + ret.name + ";\n");
        }
        if (req.has_handshake) {
            helper_field.push_back(CodeTab + "return reqready_" + req_name + ";\n");
        }
        helper_field.push_back("};\n");
    }
    // services, only generated if user-implemented
    std::map<int32_t, vector<ReqServName>> sorted_services; // sort by priority
    for (const auto &servimpl_entry : module.serv_codelines) {
        const string srv_name = servimpl_entry.first;
        auto serv_iter = module.services.find(srv_name);
        if (serv_iter == module.services.end()) {
            return ErrorMsg(ERTL, "Service " + srv_name + " has implementation code but is not declared in module");
        }
        const VulReqServ &srv = serv_iter->second;

        auto priority_iter = module.serv_priority.find(srv_name);
        int32_t priority = 0;
        if (priority_iter != module.serv_priority.end()) {
            priority = priority_iter->second;
        }
        sorted_services[priority].push_back(srv_name);

        argument_lines.push_back("const bool & srvvalid_" + srv_name);
        input_ports.push_back("srvvalid_" + srv_name);
        if (srv.has_handshake) {
            argument_lines.push_back("bool & srvready_" + srv_name);
            output_ports.push_back("srvready_" + srv_name);
            init_field.push_back("bool srvreadybuf_" + srv_name + " = false;\n");
        }
        for (const auto &arg : srv.args) {
            uint32_t offset = 0;
            std::vector<FlatField> out;
            flatten_reqserv_arg(arg, bundle_table, srv_name + "_" + arg.name, offset, out);
            argument_lines.push_back("const UInt<" + std::to_string(offset) + "> & srvarg_" + srv_name + "_" + arg.name);
            input_ports.push_back("srvarg_" + srv_name + "_" + arg.name);
            init_field.push_back(arg.type + " " + srv_name + "_" + arg.name + ";\n");
            for (const auto &field : out) {
                init_field.push_back(field.name + " = srvarg_" + srv_name + "_" + arg.name + "(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ");\n");
            }
        }
        for (const auto &ret : srv.rets) {
            uint32_t offset = 0;
            std::vector<FlatField> out;
            flatten_reqserv_arg(ret, bundle_table, srv_name + "_" + ret.name, offset, out);
            argument_lines.push_back("UInt<" + std::to_string(offset) + "> & srvret_" + srv_name + "_" + ret.name);
            output_ports.push_back("srvret_" + srv_name + "_" + ret.name);
            init_field.push_back(ret.type + " " + srv_name + "_" + ret.name + ";\n");
            final_field.push_back("UInt<" + std::to_string(offset) + "> srvretbuf_" + srv_name + "_" + ret.name + " = 0;\n");
            for (const auto &field : out) {
                final_field.push_back("srvretbuf_" + srv_name + "_" + ret.name + "(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ") = " + field.name + ";\n");
            }
            final_field.push_back("srvret_" + srv_name + "_" + ret.name + " = srvretbuf_" + srv_name + "_" + ret.name + ";\n");
        }
        string rettype = srv.returnType();
        string argtypes = srv.signatureArgTypeOnly();
        string argnames = srv.signatureArgNameList();
        string arglists = srv.signatureArgOnly();
        helper_field.push_back("auto srvimpl_" + srv_name + " = [&](" + arglists + ") -> void {\n");
        for (const auto &line : insert_hls_unroll(servimpl_entry.second)) {
            helper_field.push_back(line + (line.ends_with("\n") ? "" : "\n"));
        }
        helper_field.push_back("};\n");
        if (srv.has_handshake) {
            auto cond_iter = module.serv_cond_codelines.find(srv_name);
            if (cond_iter == module.serv_cond_codelines.end()) {
                return ErrorMsg(ERTL, "Service " + srv_name + " has handshake but no condition code");
            }
            helper_field.push_back("auto srvcond_" + srv_name + " = [&](" + arglists + ") -> bool {\n");
            for (const auto &line : insert_hls_unroll(cond_iter->second)) {
                helper_field.push_back(line + (line.ends_with("\n") ? "" : "\n"));
            }
            helper_field.push_back("};\n");
        }
    }
    // tick
    for (const auto &tick_entry : module.user_tick_codeblocks) {
        const string tick_name = tick_entry.first;
        const VulTickCodeBlock &codeblock = tick_entry.second;
        helper_field.push_back("auto tickimpl_" + tick_name + " = [&]() {\n");
        for (const auto &line : insert_hls_unroll(codeblock.codelines)) {
            helper_field.push_back(line + (line.ends_with("\n") ? "" : "\n"));
        }
        helper_field.push_back("};\n");
    }
    // child services
    for (const auto &inst_entry : module.instances) {
        const auto &inst = inst_entry.second;
        if (inst.referenced_services.empty()) continue; // no child service, skip
        for (const ReqServName &srv_name : inst.referenced_services) {
            auto mod_iter = project.modulelib->modules.find(inst.module_name);
            if (mod_iter == project.modulelib->modules.end()) {
                return ErrorMsg(ERTL, "Module " + inst.module_name + " not found in module library");
            }
            shared_ptr<VulModule> child_mod = dynamic_pointer_cast<VulModule>(mod_iter->second);
            if (!child_mod) {
                return ErrorMsg(ERTL, "Module " + inst.module_name + " is not a valid module");
            }
            auto srv_iter = child_mod->services.find(srv_name);
            if (srv_iter == child_mod->services.end()) {
                return ErrorMsg(ERTL, "Service " + srv_name + " not found in module " + inst.module_name);
            }
            const VulReqServ &srv = srv_iter->second;

            // treat child service as a request to be called by parent
            const VulReqServ &req = srv;
            const string req_name = inst_entry.first + "_" + srv_name; // use instance name + service name as unique request name

            argument_lines.push_back("bool & reqvalid_" + req_name);
            output_ports.push_back("reqvalid_" + req_name);
            init_field.push_back("bool reqvalidbuf_" + req_name + " = false;\n");
            final_field.push_back("reqvalid_" + req_name + " = reqvalidbuf_" + req_name + ";\n");
            if (req.has_handshake) {
                argument_lines.push_back("const bool & reqready_" + req_name);
                input_ports.push_back("reqready_" + req_name);
            }
            for (const auto &arg : req.args) {
                uint32_t offset = 0;
                std::vector<FlatField> out;
                flatten_reqserv_arg(arg, bundle_table, req_name + "_" + arg.name, offset, out);
                argument_lines.push_back("UInt<" + std::to_string(offset) + "> & reqarg_" + req_name + "_" + arg.name);
                output_ports.push_back("reqarg_" + req_name + "_" + arg.name);
                init_field.push_back(arg.type + " " + req_name + "_" + arg.name + ";\n");
                final_field.push_back("UInt<" + std::to_string(offset) + "> reqargbuf_" + req_name + "_" + arg.name + " = 0;\n");
                for (const auto &field : out) {
                    final_field.push_back("reqargbuf_" + req_name + "_" + arg.name + "(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ") = " + field.name + ";\n");
                }
                final_field.push_back("reqarg_" + req_name + "_" + arg.name + " = reqargbuf_" + req_name + "_" + arg.name + ";\n");
            }
            for (const auto &ret : req.rets) {
                uint32_t offset = 0;
                std::vector<FlatField> out;
                flatten_reqserv_arg(ret, bundle_table, req_name + "_" + ret.name, offset, out);
                argument_lines.push_back("const UInt<" + std::to_string(offset) + "> & reqret_" + req_name + "_" + ret.name);
                input_ports.push_back("reqret_" + req_name + "_" + ret.name);
                init_field.push_back(ret.type + " " + req_name + "_" + ret.name + ";\n");
                for (const auto &field : out) {
                    init_field.push_back(field.name + " = reqret_" + req_name + "_" + ret.name + "(" + std::to_string(field.offset + field.width - 1) + ", " + std::to_string(field.offset) + ");\n");
                }
            }
            string rettype = req.returnType();
            string argtypes = req.signatureArgTypeOnly();
            string argnames = req.signatureArgNameList();
            string arglists = req.signatureArgOnly();
            helper_field.push_back("auto " + req_name + " = [&](" + arglists + ") -> " + rettype + " {\n");
            helper_field.push_back(CodeTab + "reqvalidbuf_" + req_name + " = true;\n");
            for (const auto &arg : req.args) {
                helper_field.push_back(CodeTab + req_name + "_" + arg.name + " = " + arg.name + ";\n");
            }
            for (const auto &ret : req.rets) {
                helper_field.push_back(CodeTab + ret.name + " = " + req_name + "_" + ret.name + ";\n");
            }
            if (req.has_handshake) {
                helper_field.push_back(CodeTab + "return reqready_" + req_name + ";\n");
            }
            helper_field.push_back("};\n");

        }

    }

    // construct body field
    vector<string> serv_before_tick;
    vector<string> serv_after_tick;
    for (const auto &[prio, srv_names] : sorted_services) {
        if (prio < 0) serv_after_tick.insert(serv_after_tick.end(), srv_names.begin(), srv_names.end());
        else serv_before_tick.insert(serv_before_tick.end(), srv_names.begin(), srv_names.end());
    }
    // reverse to get correct order (smaller priority value means higher priority)
    std::reverse(serv_before_tick.begin(), serv_before_tick.end());
    std::reverse(serv_after_tick.begin(), serv_after_tick.end());
    auto add_service_calls = [&](const vector<string> &srv_names) {
        for (const auto &srv_name : srv_names) {
            const VulReqServ &srv = module.services.at(srv_name);
            string real_arg_names = "";
            for (const auto &arg : srv.args) {
                if (!real_arg_names.empty()) real_arg_names += ", ";
                real_arg_names += srv_name + "_" + arg.name;
            }
            for (const auto &ret : srv.rets) {
                if (!real_arg_names.empty()) real_arg_names += ", ";
                real_arg_names += srv_name + "_" + ret.name;
            }
            if (srv.has_handshake) {
                body_field.push_back("srvreadybuf_" + srv_name + " = srvcond_" + srv_name + "(" + real_arg_names + ");\n");
                body_field.push_back("srvready_" + srv_name + " = srvreadybuf_" + srv_name + ";\n");
                body_field.push_back("if (srvvalid_" + srv_name + " && srvreadybuf_" + srv_name + ") srvimpl_" + srv_name + "(" + real_arg_names + ");\n");
            } else {
                body_field.push_back("if (srvvalid_" + srv_name + ") srvimpl_" + srv_name + "(" + real_arg_names + ");\n");
            }
        }
    };
    add_service_calls(serv_before_tick);
    for (const auto & tick_entry : module.user_tick_codeblocks) {
        const string tick_name = tick_entry.first;
        body_field.push_back("tickimpl_" + tick_name + "();\n");
    }
    add_service_calls(serv_after_tick);

    // preparation done, generate function

    out_lines.push_back("#include <array>\n");
    out_lines.push_back("#include <stdint.h>\n");
    out_lines.push_back("using int128_t = __int128_t;\n");
    out_lines.push_back("using uint128_t = __uint128_t;\n");
    out_lines.push_back("using int8 = int8_t;\n");
    out_lines.push_back("using uint8 = uint8_t;\n");
    out_lines.push_back("using int16 = int16_t;\n");
    out_lines.push_back("using uint16 = uint16_t;\n");
    out_lines.push_back("using int32 = int32_t;\n");
    out_lines.push_back("using uint32 = uint32_t;\n");
    out_lines.push_back("using int64 = int64_t;\n");
    out_lines.push_back("using uint64 = uint64_t;\n");
    out_lines.push_back("using int128 = int128_t;\n");
    out_lines.push_back("using uint128 = uint128_t;\n");
    out_lines.push_back("\n");
    out_lines.push_back("#include <ap_int.h>\n");
    out_lines.push_back("template<uint64_t N>\n");
    out_lines.push_back("using UInt = ap_uint<N>;\n");
    out_lines.push_back("\n");
    out_lines.push_back("void " + LogicSubModuleName(module.name) + "(");
    for (size_t i = 0; i < argument_lines.size(); i++) {
        out_lines.push_back(CodeTab + argument_lines[i] + (i == argument_lines.size() - 1 ? "" : ",") + "\n");
    }
    out_lines.push_back(") {\n");
    // add port constraints
    out_lines.push_back("#pragma HLS INTERFACE ap_ctrl_none port=return\n");
    for (const auto &port : input_ports) {
        out_lines.push_back("#pragma HLS INTERFACE ap_none port=" + port + "\n");
    }
    for (const auto &port : output_ports) {
        out_lines.push_back("#pragma HLS INTERFACE ap_none port=" + port + "\n");
    }
    out_lines.push_back("\n");
    for (const auto &line : init_field) {
        out_lines.push_back(line);
    }
    out_lines.push_back("\n");
    for (const auto &line : helper_field) {
        out_lines.push_back(line);
    }
    out_lines.push_back("\n");
    for (const auto &line : body_field) {
        out_lines.push_back(line);
    }
    out_lines.push_back("\n");
    for (const auto &line : final_field) {
        out_lines.push_back(line);
    }

    out_lines.push_back("}\n");
    out_lines.push_back("\n");

    return "";
}

ErrorMsg _genRegisterSubmoduleRTL(
    const VulModule &module,
    const BundleTable &bundle_table,
    vector<string> &out_lines,
    vector<string> &out_ports,
    vector<uint32_t> &out_ports_width
) {

    vector<string> port_lines;
    vector<string> ports;
    vector<uint32_t> ports_width;
    vector<string> reg_field;
    vector<string> logic_field;

    // storage
    for (const auto& sto_entry : module.storages) {
        const VulStorage &sto = sto_entry.second;
        if (sto.dims.size() > 0) {
            return ErrorMsg(ERTL, "storagenext does not support array type");
        }
        uint32_t offset = 0;
        std::vector<FlatField> out;
        flatten_member(sto, bundle_table, sto_entry.first, offset, out);
        string widthstr = (offset == 1) ? "" : ("[" + std::to_string(offset - 1) + ":0]");
        port_lines.push_back("output wire" + widthstr + " varin_" + sto_entry.first);
        port_lines.push_back("input  wire" + widthstr + " varout_" + sto_entry.first);
        ports.push_back("varin_" + sto_entry.first);
        ports.push_back("varout_" + sto_entry.first);
        ports_width.push_back(offset);
        ports_width.push_back(offset);
        reg_field.push_back("reg " + widthstr + " " + sto_entry.first + ";\n");
        logic_field.push_back("assign varin_" + sto_entry.first + " = " + sto_entry.first + ";\n");
        logic_field.push_back("always @(posedge clk or negedge rstn) begin\n");
        logic_field.push_back("    if (!rstn) begin\n");
        logic_field.push_back("        " + sto_entry.first + " <= 0;\n");
        logic_field.push_back("    end else begin\n");
        logic_field.push_back("        " + sto_entry.first + " <= varout_" + sto_entry.first + ";\n");
        logic_field.push_back("    end\n");
        logic_field.push_back("end\n");
        logic_field.push_back("\n");
    }

    // storagenext
    for (const auto& reg_entry : module.storagenexts) {
        const string &reg_name = reg_entry.first;
        const VulStorage &sto = reg_entry.second;
        if (sto.dims.size() > 0) {
            return ErrorMsg(ERTL, "storagenext does not support array type");
        }
        uint32_t offset = 0;
        std::vector<FlatField> out;
        flatten_member(sto, bundle_table, reg_name, offset, out);
        string widthstr = (offset == 1) ? "" : ("[" + std::to_string(offset - 1) + ":0]");
        uint32_t port_num = 1;
        auto port_num_iter = module.storagenext_ports.find(reg_entry.first);
        if (port_num_iter != module.storagenext_ports.end()) {
            port_num = port_num_iter->second;
        }
        port_lines.push_back("output wire" + widthstr + " regin_" + reg_name);
        ports.push_back("regin_" + reg_name);
        ports_width.push_back(offset);
        reg_field.push_back("reg " + widthstr + " " + reg_name + ";\n");
        logic_field.push_back("assign regin_" + reg_name + " = " + reg_name + ";\n");
        for (uint32_t i = 0; i < port_num; i++) {
            port_lines.push_back("input  wire regwen_" + reg_name + "_" + std::to_string(i));
            port_lines.push_back("input  wire" + widthstr + " regout_" + reg_name + "_" + std::to_string(i));
            ports.push_back("regwen_" + reg_name + "_" + std::to_string(i));
            ports.push_back("regout_" + reg_name + "_" + std::to_string(i));
            ports_width.push_back(1);
            ports_width.push_back(offset);
        }
        logic_field.push_back("always @(posedge clk or negedge rstn) begin\n");
        logic_field.push_back("    if (!rstn) begin\n");
        logic_field.push_back("        " + reg_name + " <= 0;\n");
        logic_field.push_back("    end else begin\n");
        for (uint32_t i = 0; i < port_num; i++) {
            if (i == 0) {
                logic_field.push_back("        if (regwen_" + reg_name + "_" + std::to_string(i) + ") " + reg_name + " <= regout_" + reg_name + "_" + std::to_string(i) + ";\n");
            } else {
                logic_field.push_back("        else if (regwen_" + reg_name + "_" + std::to_string(i) + ") " + reg_name + " <= regout_" + reg_name + "_" + std::to_string(i) + ";\n");
            }
        }
        logic_field.push_back("    end\n");
        logic_field.push_back("end\n");
        logic_field.push_back("\n");
    }

    out_lines.push_back("module " + RegisterSubModuleName(module.name) + "(\n");
    out_lines.push_back(CodeTab + "input wire clk,\n");
    out_lines.push_back(CodeTab + "input wire rstn" + (port_lines.empty() ? "" : ",") + "\n");
    for (size_t i = 0; i < port_lines.size(); i++) {
        out_lines.push_back(CodeTab + port_lines[i] + (i == port_lines.size() - 1 ? "" : ",") + "\n");
    }
    out_lines.push_back(");\n");
    out_lines.push_back("\n");
    for (const auto &line : reg_field) {
        out_lines.push_back(line);
    }
    out_lines.push_back("\n");
    for (const auto &line : logic_field) {
        out_lines.push_back(line);
    }
    out_lines.push_back("endmodule\n");
    out_lines.push_back("\n");

    out_ports = ports;

    return "";
}

struct ModulePort {
    string name;
    uint32_t width;
    bool is_input; // true for input port, false for output port
};

vector<ModulePort> extractModulePorts(const VulModule &module, const BundleTable &bundle_table) {
    vector<ModulePort> ports;
    ports.push_back({"clk", 1, true});
    ports.push_back({"rstn", 1, true});
    for (const auto & req_entry : module.requests) {
        const VulReqServ &req = req_entry.second;
        const string &req_name = req_entry.first;

        ports.push_back({"reqvalid_" + req_name, 1, false});
        if (req.has_handshake) {
            ports.push_back({"reqready_" + req_name, 1, true});
        }
        for (const auto &arg : req.args) {
            uint32_t offset = 0;
            std::vector<FlatField> out;
            flatten_reqserv_arg(arg, bundle_table, req_name + "_" + arg.name, offset, out);
            ports.push_back({"reqarg_" + req_name + "_" + arg.name, offset, false});
        }
        for (const auto &ret : req.rets) {
            uint32_t offset = 0;
            std::vector<FlatField> out;
            flatten_reqserv_arg(ret, bundle_table, req_name + "_" + ret.name, offset, out);
            ports.push_back({"reqret_" + req_name + "_" + ret.name, offset, true});
        }
    }
    for (const auto &srv_entry : module.services) {
        const VulReqServ &srv = srv_entry.second;
        const string &srv_name = srv_entry.first;

        // ignore if connected to child request
        bool connected = false;
        for (const auto &conne : module.req_connections) {
            for (const auto &conn : conne.second) {
                if (conn.serv_instance == module.TopInterface && conn.serv_name == srv_name) {
                    connected = true;
                    break;
                }
            }
        }
        if (connected) continue;

        ports.push_back({"srvvalid_" + srv_name, 1, true});
        if (srv.has_handshake) {
            ports.push_back({"srvready_" + srv_name, 1, false});
        }
        for (const auto &arg : srv.args) {
            uint32_t offset = 0;
            std::vector<FlatField> out;
            flatten_reqserv_arg(arg, bundle_table, srv_name + "_" + arg.name, offset, out);
            ports.push_back({"srvarg_" + srv_name + "_" + arg.name, offset, true});
        }
        for (const auto &ret : srv.rets) {
            uint32_t offset = 0;
            std::vector<FlatField> out;
            flatten_reqserv_arg(ret, bundle_table, srv_name + "_" + ret.name, offset, out);
            ports.push_back({"srvret_" + srv_name + "_" + ret.name, offset, false});
        }
    }
    return ports;
}

ErrorMsg genModuleRTLSkeletonWithReg(
    const VulModule &module,
    const BundleTable &bundle_table,
    const unordered_map<ConfigName, ConfigValue> &param_override,
    const VulProject &project,
    vector<string> &out_lines
) {
    vector<string> wire_field;
    vector<string> instance_field;

    vector<ModulePort> ports = extractModulePorts(module, bundle_table);

    vector<string> reg_ports;
    vector<uint32_t> reg_ports_width;
    vector<string> reg_verilog_lines;
    ErrorMsg err = _genRegisterSubmoduleRTL(module, bundle_table, reg_verilog_lines, reg_ports, reg_ports_width);
    if (!err.empty()) return err;

    for (size_t i = 0; i < reg_ports.size(); i++) {
        wire_field.push_back("wire [" + std::to_string(reg_ports_width[i] - 1) + ":0] " + reg_ports[i] + ";\n");
    }

    instance_field.push_back(RegisterSubModuleName(module.name) + " u_register (\n");
    instance_field.push_back(CodeTab + ".clk(clk),\n");
    instance_field.push_back(CodeTab + ".rstn(rstn)" + (reg_ports.size() ? "," : "") + "\n");
    for (size_t i = 0; i < reg_ports.size(); i++) {
        instance_field.push_back(CodeTab + "." + reg_ports[i] + "(" + reg_ports[i] + ")" + (i == reg_ports.size() - 1 ? "" : ",") + "\n");
    }
    instance_field.push_back(");\n");

    // CR-CS 需要wire，子实例-子实例
    // CR-S 需要wire，子实例-逻辑子模块
    // CR-R 不需要wire，直接从子实例端口连到模块端口
    // S-CS 不需要wire，直接从子实例端口连到模块端口
    // L-CS 需要wire，逻辑子模块-子实例
    auto conn_to_str = [](const VulReqServConnection &conn) -> string {
        return conn.serv_instance + "_" + conn.serv_name + "_" + conn.req_instance + "_" + conn.req_name;
    };
    for (const auto &inst_entry : module.instances) {
        const auto &inst = inst_entry.second;
        auto mod_iter = project.modulelib->modules.find(inst.module_name);
        if (mod_iter == project.modulelib->modules.end()) {
            return ErrorMsg(ERTL, "Module " + inst.module_name + " not found in module library");
        }
        shared_ptr<VulModule> child_mod = dynamic_pointer_cast<VulModule>(mod_iter->second);
        if (!child_mod) {
            return ErrorMsg(ERTL, "Module " + inst.module_name + " is not a valid module");
        }
        const string &inst_name = inst_entry.first;
        vector<string> child_port_lines;
        child_port_lines.push_back(".clk(clk)");
        child_port_lines.push_back(".rstn(rstn)");
        for (const auto &req_entry : child_mod->requests) {
            const VulReqServ &req = req_entry.second;
            const string &req_name = req_entry.first;
            VulReqServConnection conn;
            bool connected = false;
            for (const auto &conne : module.req_connections) {
                for (const auto &c : conne.second) {
                    if (c.req_instance == inst_entry.first && c.req_name == req_name) {
                        conn = c;
                        connected = true;
                        break;
                    }
                }
            }
            if (!connected) {
                return ErrorMsg(ERTL, "Request " + req_name + " of instance " + inst_entry.first + " is not connected");
            }
            // CR-CS 需要wire，子实例-子实例
            // CR-S 需要wire，子实例-逻辑子模块
            if (conn.serv_instance != module.TopInterface || module.services.find(conn.serv_name) != module.services.end()) {
                string conn_str = conn_to_str(conn);
                wire_field.push_back("wire " + conn_str + "_valid;\n");
                child_port_lines.push_back(string(".") + "reqvalid_" + req_name + "(" + conn_str + "_valid)" );
                if (req.has_handshake) {
                    wire_field.push_back("wire " + conn_str + "_ready;\n");
                    child_port_lines.push_back(string(".") + "reqready_" + req_name + "(" + conn_str + "_ready)" );
                }
                for (const auto &arg : req.args) {
                    uint32_t offset = 0;
                    std::vector<FlatField> out;
                    flatten_reqserv_arg(arg, bundle_table, req_name + "_" + arg.name, offset, out);
                    wire_field.push_back("wire [" + std::to_string(offset - 1) + ":0] " + conn_str + "_arg_" + arg.name + ";\n");
                    child_port_lines.push_back(string(".") + "reqarg_" + req_name + "_" + arg.name + "(" + conn_str + "_arg_" + arg.name + ")" );
                }
                for (const auto &ret : req.rets) {
                    uint32_t offset = 0;
                    std::vector<FlatField> out;
                    flatten_reqserv_arg(ret, bundle_table, req_name + "_" + ret.name, offset, out);
                    wire_field.push_back("wire [" + std::to_string(offset - 1) + ":0] " + conn_str + "_ret_" + ret.name + ";\n");
                    child_port_lines.push_back(string(".") + "reqret_" + req_name + "_" + ret.name + "(" + conn_str + "_ret_" + ret.name + ")" );
                }
            } else {
                // CR-R 不需要wire，直接从子实例端口连到模块端口
                child_port_lines.push_back(string(".") + "reqvalid_" + req_name + "(reqvalid_" + conn.serv_name + ")" );
                if (req.has_handshake) {
                    child_port_lines.push_back(string(".") + "reqready_" + req_name + "(reqready_" + conn.serv_name + ")" );
                }
                for (const auto &arg : req.args) {
                    child_port_lines.push_back(string(".") + "reqarg_" + req_name + "_" + arg.name + "(reqarg_" + conn.serv_name + "_" + arg.name + ")" );
                }
                for (const auto &ret : req.rets) {
                    child_port_lines.push_back(string(".") + "reqret_" + req_name + "_" + ret.name + "(reqret_" + conn.serv_name + "_" + ret.name + ")" );
                }
            }
        }
        for (const auto &srv_entry : child_mod->services) {
            const VulReqServ &srv = srv_entry.second;
            const string &srv_name = srv_entry.first;
            VulReqServConnection conn;
            bool connected = false;
            for (const auto &conne : module.req_connections) {
                for (const auto &c : conne.second) {
                    if (c.serv_instance == inst_entry.first && c.serv_name == srv_name) {
                        conn = c;
                        connected = true;
                        break;
                    }
                }
            }
            bool logic_ref = (inst.referenced_services.find(srv_name) != inst.referenced_services.end());
            if (!connected && !logic_ref) {
                return ErrorMsg(ERTL, "Service " + srv_name + " of instance " + inst_entry.first + " is not connected");
            }
            if (connected && conn.req_instance == module.TopInterface) {
                // S-CS 不需要wire，直接从子实例端口连到模块端口
                child_port_lines.push_back(string(".") + "srvvalid_" + srv_name + "(srvvalid_" + conn.req_name + ")" );
                if (srv.has_handshake) {
                    child_port_lines.push_back(string(".") + "srvready_" + srv_name + "(srvready_" + conn.req_name + ")" );
                }
                for (const auto &arg : srv.args) {
                    child_port_lines.push_back(string(".") + "srvarg_" + srv_name + "_" + arg.name + "(srvarg_" + conn.req_name + "_" + arg.name + ")" );
                }
                for (const auto &ret : srv.rets) {
                    child_port_lines.push_back(string(".") + "srvret_" + srv_name + "_" + ret.name + "(srvret_" + conn.req_name + "_" + ret.name + ")" );
                }
            } else if (connected) {
                // CR-CS 需要wire，子实例-子实例，但wire已经在Req侧生成，这里只连接子实例的端口
                string conn_str = conn_to_str(conn);
                child_port_lines.push_back(string(".") + "srvvalid_" + srv_name + "(" + conn_str + "_valid)" );
                if (srv.has_handshake) {
                    child_port_lines.push_back(string(".") + "srvready_" + srv_name + "(" + conn_str + "_ready)" );
                }
                for (const auto &arg : srv.args) {
                    child_port_lines.push_back(string(".") + "srvarg_" + srv_name + "_" + arg.name + "(" + conn_str + "_arg_" + arg.name + ")" );
                }
                for (const auto &ret : srv.rets) {
                    child_port_lines.push_back(string(".") + "srvret_" + srv_name + "_" + ret.name + "(" + conn_str + "_ret_" + ret.name + ")" );
                }
            } else {
                // L-CS 需要wire，逻辑子模块-子实例
                wire_field.push_back("wire srvvalid_" + inst_name + "_" + srv_name + ";\n");
                child_port_lines.push_back(string(".") + "srvvalid_" + srv_name + "(srvvalid_" + inst_name + "_" + srv_name + ")" );
                if (srv.has_handshake) {
                    wire_field.push_back("wire srvready_" + inst_name + "_" + srv_name + ";\n");
                    child_port_lines.push_back(string(".") + "srvready_" + srv_name + "(srvready_" + inst_name + "_" + srv_name + ")" );
                }
                for (const auto &arg : srv.args) {
                    uint32_t offset = 0;
                    std::vector<FlatField> out;
                    flatten_reqserv_arg(arg, bundle_table, srv_name + "_" + arg.name, offset, out);
                    wire_field.push_back("wire [" + std::to_string(offset - 1) + ":0] srvarg_" + inst_name + "_" + srv_name + "_" + arg.name + ";\n");
                    child_port_lines.push_back(string(".") + "srvarg_" + srv_name + "_" + arg.name + "(srvarg_" + inst_name + "_" + srv_name + "_" + arg.name + ")" );
                }
                for (const auto &ret : srv.rets) {
                    uint32_t offset = 0;
                    std::vector<FlatField> out;
                    flatten_reqserv_arg(ret, bundle_table, srv_name + "_" + ret.name, offset, out);
                    wire_field.push_back("wire [" + std::to_string(offset - 1) + ":0] srvret_" + inst_name + "_" + srv_name + "_" + ret.name + ";\n");
                    child_port_lines.push_back(string(".") + "srvret_" + srv_name + "_" + ret.name + "(srvret_" + inst_name + "_" + srv_name + "_" + ret.name + ")" );
                }
            }
        }
        instance_field.push_back(inst.module_name + " " + inst_name + " (\n");
        for (size_t i = 0; i < child_port_lines.size(); i++) {
            instance_field.push_back(CodeTab + child_port_lines[i] + (i == child_port_lines.size() - 1 ? "" : ",") + "\n");
        }
        instance_field.push_back(");\n");
    }

    // Logic 子模块
    vector<string> logic_port_lines;
    for (const auto &port : reg_ports) {
        logic_port_lines.push_back(string(".") + port + "(" + port + ")" );
    }
    // requests
    for (const auto &req_entry : module.requests) {
        const VulReqServ &req = req_entry.second;
        const string &req_name = req_entry.first;
        // pass if connected
        bool connected = false;
        for (const auto &conne : module.req_connections) {
            for (const auto &conn : conne.second) {
                if (conn.serv_instance == module.TopInterface && conn.serv_name == req_name) {
                    connected = true;
                    break;
                }
            }
        }
        if (connected) continue;
        // 这些都是直接连接到模块端口的，所以直接连接
        logic_port_lines.push_back(string(".") + "reqvalid_" + req_name + "(reqvalid_" + req_name + ")" );
        if (req.has_handshake) {
            logic_port_lines.push_back(string(".") + "reqready_" + req_name + "(reqready_" + req_name + ")" );
        }
        for (const auto &arg : req.args) {
            logic_port_lines.push_back(string(".") + "reqarg_" + req_name + "_" + arg.name + "(reqarg_" + req_name + "_" + arg.name + ")" );
        }
        for (const auto &ret : req.rets) {
            logic_port_lines.push_back(string(".") + "reqret_" + req_name + "_" + ret.name + "(reqret_" + req_name + "_" + ret.name + ")" );
        }
    }
    // fake request to child service
    for (const auto &inst_entry : module.instances) {
        const auto &inst = inst_entry.second;
        if (inst.referenced_services.empty()) continue; // no child service, skip
        for (const ReqServName &srv_name : inst.referenced_services) {
            auto mod_iter = project.modulelib->modules.find(inst.module_name);
            if (mod_iter == project.modulelib->modules.end()) {
                return ErrorMsg(ERTL, "Module " + inst.module_name + " not found in module library");
            }
            shared_ptr<VulModule> child_mod = dynamic_pointer_cast<VulModule>(mod_iter->second);
            if (!child_mod) {
                return ErrorMsg(ERTL, "Module " + inst.module_name + " is not a valid module");
            }
            auto srv_iter = child_mod->services.find(srv_name);
            if (srv_iter == child_mod->services.end()) {
                return ErrorMsg(ERTL, "Service " + srv_name + " not found in module " + inst.module_name);
            }
            const VulReqServ &srv = srv_iter->second;

            // treat child service as a request to be called by parent
            const VulReqServ &req = srv;
            const string req_name = inst_entry.first + "_" + srv_name; // use instance name + service name as unique request name
            // L-CS 
            // wire 已经在连接子实例时生成，这里直接连接
            logic_port_lines.push_back(string(".") + "reqvalid_" + req_name + "(srvvalid_" + req_name + ")" );
            if (req.has_handshake) {
                logic_port_lines.push_back(string(".") + "reqready_" + req_name + "(srvready_" + req_name + ")" );
            }
            for (const auto &arg : req.args) {
                logic_port_lines.push_back(string(".") + "reqarg_" + req_name + "_" + arg.name + "(srvarg_" + req_name + "_" + arg.name + ")" );
            }
            for (const auto &ret : req.rets) {
                logic_port_lines.push_back(string(".") + "reqret_" + req_name + "_" + ret.name + "(srvret_" + req_name + "_" + ret.name + ")" );
            }
        }
    }
    // services
    for (const auto &servimpl_entry : module.serv_codelines) {
        const string srv_name = servimpl_entry.first;
        auto serv_iter = module.services.find(srv_name);
        if (serv_iter == module.services.end()) {
            return ErrorMsg(ERTL, "Service " + srv_name + " has implementation code but is not declared in module");
        }
        const VulReqServ &srv = serv_iter->second;
        // 可能是连接到模块端口，或是被连接到子实例的Request
        bool connected_to_port = false;
        VulReqServConnection conn;
        for (const auto &conne : module.req_connections) {
            for (const auto &c : conne.second) {
                if (c.serv_instance == module.TopInterface && c.serv_name == srv_name) {
                    conn = c;
                    connected_to_port = true;
                    break;
                }
            }
        }
        if (connected_to_port) {
            // CR-S 需要wire，子实例-逻辑子模块，但wire已经在Req侧生成，这里只连接子实例的端口
            string conn_str = conn_to_str(conn);
            logic_port_lines.push_back(string(".") + "srvvalid_" + srv_name + "(" + conn_str + "_valid)" );
            if (srv.has_handshake) {
                logic_port_lines.push_back(string(".") + "srvready_" + srv_name + "(" + conn_str + "_ready)" );
            }
            for (const auto &arg : srv.args) {
                logic_port_lines.push_back(string(".") + "srvarg_" + srv_name + "_" + arg.name + "(" + conn_str + "_arg_" + arg.name + ")" );
            }
            for (const auto &ret : srv.rets) {
                logic_port_lines.push_back(string(".") + "srvret_" + srv_name + "_" + ret.name + "(" + conn_str + "_ret_" + ret.name + ")" );
            }
        } else {
            // 直接连接到模块端口
            logic_port_lines.push_back(string(".") + "srvvalid_" + srv_name + "(srvvalid_" + srv_name + ")" );
            if (srv.has_handshake) {
                logic_port_lines.push_back(string(".") + "srvready_" + srv_name + "(srvready_" + srv_name + ")" );
            }
            for (const auto &arg : srv.args) {
                logic_port_lines.push_back(string(".") + "srvarg_" + srv_name + "_" + arg.name + "(srvarg_" + srv_name + "_" + arg.name + ")" );
            }
            for (const auto &ret : srv.rets) {
                logic_port_lines.push_back(string(".") + "srvret_" + srv_name + "_" + ret.name + "(srvret_" + srv_name + "_" + ret.name + ")" );
            }
        }
    }
    instance_field.push_back(LogicSubModuleName(module.name) + " u_logic (\n");
    for (size_t i = 0; i < logic_port_lines.size(); i++) {
        instance_field.push_back(CodeTab + logic_port_lines[i] + (i == logic_port_lines.size() - 1 ? "" : ",") + "\n");
    }
    instance_field.push_back(");\n");

    out_lines.push_back("module " + module.name + "(\n");
    for (uint32_t i = 0; i < ports.size(); i++) {
        const auto &port = ports[i];
        string def_str = (port.is_input ? "input wire " : "output wire ") + ("[" + std::to_string(port.width - 1) + ":0]") + " " + port.name;
        out_lines.push_back(CodeTab + def_str + (i == ports.size() - 1 ? "" : ",") + "\n");
    }
    out_lines.push_back(");\n");
    out_lines.push_back("\n");
    for (const auto &line : wire_field) {
        out_lines.push_back(line);
    }
    out_lines.push_back("\n");
    for (const auto &line : instance_field) {
        out_lines.push_back(line);
    }
    out_lines.push_back("\n");
    out_lines.push_back("endmodule\n");

    out_lines.push_back("\n");
    for (const auto &line : reg_verilog_lines) {
        out_lines.push_back(line);
    }
    out_lines.push_back("\n");

    return "";
}



} // namespace rtlgen
