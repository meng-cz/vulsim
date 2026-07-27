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

#include <array>
#include <vector>
#include <string>
#include "type.h"

namespace cppparse {

struct TrimResult {
    std::vector<std::string> lines;   // 去注释后的代码
    std::vector<uint32_t> mapping;    // 新行号 -> 原始行号（1-based）
};

std::vector<std::string> readFileLines(const std::string& filename);

TrimResult stripComments(const std::vector<std::string>& input);

struct LinePosition {
    int32_t line;   // 0-based line number
    int32_t column; // 0-based column number
};

LinePosition findNext(
    const std::vector<std::string>& code,
    LinePosition start,
    const std::string& target
);

struct MatchMacroResult {
    LinePosition pos;
    std::vector<std::string> args;
};

std::vector<MatchMacroResult> matchMacros(const std::vector<std::string>& code, const std::string& pattern);

struct BlockResult {
    LinePosition end_pos;
    std::string content;
};

BlockResult findNextBraceBlock(const std::vector<std::string>& code, LinePosition start, const char begin, const char end, bool keep_nextline = false);

bool codeblockContainsFunctionCall(const std::vector<std::string>& code, const std::string& func_name);

struct MacroEntry {
    LinePosition pos;
    std::string name;
    std::vector<std::string> args;
    std::vector<std::string> body;
    std::vector<LinePosition> body_pos;
};

std::vector<MacroEntry> findAllMacroEntries(const std::vector<std::string>& code);

} // namespace cppparse
