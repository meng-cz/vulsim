// Copyright (c) 2025 Meng Chengzhen, in Shandong University
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <algorithm>
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

    std::vector<string> temp;
    temp.reserve(indeg.size());
    for (const auto &p : indeg) if (p.second == 0) temp.push_back(p.first);
    std::sort(temp.begin(), temp.end());
    std::queue<string> q;
    for (const auto &item : temp) q.push(item);

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



