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

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <array>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

#include <cstdio>
#include <cstdint>

using std::array;
using std::vector;
using std::list;
using std::deque;
using std::map;
using std::unordered_map;
using std::unordered_multimap;
using std::set;
using std::unordered_set;
using std::make_pair;
using std::move;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::shared_ptr;
using std::make_shared;
using std::make_unique;
using std::make_pair;

#if defined(__GNUC__) || defined(__clang__)
#define FORCE_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline
#endif

static_assert(sizeof(bool) == 1, "sizeof(bool) == 1");
static_assert(sizeof(__uint128_t) == 16, "sizeof(__uint128_t) == 16");
static_assert(sizeof(__int128_t) == 16, "sizeof(__int128_t) == 16");
static_assert(sizeof(uint64_t) == 8, "sizeof(uint64) == 8");
static_assert(sizeof(int64_t) == 8, "sizeof(int64) == 8");
static_assert(sizeof(uint32_t) == 4, "sizeof(uint32) == 4");
static_assert(sizeof(int32_t) == 4, "sizeof(int32) == 4");
static_assert(sizeof(uint16_t) == 2, "sizeof(uint16) == 2");
static_assert(sizeof(int16_t) == 2, "sizeof(int16) == 2");
static_assert(sizeof(uint8_t) == 1, "sizeof(uint8) == 1");
static_assert(sizeof(int8_t) == 1, "sizeof(int8) == 1");

using int128_t = __int128_t;
using uint128_t = __uint128_t;

using int8 = int8_t;
using uint8 = uint8_t;
using int16 = int16_t;
using uint16 = uint16_t;
using int32 = int32_t;
using uint32 = uint32_t;
using int64 = int64_t;
using uint64 = uint64_t;
using int128 = __int128_t;
using uint128 = __uint128_t;

constexpr uint32_t log2ceil(uint32_t n) {
    return (n <= 1) ? 0 : 32u - std::countl_zero(n - 1);
}
constexpr uint64_t log2ceil(uint64_t n) {
    return (n <= 1) ? 0 : 64u - std::countl_zero(n - 1UL);
}

void stopSimulation(const string &msg);
void stopSimulation();

#define sim_assert(cond) \
    do { \
        if (!(cond)) [[unlikely]] { \
            printf("Assert failed '%s' in file %s, line %d: %s\n", #cond, __FILE__, __LINE__, #cond); \
            stopSimulation("Stopped by assert"); \
        } \
    } while (0)

#define sim_assertf(cond, fmt, ...) \
    do { \
        if (!(cond)) [[unlikely]] { \
            printf("Assert failed '%s' in file %s, line %d: " fmt, #cond, __FILE__, __LINE__, ##__VA_ARGS__); \
            stopSimulation("Stopped by assert"); \
        } \
    } while (0)

