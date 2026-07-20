// MIT License

// Copyright (c) 2025 Meng Chengzhen, in Shandong University

#include "apiinline/register.hpp"

#include "apiinline/utils.hpp"

#include <sstream>
#include <unordered_map>

namespace apiinline {

namespace {

struct RegisterInfo {
    const VulStaticRegister *reg = nullptr;
    uint32_t width = 0;
    vector<FlatField> fields;
    string type_str;
    string helper_name;
    string rdata_port;
    string wen_port;
    string wdata_port;
    string holdnext_port;
    string resetnext_port;
    string default_expr;
};

string nestedArrayType(string element, const RegisterInfo &info, bool include_ports) {
    if (include_ports && info.reg->ports > 1) {
        element = "std::array<" + element + ", " + std::to_string(info.reg->ports) + ">";
    }
    if (!info.reg->dims.empty()) {
        element = "std::array<" + element + ", " + std::to_string(info.reg->dims[0]) + ">";
    }
    return element;
}

string readRegisterExpr(const RegisterInfo &info, const string &rdata_expr) {
    return info.helper_name + "(" + rdata_expr + ")";
}

string readRegisterHelper(const RegisterInfo &info) {
    std::ostringstream os;
    const string rdata_arg = "__vul_rdata_" + info.reg->name;
    os << info.type_str << " " << info.helper_name
       << "(const Int<" << info.width << "> &" << rdata_arg << ") {\n";
    if (info.fields.size() == 1 && info.fields[0].name == "value") {
        os << "  " << info.type_str << " value = " << info.default_expr << ";\n";
    } else {
        os << "  " << info.type_str << " value = " << info.default_expr << ";\n";
    }
    for (const auto &field : info.fields) {
        const string extracted = uintExtractExpr(rdata_arg, field.offset + field.width - 1, field.offset);
        os << "  " << field.name << " = "
           << unpackFlatFieldValueExpr(field.name, extracted, field) << ";\n";
    }
    os << "  return value;\n";
    os << "}\n";
    return os.str();
}

string registerWriteHelper(const RegisterInfo &info) {
    const bool is_array = !info.reg->dims.empty();
    std::ostringstream os;
    os << "void __vul_reg_setnext_" << info.reg->name << "(";
    if (is_array) {
        os << "uint32_t __vul_reg_idx, ";
    }
    if (info.reg->ports > 1) {
        os << "uint32_t __vul_reg_port, ";
    }
    os << info.type_str << " __vul_reg_value, "
       << nestedArrayType("Int<" + std::to_string(info.width) + ">", info, true)
       << " &__vul_wdata, "
       << nestedArrayType("bool", info, true) << " &__vul_wen) {\n";
    os << "  Int<" << info.width << "> __vul_reg_wdata = 0;\n";
    for (const auto &field : info.fields) {
        const string value = packFlatFieldValueExpr(
            flatFieldValueExpr("__vul_reg_value", field.name), field);
        os << "  " << uintExtractExpr("__vul_reg_wdata", field.offset + field.width - 1, field.offset)
           << " = " << value << ";\n";
    }
    const string idx = is_array ? "[__vul_reg_idx]" : "";
    const string port = info.reg->ports > 1 ? "[__vul_reg_port]" : "";
    os << "  __vul_wdata" << idx << port << " = __vul_reg_wdata;\n";
    os << "  __vul_wen" << idx << port << " = true;\n";
    os << "}\n";
    return os.str();
}

string registerControlHelpers(const RegisterInfo &info) {
    std::ostringstream os;
    const bool is_array = !info.reg->dims.empty();
    for (const auto &method : {"holdnext", "resetnext"}) {
        const string port = method == string("holdnext")
            ? info.holdnext_port : info.resetnext_port;
        const string helper = "__vul_reg_" + string(method) + "_" + info.reg->name;
        if (is_array) {
            os << "void " << helper << "(uint32_t __vul_reg_idx, "
               << nestedArrayType("bool", info, false) << " &__vul_control) {\n";
            os << "  __vul_control[__vul_reg_idx] = true;\n";
            os << "}\n";
        }
        os << "void " << helper << "("
           << nestedArrayType("bool", info, false) << " &__vul_control) {\n";
        if (is_array) {
            os << "  for (uint32_t __vul_i = 0; __vul_i < " << info.reg->dims[0]
               << "; ++__vul_i) {\n";
            os << "    __vul_control[__vul_i] = true;\n";
            os << "  }\n";
        } else {
            os << "  __vul_control = true;\n";
        }
        os << "}\n";
    }
    return os.str();
}

string controlRegisterBlock(
    const RegisterInfo &info,
    const string &method_name,
    const string &idx_expr
) {
    const bool is_array = (!info.reg->dims.empty());
    const string &port = (method_name == "holdnext") ? info.holdnext_port : info.resetnext_port;
    std::ostringstream os;
    os << "{\n";
    if (is_array && idx_expr.empty()) {
        os << "  for (uint32_t __vul_i = 0; __vul_i < " << info.reg->dims[0] << "; ++__vul_i) {\n";
        os << "    " << port << "[__vul_i] = true;\n";
        os << "  }\n";
    } else if (is_array) {
        os << "  " << port << "[" << idx_expr << "] = true;\n";
    } else {
        os << "  " << port << " = true;\n";
    }
    os << "}";
    return os.str();
}

string writeRegisterBlock(
    const RegisterInfo &info,
    const string &idx_expr,
    const string &port_expr,
    const string &value_expr
) {
    const bool is_ported = (info.reg->ports > 1);
    const bool is_array = (!info.reg->dims.empty());
    std::ostringstream os;
    os << "{\n";
    os << "  " << info.type_str << " __vul_reg_value = (" << value_expr << ");\n";
    os << "  Int<" << info.width << "> __vul_reg_wdata = 0;\n";
    for (const auto &field : info.fields) {
        const string value = packFlatFieldValueExpr(flatFieldValueExpr("__vul_reg_value", field.name), field);
        os << "  " << uintExtractExpr("__vul_reg_wdata", field.offset + field.width - 1, field.offset)
           << " = " << value << ";\n";
    }
    if (is_array && is_ported) {
        os << "  " << info.wdata_port << "[" << idx_expr << "][" << port_expr << "] = __vul_reg_wdata;\n";
        os << "  " << info.wen_port << "[" << idx_expr << "][" << port_expr << "] = true;\n";
    } else if (is_array) {
        os << "  " << info.wdata_port << "[" << idx_expr << "] = __vul_reg_wdata;\n";
        os << "  " << info.wen_port << "[" << idx_expr << "] = true;\n";
    } else if (is_ported) {
        os << "  " << info.wdata_port << "[" << port_expr << "] = __vul_reg_wdata;\n";
        os << "  " << info.wen_port << "[" << port_expr << "] = true;\n";
    } else {
        os << "  " << info.wdata_port << " = __vul_reg_wdata;\n";
        os << "  " << info.wen_port << " = true;\n";
    }
    os << "}";
    return os.str();
}

string inlineRegisterReadsInExpr(
    const string &expr,
    const unordered_map<string, RegisterInfo> &registers
) {
    vector<TokenInfo> tokens = tokenizeWithLibclang(expr);
    vector<Replacement> repls;
    for (int i = 0; i < static_cast<int>(tokens.size()); ++i) {
        auto reg_it = registers.find(tokens[i].spelling);
        if (reg_it == registers.end() || tokens[i].kind != CXToken_Identifier) {
            continue;
        }
        const RegisterInfo &info = reg_it->second;

        if (i + 2 < static_cast<int>(tokens.size()) && tokens[i + 1].spelling == "." && tokens[i + 2].spelling == "get") {
            int open_idx = i + 3;
            if (open_idx < static_cast<int>(tokens.size()) && tokens[open_idx].spelling == "(") {
                int close_idx = findMatching(tokens, open_idx, "(", ")");
                if (close_idx >= 0 && !overlapsExisting(repls, tokens[i].start, tokens[close_idx].end)) {
                    repls.push_back(Replacement{
                        tokens[i].start,
                        tokens[close_idx].end,
                        readRegisterExpr(info, info.rdata_port)
                    });
                }
            }
            continue;
        }

        if (!info.reg->dims.empty() && i + 1 < static_cast<int>(tokens.size()) && tokens[i + 1].spelling == "[") {
            int close_idx = findMatching(tokens, i + 1, "[", "]");
            if (close_idx >= 0 && i + 2 <= close_idx - 1 && !overlapsExisting(repls, tokens[i].start, tokens[close_idx].end)) {
                string idx_expr = sourceBetween(expr, tokens[i + 2], tokens[close_idx - 1]);
                idx_expr = inlineRegisterReadsInExpr(idx_expr, registers);
                repls.push_back(Replacement{
                    tokens[i].start,
                    tokens[close_idx].end,
                    readRegisterExpr(info, info.rdata_port + "[" + idx_expr + "]")
                });
            }
            continue;
        }

        bool is_member_receiver = (i + 1 < static_cast<int>(tokens.size()) && tokens[i + 1].spelling == ".");
        bool is_member_name = (i > 0 && tokens[i - 1].spelling == ".");
        if (!is_member_receiver && !is_member_name && !overlapsExisting(repls, tokens[i].start, tokens[i].end)) {
            repls.push_back(Replacement{
                tokens[i].start,
                tokens[i].end,
                readRegisterExpr(info, info.rdata_port)
            });
        }
    }
    return applyReplacements(expr, std::move(repls));
}

} // namespace

InlineCode inlineRegisterAPIs(
    const VulStaticModuleInstance &module,
    const VulStaticBundleLib &bundlelib,
    const vector<string> &logic_hls_codes,
    const VulDebugLocs &logic_hls_debug
) {
    unordered_map<string, RegisterInfo> registers;
    for (const auto &reg : module.registers) {
        RegisterInfo info;
        info.reg = &reg;
        info.type_str = reg.signature.toString();
        info.helper_name = "__vul_read_reg_" + reg.name;
        info.rdata_port = "rdata_" + reg.name + "__";
        info.wen_port = "wen_" + reg.name + "__";
        info.wdata_port = "wdata_" + reg.name + "__";
        info.holdnext_port = "holdnext_" + reg.name + "__";
        info.resetnext_port = "resetnext_" + reg.name + "__";
        flatten_type_signature(reg.signature, bundlelib, "value", info.width, info.fields);
        if (info.fields.size() == 1 && info.fields[0].name == "value" &&
            enumDefaultValueExpr(reg.signature, bundlelib).empty()) {
            info.default_expr = "0";
        } else {
            info.default_expr = defaultValueExprForType(reg.signature, bundlelib);
        }
        registers[reg.name] = std::move(info);
    }
    if (registers.empty()) {
        return {logic_hls_codes, logic_hls_debug};
    }

    string code = joinLines(logic_hls_codes);
    vector<TokenInfo> tokens = tokenizeWithLibclang(code);
    if (tokens.empty()) {
        return {logic_hls_codes, logic_hls_debug};
    }
    vector<Replacement> repls;
    string helper_defs;
    for (const auto &entry : registers) {
        helper_defs += readRegisterHelper(entry.second);
        helper_defs += registerWriteHelper(entry.second);
        helper_defs += registerControlHelpers(entry.second);
        helper_defs += "\n";
    }
    size_t logic_func_pos = findLogicSubmoduleFunctionStart(code);
    size_t helper_pos = findLogicSubmoduleFunctionStart(code);
    if (helper_pos != string::npos && !helper_defs.empty()) {
        repls.push_back(Replacement{
            static_cast<uint32_t>(helper_pos),
            static_cast<uint32_t>(helper_pos),
            helper_defs
        });
    }
    for (int i = 0; i < static_cast<int>(tokens.size()); ++i) {
        if (logic_func_pos != string::npos && tokens[i].start < logic_func_pos) {
            continue;
        }
        auto reg_it = registers.find(tokens[i].spelling);
        if (reg_it == registers.end() || tokens[i].kind != CXToken_Identifier) {
            continue;
        }
        const RegisterInfo &info = reg_it->second;

        if (i + 2 < static_cast<int>(tokens.size()) && tokens[i + 1].spelling == "." && tokens[i + 2].spelling == "get") {
            int open_idx = i + 3;
            if (open_idx < static_cast<int>(tokens.size()) && tokens[open_idx].spelling == "(") {
                int close_idx = findMatching(tokens, open_idx, "(", ")");
                if (close_idx >= 0 && !overlapsExisting(repls, tokens[i].start, tokens[close_idx].end)) {
                    repls.push_back(Replacement{
                        tokens[i].start,
                        tokens[close_idx].end,
                        readRegisterExpr(info, info.rdata_port)
                    });
                }
            }
            continue;
        }

        int method_idx = -1;
        int cursor = i + 1;
        if (cursor < static_cast<int>(tokens.size()) && tokens[cursor].spelling == ".") {
            ++cursor;
            if (cursor < static_cast<int>(tokens.size()) && tokens[cursor].spelling == "template") {
                ++cursor;
            }
            if (cursor < static_cast<int>(tokens.size()) &&
                (tokens[cursor].spelling == "setnext" ||
                 tokens[cursor].spelling == "holdnext" ||
                 tokens[cursor].spelling == "resetnext")) {
                method_idx = cursor;
            }
        }
        if (method_idx >= 0) {
            const string method_name = tokens[method_idx].spelling;
            string port_expr = "0";
            int open_idx = method_idx + 1;
            if (method_name == "setnext" && open_idx < static_cast<int>(tokens.size()) && tokens[open_idx].spelling == "<") {
                int close_angle = findMatching(tokens, open_idx, "<", ">");
                if (close_angle >= 0) {
                    if (open_idx + 1 <= close_angle - 1) {
                        port_expr = sourceBetween(code, tokens[open_idx + 1], tokens[close_angle - 1]);
                    }
                    open_idx = close_angle + 1;
                }
            }
            if (open_idx < static_cast<int>(tokens.size()) && tokens[open_idx].spelling == "(") {
                int close_idx = findMatching(tokens, open_idx, "(", ")");
                if (close_idx >= 0) {
                    vector<string> args = splitTopLevelArgs(code, tokens, open_idx + 1, close_idx - 1);
                    const bool is_array = !info.reg->dims.empty();
                    if (method_name == "holdnext" || method_name == "resetnext") {
                        string idx_expr;
                        if (is_array && args.size() == 1) {
                            idx_expr = inlineRegisterReadsInExpr(args[0], registers);
                        } else if (!args.empty()) {
                            continue;
                        }
                        if (!overlapsExisting(repls, tokens[i].start, tokens[close_idx].end)) {
                            string call = "__vul_reg_" + method_name + "_" + info.reg->name + "(";
                            if (!idx_expr.empty()) {
                                call += idx_expr + ", ";
                            }
                            call += method_name == "holdnext"
                                ? info.holdnext_port : info.resetnext_port;
                            call += ")";
                            repls.push_back(Replacement{
                                tokens[i].start,
                                tokens[close_idx].end,
                                call
                            });
                        }
                        continue;
                    }
                    string idx_expr;
                    string value_expr;
                    if (is_array && args.size() >= 2) {
                        idx_expr = inlineRegisterReadsInExpr(args[0], registers);
                        value_expr = inlineRegisterReadsInExpr(args[1], registers);
                    } else if (!is_array && args.size() >= 1) {
                        value_expr = inlineRegisterReadsInExpr(args[0], registers);
                    } else {
                        continue;
                    }
                    if (!overlapsExisting(repls, tokens[i].start, tokens[close_idx].end)) {
                        string call = "__vul_reg_setnext_" + info.reg->name;
                        call += "(";
                        if (is_array) {
                            call += idx_expr + ", ";
                        }
                        if (info.reg->ports > 1) {
                            call += port_expr + ", ";
                        }
                        call += value_expr + ", " + info.wdata_port + ", " + info.wen_port + ")";
                        repls.push_back(Replacement{
                            tokens[i].start,
                            tokens[close_idx].end,
                            call
                        });
                    }
                }
            }
            continue;
        }

        if (!info.reg->dims.empty() && i + 1 < static_cast<int>(tokens.size()) && tokens[i + 1].spelling == "[") {
            int close_idx = findMatching(tokens, i + 1, "[", "]");
            if (close_idx >= 0 && i + 2 <= close_idx - 1 && !overlapsExisting(repls, tokens[i].start, tokens[close_idx].end)) {
                string idx_expr = sourceBetween(code, tokens[i + 2], tokens[close_idx - 1]);
                repls.push_back(Replacement{
                    tokens[i].start,
                    tokens[close_idx].end,
                    readRegisterExpr(info, info.rdata_port + "[" + idx_expr + "]")
                });
            }
            continue;
        }

        bool is_member_receiver = (i + 1 < static_cast<int>(tokens.size()) && tokens[i + 1].spelling == ".");
        bool is_member_name = (i > 0 && tokens[i - 1].spelling == ".");
        if (!is_member_receiver && !is_member_name && !overlapsExisting(repls, tokens[i].start, tokens[i].end)) {
            repls.push_back(Replacement{
                tokens[i].start,
                tokens[i].end,
                readRegisterExpr(info, info.rdata_port)
            });
        }
    }

    return applyReplacementsWithDebug(logic_hls_codes, logic_hls_debug, std::move(repls));
}

vector<string> inlineRegisterAPIs(
    const VulStaticModuleInstance &module,
    const VulStaticBundleLib &bundlelib,
    const vector<string> &logic_hls_codes
) {
    return inlineRegisterAPIs(module, bundlelib, logic_hls_codes, {}).lines;
}

} // namespace apiinline
