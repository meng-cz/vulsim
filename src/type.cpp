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
    if (name.empty()) return false;
    // first char: letter or underscore
    unsigned char c0 = (unsigned char)name[0];
    if (!(std::isalpha(c0) || name[0] == '_')) return false;
    for (size_t i = 1; i < name.size(); ++i) {
        unsigned char c = (unsigned char)name[i];
        if (!(std::isalnum(c) || name[i] == '_')) return false;
    }
    return true;
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
