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

#include "errormsg.hpp"
#include "module.h"

#include <fstream>

inline void vulDebugAppendLine(vector<string> &codes, VulDebugLocs &debug, const string &line, const VulDebugLoc &loc = {}) {
    while (debug.size() < codes.size()) {
        debug.push_back({});
    }
    codes.push_back(line);
    debug.push_back(loc);
}

inline void vulDebugAppendLines(vector<string> &codes, VulDebugLocs &debug, const vector<string> &lines, const VulDebugLocs &locs = {}) {
    while (debug.size() < codes.size()) {
        debug.push_back({});
    }
    codes.insert(codes.end(), lines.begin(), lines.end());
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i < locs.size()) {
            debug.push_back(locs[i]);
        } else {
            debug.push_back({});
        }
    }
}

inline VulDebugLines vulDebugBuildGeneratedMap(const VulDebugLocs &debug) {
    VulDebugLines out;
    out.reserve(debug.size());
    for (size_t i = 0; i < debug.size(); ++i) {
        if (debug[i].valid()) {
            out.push_back(VulDebugLine{static_cast<uint32_t>(i + 1), debug[i]});
        }
    }
    return out;
}

inline void vulDebugNormalize(vector<string> &codes, VulDebugLocs &debug) {
    while (debug.size() < codes.size()) {
        debug.push_back({});
    }
    if (debug.size() > codes.size()) {
        debug.resize(codes.size());
    }
}

inline void vulDebugWriteMapToFile(const VulDebugLines &debug_lines, const string &filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw VulException("Failed to open file for writing: " + filepath);
    }
    file << "# generated_line source_file source_line source_column\n";
    for (const auto &line : debug_lines) {
        file << line.generated_line << " " << line.source.file << " "
             << line.source.line << " " << line.source.column << "\n";
    }
}
