#pragma once

#include <vector>
#include <unordered_map>
#include <set>
#include <array>
#include <list>
#include <string>
#include <memory>
#include <filesystem>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <inttypes.h>
#include <math.h>

#define assertf(expr, fmt, ...) { \
    if(!(static_cast <bool> (expr))) [[unlikely]] { \
        printf(fmt "\n", ##__VA_ARGS__); \
        printf("assertion failed: %s, file %s, line %d\n", #expr, __FILE__, __LINE__); \
        fflush(stdout); exit_simulation(1); \
    }}

using std::vector;
using std::unordered_map;
using std::set;
using std::array;
using std::list;
using std::string;
using std::unique_ptr;
using std::shared_ptr;
using std::make_unique;
using std::make_shared;

typedef __uint128_t uint128; 
typedef uint64_t    uint64; 
typedef uint32_t    uint32;
typedef uint16_t    uint16;
typedef uint8_t     uint8;

typedef __int128_t  int128; 
typedef int64_t     int64;
typedef int32_t     int32;
typedef int16_t     int16;
typedef int8_t      int8;


static_assert(sizeof(uint128) == 16, "sizeof(uint128_t) == 16");
static_assert(sizeof(int128) == 16, "sizeof(int128_t) == 16");
static_assert(sizeof(uint64) == 8, "sizeof(uint64_t) == 8");
static_assert(sizeof(int64) == 8, "sizeof(int64_t) == 8");
static_assert(sizeof(uint32) == 4, "sizeof(uint32_t) == 4");
static_assert(sizeof(int32) == 4, "sizeof(int32_t) == 4");
static_assert(sizeof(uint16) == 2, "sizeof(uint16_t) == 2");
static_assert(sizeof(int16) == 2, "sizeof(int16_t) == 2");
static_assert(sizeof(uint8) == 1, "sizeof(uint8_t) == 1");
static_assert(sizeof(int8) == 1, "sizeof(int8_t) == 1");

static_assert(sizeof(double) == 8, "sizeof(double) == 8");
static_assert(sizeof(float) == 4, "sizeof(float) == 4");


#define RAND(from, to) ((rand()%((to)-(from)))+(from))

inline uint64_t rand_long() {
    return ((uint64_t)rand() | ((uint64_t)rand() << 32UL));
}

#define CEIL_DIV(x,y) (((x) + (y) - 1) / (y))
#define ALIGN(x,y) ((y)*CEIL_DIV((x),(y)))
#define BOEQ(a, b) ((!(a))==(!(b)))

uint64 get_current_tick();
uint64 get_current_time_us();

vector<string> get_cmdline_args(const char prefix);

int64 get_design_config_item(const string key);

void exit_simulation(int32 code);
