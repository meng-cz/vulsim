// MIT License
//
// Copyright (c) 2026 Meng Chengzhen, in Shandong University
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <cctype>
#include <string>
#include <vector>

namespace stringop {

inline void ltrim_inplace(std::string &s) {
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
    s.erase(0, i);
}

inline void rtrim_inplace(std::string &s) {
    size_t end = s.find_last_not_of(" \t\r\n");
    if (end == std::string::npos) {
        s.clear();
        return;
    }
    s.resize(end + 1);
}

inline void trim_inplace(std::string &s) {
    ltrim_inplace(s);
    rtrim_inplace(s);
}

inline std::string ltrim(std::string s) {
    ltrim_inplace(s);
    return s;
}

inline std::string rtrim(std::string s) {
    rtrim_inplace(s);
    return s;
}

inline std::string trim(std::string s) {
    trim_inplace(s);
    return s;
}

inline size_t skip_spaces(const std::string &s, size_t pos) {
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) {
        ++pos;
    }
    return pos;
}

inline std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> out;
    std::string current;
    for (char c : s) {
        if (c == delim) {
            out.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    out.push_back(current);
    return out;
}

inline std::vector<std::string> split(const std::string &s, const std::string &delim) {
    std::vector<std::string> out;
    if (delim.empty()) {
        out.push_back(s);
        return out;
    }
    size_t start = 0;
    while (start <= s.size()) {
        size_t pos = s.find(delim, start);
        if (pos == std::string::npos) {
            out.push_back(s.substr(start));
            break;
        }
        out.push_back(s.substr(start, pos - start));
        start = pos + delim.size();
    }
    return out;
}

} // namespace stringop

