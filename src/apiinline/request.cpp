// MIT License

// Copyright (c) 2025 Meng Chengzhen, in Shandong University

#include "apiinline/request.hpp"

#include "apiinline/utils.hpp"

#include <sstream>
#include <unordered_map>

namespace apiinline {

namespace {

struct ReqArgInfo {
    string name;
    string type_str;
    uint32_t width = 0;
    vector<FlatField> fields;
};

struct RequestInfo {
    const VulStaticReqServ *req = nullptr;
    vector<ReqArgInfo> args;
    vector<ReqArgInfo> rets;
};

string vldPort(const string &name) { return name + "__vld__"; }
string rdyPort(const string &name) { return name + "__rdy__"; }
string argPort(const string &name, const string &arg) { return name + "_" + arg + "__"; }

string argFieldExpr(const string &root_expr, const string &decl_name, const string &flat_name) {
    if (flat_name == decl_name) {
        return root_expr;
    }
    const string prefix = decl_name + ".";
    if (flat_name.rfind(prefix, 0) == 0) {
        return root_expr + "." + flat_name.substr(prefix.size());
    }
    return flatFieldValueExpr(root_expr, flat_name);
}

bool parseRequestCall(
    const string &code,
    const vector<TokenInfo> &tokens,
    int name_idx,
    string &idx_expr,
    vector<string> &args,
    int &close_idx
) {
    int cursor = name_idx + 1;
    idx_expr = "0";
    if (cursor < static_cast<int>(tokens.size()) && tokens[cursor].spelling == "<") {
        int close_angle = findMatching(tokens, cursor, "<", ">");
        if (close_angle < 0) {
            return false;
        }
        if (cursor + 1 <= close_angle - 1) {
            idx_expr = sourceBetween(code, tokens[cursor + 1], tokens[close_angle - 1]);
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

string requestCallExpr(const RequestInfo &info, const string &idx_expr, const vector<string> &call_args) {
    const auto &req = *info.req;
    std::ostringstream os;
    os << "([&]()";
    if (req.has_handshake) {
        os << " -> bool";
    }
    os << " {\n";
    const string sel = req.is_arrayed ? ("[" + idx_expr + "]") : "";
    os << "  " << vldPort(req.name) << sel << " = true;\n";

    size_t arg_idx = 0;
    for (const auto &arg : info.args) {
        if (arg_idx >= call_args.size()) {
            break;
        }
        os << "  auto __vul_req_arg_" << arg.name << " = (" << call_args[arg_idx] << ");\n";
        for (const auto &field : arg.fields) {
            os << "  " << uintExtractExpr(argPort(req.name, arg.name) + sel, field.offset + field.width - 1, field.offset)
               << " = " << argFieldExpr("__vul_req_arg_" + arg.name, arg.name, field.name) << ";\n";
        }
        ++arg_idx;
    }

    for (const auto &ret : info.rets) {
        if (arg_idx >= call_args.size()) {
            break;
        }
        const string ret_expr = call_args[arg_idx];
        for (const auto &field : ret.fields) {
            os << "  " << argFieldExpr(ret_expr, ret.name, field.name) << " = "
               << uintExtractExpr(argPort(req.name, ret.name) + sel, field.offset + field.width - 1, field.offset)
               << ";\n";
        }
        ++arg_idx;
    }

    if (req.has_handshake) {
        os << "  return " << rdyPort(req.name) << sel << ";\n";
    }
    os << "}())";
    return os.str();
}

} // namespace

InlineCode inlineRequestAPIs(
    const VulStaticModuleInstance &module,
    const VulStaticBundleLib &bundlelib,
    const vector<string> &logic_hls_codes,
    const VulDebugLocs &logic_hls_debug
) {
    unordered_map<string, RequestInfo> requests;
    for (const auto &[name, req] : module.requests) {
        RequestInfo info;
        info.req = &req;
        for (const auto &arg : req.args) {
            ReqArgInfo out;
            out.name = arg.name;
            out.type_str = arg.type.toString();
            flatten_type_signature(arg.type, bundlelib, arg.name, out.width, out.fields);
            info.args.push_back(std::move(out));
        }
        for (const auto &ret : req.rets) {
            ReqArgInfo out;
            out.name = ret.name;
            out.type_str = ret.type.toString();
            flatten_type_signature(ret.type, bundlelib, ret.name, out.width, out.fields);
            info.rets.push_back(std::move(out));
        }
        requests[name] = std::move(info);
    }
    if (requests.empty()) {
        return {logic_hls_codes, logic_hls_debug};
    }

    string code = joinLines(logic_hls_codes);
    vector<TokenInfo> tokens = tokenizeWithLibclang(code);
    vector<Replacement> repls;
    for (int i = 0; i < static_cast<int>(tokens.size()); ++i) {
        auto it = requests.find(tokens[i].spelling);
        if (it == requests.end() || tokens[i].kind != CXToken_Identifier) {
            continue;
        }
        bool is_member_name = (i > 0 && tokens[i - 1].spelling == ".");
        bool is_decl_name = (i > 0 && (tokens[i - 1].spelling == "void" || tokens[i - 1].spelling == "bool"));
        if (is_member_name || is_decl_name) {
            continue;
        }
        string idx_expr;
        vector<string> args;
        int close_idx = -1;
        if (!parseRequestCall(code, tokens, i, idx_expr, args, close_idx)) {
            continue;
        }
        if (!overlapsExisting(repls, tokens[i].start, tokens[close_idx].end)) {
            repls.push_back(Replacement{
                tokens[i].start,
                tokens[close_idx].end,
                requestCallExpr(it->second, idx_expr, args)
            });
        }
    }

    return applyReplacementsWithDebug(logic_hls_codes, logic_hls_debug, std::move(repls));
}

vector<string> inlineRequestAPIs(
    const VulStaticModuleInstance &module,
    const VulStaticBundleLib &bundlelib,
    const vector<string> &logic_hls_codes
) {
    return inlineRequestAPIs(module, bundlelib, logic_hls_codes, {}).lines;
}

} // namespace apiinline
