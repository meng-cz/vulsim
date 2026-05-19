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

#include <array>
#include <vector>
#include <string>
#include "type.h"

namespace cppparse {

inline void rtrim(std::string& s) {
    size_t end = s.find_last_not_of(" \t");
    if (end == std::string::npos) s.clear();
    else s.resize(end + 1);
}

inline std::string trim(const std::string& s) {
    size_t l = s.find_first_not_of(" \t");
    if (l == std::string::npos) return "";
    size_t r = s.find_last_not_of(" \t");
    return s.substr(l, r - l + 1);
}

inline size_t skip_spaces(const std::string& s, size_t pos) {
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) {
        ++pos;
    }
    return pos;
}


inline std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> result;
    std::string current;

    for (char c : s) {
        if (c == delim) {
            result.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }

    result.push_back(current); // 最后一段
    return result;
}

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

} // namespace cppparse
