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

vector<string> splitByChar(const string &s, char delim) {
    vector<string> out;
    size_t start = 0;
    while (start <= s.size()) {
        size_t pos = s.find(delim, start);
        if (pos == string::npos) {
            out.push_back(s.substr(start));
            break;
        }
        out.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return out;
}

vector<string> splitByToken(const string &s, const string &token) {
    vector<string> out;
    size_t start = 0;
    while (start <= s.size()) {
        size_t pos = s.find(token, start);
        if (pos == string::npos) {
            out.push_back(s.substr(start));
            break;
        }
        out.push_back(s.substr(start, pos - start));
        start = pos + token.size();
    }
    return out;
}

bool matchPathSegments(const string &pattern, const string &value, const string &sep) {
    if (pattern == "*") return true;
    vector<string> p = (sep == "::" ? splitByToken(pattern, sep) : splitByChar(pattern, sep[0]));
    vector<string> v = (sep == "::" ? splitByToken(value, sep) : splitByChar(value, sep[0]));
    if (p.size() != v.size()) return false;
    for (size_t i = 0; i < p.size(); ++i) {
        if (p[i] == "*") continue;
        if (p[i] != v[i]) return false;
    }
    return true;
}


vector<VulTracedSignal> collectModuleSignals(
    const shared_ptr<VulModule> &module_ptr,
    const BundleTable &bundlelib
) {
    vector<VulTracedSignal> out;
    auto appendStorageMap = [&](const unordered_map<StorageName, VulStorage> &storage_map) {
        for (const auto &entry : storage_map) {
            const string &signal_name = entry.first;
            VulBundleMember signal_member = entry.second;
            signal_member.name = signal_name;
            vector<FlatField> flat_fields;
            uint32_t offset = 0;
            flatten_member(signal_member, bundlelib, signal_name, offset, flat_fields);
            for (const auto &f : flat_fields) {
                out.push_back(VulTracedSignal{f.name, f.width});
            }
        }
    };

    // appendStorageMap(module_ptr->storages);
    appendStorageMap(module_ptr->storagenexts);

    return out;
}

} // namespace

/*
请实现trace.cpp中的parseTraceOptions函数，功能如下：
- 输入：VulProject对象和一个VulTraceMatcher对象的列表。
- 输出：一个VulTracedModule对象的列表，表示根据输入的VulTraceMatcher列表解析得到的需要被追踪的模块和信号信息。
- 解析规则：
  - VulTraceMatcher中的instance_path_matcher和signal_path_matcher都支持通配符"*"，表示匹配任意实例路径或信号路径。
  - instance path 通过双冒号分隔层级，例如"a::b::c"表示模块a下的实例b下的实例c。
  - signal path 通过点号分隔层级，例如"sig1.sig2"表示模块的sig1信号下的子信号sig2。
  - VulProject对象中包含了模块库和实例信息，从top_module开始，递归解析实例路径，找到匹配的实例和信号，并根据VulTraceMatcher的规则筛选出需要追踪的信号。
  - 输出的VulTracedModule对象中，module_name表示被追踪的模块名称，traced_signals表示被追踪的信号路径列表，traced_signals_of_instance表示对于特定的实例名哪些信号被追踪。
- 后续用法：
  - 这个函数的输出将被用于生成仿真器的追踪代码，确保在仿真过程中能够正确地追踪到用户指定的信号。
  - 每个被追踪的module都会为所有可能被追踪的信号生成追踪代码，然后通过一个运行时的bitmap控制哪些信号追踪代码被运行，这个bitmap被按照输出的traced_signals_of_instance中的信息在目标模块构造函数中进行运行时设置。
*/
vector<VulTracedModule> parseTraceOptions(const VulProject &project, const vector<VulTraceMatcher> &trace_matchers) {
    struct InstanceNode {
        ModuleName module_name;
        InstancePath instance_path;
    };

    BundleTable bundle_table; // 解析过程中需要频繁查询bundle定义，提前构建一个bundle name -> bundle item的map以加速查询
    auto err = resolveBundleTable(*project.bundlelib, *project.configlib, {}, bundle_table);
    if (!err.empty()) {
        throw std::runtime_error("Failed to resolve bundle table: " + err.msg);
    }

    vector<InstanceNode> all_instances;
    std::deque<InstanceNode> q;
    q.push_back({project.top_module, "top"});

    while (!q.empty()) {
        InstanceNode node = q.front();
        q.pop_front();
        all_instances.push_back(node);

        auto mod_iter = project.modulelib->modules.find(node.module_name);
        if (mod_iter == project.modulelib->modules.end()) continue;
        shared_ptr<VulModule> mod_ptr = dynamic_pointer_cast<VulModule>(mod_iter->second);
        if (!mod_ptr) continue;

        for (const auto &inst_entry : mod_ptr->instances) {
            const string &inst_name = inst_entry.first;
            const VulInstance &inst = inst_entry.second;
            string child_path = node.instance_path.empty() ? inst_name : (node.instance_path + "::" + inst_name);
            q.push_back({inst.module_name, child_path});
        }
    }

    unordered_map<ModuleName, vector<VulTracedSignal>> module_signals;
    for (const auto &inst : all_instances) {
        if (module_signals.find(inst.module_name) != module_signals.end()) continue;
        auto mod_iter = project.modulelib->modules.find(inst.module_name);
        if (mod_iter == project.modulelib->modules.end()) continue;
        shared_ptr<VulModule> mod_ptr = dynamic_pointer_cast<VulModule>(mod_iter->second);
        if (!mod_ptr) continue;
        module_signals[inst.module_name] = collectModuleSignals(mod_ptr, bundle_table);
    }

    vector<VulTracedModule> traced_modules;
    unordered_map<ModuleName, size_t> module_index;
    unordered_map<ModuleName, unordered_map<SignalPath, size_t>> signal_index;

    for (const auto &matcher : trace_matchers) {
        for (const auto &inst : all_instances) {
            if (!matchPathSegments(matcher.instance_path_matcher, inst.instance_path, "::")) continue;
            auto sigs_iter = module_signals.find(inst.module_name);
            if (sigs_iter == module_signals.end()) continue;
            const vector<VulTracedSignal> &all_signals = sigs_iter->second;

            size_t mod_idx = 0;
            auto mod_idx_iter = module_index.find(inst.module_name);
            if (mod_idx_iter == module_index.end()) {
                mod_idx = traced_modules.size();
                module_index[inst.module_name] = mod_idx;
                traced_modules.push_back(VulTracedModule{inst.module_name, {}, {}});
            } else {
                mod_idx = mod_idx_iter->second;
            }
            VulTracedModule &tm = traced_modules[mod_idx];
            vector<bool> &inst_bits = tm.traced_signals_of_instance[inst.instance_path];
            if (inst_bits.size() < tm.traced_signals.size()) {
                inst_bits.resize(tm.traced_signals.size(), false);
            }

            for (const auto &sig : all_signals) {
                if (!matchPathSegments(matcher.signal_path_matcher, sig.signal_path, ".")) continue;

                size_t sig_idx = 0;
                auto &mod_sig_idx = signal_index[inst.module_name];
                auto sig_idx_iter = mod_sig_idx.find(sig.signal_path);
                if (sig_idx_iter == mod_sig_idx.end()) {
                    sig_idx = tm.traced_signals.size();
                    tm.traced_signals.push_back(sig);
                    mod_sig_idx[sig.signal_path] = sig_idx;
                    for (auto &entry : tm.traced_signals_of_instance) {
                        if (entry.second.size() < tm.traced_signals.size()) {
                            entry.second.resize(tm.traced_signals.size(), false);
                        }
                    }
                } else {
                    sig_idx = sig_idx_iter->second;
                }
                inst_bits[sig_idx] = true;
            }
        }
    }

    return traced_modules;
}

