// MIT License

// Copyright (c) 2026 Meng Chengzhen, in Shandong University

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

#include "trace.hpp"
#include "stringop.hpp"

#include <deque>

VulTraceMatcher parseTraceMatcher(const string &matcher_str) {
    VulTraceMatcher matcher;

    if (matcher_str.empty()) {
        matcher.instance_path_matcher = "*";
        matcher.signal_path_matcher = "*";
        return matcher;
    }

    // "*" is a shorthand of "*.*" (trace all signals of all instances).
    if (matcher_str == "*") {
        matcher.instance_path_matcher = "*";
        matcher.signal_path_matcher = "*";
        return matcher;
    }

    // Split by the first '.' so nested instance path like "a.b.c.sig" works.
    size_t dot_pos = matcher_str.find('.');
    if (dot_pos == string::npos) {
        // No explicit signal matcher: treat as tracing all signals in this instance.
        matcher.instance_path_matcher = matcher_str;
        matcher.signal_path_matcher = "*";
        return matcher;
    }

    matcher.instance_path_matcher = matcher_str.substr(0, dot_pos);
    matcher.signal_path_matcher = matcher_str.substr(dot_pos + 1);

    if (matcher.instance_path_matcher.empty()) matcher.instance_path_matcher = "*";
    if (matcher.signal_path_matcher.empty()) matcher.signal_path_matcher = "*";

    return matcher;
}

namespace {

struct IndexedSegment {
    string base;
    vector<string> indices;
};

IndexedSegment parseIndexedSegment(const string &seg) {
    IndexedSegment out;
    size_t i = 0;
    while (i < seg.size() && seg[i] != '[') ++i;
    out.base = seg.substr(0, i);

    while (i < seg.size()) {
        if (seg[i] != '[') {
            break;
        }
        size_t close = seg.find(']', i + 1);
        if (close == string::npos) {
            break;
        }
        out.indices.push_back(seg.substr(i + 1, close - i - 1));
        i = close + 1;
    }
    return out;
}

bool matchSegmentWithIndexRule(const string &lhs, const string &rhs) {
    IndexedSegment a = parseIndexedSegment(lhs);
    IndexedSegment b = parseIndexedSegment(rhs);
    if (a.base != b.base) return false;

    // 若任一侧无索引，则仅比较名称。
    if (a.indices.empty() || b.indices.empty()) return true;

    // 若两侧均有索引，则比较共同前缀索引（允许一侧有更多维）。
    size_t common = std::min(a.indices.size(), b.indices.size());
    for (size_t i = 0; i < common; ++i) {
        if (a.indices[i] != b.indices[i]) return false;
    }
    return true;
}

bool matchPathSegments(const string &pattern, const string &value, const string &sep) {
    vector<string> p = (sep == "::" ? stringop::split(pattern, sep) : stringop::split(pattern, sep[0]));
    vector<string> v = (sep == "::" ? stringop::split(value, sep) : stringop::split(value, sep[0]));

    // dp[i][j]: pattern前i段是否可匹配value前j段
    // 规则：
    // - 普通分段：精确匹配1段
    // - "*"：匹配任意个(>=1)分段，不允许匹配0段
    vector<vector<uint8_t>> dp(p.size() + 1, vector<uint8_t>(v.size() + 1, 0));
    dp[0][0] = 1;

    for (size_t i = 0; i < p.size(); ++i) {
        for (size_t j = 0; j <= v.size(); ++j) {
            if (!dp[i][j]) continue;
            if (p[i] == "*") {
                for (size_t k = j + 1; k <= v.size(); ++k) {
                    dp[i + 1][k] = 1;
                }
            } else {
                if (j < v.size() && matchSegmentWithIndexRule(p[i], v[j])) {
                    dp[i + 1][j + 1] = 1;
                }
            }
        }
    }

    return dp[p.size()][v.size()] != 0;
}


} // namespace

VulTraceTable parseTraceOptions(const VulStaticProject &project, const vector<VulTraceMatcher> &trace_matchers) {

    VulErrorContextGuard context_guard("parsing trace options");

    VulTraceTable trace_table;

    std::deque<shared_ptr<VulStaticModuleInstance>> bfs_queue;
    bfs_queue.push_back(project.top_module_instance);

    while (!bfs_queue.empty()) {
        auto instance_ptr = bfs_queue.front();
        bfs_queue.pop_front();

        for (const auto &child : instance_ptr->children) {
            bfs_queue.push_back(child);
        }

        VulErrorContextGuard instance_context_guard("processing instance " + instance_ptr->simClassName());

        VulStaticBundleLib local_bundlelib = instance_ptr->local_bundles;
        local_bundlelib.insert(local_bundlelib.end(), project.global_bundlelib.begin(), project.global_bundlelib.end());

        vector<SignalPath> applied_signal_path_matchers;
        for (const auto &matcher : trace_matchers) {
            string instance_path = instance_ptr->concatInstancePath("::", true);
            if (matchPathSegments(matcher.instance_path_matcher, instance_path, "::")) {
                applied_signal_path_matchers.push_back(matcher.signal_path_matcher);
            }
        }

        vector<VulTracedSignal> all_signals;
        for (const auto &reg : instance_ptr->registers) {
            vector<FlatField> flat_fields;
            uint32_t offset = 0;
            flatten_member(reg.signature.toBundleMember(reg.name), local_bundlelib, reg.name, offset, flat_fields);
            for (const auto &f : flat_fields) {
                all_signals.push_back(VulTracedSignal{f.name, f.width});
            }
        }

        vector<VulTracedSignal> &traced_signals = trace_table[instance_ptr->instance_id];
        for (const auto &signal : all_signals) {
            bool matched = false;
            for (const auto &sig_matcher : applied_signal_path_matchers) {
                if (matchPathSegments(sig_matcher, signal.signal_path, ".")) {
                    matched = true;
                    break;
                }
            }
            if (matched) {
                traced_signals.push_back(signal);
            }
        }

    }

    return trace_table;
}

