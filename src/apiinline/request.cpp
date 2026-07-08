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
    string helper_name;
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

void emitRequestBody(
    std::ostringstream &os,
    const RequestInfo &info,
    const string &idx_expr,
    const vector<string> &call_args,
    bool emit_return = true
) {
    const auto &req = *info.req;
    const string sel = req.is_arrayed ? ("[" + idx_expr + "]") : "";
    os << "  " << vldPort(req.name) << sel << " = true;\n";

    size_t arg_idx = 0;
    for (const auto &arg : info.args) {
        if (arg_idx >= call_args.size()) {
            break;
        }
        os << "  auto __vul_req_arg_" << arg.name << " = (" << call_args[arg_idx] << ");\n";
        for (const auto &field : arg.fields) {
            const string value = packFlatFieldValueExpr(
                argFieldExpr("__vul_req_arg_" + arg.name, arg.name, field.name),
                field);
            os << "  " << uintExtractExpr(argPort(req.name, arg.name) + sel, field.offset + field.width - 1, field.offset)
               << " = " << value << ";\n";
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

    if (emit_return && req.has_handshake) {
        os << "  return " << rdyPort(req.name) << sel << ";\n";
    }
}

string requestHelperDef(const RequestInfo &info) {
    const auto &req = *info.req;
    if (!req.has_handshake) {
        return "";
    }
    std::ostringstream os;
    os << "auto " << info.helper_name << " = [&](";
    bool need_comma = false;
    if (req.is_arrayed) {
        os << "uint32_t __vul_req_idx";
        need_comma = true;
    }
    for (const auto &arg : info.args) {
        if (need_comma) os << ", ";
        os << "const " << arg.type_str << " &" << arg.name;
        need_comma = true;
    }
    for (const auto &ret : info.rets) {
        if (need_comma) os << ", ";
        os << ret.type_str << " &" << ret.name;
        need_comma = true;
    }
    os << ") -> bool {\n";
    vector<string> arg_names;
    for (const auto &arg : info.args) {
        arg_names.push_back(arg.name);
    }
    for (const auto &ret : info.rets) {
        arg_names.push_back(ret.name);
    }
    emitRequestBody(os, info, req.is_arrayed ? "__vul_req_idx" : "0", arg_names);
    os << "};\n";
    return os.str();
}

bool findDirectIfCondition(
    const vector<TokenInfo> &tokens,
    int call_idx,
    int close_idx,
    int &if_idx
) {
    if (call_idx < 2) {
        return false;
    }
    if (tokens[call_idx - 1].spelling != "(" || tokens[call_idx - 2].spelling != "if") {
        return false;
    }
    if (close_idx + 1 >= static_cast<int>(tokens.size()) || tokens[close_idx + 1].spelling != ")") {
        return false;
    }
    if_idx = call_idx - 2;
    return true;
}

string requestReadyPrelude(
    const RequestInfo &info,
    const string &idx_expr,
    const vector<string> &call_args,
    const string &ready_name
) {
    const auto &req = *info.req;
    std::ostringstream os;
    os << "{\n";
    emitRequestBody(os, info, idx_expr, call_args, false);
    os << "}\n";
    const string sel = req.is_arrayed ? ("[" + idx_expr + "]") : "";
    os << "bool " << ready_name << " = " << rdyPort(req.name) << sel << ";\n";
    return os.str();
}

string requestCallExpr(const RequestInfo &info, const string &idx_expr, const vector<string> &call_args) {
    const auto &req = *info.req;
    std::ostringstream os;
    if (req.has_handshake) {
        os << info.helper_name << "(";
        bool need_comma = false;
        if (req.is_arrayed) {
            os << idx_expr;
            need_comma = true;
        }
        for (const auto &arg : call_args) {
            if (need_comma) os << ", ";
            os << arg;
            need_comma = true;
        }
        os << ")";
        return os.str();
    }
    os << "{\n";
    emitRequestBody(os, info, idx_expr, call_args);
    if (!req.has_handshake) {
        os << "}";
    }
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
        info.helper_name = "__vul_req_call_" + req.name;
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
    string helper_defs;
    for (const auto &[name, info] : requests) {
        string helper = requestHelperDef(info);
        if (!helper.empty()) {
            helper_defs += helper;
            helper_defs += "\n";
        }
    }
    if (!helper_defs.empty()) {
        size_t logic_func_pos = code.find("\nvoid LogicSubModule_");
        if (logic_func_pos == string::npos) {
            logic_func_pos = code.find("void LogicSubModule_");
        } else {
            ++logic_func_pos;
        }
        size_t body_pos = logic_func_pos == string::npos ? string::npos : code.find(") {\n", logic_func_pos);
        if (body_pos != string::npos) {
            repls.push_back(Replacement{
                static_cast<uint32_t>(body_pos + 4),
                static_cast<uint32_t>(body_pos + 4),
                helper_defs
            });
        }
    }
    int ready_counter = 0;
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
        if (it->second.req->has_handshake) {
            int if_idx = -1;
            if (findDirectIfCondition(tokens, i, close_idx, if_idx)) {
                string ready_name = "__vul_req_ready_" + it->second.req->name + "_" + std::to_string(ready_counter++);
                if (!overlapsExisting(repls, tokens[if_idx].start, tokens[if_idx].start) &&
                    !overlapsExisting(repls, tokens[i].start, tokens[close_idx].end)) {
                    repls.push_back(Replacement{
                        tokens[if_idx].start,
                        tokens[if_idx].start,
                        requestReadyPrelude(it->second, idx_expr, args, ready_name)
                    });
                    repls.push_back(Replacement{
                        tokens[i].start,
                        tokens[close_idx].end,
                        ready_name
                    });
                }
                continue;
            }
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
