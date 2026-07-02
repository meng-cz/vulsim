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
    string enqready;
    string deqready;
    string enqvalid;
    string deqvalid;
    string enqdata;
    string deqdata;
    string clrnext;
};

string unpackValueExpr(const QueueInfo &info, const string &packed_expr) {
    std::ostringstream os;
    os << "([&]() -> " << info.type_str << " {\n";
    os << "  " << info.type_str << " value;\n";
    for (const auto &field : info.fields) {
        os << "  " << field.name << " = "
           << uintExtractExpr(packed_expr, field.offset + field.width - 1, field.offset)
           << ";\n";
    }
    os << "  return value;\n";
    os << "}())";
    return os.str();
}

void emitPackValue(std::ostringstream &os, const QueueInfo &info, const string &packed_name, const string &value_name) {
    os << "  Int<" << info.data_width << "> " << packed_name << ";\n";
    for (const auto &field : info.fields) {
        os << "  " << uintExtractExpr(packed_name, field.offset + field.width - 1, field.offset)
           << " = " << flatFieldValueExpr(value_name, field.name) << ";\n";
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
            os << "    Int<" << info.data_width << "> __vul_queue_packed;\n";
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
    std::ostringstream os;
    os << "([&]() -> std::array<" << info.type_str << ", " << deq_width << "> {\n";
    os << "  std::array<" << info.type_str << ", " << deq_width << "> __vul_queue_values;\n";
    for (uint32_t i = 0; i < deq_width; ++i) {
        os << "  if (" << info.deqvalid << " > " << i << ") {\n";
        os << "    auto &value = __vul_queue_values[" << i << "];\n";
        for (const auto &field : info.fields) {
            os << "    " << field.name << " = "
               << uintExtractExpr(info.deqdata + "[" + std::to_string(i) + "]", field.offset + field.width - 1, field.offset)
               << ";\n";
        }
        os << "  }\n";
    }
    os << "  return __vul_queue_values;\n";
    os << "}())";
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

vector<string> inlineQueueAPIs(
    const VulStaticModuleInstance &module,
    const VulStaticBundleLib &bundlelib,
    const vector<string> &logic_hls_codes
) {
    unordered_map<string, QueueInfo> queues;
    for (const auto &queue : module.queues) {
        QueueInfo info;
        info.queue = &queue;
        info.type_str = queue.type.toString();
        flatten_type_signature(queue.type, bundlelib, "value", info.data_width, info.fields);
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
        return logic_hls_codes;
    }

    string code = joinLines(logic_hls_codes);
    vector<TokenInfo> tokens = tokenizeWithLibclang(code);
    vector<Replacement> repls;
    for (int i = 0; i < static_cast<int>(tokens.size()); ++i) {
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

    return splitLinesKeepEnds(applyReplacements(std::move(code), std::move(repls)));
}

} // namespace apiinline
