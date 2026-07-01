// MIT License

// Copyright (c) 2025 Meng Chengzhen, in Shandong University

#pragma once

#include "bundlelib.h"

#include <clang-c/Index.h>

namespace apiinline {

struct TokenInfo {
    string spelling;
    CXTokenKind kind;
    uint32_t start = 0;
    uint32_t end = 0;
};

struct Replacement {
    uint32_t start = 0;
    uint32_t end = 0;
    string text;
};

string joinLines(const vector<string> &lines);

vector<string> splitLinesKeepEnds(const string &code);

string uintExtractExpr(const string &var, uint32_t high, uint32_t low);

string flatFieldValueExpr(const string &root, const string &flat_name);

int findMatching(
    const vector<TokenInfo> &tokens,
    int open_idx,
    const string &open_spelling,
    const string &close_spelling
);

string sourceBetween(const string &code, const TokenInfo &first, const TokenInfo &last);

vector<string> splitTopLevelArgs(
    const string &code,
    const vector<TokenInfo> &tokens,
    int first_idx,
    int last_idx
);

bool overlapsExisting(const vector<Replacement> &repls, uint32_t start, uint32_t end);

vector<TokenInfo> tokenizeWithLibclang(const string &code);

string applyReplacements(string code, vector<Replacement> repls);

} // namespace apiinline
