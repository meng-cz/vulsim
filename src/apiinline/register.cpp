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
    string rdata_port;
    string wen_port;
    string wdata_port;
};

string readRegisterExpr(const RegisterInfo &info, const string &rdata_expr) {
    std::ostringstream os;
    os << "([&]() -> " << info.type_str << " {\n";
    os << "  " << info.type_str << " value;\n";
    for (const auto &field : info.fields) {
        os << "  " << field.name << " = "
           << uintExtractExpr(rdata_expr, field.offset + field.width - 1, field.offset)
           << ";\n";
    }
    os << "  return value;\n";
    os << "}())";
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
    os << "  auto __vul_reg_value = (" << value_expr << ");\n";
    os << "  Int<" << info.width << "> __vul_reg_wdata;\n";
    for (const auto &field : info.fields) {
        os << "  " << uintExtractExpr("__vul_reg_wdata", field.offset + field.width - 1, field.offset)
           << " = " << flatFieldValueExpr("__vul_reg_value", field.name) << ";\n";
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

vector<string> inlineRegisterAPIs(
    const VulStaticModuleInstance &module,
    const VulStaticBundleLib &bundlelib,
    const vector<string> &logic_hls_codes
) {
    unordered_map<string, RegisterInfo> registers;
    for (const auto &reg : module.registers) {
        RegisterInfo info;
        info.reg = &reg;
        info.type_str = reg.signature.toString();
        info.rdata_port = "rdata_" + reg.name + "__";
        info.wen_port = "wen_" + reg.name + "__";
        info.wdata_port = "wdata_" + reg.name + "__";
        flatten_type_signature(reg.signature, bundlelib, "value", info.width, info.fields);
        registers[reg.name] = std::move(info);
    }
    if (registers.empty()) {
        return logic_hls_codes;
    }

    string code = joinLines(logic_hls_codes);
    vector<TokenInfo> tokens = tokenizeWithLibclang(code);
    if (tokens.empty()) {
        return logic_hls_codes;
    }

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

        int method_idx = -1;
        int cursor = i + 1;
        if (cursor < static_cast<int>(tokens.size()) && tokens[cursor].spelling == ".") {
            ++cursor;
            if (cursor < static_cast<int>(tokens.size()) && tokens[cursor].spelling == "template") {
                ++cursor;
            }
            if (cursor < static_cast<int>(tokens.size()) && tokens[cursor].spelling == "setnext") {
                method_idx = cursor;
            }
        }
        if (method_idx >= 0) {
            string port_expr = "0";
            int open_idx = method_idx + 1;
            if (open_idx < static_cast<int>(tokens.size()) && tokens[open_idx].spelling == "<") {
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
                        repls.push_back(Replacement{
                            tokens[i].start,
                            tokens[close_idx].end,
                            writeRegisterBlock(info, idx_expr, port_expr, value_expr)
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

    return splitLinesKeepEnds(applyReplacements(std::move(code), std::move(repls)));
}

} // namespace apiinline
