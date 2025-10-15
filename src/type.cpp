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
