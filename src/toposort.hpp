// MIT License

// Copyright (c) 2025 Meng Chengzhen, in Shandong University

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

#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <queue>

using std::string;
using std::vector;
using std::unordered_set;
using std::unordered_map;
using std::unique_ptr;

inline unique_ptr<vector<string>> topologicalSort(
    const unordered_set<string> &all_items,
    const unordered_map<string, unordered_set<string>> &edges_former_to_latter,
    vector<string> &out_loop_nodes
) {
    // Kahn's algorithm
    auto out = std::make_unique<vector<string>>();

    // initialize indegree map
    unordered_map<string, int> indeg;
    indeg.reserve(all_items.size());
    for (const auto &it : all_items) indeg[it] = 0;

    // adjacency list (only consider items in all_items)
    unordered_map<string, vector<string>> adj;
    adj.reserve(edges_former_to_latter.size());

    for (const auto &kv : edges_former_to_latter) {
        const string &u = kv.first;
        if (all_items.find(u) == all_items.end()) continue;
        for (const auto &v : kv.second) {
            if (all_items.find(v) == all_items.end()) continue;
            adj[u].push_back(v);
            indeg[v] += 1;
        }
    }

    std::queue<string> q;
    for (const auto &p : indeg) if (p.second == 0) q.push(p.first);

    while (!q.empty()) {
        string u = q.front(); q.pop();
        out->push_back(u);
        auto it = adj.find(u);
        if (it == adj.end()) continue;
        for (const auto &v : it->second) {
            auto itdeg = indeg.find(v);
            if (itdeg == indeg.end()) continue;
            itdeg->second -= 1;
            if (itdeg->second == 0) q.push(v);
        }
    }

    if (out->size() != all_items.size()) {
        for (const auto &p : indeg) {
            if (p.second > 0) out_loop_nodes.push_back(p.first);
        }
        return nullptr;
    }
    return out;
}



