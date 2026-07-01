// MIT License

// Copyright (c) 2025 Meng Chengzhen, in Shandong University

#include "apiinline/utils.hpp"

#include <algorithm>

namespace apiinline {

namespace {

struct ClangString {
    CXString value;
    ~ClangString() { clang_disposeString(value); }
    string str() const { return clang_getCString(value); }
};

} // namespace

string joinLines(const vector<string> &lines) {
    string out;
    for (const auto &line : lines) {
        out += line;
    }
    return out;
}

vector<string> splitLinesKeepEnds(const string &code) {
    vector<string> lines;
    size_t pos = 0;
    while (pos < code.size()) {
        size_t next = code.find('\n', pos);
        if (next == string::npos) {
            lines.push_back(code.substr(pos));
            return lines;
        }
        lines.push_back(code.substr(pos, next - pos + 1));
        pos = next + 1;
    }
    return lines;
}

string uintExtractExpr(const string &var, uint32_t high, uint32_t low) {
    return var + ".at<" + std::to_string(high) + ", " + std::to_string(low) + ">()";
}

string flatFieldValueExpr(const string &root, const string &flat_name) {
    if (flat_name == "value") {
        return root;
    }
    if (flat_name == root) {
        return root;
    }
    const string prefix = root + ".";
    if (flat_name.rfind(prefix, 0) == 0) {
        return root + "." + flat_name.substr(prefix.size());
    }
    return flat_name;
}

int findMatching(
    const vector<TokenInfo> &tokens,
    int open_idx,
    const string &open_spelling,
    const string &close_spelling
) {
    int depth = 0;
    for (int i = open_idx; i < static_cast<int>(tokens.size()); ++i) {
        if (tokens[i].spelling == open_spelling) {
            ++depth;
        } else if (tokens[i].spelling == close_spelling) {
            --depth;
            if (depth == 0) {
                return i;
            }
        }
    }
    return -1;
}

string sourceBetween(const string &code, const TokenInfo &first, const TokenInfo &last) {
    if (last.end <= first.start) {
        return "";
    }
    return code.substr(first.start, last.end - first.start);
}

vector<string> splitTopLevelArgs(
    const string &code,
    const vector<TokenInfo> &tokens,
    int first_idx,
    int last_idx
) {
    vector<string> args;
    if (first_idx > last_idx) {
        return args;
    }
    int start_idx = first_idx;
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;
    int angle_depth = 0;
    for (int i = first_idx; i <= last_idx; ++i) {
        const string &s = tokens[i].spelling;
        if (s == "(") ++paren_depth;
        else if (s == ")") --paren_depth;
        else if (s == "[") ++bracket_depth;
        else if (s == "]") --bracket_depth;
        else if (s == "{") ++brace_depth;
        else if (s == "}") --brace_depth;
        else if (s == "<") ++angle_depth;
        else if (s == ">") --angle_depth;
        else if (s == "," && paren_depth == 0 && bracket_depth == 0 && brace_depth == 0 && angle_depth == 0) {
            if (start_idx <= i - 1) {
                args.push_back(sourceBetween(code, tokens[start_idx], tokens[i - 1]));
            } else {
                args.push_back("");
            }
            start_idx = i + 1;
        }
    }
    if (start_idx <= last_idx) {
        args.push_back(sourceBetween(code, tokens[start_idx], tokens[last_idx]));
    } else if (first_idx <= last_idx + 1) {
        args.push_back("");
    }
    return args;
}

bool overlapsExisting(const vector<Replacement> &repls, uint32_t start, uint32_t end) {
    for (const auto &repl : repls) {
        if (start < repl.end && repl.start < end) {
            return true;
        }
    }
    return false;
}

vector<TokenInfo> tokenizeWithLibclang(const string &code) {
    const char *args[] = {
        "-std=c++20",
        "-xc++",
        "-I.",
        "-I/usr/lib/llvm-14/include"
    };
    const string filename = "vulsim_apiinline.cpp";
    CXUnsavedFile unsaved;
    unsaved.Filename = filename.c_str();
    unsaved.Contents = code.c_str();
    unsaved.Length = code.size();

    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit tu = clang_parseTranslationUnit(
        index,
        filename.c_str(),
        args,
        sizeof(args) / sizeof(args[0]),
        &unsaved,
        1,
        CXTranslationUnit_KeepGoing
    );
    vector<TokenInfo> out;
    if (tu == nullptr) {
        clang_disposeIndex(index);
        return out;
    }

    CXFile file = clang_getFile(tu, filename.c_str());
    CXSourceLocation start = clang_getLocationForOffset(tu, file, 0);
    CXSourceLocation end = clang_getLocationForOffset(tu, file, static_cast<unsigned>(code.size()));
    CXSourceRange range = clang_getRange(start, end);
    CXToken *tokens = nullptr;
    unsigned num_tokens = 0;
    clang_tokenize(tu, range, &tokens, &num_tokens);
    out.reserve(num_tokens);
    for (unsigned i = 0; i < num_tokens; ++i) {
        CXSourceRange extent = clang_getTokenExtent(tu, tokens[i]);
        CXSourceLocation loc_start = clang_getRangeStart(extent);
        CXSourceLocation loc_end = clang_getRangeEnd(extent);
        unsigned start_offset = 0;
        unsigned end_offset = 0;
        clang_getFileLocation(loc_start, nullptr, nullptr, nullptr, &start_offset);
        clang_getFileLocation(loc_end, nullptr, nullptr, nullptr, &end_offset);
        ClangString spelling{clang_getTokenSpelling(tu, tokens[i])};
        out.push_back(TokenInfo{
            spelling.str(),
            clang_getTokenKind(tokens[i]),
            start_offset,
            end_offset
        });
    }
    clang_disposeTokens(tu, tokens, num_tokens);
    clang_disposeTranslationUnit(tu);
    clang_disposeIndex(index);
    return out;
}

string applyReplacements(string code, vector<Replacement> repls) {
    std::sort(repls.begin(), repls.end(), [](const Replacement &a, const Replacement &b) {
        return a.start > b.start;
    });
    for (const auto &repl : repls) {
        code.replace(repl.start, repl.end - repl.start, repl.text);
    }
    return code;
}

} // namespace apiinline
