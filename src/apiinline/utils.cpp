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

string castToLvalueTypeExpr(const string &lvalue_expr, const string &value_expr) {
    return "static_cast<typename std::remove_reference<decltype(" +
           lvalue_expr + ")>::type>(static_cast<uint64_t>(" + value_expr + "))";
}

string packFlatFieldValueExpr(const string &value_expr, const FlatField &field) {
    if (!field.enum_type.empty()) {
        return "Int<" + std::to_string(field.width) + ">(static_cast<uint64_t>(" + value_expr + "))";
    }
    return "Int<" + std::to_string(field.width) + ">(" + value_expr + ")";
}

string unpackFlatFieldValueExpr(
    const string &lvalue_expr,
    const string &value_expr,
    const FlatField &field
) {
    if (field.is_fixint) {
        return "Int<" + std::to_string(field.width) + ">(" + value_expr + ")";
    }
    if (!field.enum_type.empty()) {
        return "static_cast<" + field.enum_type + ">(Int<" +
               std::to_string(field.width) + ">(" + value_expr +
               ").template to<uint64_t>())";
    }
    if (field.width == 1) {
        return "ReduceOr(" + value_expr + ")";
    }
    return "Int<" + std::to_string(field.width) + ">(" + value_expr +
           ").template to<typename std::remove_reference<decltype(" +
           lvalue_expr + ")>::type>()";
}

string flatFieldValueExpr(const string &root, const string &flat_name) {
    if (flat_name == "value") {
        return root;
    }
    if (flat_name == root) {
        return root;
    }
    const string value_prefix = "value.";
    if (flat_name.rfind(value_prefix, 0) == 0) {
        return root + flat_name.substr(std::string("value").size());
    }
    const string prefix = root + ".";
    if (flat_name.rfind(prefix, 0) == 0) {
        return root + "." + flat_name.substr(prefix.size());
    }
    return flat_name;
}

string enumDefaultValueExpr(
    const VulStaticTypeSignature &type,
    const VulStaticBundleLib &bundlelib
) {
    if (type.uint_length > 0) {
        return "";
    }
    string type_name = type.type;
    unordered_set<string> visited;
    for (int depth = 0; depth < 32 && !type_name.empty(); ++depth) {
        if (!visited.insert(type_name).second) {
            return "";
        }
        const VulStaticBundle *bundle = nullptr;
        for (const auto &item : bundlelib) {
            if (item.name == type_name) {
                bundle = &item;
                break;
            }
        }
        if (bundle == nullptr) {
            return "";
        }
        if (!bundle->enum_members.empty()) {
            return type.toString() + "::" + bundle->enum_members.front().name;
        }
        if (!bundle->is_alias || bundle->members.empty()) {
            return "";
        }
        const auto &alias = bundle->members.front();
        if (alias.type.uint_length > 0) {
            return "";
        }
        type_name = alias.type.type;
    }
    return "";
}

string defaultValueExprForType(
    const VulStaticTypeSignature &type,
    const VulStaticBundleLib &bundlelib
) {
    string enum_default = enumDefaultValueExpr(type, bundlelib);
    return enum_default.empty() ? "{}" : enum_default;
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

size_t findLogicSubmoduleFunctionStart(const string &code) {
    size_t logic_func_pos = code.find("\nvoid LogicSubModule_");
    if (logic_func_pos == string::npos) {
        return code.find("void LogicSubModule_");
    }
    return logic_func_pos + 1;
}

size_t findLogicSubmoduleBodyInsertion(const string &code) {
    size_t logic_func_pos = findLogicSubmoduleFunctionStart(code);
    if (logic_func_pos == string::npos) {
        return string::npos;
    }
    size_t body_pos = code.find(") {\n", logic_func_pos);
    if (body_pos == string::npos) {
        return string::npos;
    }
    return body_pos + 4;
}

InlineCode applyReplacementsWithDebug(
    const vector<string> &lines,
    const VulDebugLocs &debug,
    vector<Replacement> repls
) {
    const string code = joinLines(lines);
    vector<uint32_t> line_start_offsets;
    line_start_offsets.reserve(lines.size());
    uint32_t offset = 0;
    for (const auto &line : lines) {
        line_start_offsets.push_back(offset);
        offset += static_cast<uint32_t>(line.size());
    }

    auto loc_for_offset = [&](uint32_t pos) -> VulDebugLoc {
        if (line_start_offsets.empty()) {
            return {};
        }
        auto it = std::upper_bound(line_start_offsets.begin(), line_start_offsets.end(), pos);
        size_t idx = (it == line_start_offsets.begin()) ? 0 : static_cast<size_t>((it - line_start_offsets.begin()) - 1);
        if (idx < debug.size()) {
            return debug[idx];
        }
        return {};
    };

    std::sort(repls.begin(), repls.end(), [](const Replacement &a, const Replacement &b) {
        return a.start < b.start;
    });

    string out_text;
    vector<VulDebugLoc> char_debug;
    out_text.reserve(code.size());
    char_debug.reserve(code.size());

    auto append_text = [&](const string &text, const VulDebugLoc &loc) {
        out_text += text;
        char_debug.insert(char_debug.end(), text.size(), loc);
    };

    uint32_t cursor = 0;
    for (const auto &repl : repls) {
        if (repl.start > cursor) {
            uint32_t p = cursor;
            while (p < repl.start) {
                VulDebugLoc loc = loc_for_offset(p);
                uint32_t line_end = p;
                while (line_end < repl.start && line_end < code.size() && code[line_end] != '\n') {
                    ++line_end;
                }
                if (line_end < repl.start && line_end < code.size() && code[line_end] == '\n') {
                    ++line_end;
                }
                append_text(code.substr(p, line_end - p), loc);
                p = line_end;
            }
        }
        append_text(repl.text, loc_for_offset(repl.start));
        cursor = repl.end;
    }
    if (cursor < code.size()) {
        uint32_t p = cursor;
        while (p < code.size()) {
            VulDebugLoc loc = loc_for_offset(p);
            uint32_t line_end = p;
            while (line_end < code.size() && code[line_end] != '\n') {
                ++line_end;
            }
            if (line_end < code.size() && code[line_end] == '\n') {
                ++line_end;
            }
            append_text(code.substr(p, line_end - p), loc);
            p = line_end;
        }
    }

    InlineCode out;
    size_t begin = 0;
    while (begin < out_text.size()) {
        size_t end = out_text.find('\n', begin);
        if (end == string::npos) {
            out.lines.push_back(out_text.substr(begin));
            out.debug.push_back(begin < char_debug.size() ? char_debug[begin] : VulDebugLoc{});
            break;
        }
        out.lines.push_back(out_text.substr(begin, end - begin + 1));
        out.debug.push_back(begin < char_debug.size() ? char_debug[begin] : VulDebugLoc{});
        begin = end + 1;
    }
    return out;
}

} // namespace apiinline
