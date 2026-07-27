// Copyright (c) 2026 Meng Chengzhen, in Shandong University
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

//
// Copyright (c) 2026 Meng Chengzhen, in Shandong University
//
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

