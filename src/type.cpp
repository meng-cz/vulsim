/*
 * Copyright (c) 2025 Meng-CZ
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

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
        "uint8", "uint16", "uint32", "uint64", "uint128", "int8", "int16", "int32", "int64", "int128"
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

/**
 * @brief Extract referenced config item names from a VulConfig's value string.
 * This function parses the value string to identify identifiers.
 * @param vc The VulConfig object to extract references for. The references field will be populated.
 */
void extractConfigReferences(VulConfig &vc) {
    // Simple lexer: split into identifiers, numbers, operators, and parentheses
    const string &value = vc.value;
    vc.references.clear();
    size_t i = 0;
    while (i < value.size()) {
        char c = value[i];
        if (std::isspace((unsigned char)c)) {  i++; continue; }
        if (std::isalpha((unsigned char)c) || c == '_') {
            // identifier
            size_t j = i + 1;
            while (j < value.size() && (std::isalnum((unsigned char)value[j]) || value[j] == '_')) j++;
            string ident = value.substr(i, j - i);
            // get ident
            vc.references.insert(ident);
            i = j;
            continue;
        }
        if (std::isdigit((unsigned char)c)) {
            // number literal (integer)
            size_t j = i + 1;
            while (j < value.size() && (
                std::isdigit((unsigned char)value[j]) ||
                value[j] == 'x' || value[j] == 'X' ||
                value[j] == 'u' || value[j] == 'U' ||
                value[j] == 'l' || value[j] == 'L'
            )) j++; // allow hex (0x), unsigned (u), long (l) suffixes
            i = j;
            continue;
        }
        // operators or punctuation: copy as-is (handles + - * / % ( ) etc.)
        i++;
    }
}

/**
 * @brief Create a fake VulCombine from a VulPrefab for design-time representation.
 * @param prefab The VulPrefab to convert.
 * @param out_combine The output VulCombine object to populate.
 */
void fakeCombineFromPrefab(const VulPrefab &prefab, VulCombine &out_combine) {
    out_combine.name = prefab.name;
    out_combine.comment = prefab.comment;
    out_combine.pipein = prefab.pipein;
    out_combine.pipeout = prefab.pipeout;
    out_combine.request = prefab.request;
    out_combine.service = prefab.service;
    out_combine.storage.clear();
    out_combine.storagenext.clear();
    out_combine.storagetick.clear();
    out_combine.tick = VulCppFunc();
    out_combine.applytick = VulCppFunc();
    out_combine.init = VulCppFunc();
    out_combine.config = prefab.config;
    out_combine.stallable = prefab.stallable;
}

/**
 * @brief Detect the type of a VulPipe based on its properties.
 * @param vp The VulPipe to analyze.
 * @return The detected VulPipeType.
 */
VulPipeType detectVulPipeType(const VulPipe &vp) {
    if (vp.inputsize == 1 && vp.outputsize == 1 && vp.buffersize == 0) {
        if (vp.handshake) {
            return VulPipeType::simple_handshake;
        } else {
            return VulPipeType::simple_nonhandshake;
        }
    } else if (vp.inputsize == 1 && vp.outputsize == 1 && vp.handshake) {
        return VulPipeType::simple_buffered;
    } else {
        return VulPipeType::invalid;
    }
}

