#pragma once

#include "errormsg.hpp"
#include "project.h"
#include "trace.hpp"

#include <cctype>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

struct VulBreakConditionSpec {
    std::string user_signal_ref;
    std::string runtime_signal_name;
    std::string expected_bits;
    std::string expected_display;
};

struct VulBreakPointSpec {
    std::string expr_text;
    std::vector<VulBreakConditionSpec> conditions;
};

inline std::string breakpointTrimCopy(const std::string &s) {
    size_t l = 0;
    while (l < s.size() && std::isspace(static_cast<unsigned char>(s[l]))) ++l;
    size_t r = s.size();
    while (r > l && std::isspace(static_cast<unsigned char>(s[r - 1]))) --r;
    return s.substr(l, r - l);
}

inline std::vector<ConfigRealValue> breakpointChildArrayDims(const shared_ptr<VulStaticModuleInstance> &instance_ptr) {
    if (!instance_ptr->parent) return {};
    std::string key = instance_ptr->instance_decl_name.empty()
        ? (instance_ptr->instance_path.empty() ? std::string{} : instance_ptr->instance_path.back())
        : instance_ptr->instance_decl_name;
    if (key.empty()) return {};
    auto it = instance_ptr->parent->instances.find(key);
    if (it == instance_ptr->parent->instances.end()) return {};
    return it->second.array_dims;
}

template<typename Fn>
inline void breakpointForEachIndexTuple(const std::vector<ConfigRealValue> &dims, Fn fn) {
    if (dims.empty()) {
        fn(std::vector<ConfigRealValue>{});
        return;
    }
    std::vector<ConfigRealValue> cur(dims.size(), 0);
    while (true) {
        fn(cur);
        size_t d = dims.size();
        while (d > 0) {
            --d;
            ++cur[d];
            if (cur[d] < dims[d]) {
                break;
            }
            cur[d] = 0;
        }
        if (d == 0 && cur[0] == 0) {
            break;
        }
    }
}

inline std::string breakpointInstancePathToRuntimePath(const std::string &instance_path) {
    std::string out;
    for (size_t i = 0; i < instance_path.size(); ++i) {
        if (i + 1 < instance_path.size() && instance_path[i] == ':' && instance_path[i + 1] == ':') {
            out.push_back('.');
            ++i;
        } else {
            out.push_back(instance_path[i]);
        }
    }
    return out;
}

inline std::vector<std::string> breakpointSplitTopLevelAndClauses(const std::string &expr) {
    std::vector<std::string> parts;
    size_t start = 0;
    int depth = 0;
    for (size_t i = 0; i < expr.size(); ++i) {
        if (expr[i] == '(') ++depth;
        else if (expr[i] == ')') --depth;
        else if (depth == 0 && i + 1 < expr.size() && expr[i] == '&' && expr[i + 1] == '&') {
            parts.push_back(breakpointTrimCopy(expr.substr(start, i - start)));
            i += 1;
            start = i + 1;
        }
    }
    if (depth != 0) {
        throw VulException("Unbalanced parentheses in breakpoint expression: " + expr);
    }
    parts.push_back(breakpointTrimCopy(expr.substr(start)));
    return parts;
}

inline std::string breakpointStripOneLayerParens(const std::string &expr) {
    std::string s = breakpointTrimCopy(expr);
    if (s.size() < 2 || s.front() != '(' || s.back() != ')') {
        return s;
    }
    int depth = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '(') ++depth;
        else if (s[i] == ')') --depth;
        if (depth == 0 && i + 1 < s.size()) {
            return s;
        }
    }
    return breakpointTrimCopy(s.substr(1, s.size() - 2));
}

inline std::string breakpointDecimalToBits(std::string digits, uint32_t width) {
    digits = breakpointTrimCopy(digits);
    if (digits.empty()) {
        throw VulException("Empty decimal literal in breakpoint");
    }
    for (char ch : digits) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            throw VulException("Invalid decimal literal in breakpoint: " + digits);
        }
    }
    if (digits == "0") {
        return std::string(width, '0');
    }
    std::string bits_rev;
    while (!(digits.size() == 1 && digits[0] == '0')) {
        int carry = 0;
        std::string next;
        next.reserve(digits.size());
        for (char ch : digits) {
            int cur = carry * 10 + (ch - '0');
            int q = cur / 2;
            carry = cur % 2;
            if (!next.empty() || q != 0) {
                next.push_back(static_cast<char>('0' + q));
            }
        }
        bits_rev.push_back(static_cast<char>('0' + carry));
        digits = next.empty() ? "0" : next;
        if (bits_rev.size() > width) {
            throw VulException("Breakpoint literal exceeds traced signal width");
        }
    }
    std::string bits(width, '0');
    for (size_t i = 0; i < bits_rev.size(); ++i) {
        bits[width - 1 - i] = bits_rev[i];
    }
    return bits;
}

inline std::string breakpointParseLiteralToBits(const std::string &literal, uint32_t width) {
    const std::string value = breakpointTrimCopy(literal);
    if (value.empty()) {
        throw VulException("Empty breakpoint literal");
    }
    if (value[0] == '-') {
        throw VulException("Negative breakpoint literals are not supported: " + value);
    }
    if (value.starts_with("0b") || value.starts_with("0B")) {
        const std::string digits = value.substr(2);
        if (digits.empty()) throw VulException("Invalid binary literal in breakpoint: " + value);
        for (char ch : digits) {
            if (ch != '0' && ch != '1') {
                throw VulException("Invalid binary literal in breakpoint: " + value);
            }
        }
        if (digits.size() > width) {
            throw VulException("Breakpoint literal exceeds traced signal width");
        }
        return std::string(width - digits.size(), '0') + digits;
    }
    if (value.starts_with("0x") || value.starts_with("0X")) {
        const std::string digits = value.substr(2);
        if (digits.empty()) throw VulException("Invalid hexadecimal literal in breakpoint: " + value);
        std::string bits;
        bits.reserve(digits.size() * 4);
        for (char ch : digits) {
            uint32_t v = 0;
            if (ch >= '0' && ch <= '9') v = static_cast<uint32_t>(ch - '0');
            else if (ch >= 'a' && ch <= 'f') v = 10U + static_cast<uint32_t>(ch - 'a');
            else if (ch >= 'A' && ch <= 'F') v = 10U + static_cast<uint32_t>(ch - 'A');
            else throw VulException("Invalid hexadecimal literal in breakpoint: " + value);
            for (int bit = 3; bit >= 0; --bit) {
                bits.push_back(((v >> bit) & 1U) ? '1' : '0');
            }
        }
        size_t first_one = bits.find('1');
        std::string normalized = (first_one == std::string::npos) ? "0" : bits.substr(first_one);
        if (normalized.size() > width) {
            throw VulException("Breakpoint literal exceeds traced signal width");
        }
        return std::string(width - normalized.size(), '0') + normalized;
    }
    return breakpointDecimalToBits(value, width);
}

inline std::map<std::string, uint32_t> collectTracedSignalWidths(
    const VulStaticProject &project,
    const VulTraceTable &trace_table
) {
    std::map<std::string, uint32_t> out;
    std::deque<shared_ptr<VulStaticModuleInstance>> bfs_queue;
    bfs_queue.push_back(project.top_module_instance);
    while (!bfs_queue.empty()) {
        auto instance_ptr = bfs_queue.front();
        bfs_queue.pop_front();
        for (const auto &child : instance_ptr->children) {
            bfs_queue.push_back(child);
        }

        const auto dims = breakpointChildArrayDims(instance_ptr);
        const bool is_array_template = !dims.empty() && instance_ptr->instance_array_indices.empty();
        const auto it = trace_table.find(instance_ptr->instance_id);
        if (it == trace_table.end()) {
            continue;
        }
        for (const auto &sig : it->second) {
            if (!is_array_template) {
                out[instance_ptr->concatInstancePath(".") + "." + sig.signal_path] = sig.bit_width;
                continue;
            }
            std::set<std::vector<ConfigRealValue>> selected_indices;
            if (sig.trace_all_instances) {
                breakpointForEachIndexTuple(dims, [&](const std::vector<ConfigRealValue> &indices) {
                    selected_indices.insert(indices);
                });
            } else {
                for (const auto &filter : sig.instance_index_filters) {
                    breakpointForEachIndexTuple(dims, [&](const std::vector<ConfigRealValue> &indices) {
                        bool ok = true;
                        for (size_t dim = 0; dim < filter.size(); ++dim) {
                            if (filter[dim].has_value() && indices[dim] != *filter[dim]) {
                                ok = false;
                                break;
                            }
                        }
                        if (ok) {
                            selected_indices.insert(indices);
                        }
                    });
                }
            }
            for (const auto &indices : selected_indices) {
                std::string path = instance_ptr->concatInstancePath(".");
                for (auto idx : indices) {
                    path += "[" + std::to_string(idx) + "]";
                }
                path += "." + sig.signal_path;
                out[path] = sig.bit_width;
            }
        }
    }
    return out;
}

inline VulBreakPointSpec parseBreakExpression(
    const std::string &expr,
    const std::map<std::string, uint32_t> &traced_signal_widths
) {
    VulBreakPointSpec spec;
    spec.expr_text = breakpointTrimCopy(expr);
    if (spec.expr_text.empty()) {
        throw VulException("Empty breakpoint expression");
    }
    for (const std::string &raw_clause : breakpointSplitTopLevelAndClauses(spec.expr_text)) {
        const std::string clause = breakpointStripOneLayerParens(raw_clause);
        const size_t eq_pos = clause.find("==");
        if (eq_pos == std::string::npos || clause.find("==", eq_pos + 2) != std::string::npos) {
            throw VulException("Invalid breakpoint clause: " + clause);
        }
        const std::string signal_ref = breakpointTrimCopy(clause.substr(0, eq_pos));
        const std::string value_ref = breakpointTrimCopy(clause.substr(eq_pos + 2));
        if (signal_ref.empty() || value_ref.empty()) {
            throw VulException("Invalid breakpoint clause: " + clause);
        }
        if (signal_ref.find('*') != std::string::npos) {
            throw VulException("Breakpoint signal must be an exact traced signal path, wildcard is not allowed: " + signal_ref);
        }
        VulTraceMatcher matcher = parseTraceMatcher(signal_ref);
        if (!matcher.uses_double_colon) {
            throw VulException("Breakpoint signal instance path must use '::': " + signal_ref);
        }
        if (matcher.signal_path_matcher.empty() || matcher.signal_path_matcher == "*") {
            throw VulException("Breakpoint signal must name one exact traced signal: " + signal_ref);
        }
        const std::string runtime_signal_name =
            breakpointInstancePathToRuntimePath(matcher.instance_path_matcher) + "." + matcher.signal_path_matcher;
        const auto width_it = traced_signal_widths.find(runtime_signal_name);
        if (width_it == traced_signal_widths.end()) {
            throw VulException(
                "Breakpoint signal '" + signal_ref +
                "' is not a traced signal. Add a matching --trace rule first."
            );
        }
        spec.conditions.push_back(VulBreakConditionSpec{
            signal_ref,
            runtime_signal_name,
            breakpointParseLiteralToBits(value_ref, width_it->second),
            value_ref
        });
    }
    return spec;
}

inline std::vector<VulBreakPointSpec> parseBreakSpecs(
    const std::string &break_file,
    const std::string &break_line,
    const std::map<std::string, uint32_t> &traced_signal_widths
) {
    std::vector<VulBreakPointSpec> specs;
    if (!break_file.empty()) {
        std::filesystem::path break_path(break_file);
        if (!std::filesystem::exists(break_path) || !std::filesystem::is_regular_file(break_path)) {
            throw VulException("Breakpoint description file does not exist: " + break_file);
        }
        std::ifstream break_file_stream(break_path.string());
        if (!break_file_stream.is_open()) {
            throw VulException("Failed to open breakpoint description file: " + break_file);
        }
        std::string line;
        while (std::getline(break_file_stream, line)) {
            uint64_t commentpos = 0;
            if ((commentpos = line.find('#')) != string::npos) {
                line = line.substr(0, commentpos);
            }
            if ((commentpos = line.find("//")) != string::npos) {
                line = line.substr(0, commentpos);
            }
            line = breakpointTrimCopy(line);
            if (line.empty()) continue;
            specs.push_back(parseBreakExpression(line, traced_signal_widths));
        }
    }
    if (!break_line.empty()) {
        specs.push_back(parseBreakExpression(break_line, traced_signal_widths));
    }
    return specs;
}
