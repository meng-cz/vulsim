
#include "cppparse.hpp"
#include "stringop.hpp"
#include "errormsg.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <assert.h>

namespace cppparse {

using stringop::trim;
using stringop::skip_spaces;
using stringop::split;


std::vector<std::string> readFileLines(const std::string& filename) {
    std::vector<std::string> lines;
    std::ifstream file(filename);

    if (!file.is_open()) {
        throw VulException("Failed to open file: " + filename);
    }

    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    return lines;
}

TrimResult stripComments(const std::vector<std::string>& input) {
    std::vector<std::string> out;
    std::vector<uint32_t> map;

    bool in_block_comment = false;

    std::string current_line;
    uint32_t current_origin = 0;
    bool continuing = false; // 是否处于 \ 拼接中

    for (uint32_t i = 0; i < input.size(); ++i) {
        const std::string& line = input[i];

        std::string processed;

        bool in_string = false;
        bool in_char = false;
        bool escape = false;

        for (size_t j = 0; j < line.size(); ++j) {
            char c = line[j];

            // ===== block comment =====
            if (in_block_comment) {
                if (c == '*' && j + 1 < line.size() && line[j + 1] == '/') {
                    in_block_comment = false;
                    ++j;
                }
                continue;
            }

            // ===== string / char =====
            if (in_string || in_char) {
                processed.push_back(c);

                if (escape) {
                    escape = false;
                } else if (c == '\\') {
                    escape = true;
                } else if (in_string && c == '"') {
                    in_string = false;
                } else if (in_char && c == '\'') {
                    in_char = false;
                }

                continue;
            }

            // ===== start string =====
            if (c == '"') {
                in_string = true;
                processed.push_back(c);
                continue;
            }
            if (c == '\'') {
                in_char = true;
                processed.push_back(c);
                continue;
            }

            // ===== comments =====
            if (c == '/' && j + 1 < line.size()) {
                if (line[j + 1] == '/') {
                    break; // 单行注释
                }
                if (line[j + 1] == '*') {
                    in_block_comment = true;
                    ++j;
                    continue;
                }
            }

            processed.push_back(c);
        }

        // 去掉行尾空白（方便判断 \）
        stringop::rtrim_inplace(processed);

        // ===== 检查是否以 \ 结尾 =====
        bool has_backslash = false;
        if (!processed.empty() && processed.back() == '\\') {
            has_backslash = true;
            processed.pop_back(); // 去掉反斜线
            stringop::rtrim_inplace(processed);
        }

        // ===== 拼接逻辑 =====
        if (!continuing) {
            current_line = processed;
            current_origin = i;
        } else {
            current_line += processed;
        }

        continuing = has_backslash;

        // ===== 如果不再继续，输出一行 =====
        if (!continuing) {
            if (!current_line.empty()) {
                out.push_back(current_line);
                map.push_back(current_origin);
            }
            current_line.clear();
        }
    }

    // 处理文件结尾仍在拼接的情况
    if (continuing && !current_line.empty()) {
        out.push_back(current_line);
        map.push_back(current_origin);
    }

    return {out, map};
}


LinePosition findNext(
    const std::vector<std::string>& code,
    LinePosition start,
    const std::string& target
) {
    if (target.empty()) {
        return {-1, -1};
    }

    int32_t n = static_cast<int32_t>(code.size());
    if (start.line < 0 || start.line >= n) {
        return {-1, -1};
    }

    for (int32_t i = start.line; i < n; ++i) {
        const std::string& line = code[i];

        // 起始列：第一行用 start.column，其余从 0 开始
        size_t begin = (i == start.line) ? static_cast<size_t>(start.column) : 0;

        if (begin > line.size()) continue;

        size_t pos = line.find(target, begin);
        if (pos != std::string::npos) {
            return {i, static_cast<int32_t>(pos)};
        }
    }

    return {-1, -1};
}

BlockResult findNextBraceBlock(const std::vector<std::string>& code, LinePosition start, const char begin, const char end, bool keep_nextline) {
    // 1️⃣ 找到下一个 '{'
    LinePosition pos = findNext(code, start, std::string(1, begin));
    if (pos.line == -1) {
        return {{-1, -1}, ""};
    }

    // 如果先出现了一个;，代表没有合法的大括号匹配
    LinePosition semicolon_pos = findNext(code, start, ";");
    if (semicolon_pos.line != -1) {
        if (semicolon_pos.line < pos.line || (semicolon_pos.line == pos.line && semicolon_pos.column < pos.column)) {
            return {{-1, -1}, ""};
        }
    }

    int depth = 0;
    bool in_string = false;
    bool in_char = false;
    bool escape = false;

    std::string content;

    for (int32_t i = pos.line; i < (int32_t)code.size(); ++i) {
        const std::string& line = code[i];

        size_t j = (i == pos.line) ? pos.column : 0;

        for (; j < line.size(); ++j) {
            char c = line[j];

            // ===== 字符串/字符处理 =====
            if (in_string || in_char) {
                if (depth >= 1) content.push_back(c);

                if (escape) {
                    escape = false;
                } else if (c == '\\') {
                    escape = true;
                } else if (in_string && c == '"') {
                    in_string = false;
                } else if (in_char && c == '\'') {
                    in_char = false;
                }
                continue;
            }

            if (c == '"') {
                in_string = true;
                if (depth >= 1) content.push_back(c);
                continue;
            }

            if (c == '\'') {
                in_char = true;
                if (depth >= 1) content.push_back(c);
                continue;
            }

            // ===== 大括号处理 =====
            if (c == begin) {
                if (depth >= 1) content.push_back(c);
                ++depth;
                continue;
            }

            if (c == end) {
                --depth;

                if (depth == 0) {
                    // 找到匹配的结束
                    return {{i, (int32_t)j}, content};
                }

                if (depth >= 1) content.push_back(c);
                continue;
            }

            // ===== 普通字符 =====
            if (depth >= 1) {
                content.push_back(c);
            }
        }
        if (depth >= 1 && keep_nextline) {
            content.push_back('\n');
        }
    }

    // 没有匹配到
    return {{-1, -1}, ""};
}

// 解析描述串：NAME($,$,...) -> (name, argc)
static bool parse_pattern(const std::string& pattern, std::string& name, int& argc) {
    auto l = pattern.find('(');
    auto r = pattern.find(')');
    if (l == std::string::npos || r == std::string::npos || r < l) return false;

    name = pattern.substr(0, l);

    argc = 0;
    bool has_dollar = false;
    for (size_t i = l + 1; i < r; ++i) {
        if (pattern[i] == '$') {
            ++argc;
            has_dollar = true;
        }
    }

    // 允许空参数宏：NAME()
    if (!has_dollar) argc = 0;

    return true;
}

std::vector<MatchMacroResult> matchMacros(const std::vector<std::string>& code, const std::string& pattern) {
    LinePosition pos{0, 0};
    std::vector<MatchMacroResult> results;
    std::string macro_name;
    int macro_argc;
    if (!parse_pattern(pattern, macro_name, macro_argc)) {
        throw VulException("Invalid macro pattern: " + pattern);
    }
    while (true) {
        pos = findNext(code, pos, macro_name);
        if (pos.line == -1) break;

        // 检查后面是否跟着 '('
        const std::string& line = code[pos.line];
        size_t after_name_pos = pos.column + macro_name.size();
        if (after_name_pos >= line.size() || line[after_name_pos] != '(') {
            pos.column = after_name_pos;
            continue; // 不是宏调用，继续搜索
        }

        // 解析括号内的内容
        auto block = findNextBraceBlock(code, pos, '(', ')', false);
        if (block.end_pos.line == -1) {
            pos.column = after_name_pos;
            continue; // 没有找到匹配的括号，继续搜索
        }

        // 拆分参数
        std::vector<std::string> args = split(block.content, ',');
        if (macro_argc != 0 && (args.size() != macro_argc)) {
            pos.column = block.end_pos.column + 1;
            continue; // 参数数量不匹配，继续搜索
        }

        // 去掉参数两端空白
        for (auto& arg : args) {
            arg = trim(arg);
        }

        results.push_back({pos, args});

        pos = {block.end_pos.line, block.end_pos.column + 1};
    }

    return results;
}

bool codeblockContainsFunctionCall(const std::vector<std::string>& code, const std::string& func_name) {
    if (func_name.empty()) return false;

    auto is_ident_char = [](char c) {
        return (c >= 'a' && c <= 'z') ||
               (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') ||
               c == '_';
    };

    for (size_t i = 0; i < code.size(); ++i) {
        const std::string& line = code[i];
        size_t pos = 0;

        while (true) {
            pos = line.find(func_name, pos);
            if (pos == std::string::npos) break;

            // 仅匹配完整标识符，避免 foo 匹配到 myfoo
            if (pos > 0 && is_ident_char(line[pos - 1])) {
                pos += func_name.size();
                continue;
            }
            size_t end = pos + func_name.size();
            if (end < line.size() && is_ident_char(line[end])) {
                pos += func_name.size();
                continue;
            }

            // 找 name 后第一个非空白字符（可跨行）
            size_t li = i;
            size_t ci = end;
            while (li < code.size()) {
                const std::string& cur = code[li];
                while (ci < cur.size() && (cur[ci] == ' ' || cur[ci] == '\t' || cur[ci] == '\r' || cur[ci] == '\n')) {
                    ++ci;
                }
                if (ci < cur.size()) {
                    if (cur[ci] == '(') return true;
                    break;
                }
                ++li;
                ci = 0;
            }

            pos += func_name.size();
        }
    }
    return false;
}

using namespace stringop;

namespace {

struct FlatCode {
    std::string text;
    std::vector<LinePosition> pos;
};

bool isIdentStart(char c) {
    unsigned char u = static_cast<unsigned char>(c);
    return std::isalpha(u) || c == '_';
}

bool isIdentChar(char c) {
    unsigned char u = static_cast<unsigned char>(c);
    return std::isalnum(u) || c == '_';
}

bool isSpace(char c) {
    unsigned char u = static_cast<unsigned char>(c);
    return std::isspace(u);
}

std::string locStr(const std::vector<LinePosition>& pos, size_t i) {
    if (i >= pos.size()) {
        return "<eof>";
    }

    return "line " + std::to_string(pos[i].line) +
           ", column " + std::to_string(pos[i].column);
}

FlatCode flattenCodeWithoutPreprocessorLines(const std::vector<std::string>& code) {
    FlatCode out;

    for (int32_t line = 0; line < static_cast<int32_t>(code.size()); ++line) {
        const std::string& s = code[line];

        if (!ltrim(s).empty() && ltrim(s)[0] == '#') {
            continue;
        }

        for (int32_t col = 0; col < static_cast<int32_t>(s.size()); ++col) {
            out.text.push_back(s[col]);
            out.pos.push_back({line, col});
        }

        out.text.push_back('\n');
        out.pos.push_back({line, static_cast<int32_t>(s.size())});
    }

    return out;
}

size_t skipInterEntryTokens(const std::string& s, size_t p) {
    while (p < s.size()) {
        if (isSpace(s[p]) || s[p] == ';') {
            ++p;
        } else {
            break;
        }
    }

    return p;
}

size_t skipSpacesOnly(const std::string& s, size_t p) {
    while (p < s.size() && isSpace(s[p])) {
        ++p;
    }

    return p;
}

// 跳过普通字符串、字符字面量，以及形如 R"delim(...)delim" 的 raw string。
// 如果当前位置不是字面量起点，则返回原位置。
size_t skipLiteralIfAt(const std::string& s, size_t p) {
    if (p >= s.size()) {
        return p;
    }

    // raw string literal: R"delim(...)delim"
    // 对 u8R"...", LR"...", uR"...", UR"..." 这种情况，扫描到 R 时也能处理。
    if (s[p] == 'R' && p + 1 < s.size() && s[p + 1] == '"') {
        size_t delimBegin = p + 2;
        size_t openParen = delimBegin;

        while (openParen < s.size() && s[openParen] != '(') {
            ++openParen;
        }

        if (openParen >= s.size()) {
            return s.size();
        }

        std::string delim = s.substr(delimBegin, openParen - delimBegin);
        std::string endMark = ")" + delim + "\"";

        size_t end = s.find(endMark, openParen + 1);
        if (end == std::string::npos) {
            return s.size();
        }

        return end + endMark.size();
    }

    // normal string / char literal
    if (s[p] != '"' && s[p] != '\'') {
        return p;
    }

    char quote = s[p];
    ++p;

    while (p < s.size()) {
        if (s[p] == '\\') {
            // 跳过转义字符，例如 \"、\\、\n、'\'' 等。
            p += 2;
            continue;
        }

        if (s[p] == quote) {
            return p + 1;
        }

        ++p;
    }

    return s.size();
}

size_t findMatchingByCounter(
    const std::string& s,
    const std::vector<LinePosition>& pos,
    size_t openPos,
    char openCh,
    char closeCh
) {
    if (openPos >= s.size() || s[openPos] != openCh) {
        throw VulException("internal parser error: invalid open position");
    }

    int depth = 1;
    size_t p = openPos + 1;

    while (p < s.size()) {
        size_t afterLiteral = skipLiteralIfAt(s, p);
        if (afterLiteral != p) {
            p = afterLiteral;
            continue;
        }

        if (s[p] == openCh) {
            ++depth;
            ++p;
            continue;
        }

        if (s[p] == closeCh) {
            --depth;

            if (depth == 0) {
                return p;
            }

            ++p;
            continue;
        }

        ++p;
    }

    throw VulException(
        std::string("unmatched '") + openCh + "' at " + locStr(pos, openPos)
    );
}

std::vector<std::string> splitTopLevelArgs(std::string_view sv) {
    std::vector<std::string> result;

    size_t begin = 0;
    size_t p = 0;

    int parenDepth = 0;
    int braceDepth = 0;
    int bracketDepth = 0;
    int angleDepth = 0;

    auto isTemplateAngleOpen = [&](size_t i) -> bool {
        // 这里故意只做保守处理。
        // 对普通宏参数，逗号通常只需要避开 (), {}, []。
        // 如果参数中包含 std::map<int,int> 这种模板类型，可以通过 angleDepth 避免误切。
        return i < sv.size() && sv[i] == '<';
    };

    std::string tmp(sv);

    while (p < tmp.size()) {
        size_t afterLiteral = skipLiteralIfAt(tmp, p);
        if (afterLiteral != p) {
            p = afterLiteral;
            continue;
        }

        char c = tmp[p];

        if (c == '(') {
            ++parenDepth;
        } else if (c == ')') {
            if (parenDepth > 0) --parenDepth;
        } else if (c == '{') {
            ++braceDepth;
        } else if (c == '}') {
            if (braceDepth > 0) --braceDepth;
        } else if (c == '[') {
            ++bracketDepth;
        } else if (c == ']') {
            if (bracketDepth > 0) --bracketDepth;
        } else if (isTemplateAngleOpen(p)) {
            ++angleDepth;
        } else if (c == '>') {
            if (angleDepth > 0) --angleDepth;
        } else if (
            c == ',' &&
            parenDepth == 0 &&
            braceDepth == 0 &&
            bracketDepth == 0 &&
            angleDepth == 0
        ) {
            result.push_back(trim(tmp.substr(begin, p - begin)));
            begin = p + 1;
        }

        ++p;
    }

    std::string last = trim(tmp.substr(begin));

    if (!last.empty() || !result.empty()) {
        result.push_back(std::move(last));
    }

    return result;
}

std::vector<std::string> splitBodyLines(std::string_view sv) {
    std::string text(sv);

    // 常见格式：
    //
    // MACRO(...) {
    //   body
    // }
    //
    // 去掉由外层大括号引入的首尾换行，避免 body 变成 ["", "...", ""]。
    if (!text.empty() && text.front() == '\n') {
        text.erase(text.begin());
    }

    if (!text.empty() && text.back() == '\n') {
        text.pop_back();
    }

    std::vector<std::string> lines;
    size_t begin = 0;

    while (begin <= text.size()) {
        size_t end = text.find('\n', begin);

        if (end == std::string::npos) {
            lines.push_back(text.substr(begin));
            break;
        }

        lines.push_back(text.substr(begin, end - begin));
        begin = end + 1;
    }

    // 空 body 返回空 vector，而不是 {""}。
    if (lines.size() == 1 && lines[0].empty()) {
        lines.clear();
    }

    return lines;
}

} // namespace

std::vector<MacroEntry> findAllMacroEntries(const std::vector<std::string>& code) {
    FlatCode flat = flattenCodeWithoutPreprocessorLines(code);

    const std::string& s = flat.text;
    const std::vector<LinePosition>& pos = flat.pos;

    std::vector<MacroEntry> entries;

    size_t p = 0;

    while (true) {
        p = skipInterEntryTokens(s, p);

        if (p >= s.size()) {
            break;
        }

        if (!isIdentStart(s[p])) {
            throw VulException(
                "expected macro name at " + locStr(pos, p)
            );
        }

        size_t nameBegin = p;
        ++p;

        while (p < s.size() && isIdentChar(s[p])) {
            ++p;
        }

        std::string name = s.substr(nameBegin, p - nameBegin);
        LinePosition entryPos = pos[nameBegin];

        p = skipSpacesOnly(s, p);

        if (p >= s.size() || s[p] != '(') {
            throw VulException(
                "expected '(' after macro name '" + name + "' at " + locStr(pos, p)
            );
        }

        size_t argOpen = p;
        size_t argClose = findMatchingByCounter(s, pos, argOpen, '(', ')');

        std::string_view argText(
            s.data() + argOpen + 1,
            argClose - argOpen - 1
        );

        std::vector<std::string> args = splitTopLevelArgs(argText);

        p = argClose + 1;
        p = skipSpacesOnly(s, p);

        MacroEntry entry;
        entry.pos = entryPos;
        entry.name = std::move(name);
        entry.args = std::move(args);

        if (p < s.size() && s[p] == '{') {
            size_t bodyOpen = p;
            size_t bodyClose = findMatchingByCounter(s, pos, bodyOpen, '{', '}');

            std::string_view bodyText(
                s.data() + bodyOpen + 1,
                bodyClose - bodyOpen - 1
            );

            entry.body = splitBodyLines(bodyText);
            p = bodyClose + 1;
        } else if (p < s.size() && s[p] == ';') {
            entry.body.clear();
            ++p;
        } else {
            throw VulException(
                "expected '{' or ';' after macro entry '" +
                entry.name + "' at " + locStr(pos, p)
            );
        }

        entries.push_back(std::move(entry));
    }

    return entries;
}


} // namespace cppparse
