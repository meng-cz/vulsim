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
#include <map>
#include <unordered_set>

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

    matcher.uses_double_colon = matcher_str.find("::") != string::npos;

    // As documented in UserGuide 8-debugging:
    // split by the first '.'; the left side is instance path using "::",
    // and the right side is signal path using '.'.
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

vector<string> splitPathSegments(const string &path, const string &sep) {
    return (sep == "::" ? stringop::split(path, sep) : stringop::split(path, sep[0]));
}

string firstSignalSegmentBase(const string &signal_path) {
    if (signal_path.empty()) return "";
    string first_seg = stringop::split(signal_path, '.')[0];
    return parseIndexedSegment(first_seg).base;
}

vector<ConfigRealValue> currentInstanceArrayDims(const shared_ptr<VulStaticModuleInstance> &instance_ptr) {
    if (!instance_ptr->parent) return {};
    auto it = instance_ptr->parent->instances.find(instance_ptr->instance_path.back());
    if (it == instance_ptr->parent->instances.end()) return {};
    return it->second.array_dims;
}

vector<std::optional<ConfigRealValue>> extractInstanceIndexFilter(
    const string &matcher_instance_path,
    const string &sep,
    const shared_ptr<VulStaticModuleInstance> &instance_ptr
) {
    const auto dims = currentInstanceArrayDims(instance_ptr);
    if (dims.empty()) {
        return {};
    }
    auto matcher_segments = splitPathSegments(matcher_instance_path, sep);
    if (matcher_segments.empty()) {
        return {};
    }
    IndexedSegment leaf = parseIndexedSegment(matcher_segments.back());
    if (leaf.base == "*") {
        return {};
    }
    if (leaf.base != instance_ptr->instance_path.back()) {
        return {};
    }
    if (leaf.indices.empty()) {
        return {};
    }
    if (leaf.indices.size() != dims.size()) {
        throw VulException(
            "Trace matcher '" + matcher_instance_path +
            "' specifies " + std::to_string(leaf.indices.size()) +
            " indices, but instance '" + instance_ptr->concatInstancePath(".", false) +
            "' has " + std::to_string(dims.size()) + " dimensions"
        );
    }
    vector<std::optional<ConfigRealValue>> out;
    out.reserve(dims.size());
    for (size_t i = 0; i < dims.size(); ++i) {
        const string idx = stringop::trim(leaf.indices[i]);
        if (idx == "*") {
            out.push_back(std::nullopt);
            continue;
        }
        char *end = nullptr;
        errno = 0;
        long long parsed = std::strtoll(idx.c_str(), &end, 0);
        if (errno != 0 || end == nullptr || *end != '\0') {
            throw VulException("Invalid trace matcher index '" + idx + "' in '" + matcher_instance_path + "'");
        }
        if (parsed < 0 || parsed >= dims[i]) {
            throw VulException(
                "Trace matcher index '" + idx + "' out of range for dimension " + std::to_string(i) +
                " of instance '" + instance_ptr->concatInstancePath(".", false) + "'"
            );
        }
        out.push_back(static_cast<ConfigRealValue>(parsed));
    }
    return out;
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

        struct AppliedSignalMatcher {
            SignalPath signal_path_matcher;
            bool trace_all_instances = true;
            vector<std::optional<ConfigRealValue>> instance_index_filter;
        };
        vector<AppliedSignalMatcher> applied_signal_path_matchers;
        for (const auto &matcher : trace_matchers) {
            string instance_path_colon = instance_ptr->concatInstancePath("::", false);
            string instance_path_dot = instance_ptr->concatInstancePath(".", false);
            bool matched_instance = false;
            vector<std::optional<ConfigRealValue>> index_filter;
            if (matcher.instance_path_matcher.find("::") != string::npos) {
                matched_instance = matchPathSegments(matcher.instance_path_matcher, instance_path_colon, "::");
                if (matched_instance) {
                    index_filter = extractInstanceIndexFilter(matcher.instance_path_matcher, "::", instance_ptr);
                }
            } else {
                matched_instance = matchPathSegments(matcher.instance_path_matcher, instance_path_dot, ".");
                if (matched_instance) {
                    index_filter = extractInstanceIndexFilter(matcher.instance_path_matcher, ".", instance_ptr);
                }
            }
            if (matched_instance) {
                applied_signal_path_matchers.push_back({matcher.signal_path_matcher, index_filter.empty(), std::move(index_filter)});
            }
        }

        vector<VulTracedSignal> all_signals;
        for (const auto &reg : instance_ptr->registers) {
            vector<FlatField> flat_fields;
            uint32_t offset = 0;
            flatten_type_signature(reg.signature, local_bundlelib, reg.name, offset, flat_fields);
            for (const auto &f : flat_fields) {
                VulTracedSignal signal;
                signal.signal_path = f.name;
                signal.bit_width = f.width;
                signal.is_fixint = f.is_fixint;
                all_signals.push_back(std::move(signal));
            }
        }

        std::unordered_set<string> child_instance_names;
        for (const auto &child : instance_ptr->children) {
            child_instance_names.insert(child->instance_path.back());
        }

        for (const auto &matcher : trace_matchers) {
            if (matcher.uses_double_colon || matcher.signal_path_matcher.empty() || matcher.signal_path_matcher == "*") {
                continue;
            }
            string instance_path = instance_ptr->concatInstancePath("::", false);
            if (!matchPathSegments(matcher.instance_path_matcher, instance_path, "::")) {
                continue;
            }
            string first_base = firstSignalSegmentBase(matcher.signal_path_matcher);
            if (first_base.empty()) {
                continue;
            }
            if (child_instance_names.count(first_base)) {
                throw VulException(
                    "Invalid trace matcher '" +
                    matcher.instance_path_matcher + "." + matcher.signal_path_matcher +
                    "': instance paths must use '::', for example '" +
                    instance_path + "::" + matcher.signal_path_matcher + "'"
                );
            }
        }

        std::map<SignalPath, VulTracedSignal> traced_signal_map;
        for (const auto &signal : all_signals) {
            for (const auto &applied : applied_signal_path_matchers) {
                if (!matchPathSegments(applied.signal_path_matcher, signal.signal_path, ".")) {
                    continue;
                }
                auto &dst = traced_signal_map[signal.signal_path];
                dst.signal_path = signal.signal_path;
                dst.bit_width = signal.bit_width;
                dst.is_fixint = signal.is_fixint;
                if (applied.trace_all_instances) {
                    dst.trace_all_instances = true;
                    dst.instance_index_filters.clear();
                } else if (dst.trace_all_instances && dst.instance_index_filters.empty()) {
                    dst.trace_all_instances = false;
                    dst.instance_index_filters.push_back(applied.instance_index_filter);
                } else {
                    dst.instance_index_filters.push_back(applied.instance_index_filter);
                }
            }
        }

        vector<VulTracedSignal> &traced_signals = trace_table[instance_ptr->instance_id];
        for (auto &[_, sig] : traced_signal_map) {
            traced_signals.push_back(std::move(sig));
        }

    }

    return trace_table;
}
