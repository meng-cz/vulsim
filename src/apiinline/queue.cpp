// MIT License

// Copyright (c) 2025 Meng Chengzhen, in Shandong University

#include "apiinline/queue.hpp"

#include "apiinline/utils.hpp"

#include <sstream>
#include <unordered_map>

namespace apiinline {

namespace {

struct QueueInfo {
    const VulStaticQueue *queue = nullptr;
    uint32_t data_width = 0;
    vector<FlatField> fields;
    string type_str;
    string helper_name;
    string front_helper_name;
    string enqready;
    string deqready;
    string enqvalid;
    string deqvalid;
    string enqdata;
    string deqdata;
    string clrnext;
    string default_expr;
};

string unpackValueExpr(const QueueInfo &info, const string &packed_expr) {
    return info.helper_name + "(" + packed_expr + ")";
}

string unpackValueHelper(const QueueInfo &info) {
    std::ostringstream os;
    os << "auto " << info.helper_name
       << " = [&](const Int<" << info.data_width
       << "> &__vul_queue_packed) -> " << info.type_str << " {\n";
    os << "  " << info.type_str << " value = " << info.default_expr << ";\n";
    for (const auto &field : info.fields) {
        const string extracted = uintExtractExpr("__vul_queue_packed", field.offset + field.width - 1, field.offset);
        os << "  " << field.name << " = "
           << castToLvalueTypeExpr(field.name, extracted) << ";\n";
    }
    os << "  return value;\n";
    os << "};\n";
    return os.str();
}

void emitPackValue(std::ostringstream &os, const QueueInfo &info, const string &packed_name, const string &value_name) {
    os << "  Int<" << info.data_width << "> " << packed_name << " = 0;\n";
    for (const auto &field : info.fields) {
        const string value = packFlatFieldValueExpr(flatFieldValueExpr(value_name, field.name), field.width);
        os << "  " << uintExtractExpr(packed_name, field.offset + field.width - 1, field.offset)
           << " = " << value << ";\n";
    }
}

string enqNextBlock(const QueueInfo &info, const vector<string> &args) {
    const uint32_t enq_width = static_cast<uint32_t>(info.queue->enq_width);
    const bool is_mp = (info.queue->enq_width > 1 || info.queue->deq_width > 1);
    std::ostringstream os;
    os << "{\n";
    if (!is_mp) {
        string value_expr = args.empty() ? "{}" : args[0];
        os << "  auto __vul_queue_value = (" << value_expr << ");\n";
        emitPackValue(os, info, "__vul_queue_packed", "__vul_queue_value");
        os << "  " << info.enqdata << " = __vul_queue_packed;\n";
        os << "  " << info.enqvalid << " = true;\n";
    } else {
        string values_expr = args.empty() ? "{}" : args[0];
        string num_expr = (args.size() >= 2) ? args[1] : std::to_string(enq_width);
        os << "  auto __vul_queue_values = (" << values_expr << ");\n";
        os << "  uint32_t __vul_queue_req = (" << num_expr << ") < " << enq_width
           << " ? (" << num_expr << ") : " << enq_width << ";\n";
        for (uint32_t i = 0; i < enq_width; ++i) {
            os << "  if (__vul_queue_req > " << i << ") {\n";
            os << "    auto __vul_queue_value = __vul_queue_values[" << i << "];\n";
            os << "    Int<" << info.data_width << "> __vul_queue_packed = 0;\n";
            for (const auto &field : info.fields) {
                os << "    " << uintExtractExpr("__vul_queue_packed", field.offset + field.width - 1, field.offset)
                   << " = " << flatFieldValueExpr("__vul_queue_value", field.name) << ";\n";
            }
            os << "    " << info.enqdata << "[" << i << "] = __vul_queue_packed;\n";
            os << "  }\n";
        }
        os << "  " << info.enqvalid << " = __vul_queue_req;\n";
    }
    os << "}";
    return os.str();
}

string frontExpr(const QueueInfo &info, const vector<string> &args) {
    const uint32_t deq_width = static_cast<uint32_t>(info.queue->deq_width);
    const bool is_mp = (info.queue->enq_width > 1 || info.queue->deq_width > 1);
    if (!is_mp) {
        return unpackValueExpr(info, info.deqdata);
    }
    return info.front_helper_name + "(" + info.deqvalid + ", " + info.deqdata + ")";
}

string frontHelper(const QueueInfo &info) {
    const uint32_t deq_width = static_cast<uint32_t>(info.queue->deq_width);
    const bool is_mp = (info.queue->enq_width > 1 || info.queue->deq_width > 1);
    if (!is_mp) {
        return "";
    }
    std::ostringstream os;
    os << "auto " << info.front_helper_name
       << " = [&](const uint32_t __vul_queue_deqvalid, const std::array<Int<"
       << info.data_width << ">, " << deq_width
       << "> &__vul_queue_deqdata) -> std::array<"
       << info.type_str << ", " << deq_width << "> {\n";
    os << "  std::array<" << info.type_str << ", " << deq_width << "> __vul_queue_values = {};\n";
    for (uint32_t i = 0; i < deq_width; ++i) {
        os << "  if (__vul_queue_deqvalid > " << i << ") {\n";
        os << "    __vul_queue_values[" << i << "] = "
           << unpackValueExpr(info, "__vul_queue_deqdata[" + std::to_string(i) + "]") << ";\n";
        os << "  }\n";
    }
    os << "  return __vul_queue_values;\n";
    os << "};\n";
    return os.str();
}

string deqNextBlock(const QueueInfo &info, const vector<string> &args) {
    const bool is_mp = (info.queue->enq_width > 1 || info.queue->deq_width > 1);
    std::ostringstream os;
    os << "{\n";
    if (!is_mp) {
        os << "  " << info.deqready << " = true;\n";
    } else {
        const uint32_t deq_width = static_cast<uint32_t>(info.queue->deq_width);
        string num_expr = args.empty() ? std::to_string(deq_width) : args[0];
        os << "  uint32_t __vul_queue_req = (" << num_expr << ") < " << deq_width
           << " ? (" << num_expr << ") : " << deq_width << ";\n";
        os << "  " << info.deqready << " = __vul_queue_req;\n";
    }
    os << "}";
    return os.str();
}

bool parseMethodCall(
    const string &code,
    const vector<TokenInfo> &tokens,
    int object_idx,
    string &method,
    vector<string> &args,
    int &close_idx
) {
    if (object_idx + 2 >= static_cast<int>(tokens.size()) || tokens[object_idx + 1].spelling != ".") {
        return false;
    }
    int method_idx = object_idx + 2;
    method = tokens[method_idx].spelling;
    int open_idx = method_idx + 1;
    if (open_idx >= static_cast<int>(tokens.size()) || tokens[open_idx].spelling != "(") {
        return false;
    }
    close_idx = findMatching(tokens, open_idx, "(", ")");
    if (close_idx < 0) {
        return false;
    }
    args = splitTopLevelArgs(code, tokens, open_idx + 1, close_idx - 1);
    return true;
}

} // namespace

InlineCode inlineQueueAPIs(
    const VulStaticModuleInstance &module,
    const VulStaticBundleLib &bundlelib,
    const vector<string> &logic_hls_codes,
    const VulDebugLocs &logic_hls_debug
) {
    unordered_map<string, QueueInfo> queues;
    for (const auto &queue : module.queues) {
        QueueInfo info;
        info.queue = &queue;
        info.type_str = queue.type.toString();
        flatten_type_signature(queue.type, bundlelib, "value", info.data_width, info.fields);
        info.default_expr = defaultValueExprForType(queue.type, bundlelib);
        info.helper_name = "__vul_queue_unpack_" + queue.name;
        info.front_helper_name = "__vul_queue_front_" + queue.name;
        info.enqready = queue.name + "__enqready__";
        info.deqready = queue.name + "__deqready__";
        info.enqvalid = queue.name + "__enqvalid__";
        info.deqvalid = queue.name + "__deqvalid__";
        info.enqdata = queue.name + "__enqdata__";
        info.deqdata = queue.name + "__deqdata__";
        info.clrnext = queue.name + "__clrnext__";
        queues[queue.name] = std::move(info);
    }
    if (queues.empty()) {
        return {logic_hls_codes, logic_hls_debug};
    }

    string code = joinLines(logic_hls_codes);
    vector<TokenInfo> tokens = tokenizeWithLibclang(code);
    vector<Replacement> repls;
    string helper_defs;
    for (const auto &[name, info] : queues) {
        helper_defs += unpackValueHelper(info);
        helper_defs += "\n";
        string front_helper = frontHelper(info);
        if (!front_helper.empty()) {
            helper_defs += front_helper;
            helper_defs += "\n";
        }
    }
    size_t logic_func_pos = findLogicSubmoduleFunctionStart(code);
    size_t body_pos = findLogicSubmoduleBodyInsertion(code);
    if (body_pos != string::npos && !helper_defs.empty()) {
        repls.push_back(Replacement{
            static_cast<uint32_t>(body_pos),
            static_cast<uint32_t>(body_pos),
            helper_defs
        });
    }
    for (int i = 0; i < static_cast<int>(tokens.size()); ++i) {
        if (logic_func_pos != string::npos && tokens[i].start < logic_func_pos) {
            continue;
        }
        auto it = queues.find(tokens[i].spelling);
        if (it == queues.end() || tokens[i].kind != CXToken_Identifier) {
            continue;
        }
        const QueueInfo &info = it->second;
        string method;
        vector<string> args;
        int close_idx = -1;
        if (!parseMethodCall(code, tokens, i, method, args, close_idx)) {
            continue;
        }
        string text;
        if (method == "enqready" || method == "enqreqdy") {
            text = info.enqready;
        } else if (method == "deqvalid") {
            text = info.deqvalid;
        } else if (method == "front") {
            text = frontExpr(info, args);
        } else if (method == "enqnext") {
            text = enqNextBlock(info, args);
        } else if (method == "deqnext") {
            text = deqNextBlock(info, args);
        } else if (method == "clrnext") {
            text = "{\n  " + info.clrnext + " = true;\n}";
        } else {
            continue;
        }
        if (!overlapsExisting(repls, tokens[i].start, tokens[close_idx].end)) {
            repls.push_back(Replacement{tokens[i].start, tokens[close_idx].end, text});
        }
    }

    return applyReplacementsWithDebug(logic_hls_codes, logic_hls_debug, std::move(repls));
}

vector<string> inlineQueueAPIs(
    const VulStaticModuleInstance &module,
    const VulStaticBundleLib &bundlelib,
    const vector<string> &logic_hls_codes
) {
    return inlineQueueAPIs(module, bundlelib, logic_hls_codes, {}).lines;
}

} // namespace apiinline
