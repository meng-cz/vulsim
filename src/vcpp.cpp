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


#include "vcpp.hpp"
#include "project.h"
#include "configexpr.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <assert.h>

std::vector<std::string> _readFileLines(const std::string& filename) {
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


struct TrimResult {
    std::vector<std::string> lines;   // 去注释后的代码
    std::vector<uint32_t> mapping;    // 新行号 -> 原始行号（1-based）
};

static inline void rtrim(std::string& s) {
    size_t end = s.find_last_not_of(" \t");
    if (end == std::string::npos) s.clear();
    else s.resize(end + 1);
}

TrimResult _stripComments(const std::vector<std::string>& input) {
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

struct LinePosition {
    int32_t line;   // 0-based line number
    int32_t column; // 0-based column number
};

LinePosition _findNext(
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

static size_t skip_spaces(const std::string& s, size_t pos) {
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) {
        ++pos;
    }
    return pos;
}

static std::string trim(const std::string& s) {
    size_t l = s.find_first_not_of(" \t");
    if (l == std::string::npos) return "";
    size_t r = s.find_last_not_of(" \t");
    return s.substr(l, r - l + 1);
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

struct MatchMacroResult {
    LinePosition pos;
    std::vector<std::string> args;
};

std::vector<MatchMacroResult> _matchMacros(const std::vector<std::string>& code, const std::string& pattern) {
    std::vector<MatchMacroResult> results;

    std::string name;
    int argc = 0;
    if (!parse_pattern(pattern, name, argc)) {
        return results;
    }

    std::string key = name + "(";

    LinePosition pos{0, 0};

    while (true) {
        pos = _findNext(code, pos, key);
        if (pos.line == -1) break;

        const std::string& line = code[pos.line];
        size_t i = pos.column + key.size();

        std::vector<std::string> args;
        std::string current;

        int paren_depth = 0;
        bool in_string = false;
        bool escape = false;

        for (; i < line.size(); ++i) {
            char c = line[i];

            // 字符串处理
            if (in_string) {
                current.push_back(c);
                if (escape) {
                    escape = false;
                } else if (c == '\\') {
                    escape = true;
                } else if (c == '"') {
                    in_string = false;
                }
                continue;
            }

            if (c == '"') {
                in_string = true;
                current.push_back(c);
                continue;
            }

            if (c == '(') {
                ++paren_depth;
                current.push_back(c);
                continue;
            }

            if (c == ')') {
                if (paren_depth == 0) {
                    // 最后一参数结束
                    if (!current.empty() || argc > 0) {
                        args.push_back(trim(current));
                    }
                    break;
                }
                --paren_depth;
                current.push_back(c);
                continue;
            }

            if (c == ',' && paren_depth == 0) {
                args.push_back(trim(current));
                current.clear();
                continue;
            }

            current.push_back(c);
        }

        // 参数个数匹配才记录
        if (static_cast<int>(args.size()) == argc || argc == 0) {
            results.push_back({pos, args});
        } else {
            std::cerr << "Found macro call with wrong number of arguments : expected " << argc << ", got " << args.size() << ", line: " << code[pos.line] << std::endl;
            assert(0);
        }

        // 防止死循环，向后推进
        pos.column = static_cast<int32_t>(i + 1);
    }

    return results;
}

struct BlockResult {
    LinePosition end_pos;
    std::string content;
};

BlockResult _findNextBraceBlock(const std::vector<std::string>& code, LinePosition start, const char begin, const char end, bool keep_nextline = false) {
    // 1️⃣ 找到下一个 '{'
    LinePosition pos = _findNext(code, start, std::string(1, begin));
    if (pos.line == -1) {
        return {{-1, -1}, ""};
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

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> result;
    std::string current;

    for (char c : s) {
        if (c == delim) {
            result.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }

    result.push_back(current); // 最后一段
    return result;
}


struct MemberDecl {
    std::string type;
    std::string name;
    std::string init;
    std::vector<std::string> dims;
};

MemberDecl parse_member_decl(const std::string& line) {
    MemberDecl res;

    std::string s = trim(line);

    // ===== 1️⃣ 拆分初始化（=）=====
    size_t eq_pos = std::string::npos;
    int paren = 0;
    bool in_string = false, escape = false;

    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];

        if (in_string) {
            if (escape) escape = false;
            else if (c == '\\') escape = true;
            else if (c == '"') in_string = false;
            continue;
        }

        if (c == '"') {
            in_string = true;
            continue;
        }

        if (c == '(' || c == '<') ++paren;
        if (c == ')' || c == '>') --paren;

        if (c == '=' && paren == 0) {
            eq_pos = i;
            break;
        }
    }

    std::string left = (eq_pos == std::string::npos) ? s : s.substr(0, eq_pos);
    std::string init = (eq_pos == std::string::npos) ? "" : s.substr(eq_pos + 1);

    res.init = trim(init);

    // ===== 2️⃣ 解析数组维度 =====
    std::string main_part;
    size_t i = 0;

    while (i < left.size()) {
        if (left[i] == '[') {
            size_t j = i + 1;
            int depth = 1;
            while (j < left.size() && depth > 0) {
                if (left[j] == '[') ++depth;
                else if (left[j] == ']') --depth;
                ++j;
            }

            std::string dim = left.substr(i + 1, j - i - 2);
            res.dims.push_back(trim(dim));

            i = j;
        } else {
            main_part.push_back(left[i]);
            ++i;
        }
    }

    main_part = trim(main_part);

    // ===== 3️⃣ 提取 name（最后一个标识符）=====
    int end = (int)main_part.size() - 1;

    while (end >= 0 && std::isspace(main_part[end])) --end;

    int start = end;
    while (start >= 0 &&
           (std::isalnum(main_part[start]) || main_part[start] == '_')) {
        --start;
    }

    res.name = main_part.substr(start + 1, end - start);

    // ===== 4️⃣ 剩余部分是 type =====
    res.type = trim(main_part.substr(0, start + 1));

    return res;
}


vector<VulConfigItem> _parseConsts(const std::vector<std::string>& code) {
    vector<VulConfigItem> configs;

    auto matches = _matchMacros(code, "CONST($,$)");
    for (const auto& args : matches) {
        const std::string& name = args.args[0];
        const std::string& value = args.args[1];
        VulConfigItem item;
        item.name = name;
        item.value = value;
        item.comment = "";
        configs.push_back(item);
    }

    return configs;
}

vector<VulBundleItem> _parseBundles(const std::vector<std::string>& code) {
    vector<VulBundleItem> bundles;

    auto matches = _matchMacros(code, "ALIAS($,$)");
    for (const auto& args : matches) {
        const std::string& name = args.args[0];
        const std::string& target = args.args[1];
        VulBundleItem item;
        item.name = name;
        item.is_alias = true;
        VulBundleMember member;
        member.type = target;
        member.name = "alias_target";
        member.uint_length = "";
        item.members.push_back(member);
        bundles.push_back(item);
    }

    matches = _matchMacros(code, "ALIAS_ARRAY1($,$,$)");
    for (const auto& args : matches) {
        const std::string& name = args.args[0];
        const std::string& target = args.args[1];
        const std::string& dim = args.args[2];
        VulBundleItem item;
        item.name = name;
        item.is_alias = true;
        VulBundleMember member;
        member.type = target;
        member.name = "alias_target";
        member.uint_length = "";
        member.dims.push_back(dim);
        item.members.push_back(member);
        bundles.push_back(item);
    }

    matches = _matchMacros(code, "ALIAS_ARRAY2($,$,$,$)");
    for (const auto& args : matches) {
        const std::string& name = args.args[0];
        const std::string& target = args.args[1];
        const std::string& dim1 = args.args[2];
        const std::string& dim2 = args.args[3];
        VulBundleItem item;
        item.name = name;
        item.is_alias = true;
        VulBundleMember member;
        member.type = target;
        member.name = "alias_target";
        member.uint_length = "";
        member.dims.push_back(dim1);
        member.dims.push_back(dim2);
        item.members.push_back(member);
        bundles.push_back(item);
    }

    matches = _matchMacros(code, "STRUCT($)");
    for (const auto& args : matches) {
        const std::string& name = args.args[0];
        auto block = _findNextBraceBlock(code, args.pos, '{', '}');
        if (block.end_pos.line == -1) {
            std::cerr << "Error: STRUCT " << name << " has no body" << std::endl;
            assert(0);
        }
        vector<std::string> member_lines = split(block.content, ';');
        VulBundleItem item;
        item.name = name;
        item.is_alias = false;
        for (const auto& line : member_lines) {
            auto decl = parse_member_decl(line);
            if (!decl.name.empty()) {
                VulBundleMember member;
                member.name = decl.name;
                member.type = decl.type;
                member.value = decl.init;
                member.dims = decl.dims;
                member.uint_length = "";
                item.members.push_back(member);
            }
        }
        bundles.push_back(item);
    }

    return bundles;
}

void _parseHeader(const std::vector<std::string>& code, shared_ptr<VulConfigLib> configlib, shared_ptr<VulBundleLib> bundlelib) {
    
    auto configs = _parseConsts(code);
    auto bundles = _parseBundles(code);

    printf("Parsed %zu config items and %zu bundle items from header\n", configs.size(), bundles.size());

    ErrorMsg err;
    err = configlib->insertMultiConfigItems(configs);
    if (err.error()) {
        std::cerr << "Error inserting config items: " << err.msg << std::endl;
        assert(0);
    }
    err = bundlelib->insertMultiBundles(bundles);
    if (err.error()) {
        std::cerr << "Error inserting bundle items: " << err.msg << std::endl;
        assert(0);
    }
}

VulReqServ _parseReqServPort(const vector<string> & macro_args) {
    VulReqServ reqserv;
    reqserv.name = macro_args[0];
    reqserv.comment = "";
    reqserv.has_handshake = (macro_args.size() >= 2 && macro_args[1] != "void");
    for (size_t i = 2; i < macro_args.size(); ++i) {
        VulArg arg;
        vector<string> parts = split(macro_args[i], ' ');
        if (parts.size() != 2) {
            std::cerr << "Error: Invalid argument format in REQ_PORT/SERV_PORT: " << macro_args[i] << std::endl;
            assert(0);
        }
        arg.name = parts[1];
        arg.comment = "";
        // parse ARG(type) or RESP(type) for parts[0]
        string type_str = trim(parts[0]);
        if (type_str.substr(0, 4) == "ARG(" && type_str.back() == ')') {
            arg.type = type_str.substr(4, type_str.size() - 5);
            reqserv.args.push_back(arg);
        } else if (type_str.substr(0, 5) == "RESP(" && type_str.back() == ')') {
            arg.type = type_str.substr(5, type_str.size() - 6);
            reqserv.rets.push_back(arg);
        } else {
            std::cerr << "Error: Invalid argument type format in REQ_PORT/SERV_PORT: " << parts[0] << std::endl;
            assert(0);
        }
    }
    return reqserv;
}

VulModule _parseModule(const std::vector<std::string>& code, const ModuleName & name, unordered_set<ModuleName>& included_modules) {

    VulModule module;
    module.name = name;
    module.comment = "";
    included_modules.clear();

    {
        // parse params
        auto matches = _matchMacros(code, "PARAMETER($,$)");
        for (const auto& item : matches) {
            VulLocalConfigItem config;
            config.name = item.args[0];
            config.value = item.args[1];
            config.comment = "";
            module.local_configs[config.name] = config;
        }
    }
    {
        // parse bundles
        auto bundles = _parseBundles(code);
        for (const auto& item : bundles) {
            module.local_bundles[item.name] = item;
        }
    }
    {
        // parse register
        auto matches = _matchMacros(code, "REGISTER($,$)");
        for (const auto& item : matches) {
            VulStorage storage;
            storage.name = item.args[0];
            storage.comment = "";
            storage.type = item.args[1];
            module.storagenexts[storage.name] = storage;
        }
        matches = _matchMacros(code, "REGISTER_ARRAY1($,$,$)");
        for (const auto& item : matches) {
            VulStorage storage;
            storage.name = item.args[0];
            storage.comment = "";
            storage.type = item.args[1];
            storage.dims.push_back(item.args[2]);
            module.storagenexts[storage.name] = storage;
        }
        matches = _matchMacros(code, "REGISTER_INIT($,$,$)");
        for (const auto& item : matches) {
            VulStorage storage;
            storage.name = item.args[0];
            storage.comment = "";
            storage.type = item.args[1];
            storage.value = item.args[2];
            module.storagenexts[storage.name] = storage;
        }
    }
    {
        // parse wire
        auto matches = _matchMacros(code, "WIRE($,$,$)");
        for (const auto& item : matches) {
            VulStorage wire;
            wire.name = item.args[0];
            wire.comment = "";
            wire.type = item.args[1];
            wire.value = item.args[2];
            module.storagetmp[wire.name] = wire;
        }
    }
    {
        // parse req/serv ports
        auto matches = _matchMacros(code, "REQUEST_PORT()");
        for (const auto& item : matches) {
            VulReqServ req = _parseReqServPort(item.args);
            module.requests[req.name] = req;
        }
        matches = _matchMacros(code, "SERVICE_PORT()");
        for (const auto& item : matches) {
            VulReqServ serv = _parseReqServPort(item.args);
            module.services[serv.name] = serv;
        }
    }
    {
        // parse SERVICE_COND_IMPL
        auto matches = _matchMacros(code, "SERVICE_COND_IMPL()");
        for (const auto& item : matches) {
            if (item.args.size() < 1) {
                std::cerr << "Error: SERVICE_COND_IMPL requires at least one argument (service name)" << std::endl;
                assert(0);
            }
            const std::string& serv_name = item.args[0];
            auto block = _findNextBraceBlock(code, item.pos, '{', '}', true);
            if (block.end_pos.line == -1) {
                std::cerr << "Error: SERVICE_COND_IMPL for service " << serv_name << " has no body" << std::endl;
                assert(0);
            }
            module.serv_cond_codelines[serv_name] = split(block.content, '\n');
        }
    }
    {
        // parse SERVICE_LOGIC_IMPL
        auto matches = _matchMacros(code, "SERVICE_LOGIC_IMPL()");
        for (const auto& item : matches) {
            if (item.args.size() < 1) {
                std::cerr << "Error: SERVICE_LOGIC_IMPL requires at least one argument (service name)" << std::endl;
                assert(0);
            }
            const std::string& serv_name = item.args[0];
            auto block = _findNextBraceBlock(code, item.pos, '{', '}', true);
            if (block.end_pos.line == -1) {
                std::cerr << "Error: SERVICE_LOGIC_IMPL for service " << serv_name << " has no body" << std::endl;
                assert(0);
            }
            module.serv_codelines[serv_name] = split(block.content, '\n');
        }
    }
    const string tick_func_name = "tick_impl";
    {
        // parse TICK_IMPL
        auto matches = _matchMacros(code, "TICK_IMPL()");
        if (!matches.empty()) {
            auto block = _findNextBraceBlock(code, matches[0].pos, '{', '}', true);
            if (block.end_pos.line == -1) {
                std::cerr << "Error: TICK_IMPL has no body" << std::endl;
                assert(0);
            }
            VulTickCodeBlock tick_block;
            tick_block.codelines = split(block.content, '\n');
            tick_block.name = tick_func_name;
            module.user_tick_codeblocks[tick_block.name] = tick_block;
        } else {
            // 没有提供 TICK_IMPL，使用默认空实现
            VulTickCodeBlock tick_block;
            tick_block.codelines = {};
            tick_block.name = tick_func_name;
            module.user_tick_codeblocks[tick_block.name] = tick_block;
        }
    }
    {
        // parse CHILD_INSTANCE(module, instance, param1=value1, param2=value2, ...)
        auto matches = _matchMacros(code, "CHILD_INSTANCE()");
        auto matches2 = _matchMacros(code, "CHILD_INSTANCE_PRIO()");
        std::map<int32_t, vector<InstanceName>> prio_map; // 优先级 -> 实例列表
        for (const auto& item : matches2) {
            if (item.args.size() < 3) {
                std::cerr << "Error: CHILD_INSTANCE_PRIO requires at least three arguments (module name, instance name, priority)" << std::endl;
                assert(0);
            }
            vector<string> remove_arg2;
            remove_arg2.push_back(item.args[0]);
            remove_arg2.push_back(item.args[1]);
            for (size_t i = 3; i < item.args.size(); ++i) {
                remove_arg2.push_back(item.args[i]);
            }
            matches.push_back({item.pos, remove_arg2});
            // 将实例添加到对应优先级的列表中
            int32_t prio = std::stoi(item.args[2]);
            prio_map[prio].push_back(item.args[1]);
        }
        for (const auto& item : matches) {
            if (item.args.size() < 2) {
                std::cerr << "Error: CHILD_INSTANCE requires at least two arguments (module name and instance name)" << std::endl;
                assert(0);
            }
            ModuleName child_module = item.args[0];
            std::string instance_name = item.args[1];
            unordered_map<std::string, std::string> param_values;
            VulInstance instance;
            instance.module_name = child_module;
            instance.name = instance_name;
            instance.comment = "";
            for (size_t i = 2; i < item.args.size(); ++i) {
                const std::string& arg = item.args[i];
                size_t eq_pos = arg.find('=');
                if (eq_pos == std::string::npos) {
                    std::cerr << "Error: Invalid parameter assignment in CHILD_INSTANCE: " << arg << std::endl;
                    assert(0);
                }
                std::string param_name = trim(arg.substr(0, eq_pos));
                std::string param_value = trim(arg.substr(eq_pos + 1));
                instance.local_config_overrides[param_name] = param_value;
            }
            module.instances[instance.name] = instance;
            included_modules.insert(child_module);
        }
        // 根据优先级添加 update_constraints
        // 优先级越大越靠前更新，本模块的tick位于0优先级，负优先级表示后于tick更新，正优先级表示先于tick更新
        prio_map[0].push_back(tick_func_name); // 本模块的时钟也加入优先级0
        vector<InstanceName> sorted_instances; // 按优先级排序的实例列表
        for (const auto& [prio, instances] : prio_map) {
            sorted_instances.insert(sorted_instances.end(), instances.begin(), instances.end());
        }
        // 建立约束：sorted_instances[n] -> sorted_instances[n-1]
        for (size_t i = 1; i < sorted_instances.size(); ++i) {
            VulSequenceConnection conn;
            conn.former_instance = sorted_instances[i];
            conn.latter_instance = sorted_instances[i - 1];
            module.update_constraints.insert(conn);
        }
    }
    {
        // parse CONNECT_CR_CS(srcmod, srcreq, dstmod, dstserv)
        auto matches = _matchMacros(code, "CONNECT_CR_CS($,$,$,$)");
        for (const auto& item : matches) {
            VulReqServConnection conn;
            conn.req_instance = item.args[0];
            conn.req_name = item.args[1];
            conn.serv_instance = item.args[2];
            conn.serv_name = item.args[3];
            module.req_connections[conn.req_instance].insert(conn);
        }
        // parse CONNECT_CR_S(srcmod, srcreq, dstserv)
        matches = _matchMacros(code, "CONNECT_CR_S($,$,$)");
        for (const auto& item : matches) {
            VulReqServConnection conn;
            conn.req_instance = item.args[0];
            conn.req_name = item.args[1];
            conn.serv_instance = module.TopInterface; // 连接到顶层接口
            conn.serv_name = item.args[2];
            module.req_connections[conn.req_instance].insert(conn);
        }
        // parse CONNECT_CR_R(srcmod, srcreq, dstreq)
        matches = _matchMacros(code, "CONNECT_CR_R($,$,$)");
        for (const auto& item : matches) {
            VulReqServConnection conn;
            conn.req_instance = item.args[0];
            conn.req_name = item.args[1];
            conn.serv_instance = module.TopInterface; // 连接到顶层接口
            conn.serv_name = item.args[2];
            module.req_connections[conn.req_instance].insert(conn);
        }
        // parse CONNECT_S_CS(srcserv, dstmod, dstserv)
        matches = _matchMacros(code, "CONNECT_S_CS($,$,$)");
        for (const auto& item : matches) {
            VulReqServConnection conn;
            conn.req_instance = module.TopInterface; // 连接到顶层接口
            conn.req_name = item.args[0];
            conn.serv_instance = item.args[1];
            conn.serv_name = item.args[2];
            module.req_connections[conn.req_instance].insert(conn);
        }

    }

    printf("Parsed module %s\n", name.c_str());

    return module;
}

VulTestHarnessModule _parseTestHarnessModule(const vector<string>& code, const ModuleName& name) {
    VulTestHarnessModule module;
    module.name = name;
    module.comment = "";

    {
        // parse test codelines
        auto matches = _matchMacros(code, "SIMULATION()");
        if (!matches.empty()) {
            auto block = _findNextBraceBlock(code, matches[0].pos, '{', '}', true);
            if (block.end_pos.line == -1) {
                std::cerr << "Error: SIMULATION has no body" << std::endl;
                assert(0);
            }
            module.test_codelines = split(block.content, '\n');
        }
    }
    {
        // parse req/serv ports
        auto matches = _matchMacros(code, "REQUEST_PORT()");
        for (const auto& item : matches) {
            VulReqServ req = _parseReqServPort(item.args);
            module.requests[req.name] = req;
        }
        matches = _matchMacros(code, "SERVICE_PORT()");
        for (const auto& item : matches) {
            VulReqServ serv = _parseReqServPort(item.args);
            module.services[serv.name] = serv;
        }
    }
    {
        // parse SERVICE_COND_IMPL
        auto matches = _matchMacros(code, "SERVICE_COND_IMPL()");
        for (const auto& item : matches) {
            if (item.args.size() < 1) {
                std::cerr << "Error: SERVICE_COND_IMPL requires at least one argument (service name)" << std::endl;
                assert(0);
            }
            const std::string& serv_name = item.args[0];
            auto block = _findNextBraceBlock(code, item.pos, '{', '}', true);
            if (block.end_pos.line == -1) {
                std::cerr << "Error: SERVICE_COND_IMPL for service " << serv_name << " has no body" << std::endl;
                assert(0);
            }
            module.serv_cond_codelines[serv_name] = split(block.content, '\n');
        }
    }
    {
        // parse SERVICE_LOGIC_IMPL
        auto matches = _matchMacros(code, "SERVICE_LOGIC_IMPL()");
        for (const auto& item : matches) {
            if (item.args.size() < 1) {
                std::cerr << "Error: SERVICE_LOGIC_IMPL requires at least one argument (service name)" << std::endl;
                assert(0);
            }
            const std::string& serv_name = item.args[0];
            auto block = _findNextBraceBlock(code, item.pos, '{', '}', true);
            if (block.end_pos.line == -1) {
                std::cerr << "Error: SERVICE_LOGIC_IMPL for service " << serv_name << " has no body" << std::endl;
                assert(0);
            }
            module.serv_codelines[serv_name] = split(block.content, '\n');
        }
    }
    {
        // parse VAR and VAR_INIT
        auto matches = _matchMacros(code, "VAR($,$)");
        for (const auto& item : matches) {
            if (item.args.size() < 2) {
                std::cerr << "Error: VAR requires at least two arguments (type and name)" << std::endl;
                assert(0);
            }
            VulVar var;
            var.type = item.args[0];
            var.name = item.args[1];
            var.value = "";
            module.vars[var.name] = var;
        }

        matches = _matchMacros(code, "VAR_INIT($,$,$)");
        for (const auto& item : matches) {
            if (item.args.size() < 3) {
                std::cerr << "Error: VAR_INIT requires at least three arguments (type, name, and initializer)" << std::endl;
                assert(0);
            }
            VulVar var;
            var.type = item.args[0];
            var.name = item.args[1];
            var.value = item.args[2];
            module.vars[var.name] = var;
        }
    }
    {
        // parse consts and bundles
        auto consts = _parseConsts(code);
        for (const auto& item : consts) {
            VulConfigItem config;
            config.name = item.name;
            config.value = item.value;
            config.comment = item.comment;
            module.local_consts[config.name] = config;
        }
        auto bundles = _parseBundles(code);
        for (const auto& item : bundles) {
            module.local_bundles[item.name] = item;
        }
    }
    {
        // parse PARAMETER(name, value)
        auto matches = _matchMacros(code, "PARAMETER($,$)");
        for (const auto& item : matches) {
            if (item.args.size() < 2) {
                std::cerr << "Error: PARAMETER requires at least two arguments (name and value)" << std::endl;
                assert(0);
            }
            module.top_config_overrides[item.args[0]] = item.args[1];
        }
    }

    printf("Parsed test harness module %s\n", name.c_str());

    return module;
}

VulProject parseVcppProject(
    const string &dirpath,
    const string &top_file,
    const string &main_file
) {
    VulProject project;

    using namespace std::filesystem;

    path proj_dir(dirpath);
    if (!exists(proj_dir) || !is_directory(proj_dir)) {
        std::cerr << "Error: Project directory does not exist: " << dirpath << std::endl;
        assert(0);
    }

    string dir_name = proj_dir.filename().string();
    project.name = dir_name;
    project.dirpath = absolute(proj_dir).string();

    path top_path = proj_dir / top_file;
    if (!exists(top_path) || !is_regular_file(top_path)) {
        std::cerr << "Error: Top file does not exist: " << top_path << std::endl;
        assert(0);
    }
    project.top_module = top_path.stem().string();

    // find header.h or header.hpp for configs and bundles
    path header_path;
    bool header_found = true;
    if (exists(proj_dir / "header.h")) {
        header_path = proj_dir / "header.h";
    } else if (exists(proj_dir / "header.hpp")) {
        header_path = proj_dir / "header.hpp";
    } else {
        header_found = false;
    }

    if (header_found) {
        auto header_lines = _readFileLines(header_path.string());
        auto header_strip = _stripComments(header_lines);
        _parseHeader(header_strip.lines, project.configlib, project.bundlelib);
    }

    // parse modules
    unordered_set<ModuleName> processed_modules;
    std::queue<ModuleName> module_queue;
    module_queue.push(project.top_module);
    while (!module_queue.empty()) {
        ModuleName current = module_queue.front();
        module_queue.pop();
        if (processed_modules.count(current)) {
            continue;
        }
        processed_modules.insert(current);

        string target_file = current + ".hpp";
        // find this file in project directory recursively
        path module_path;
        bool module_found = false;
        for (auto& p : recursive_directory_iterator(proj_dir)) {
            if (p.is_regular_file() && p.path().filename() == target_file) {
                module_path = p.path();
                module_found = true;
                break;
            }
        }
        if (!module_found) {
            std::cerr << "Error: Module file not found for module " << current << ": expected " << target_file << std::endl;
            assert(0);
        }

        printf("Parsing module %s: %s\n", current.c_str(), module_path.string().c_str());

        auto lines = _readFileLines(module_path.string());
        auto striped = _stripComments(lines);
        unordered_set<ModuleName> included_modules;
        VulModule module = _parseModule(striped.lines, current, included_modules);
        project.modulelib->modules[current] = std::make_shared<VulModule>(std::move(module));
        for (const auto& mod : included_modules) {
            if (!processed_modules.count(mod)) {
                module_queue.push(mod);
            }
        }
    }

    if (!main_file.empty()) {
        path main_path = proj_dir / main_file;
        if (!exists(main_path) || !is_regular_file(main_path)) {
            std::cerr << "Error: Main file does not exist: " << main_path << std::endl;
            assert(0);
        }
        auto main_lines = _readFileLines(main_path.string());
        auto main_strip = _stripComments(main_lines);
        string test_module_name = main_path.stem().string();
        if (test_module_name == project.top_module) {
            std::cerr << "Error: Main file name must be different from top module name to avoid conflict: " << test_module_name << std::endl;
            assert(0);
        }
        VulTestHarnessModule test_module = _parseTestHarnessModule(main_strip.lines, test_module_name);
        project.test_harness[test_module.name] = std::move(test_module);
        project.test_module = test_module_name;
    }

    return project;
}




