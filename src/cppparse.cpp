
#include "cppparse.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <assert.h>

namespace cppparse {


std::vector<std::string> readFileLines(const std::string& filename) {
    std::vector<std::string> lines;
    std::ifstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
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
        rtrim(processed);

        // ===== 检查是否以 \ 结尾 =====
        bool has_backslash = false;
        if (!processed.empty() && processed.back() == '\\') {
            has_backslash = true;
            processed.pop_back(); // 去掉反斜线
            rtrim(processed);
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
        throw std::runtime_error("Invalid macro pattern: " + pattern);
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


} // namespace cppparse
