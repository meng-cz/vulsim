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


#include "type.h"

#include <algorithm>

/**
 * @brief Check if the given name is a valid identifier.
 * A valid identifier starts with a letter or underscore, followed by letters, digits, or underscores.
 * @param name The identifier name to check.
 * @return true if the name is a valid identifier, false otherwise.
 */
bool isValidIdentifier(const string &name) {
    static const std::unordered_set<string> cpp_keywords = {
        "alignas","alignof","and","and_eq","asm","atomic_cancel","atomic_commit","atomic_noexcept",
        "auto","bitand","bitor","bool","break","case","catch","char","char16_t","char32_t","class",
        "compl","concept","const","constexpr","const_cast","continue","co_return","co_await","co_yield",
        "decltype","default","delete","do","double","dynamic_cast","else","enum","explicit","export",
        "extern","false","float","for","friend","goto","if","inline","int","long","mutable","namespace",
        "new","noexcept","not","not_eq","nullptr","operator","or","or_eq","private","protected","public",
        "register","reinterpret_cast","return","short","signed","sizeof","static","static_assert","static_cast",
        "struct","switch","template","this","thread_local","throw","true","try","typedef","typeid","typename",
        "union","unsigned","using","virtual","void","volatile","wchar_t","while","xor","xor_eq",
        "uint8", "uint16", "uint32", "uint64", "uint128", "int8", "int16", "int32", "int64", "int128", "UInt"
    };
    if (name.empty()) return false;
    // first char: letter or underscore
    unsigned char c0 = (unsigned char)name[0];
    if (!(std::isalpha(c0) || name[0] == '_')) return false;
    for (size_t i = 1; i < name.size(); ++i) {
        unsigned char c = (unsigned char)name[i];
        if (!(std::isalnum(c) || name[i] == '_')) return false;
    }
    if (cpp_keywords.find(name) != cpp_keywords.end()) return false;
    // cannot start with "__"
    if (name.size() >= 2 && name[0] == '_' && name[1] == '_') return false;
    return true;
}

/**
 * @brief Replace all occurrences of an identifier in a string with a new identifier.
 * Only whole-word matches are replaced.
 * @param s The input string.
 * @param old_name The identifier to be replaced.
 * @param new_name The new identifier to replace with.
 * @return A new string with the replacements made.
 */
string identifierReplace(const string &s, const string &old_name, const string &new_name) {
    if (old_name.empty()) return s;
    string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (std::isalpha(c) || s[i] == '_') {
            size_t j = i + 1;
            while (j < s.size()) {
                unsigned char cj = static_cast<unsigned char>(s[j]);
                if (std::isalnum(cj) || s[j] == '_') ++j; else break;
            }
            string ident = s.substr(i, j - i);
            if (ident == old_name) out += new_name; else out += ident;
            i = j;
        } else {
            out.push_back(s[i]);
            ++i;
        }
    }
    return out;
}


/**
 * Check if the given type is a valid VulSim Basic Data Type.
 *  @param type The type string to check.
 *  @return true if the type is a valid basic type, false otherwise.
 */
bool isBasicVulType(const string &type) {
    // List of valid basic types
    // uint8, uint16, uint32, uint64, uint128
    // int8, int16, int32, int64, int128
    // bool

    static const vector<string> validTypes = {
        "uint8", "uint16", "uint32", "uint64", "uint128",
        "int8", "int16", "int32", "int64", "int128",
        "bool"
    };
    return std::find(validTypes.begin(), validTypes.end(), type) != validTypes.end();
}

