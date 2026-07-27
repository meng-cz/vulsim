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
typedef string DataType;

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
bool isBasicVulType(const DataType &s);


