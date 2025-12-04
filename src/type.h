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

#include <inttypes.h>

#include <string>
#include <vector>
#include <unordered_set>

using std::vector;
using std::pair;
using std::string;
using std::unordered_set;
using std::tuple;

static_assert(sizeof(uint8_t) == 1, "uint8_t size error");
static_assert(sizeof(uint16_t) == 2, "uint16_t size error");
static_assert(sizeof(uint32_t) == 4, "uint32_t size error");
static_assert(sizeof(uint64_t) == 8, "uint64_t size error");
static_assert(sizeof(int8_t) == 1, "int8_t size error");
static_assert(sizeof(int16_t) == 2, "int16_t size error");
static_assert(sizeof(int32_t) == 4, "int32_t size error");
static_assert(sizeof(int64_t) == 8, "int64_t size error");

typedef string Comment;

/**
 * @brief Check if the given name is a valid identifier.
 * A valid identifier starts with a letter or underscore, followed by letters, digits, or underscores.
 * @param name The identifier name to check.
 * @return true if the name is a valid identifier, false otherwise.
 */
bool isValidIdentifier(const string &s);

/**
 * @brief Replace all occurrences of an identifier in a string with a new identifier.
 * Only whole-word matches are replaced.
 * @param s The input string.
 * @param old_name The identifier to be replaced.
 * @param new_name The new identifier to replace with.
 * @return A new string with the replacements made.
 */
string identifierReplace(const string &s, const string &old_name, const string &new_name);

/**
 * @brief Check if the given type is a valid VulSim Basic Data Type.
 * List of valid basic types:
 * uint8, uint16, uint32, uint64, uint128, int8, int16, int32, int64, int128, bool
 * @param type The type string to check.
 * @return true if the type is a valid basic type, false otherwise.
 */
bool isBasicVulType(const string &s);


