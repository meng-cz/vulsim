// MIT License

// Copyright (c) 2025 Meng Chengzhen, in Shandong University

#include "apiinline/bram.hpp"

#include "apiinline/utils.hpp"

#include <sstream>
#include <unordered_map>

namespace apiinline {

namespace {

struct MemoryInfo {
    bool is_rom = false;
    bool is_1rw = false;
    string name;
    string data_type_str;
    uint32_t data_width = 0;
    vector<FlatField> fields;
};

string unpackExpr(const MemoryInfo &info, const string &packed_expr) {
    if (info.is_rom) {
        return packed_expr;
    }
    std::ostringstream os;
    os << "([&]() -> " << info.data_type_str << " {\n";
    os << "  " << info.data_type_str << " value;\n";
    for (const auto &field : info.fields) {
        os << "  " << field.name << " = "
           << uintExtractExpr(packed_expr, field.offset + field.width - 1, field.offset)
           << ";\n";
    }
    os << "  return value;\n";
    os << "}())";
    return os.str();
}

void emitPack(std::ostringstream &os, const MemoryInfo &info, const string &packed_name, const string &value_name) {
    os << "  Int<" << info.data_width << "> " << packed_name << ";\n";
    for (const auto &field : info.fields) {
        os << "  " << uintExtractExpr(packed_name, field.offset + field.width - 1, field.offset)
           << " = " << flatFieldValueExpr(value_name, field.name) << ";\n";
    }
}

bool parseTemplateMethodCall(
    const string &code,
    const vector<TokenInfo> &tokens,
    int object_idx,
    string &method,
    string &port_expr,
    vector<string> &args,
    int &close_idx
) {
    if (object_idx + 2 >= static_cast<int>(tokens.size()) || tokens[object_idx + 1].spelling != ".") {
        return false;
    }
    int cursor = object_idx + 2;
    if (tokens[cursor].spelling == "template") {
        ++cursor;
    }
    if (cursor >= static_cast<int>(tokens.size())) {
        return false;
    }
    method = tokens[cursor].spelling;
    ++cursor;
    port_expr = "0";
    if (cursor < static_cast<int>(tokens.size()) && tokens[cursor].spelling == "<") {
        int close_angle = findMatching(tokens, cursor, "<", ">");
        if (close_angle < 0) {
            return false;
        }
        if (cursor + 1 <= close_angle - 1) {
            port_expr = sourceBetween(code, tokens[cursor + 1], tokens[close_angle - 1]);
        }
        cursor = close_angle + 1;
    }
    if (cursor >= static_cast<int>(tokens.size()) || tokens[cursor].spelling != "(") {
        return false;
    }
    close_idx = findMatching(tokens, cursor, "(", ")");
    if (close_idx < 0) {
        return false;
    }
    args = splitTopLevelArgs(code, tokens, cursor + 1, close_idx - 1);
    return true;
}

string bram1rwReqBlock(const MemoryInfo &info, const vector<string> &args) {
    if (args.size() < 3) {
        return "";
    }
    const string base = "bram_" + info.name;
    std::ostringstream os;
    os << "{\n";
    os << "  auto __vul_bram_wdata_value = (" << args[1] << ");\n";
    emitPack(os, info, "__vul_bram_packed", "__vul_bram_wdata_value");
    os << "  " << base << "__s1_en = true;\n";
    os << "  " << base << "__s1_we = (" << args[2] << ");\n";
    os << "  " << base << "__s1_addr = (" << args[0] << ");\n";
    os << "  " << base << "__s1_wdata = __vul_bram_packed;\n";
    os << "}";
    return os.str();
}

string bramWriteBlock(const MemoryInfo &info, const string &port_expr, const vector<string> &args) {
    if (args.size() < 2) {
        return "";
    }
    const string base = "bram_" + info.name;
    std::ostringstream os;
    os << "{\n";
    os << "  auto __vul_bram_wdata_value = (" << args[1] << ");\n";
    emitPack(os, info, "__vul_bram_packed", "__vul_bram_wdata_value");
    os << "  " << base << "__s1_write[" << port_expr << "] = true;\n";
    os << "  " << base << "__s1_writeaddr[" << port_expr << "] = (" << args[0] << ");\n";
    os << "  " << base << "__s1_writedata[" << port_expr << "] = __vul_bram_packed;\n";
    os << "}";
    return os.str();
}

string readReqBlock(const MemoryInfo &info, const string &port_expr, const vector<string> &args) {
    if (args.empty()) {
        return "";
    }
    const string prefix = info.is_rom ? "rom_" : "bram_";
    const string base = prefix + info.name;
    std::ostringstream os;
    os << "{\n";
    os << "  " << base << "__s1_readreq[" << port_expr << "] = true;\n";
    os << "  " << base << "__s1_readaddr[" << port_expr << "] = (" << args[0] << ");\n";
    os << "}";
    return os.str();
}

string readDataExpr(const MemoryInfo &info, const string &port_expr) {
    if (info.is_rom) {
        return "rom_" + info.name + "__s2_readdata[" + port_expr + "]";
    }
    if (info.is_1rw) {
        return unpackExpr(info, "bram_" + info.name + "__s2_rdata");
    }
    return unpackExpr(info, "bram_" + info.name + "__s2_readdata[" + port_expr + "]");
}

} // namespace

InlineCode inlineMemoryAPIs(
    const VulStaticModuleInstance &module,
    const VulStaticBundleLib &bundlelib,
    const vector<string> &logic_hls_codes,
    const VulDebugLocs &logic_hls_debug
) {
    unordered_map<string, MemoryInfo> memories;
    for (const auto &bram : module.brams) {
        MemoryInfo info;
        info.name = bram.name;
        info.is_1rw = (bram.read_ports == 0 || bram.write_ports == 0);
        info.data_type_str = bram.data_type.toString();
        flatten_type_signature(bram.data_type, bundlelib, "value", info.data_width, info.fields);
        memories[bram.name] = std::move(info);
    }
    for (const auto &rom : module.roms) {
        MemoryInfo info;
        info.name = rom.name;
        info.is_rom = true;
        info.data_type_str = "Int<" + std::to_string(rom.data_width) + ">";
        info.data_width = static_cast<uint32_t>(rom.data_width);
        memories[rom.name] = std::move(info);
    }
    if (memories.empty()) {
        return {logic_hls_codes, logic_hls_debug};
    }

    string code = joinLines(logic_hls_codes);
    vector<TokenInfo> tokens = tokenizeWithLibclang(code);
    vector<Replacement> repls;
    for (int i = 0; i < static_cast<int>(tokens.size()); ++i) {
        auto it = memories.find(tokens[i].spelling);
        if (it == memories.end() || tokens[i].kind != CXToken_Identifier) {
            continue;
        }
        const MemoryInfo &info = it->second;
        string method;
        string port_expr;
        vector<string> args;
        int close_idx = -1;
        if (!parseTemplateMethodCall(code, tokens, i, method, port_expr, args, close_idx)) {
            continue;
        }
        string text;
        if (info.is_rom) {
            if (method == "readreq") {
                text = readReqBlock(info, port_expr, args);
            } else if (method == "readdata") {
                text = readDataExpr(info, port_expr);
            }
        } else if (info.is_1rw) {
            if (method == "req") {
                text = bram1rwReqBlock(info, args);
            } else if (method == "readdata") {
                text = readDataExpr(info, port_expr);
            }
        } else {
            if (method == "readreq") {
                text = readReqBlock(info, port_expr, args);
            } else if (method == "readdata") {
                text = readDataExpr(info, port_expr);
            } else if (method == "write") {
                text = bramWriteBlock(info, port_expr, args);
            }
        }
        if (text.empty()) {
            continue;
        }
        if (!overlapsExisting(repls, tokens[i].start, tokens[close_idx].end)) {
            repls.push_back(Replacement{tokens[i].start, tokens[close_idx].end, text});
        }
    }

    return applyReplacementsWithDebug(logic_hls_codes, logic_hls_debug, std::move(repls));
}

vector<string> inlineMemoryAPIs(
    const VulStaticModuleInstance &module,
    const VulStaticBundleLib &bundlelib,
    const vector<string> &logic_hls_codes
) {
    return inlineMemoryAPIs(module, bundlelib, logic_hls_codes, {}).lines;
}

} // namespace apiinline
