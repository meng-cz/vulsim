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
#include "cppparse.hpp"

using namespace cppparse;

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <assert.h>
#include <optional>

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

    auto matches = matchMacros(code, "CONST($,$)");
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

    auto matches = matchMacros(code, "ALIAS($,$)");
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

    matches = matchMacros(code, "ALIAS_ARRAY1($,$,$)");
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

    matches = matchMacros(code, "ALIAS_ARRAY2($,$,$,$)");
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

    matches = matchMacros(code, "STRUCT($)");
    for (const auto& args : matches) {
        const std::string& name = args.args[0];
        auto block = findNextBraceBlock(code, args.pos, '{', '}');
        if (block.end_pos.line == -1) {
            throw VulException("STRUCT " + name + " has no body");
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

void _parseReqServArgs(const vector<string> & macro_args, const uint64_t begin_idx, VulReqServ & reqserv) {
    for (size_t i = begin_idx; i < macro_args.size(); ++i) {
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
        auto matches = matchMacros(code, "PARAMETER($,$)");
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
        auto matches = matchMacros(code, "REGISTER($,$)");
        for (const auto& item : matches) {
            VulStorage storage;
            storage.name = item.args[0];
            storage.comment = "";
            storage.type = item.args[1];
            module.storagenexts[storage.name] = storage;
            BlockResult block = findNextBraceBlock(code, item.pos, '{', '}');
            if (block.end_pos.line != -1) {
                module.storage_reset_codelines[storage.name] = split(block.content, '\n');
            }
        }
        matches = matchMacros(code, "REGISTER_ARRAY1($,$,$,$)");
        for (const auto& item : matches) {
            VulStorage storage;
            storage.name = item.args[0];
            storage.comment = "";
            storage.type = item.args[1];
            storage.dims.push_back(item.args[2]);
            uint32_t mul = std::stoul(item.args[3]);
            module.storagenexts[storage.name] = storage;
            if (mul > 1) {
                module.storagenext_ports[storage.name] = mul;
            }
            BlockResult block = findNextBraceBlock(code, item.pos, '{', '}');
            if (block.end_pos.line != -1) {
                module.storage_reset_codelines[storage.name] = split(block.content, '\n');
            }
        }
        // matches = matchMacros(code, "REGISTER_INIT($,$,$)");
        // for (const auto& item : matches) {
        //     VulStorage storage;
        //     storage.name = item.args[0];
        //     storage.comment = "";
        //     storage.type = item.args[1];
        //     storage.value = item.args[2];
        //     module.storagenexts[storage.name] = storage;
        // }
        matches = matchMacros(code, "REGISTER_MUL($,$,$)");
        for (const auto& item : matches) {
            VulStorage storage;
            storage.name = item.args[0];
            storage.comment = "";
            storage.type = item.args[1];
            uint32_t mul = std::stoul(item.args[2]);
            module.storagenexts[storage.name] = storage;
            if (mul > 1) {
                module.storagenext_ports[storage.name] = mul;
            }
            BlockResult block = findNextBraceBlock(code, item.pos, '{', '}');
            if (block.end_pos.line != -1) {
                module.storage_reset_codelines[storage.name] = split(block.content, '\n');
            }
        }
    }
    {
        // parse wire
        auto matches = matchMacros(code, "WIRE($,$,$)");
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
        auto matches = matchMacros(code, "REQUEST_PORT()");
        for (const auto& item : matches) {
            VulReqServ req = _parseReqServPort(item.args);
            module.requests[req.name] = req;
        }
        matches = matchMacros(code, "SERVICE_PORT()");
        for (const auto& item : matches) {
            VulReqServ serv = _parseReqServPort(item.args);
            module.services[serv.name] = serv;
        }
    }
    {
        // parse SERVICE_COND_IMPL
        auto matches = matchMacros(code, "SERVICE_COND_IMPL()");
        for (const auto& item : matches) {
            if (item.args.size() < 1) {
                std::cerr << "Error: SERVICE_COND_IMPL requires at least one argument (service name)" << std::endl;
                assert(0);
            }
            const std::string& serv_name = item.args[0];
            auto block = findNextBraceBlock(code, item.pos, '{', '}', true);
            if (block.end_pos.line == -1) {
                std::cerr << "Error: SERVICE_COND_IMPL for service " << serv_name << " has no body" << std::endl;
                assert(0);
            }
            module.serv_cond_codelines[serv_name] = split(block.content, '\n');
        }
    }
    {
        // parse SERVICE_LOGIC_IMPL
        auto matches = matchMacros(code, "SERVICE_LOGIC_IMPL()");
        for (const auto& item : matches) {
            if (item.args.size() < 1) {
                std::cerr << "Error: SERVICE_LOGIC_IMPL requires at least one argument (service name)" << std::endl;
                assert(0);
            }
            const std::string& serv_name = item.args[0];
            auto block = findNextBraceBlock(code, item.pos, '{', '}', true);
            if (block.end_pos.line == -1) {
                std::cerr << "Error: SERVICE_LOGIC_IMPL for service " << serv_name << " has no body" << std::endl;
                assert(0);
            }
            module.serv_codelines[serv_name] = split(block.content, '\n');
        }
    }
    const string tick_func_name = "tick_impl";
    {
        uint32_t tick_impl_count = 0;
        // parse TICK_IMPL
        auto matches = matchMacros(code, "TICK_IMPL()");
        if (!matches.empty()) {
            for (const auto & match : matches) {
                auto block = findNextBraceBlock(code, match.pos, '{', '}', true);
                if (block.end_pos.line == -1) {
                    std::cerr << "Error: TICK_IMPL has no body" << std::endl;
                    assert(0);
                }
                VulTickCodeBlock tick_block;
                tick_block.codelines = split(block.content, '\n');
                tick_block.name = tick_func_name + std::to_string(tick_impl_count++);
                module.user_tick_codeblocks[tick_block.name] = tick_block;
            }
        } else {
            // 没有提供 TICK_IMPL，使用默认空实现
            VulTickCodeBlock tick_block;
            tick_block.codelines = {};
            tick_block.name = tick_func_name + std::to_string(tick_impl_count++);
            module.user_tick_codeblocks[tick_block.name] = tick_block;
        }
    }
    {
        // parse CHILD_INSTANCE(module, instance, param1=value1, param2=value2, ...)
        auto matches = matchMacros(code, "CHILD_INSTANCE()");
        auto matches2 = matchMacros(code, "CHILD_INSTANCE_PRIO()");
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
        auto matches = matchMacros(code, "CONNECT_CR_CS($,$,$,$)");
        for (const auto& item : matches) {
            VulReqServConnection conn;
            conn.req_instance = item.args[0];
            conn.req_name = item.args[1];
            conn.serv_instance = item.args[2];
            conn.serv_name = item.args[3];
            module.req_connections[conn.req_instance].insert(conn);
        }
        // parse CONNECT_CR_S(srcmod, srcreq, dstserv)
        matches = matchMacros(code, "CONNECT_CR_S($,$,$)");
        for (const auto& item : matches) {
            VulReqServConnection conn;
            conn.req_instance = item.args[0];
            conn.req_name = item.args[1];
            conn.serv_instance = module.TopInterface; // 连接到顶层接口
            conn.serv_name = item.args[2];
            module.req_connections[conn.req_instance].insert(conn);
        }
        // parse CONNECT_CR_R(srcmod, srcreq, dstreq)
        matches = matchMacros(code, "CONNECT_CR_R($,$,$)");
        for (const auto& item : matches) {
            VulReqServConnection conn;
            conn.req_instance = item.args[0];
            conn.req_name = item.args[1];
            conn.serv_instance = module.TopInterface; // 连接到顶层接口
            conn.serv_name = item.args[2];
            module.req_connections[conn.req_instance].insert(conn);
        }
        // parse CONNECT_S_CS(srcserv, dstmod, dstserv)
        matches = matchMacros(code, "CONNECT_S_CS($,$,$)");
        for (const auto& item : matches) {
            VulReqServConnection conn;
            conn.req_instance = module.TopInterface; // 连接到顶层接口
            conn.req_name = item.args[0];
            conn.serv_instance = item.args[1];
            conn.serv_name = item.args[2];
            module.req_connections[conn.req_instance].insert(conn);
        }

    }
    {
        // parse BRAM
        auto matches = matchMacros(code, "BRAM($,$,$,$,$)");
        for (const auto& item : matches) {
            VulBlockRAM bram;
            bram.name = item.args[0];
            bram.data_width = item.args[1];
            bram.addr_width = item.args[2];
            bram.read_ports = item.args[3];
            bram.write_ports = item.args[4];
            bram.init_path = "";
            bram.init_hex = false;
            module.bram_instances[bram.name] = bram;
        }
        matches = matchMacros(code, "BRAM_INIT_H($,$,$,$,$,$)");
        for (const auto& item : matches) {
            VulBlockRAM bram;
            bram.name = item.args[0];
            bram.data_width = item.args[1];
            bram.addr_width = item.args[2];
            bram.read_ports = item.args[3];
            bram.write_ports = item.args[4];
            bram.init_path = item.args[5];
            bram.init_hex = true;
            module.bram_instances[bram.name] = bram;
        }
        matches = matchMacros(code, "BRAM_INIT_B($,$,$,$,$,$)");
        for (const auto& item : matches) {
            VulBlockRAM bram;
            bram.name = item.args[0];
            bram.data_width = item.args[1];
            bram.addr_width = item.args[2];
            bram.read_ports = item.args[3];
            bram.write_ports = item.args[4];
            bram.init_path = item.args[5];
            bram.init_hex = false;
            module.bram_instances[bram.name] = bram;
        }
        matches = matchMacros(code, "BRAM_1RW($,$,$)");
        for (const auto& item : matches) {
            VulBlockRAM bram;
            bram.name = item.args[0];
            bram.data_width = item.args[1];
            bram.addr_width = item.args[2];
            bram.read_ports = "";
            bram.write_ports = "";
            bram.init_path = "";
            bram.init_hex = false;
            module.bram_instances[bram.name] = bram;
        }
        matches = matchMacros(code, "BRAM_1RW_INIT_H($,$,$,$)");
        for (const auto& item : matches) {
            VulBlockRAM bram;
            bram.name = item.args[0];
            bram.data_width = item.args[1];
            bram.addr_width = item.args[2];
            bram.read_ports = "";
            bram.write_ports = "";
            bram.init_path = item.args[3];
            bram.init_hex = true;
            module.bram_instances[bram.name] = bram;
        }
        matches = matchMacros(code, "BRAM_1RW_INIT_B($,$,$,$)");
        for (const auto& item : matches) {
            VulBlockRAM bram;
            bram.name = item.args[0];
            bram.data_width = item.args[1];
            bram.addr_width = item.args[2];
            bram.read_ports = "";
            bram.write_ports = "";
            bram.init_path = item.args[3];
            bram.init_hex = false;
            module.bram_instances[bram.name] = bram;
        }
    }

    printf("Parsed module %s\n", name.c_str());

    return module;
}

vector<string> extractIncludes(const vector<string>& code) {
    vector<string> result;

    for (const auto& line : code) {
        size_t i = 0;
        size_t n = line.size();

        // 1. 跳过行首空白
        while (i < n && isspace(line[i])) i++;

        // 2. 必须以 # 开头
        if (i >= n || line[i] != '#') continue;
        i++;

        // 3. 跳过 # 后空白
        while (i < n && isspace(line[i])) i++;

        // 4. 匹配 include
        const string keyword = "include";
        if (i + keyword.size() > n) continue;

        bool match = true;
        for (size_t k = 0; k < keyword.size(); k++) {
            if (line[i + k] != keyword[k]) {
                match = false;
                break;
            }
        }
        if (!match) continue;

        i += keyword.size();

        // 5. 跳过 include 后空白
        while (i < n && isspace(line[i])) i++;

        if (i >= n) continue;

        // 6. 解析 <...> 或 "..."
        if (line[i] == '<') {
            i++;
            size_t start = i;

            while (i < n && line[i] != '>') i++;

            if (i < n) {
                result.emplace_back(line.substr(start, i - start));
            }
        } else if (line[i] == '"') {
            i++;
            size_t start = i;

            while (i < n && line[i] != '"') i++;

            if (i < n) {
                result.emplace_back(line.substr(start, i - start));
            }
        }
    }

    return result;
}

VulTestHarnessModule _parseTestHarnessModule(const vector<string>& code, const ModuleName& name) {
    VulTestHarnessModule module;
    module.name = name;
    module.comment = "";

    {
        // parse test codelines
        auto matches = matchMacros(code, "SIMULATION()");
        if (!matches.empty()) {
            auto block = findNextBraceBlock(code, matches[0].pos, '{', '}', true);
            if (block.end_pos.line == -1) {
                std::cerr << "Error: SIMULATION has no body" << std::endl;
                assert(0);
            }
            module.test_codelines = split(block.content, '\n');
        }
    }
    {
        // parse req/serv ports
        auto matches = matchMacros(code, "REQUEST_PORT()");
        for (const auto& item : matches) {
            VulReqServ req = _parseReqServPort(item.args);
            module.requests[req.name] = req;
        }
        matches = matchMacros(code, "SERVICE_PORT()");
        for (const auto& item : matches) {
            VulReqServ serv = _parseReqServPort(item.args);
            module.services[serv.name] = serv;
        }
    }
    {
        // parse SERVICE_COND_IMPL
        auto matches = matchMacros(code, "SERVICE_COND_IMPL()");
        for (const auto& item : matches) {
            if (item.args.size() < 1) {
                std::cerr << "Error: SERVICE_COND_IMPL requires at least one argument (service name)" << std::endl;
                assert(0);
            }
            const std::string& serv_name = item.args[0];
            auto block = findNextBraceBlock(code, item.pos, '{', '}', true);
            if (block.end_pos.line == -1) {
                std::cerr << "Error: SERVICE_COND_IMPL for service " << serv_name << " has no body" << std::endl;
                assert(0);
            }
            module.serv_cond_codelines[serv_name] = split(block.content, '\n');
        }
    }
    {
        // parse SERVICE_LOGIC_IMPL
        auto matches = matchMacros(code, "SERVICE_LOGIC_IMPL()");
        for (const auto& item : matches) {
            if (item.args.size() < 1) {
                std::cerr << "Error: SERVICE_LOGIC_IMPL requires at least one argument (service name)" << std::endl;
                assert(0);
            }
            const std::string& serv_name = item.args[0];
            auto block = findNextBraceBlock(code, item.pos, '{', '}', true);
            if (block.end_pos.line == -1) {
                std::cerr << "Error: SERVICE_LOGIC_IMPL for service " << serv_name << " has no body" << std::endl;
                assert(0);
            }
            module.serv_codelines[serv_name] = split(block.content, '\n');
        }
    }
    {
        // parse VAR and VAR_INIT
        auto matches = matchMacros(code, "VAR($,$)");
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

        matches = matchMacros(code, "VAR_INIT($,$,$)");
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
        auto matches = matchMacros(code, "PARAMETER($,$)");
        for (const auto& item : matches) {
            if (item.args.size() < 2) {
                std::cerr << "Error: PARAMETER requires at least two arguments (name and value)" << std::endl;
                assert(0);
            }
            module.top_config_overrides[item.args[0]] = item.args[1];
        }
    }
    {
        // parse #include "xxx"
        auto includes = extractIncludes(code);
        // exclude internal includes (defhelper.hpp, header.hpp, testheader.hpp, run.hpp)
        includes.erase(std::remove_if(includes.begin(), includes.end(), [](const std::string& s) {
            return s == "defhelper.hpp" || s == "header.hpp" || s == "testheader.hpp" || s == "run.hpp";
        }), includes.end());
        module.includedHeaders = includes;
    }
    {
        // parse GLOBAL() { ...}
        auto matches = matchMacros(code, "GLOBAL() {");
        for (const auto& item : matches) {
            auto block = findNextBraceBlock(code, item.pos, '{', '}', true);
            if (block.end_pos.line == -1) {
                std::cerr << "Error: GLOBAL() has no body" << std::endl;
                assert(0);
            }
            auto codelines = split(block.content, '\n');
            module.globalCodes.insert(module.globalCodes.end(), codelines.begin(), codelines.end());
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
        auto header_lines = readFileLines(header_path.string());
        auto header_strip = stripComments(header_lines);
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

        auto lines = readFileLines(module_path.string());
        auto striped = stripComments(lines);
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
        auto main_lines = readFileLines(main_path.string());
        auto main_strip = stripComments(main_lines);
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

inline void parse_reg_uint_type(const string& type_str, const VulStaticConfigLib &configlib, VulStaticTypeSignature &signature) {
    // parse UInt<N> to extract N
    const string prefix = "UInt<";
    const string suffix = ">";
    if (type_str.substr(0, prefix.size()) == prefix && type_str.size() > prefix.size() + suffix.size() && type_str.substr(type_str.size() - suffix.size()) == suffix) {
        string num_str = type_str.substr(prefix.size(), type_str.size() - prefix.size() - suffix.size());
        ConfigRealValue value = calculateConstexprValue(num_str, configlib);
        signature.type = "UInt";
        signature.uint_length = value;
    } else {
        signature.type = type_str;
        signature.uint_length = 0;
    }
};

void _parseReqServArgs(
    const vector<string> & macro_args,
    const uint64_t begin_idx,
    const VulStaticConfigLib &configlib,
    VulStaticReqServ & reqserv
) {
    for (size_t i = begin_idx; i < macro_args.size(); ++i) {
        VulStaticArg arg;
        vector<string> parts = split(macro_args[i], ' ');
        if (parts.size() != 2) {
            throw VulException("Invalid argument format in REQ_PORT/SERV_PORT: " + macro_args[i]);
        }
        arg.name = parts[1];
        // parse ARG(type) or RESP(type) for parts[0]
        string type_str = trim(parts[0]);
        if (type_str.substr(0, 4) == "ARG(" && type_str.back() == ')') {
            parse_reg_uint_type(type_str.substr(4, type_str.size() - 5), configlib, arg.type);
            reqserv.args.push_back(arg);
        } else if (type_str.substr(0, 5) == "RESP(" && type_str.back() == ')') {
            parse_reg_uint_type(type_str.substr(5, type_str.size() - 6), configlib, arg.type);
            reqserv.rets.push_back(arg);
        } else {
            throw VulException("Invalid argument type format in REQ_PORT/SERV_PORT: " + parts[0]);
        }
    }
}

void _parseStaticModule(
    const std::vector<std::string>& code,
    const VulStaticConfigLib &configlib,
    const VulStaticConfigLib &param_overrides,
    VulStaticModuleInstance &module
) {
    VulStaticConfigLib local_configlib = configlib; // copy global config lib to local, will add local consts to it

    {
        // parse params
        VulErrorContextGuard _err{"parsing parameters"};
        auto matches = matchMacros(code, "PARAMETER($,$)");
        for (const auto& item : matches) {
            VulErrorContextGuard _err{item.args[0] + " = " + item.args[1]};
            ConfigRealValue value = 0;
            const string &param_name = item.args[0];
            const string &param_value_str = item.args[1];
            auto iter = param_overrides.find(param_name);
            if (iter != param_overrides.end()) {
                // override parameter value
                value = iter->second;
            } else {
                value = calculateConstexprValue(param_value_str, configlib);
            }
            module.local_parameters[param_name] = value;
        }
    }
    {
        // parse consts
        VulErrorContextGuard _err{"parsing consts"};
        auto consts = _parseConsts(code);
        for (const auto& item : consts) {
            VulErrorContextGuard _err{item.name + " = " + item.value};
            ConfigRealValue value = calculateConstexprValue(item.value, configlib);
            local_configlib[item.name] = value;
            module.local_consts[item.name] = value;
        }
    }
    {
        // parse bundles
        VulErrorContextGuard _err{"parsing struct"};
        auto bundles = _parseBundles(code);
        for (const auto& item : bundles) {
            VulErrorContextGuard _err{"parsing struct " + item.name};
            VulStaticBundle static_bundle = staticalizeBundle(item, local_configlib);
            module.local_bundles.push_back(std::move(static_bundle));
        }
    }
    
    {
        // parse register
        VulErrorContextGuard _err{"parsing registers"};
        auto matches = matchMacros(code, "REGISTER($,$)");
        for (const auto& item : matches) {
            VulErrorContextGuard _err{item.args[1] + " " + item.args[0]};
            VulStaticRegister reg;
            reg.name = item.args[0];
            parse_reg_uint_type(item.args[1], local_configlib, reg.signature);
            reg.ports = 1;
            BlockResult block = findNextBraceBlock(code, item.pos, '{', '}');
            if (block.end_pos.line != -1) {
                reg.reset_codelines = split(block.content, '\n');
            }
            module.registers.push_back(std::move(reg));
        }
        matches = matchMacros(code, "REGISTER_ARRAY1($,$,$,$)");
        for (const auto& item : matches) {
            VulErrorContextGuard _err{item.args[1] + "<" + item.args[3] + ">" + " " + item.args[0] + "[" + item.args[2] + "]"};
            VulStaticRegister reg;
            reg.name = item.args[0];
            parse_reg_uint_type(item.args[1], local_configlib, reg.signature);
            ConfigRealValue array_size = calculateConstexprValue(item.args[2], local_configlib);
            reg.dims.push_back(array_size);
            array_size = calculateConstexprValue(item.args[3], local_configlib);
            reg.ports = (array_size > 1) ? array_size : 1;
            BlockResult block = findNextBraceBlock(code, item.pos, '{', '}');
            if (block.end_pos.line != -1) {
                reg.reset_codelines = split(block.content, '\n');
            }
            module.registers.push_back(std::move(reg));
        }
        matches = matchMacros(code, "REGISTER_MUL($,$,$)");
        for (const auto& item : matches) {
            VulErrorContextGuard _err{item.args[1] + "<" + item.args[2] + ">" + " " + item.args[0]};
            VulStaticRegister reg;
            reg.name = item.args[0];
            parse_reg_uint_type(item.args[1], local_configlib, reg.signature);
            ConfigRealValue portnum = calculateConstexprValue(item.args[2], local_configlib);
            reg.ports = (portnum > 1) ? portnum : 1;
            BlockResult block = findNextBraceBlock(code, item.pos, '{', '}');
            if (block.end_pos.line != -1) {
                reg.reset_codelines = split(block.content, '\n');
            }
            module.registers.push_back(std::move(reg));
        }
    }
    {
        // parse wire
        VulErrorContextGuard _err{"parsing wires"};
        auto matches = matchMacros(code, "WIRE($,$)");
        for (const auto& item : matches) {
            VulErrorContextGuard _err{item.args[1] + " " + item.args[0]};
            VulStaticWire reg;
            reg.name = item.args[0];
            parse_reg_uint_type(item.args[1], local_configlib, reg.signature);
            BlockResult block = findNextBraceBlock(code, item.pos, '{', '}');
            if (block.end_pos.line != -1) {
                reg.reset_codelines = split(block.content, '\n');
            }
            module.wires.push_back(std::move(reg));
        }
    }
    {
        // parse req
        VulErrorContextGuard _err{"parsing requests"};
        auto matches = matchMacros(code, "REQUEST()");
        for (const auto& item : matches) {
            VulErrorContextGuard _err{item.args[0]};
            VulStaticReqServ req;
            req.name = item.args[0];
            req.has_handshake = false;
            _parseReqServArgs(item.args, 1, configlib, req);
            module.requests[req.name] = req;
        }
        matches = matchMacros(code, "REQUEST_READY()");
        for (const auto& item : matches) {
            VulErrorContextGuard _err{item.args[0]};
            VulStaticReqServ req;
            req.name = item.args[0];
            req.has_handshake = true;
            _parseReqServArgs(item.args, 1, configlib, req);
            module.requests[req.name] = req;
        }
    }
    {
        // parse serv
        VulErrorContextGuard _err{"parsing services"};
        vector<MatchMacroResult> unified_matches;
        auto matches = matchMacros(code, "SERVICE()");
        for (const auto& item : matches) {
            vector<string> args;
            args.push_back(item.args[0]);
            args.push_back(""); // condition = None
            args.push_back(""); // priority = None
            args.insert(args.end(), item.args.begin() + 1, item.args.end());
            unified_matches.push_back(MatchMacroResult{item.pos, args});
        }
        matches = matchMacros(code, "SERVICE_READY()");
        for (const auto& item : matches) {
            vector<string> args;
            args.push_back(item.args[0]);
            args.push_back(item.args[1]);
            args.push_back(""); // priority = None
            args.insert(args.end(), item.args.begin() + 2, item.args.end());
            unified_matches.push_back(MatchMacroResult{item.pos, args});
        }
        matches = matchMacros(code, "SERVICE_PRIO()");
        for (const auto& item : matches) {
            vector<string> args;
            args.push_back(item.args[0]);
            args.push_back(""); // condition = None
            args.push_back(item.args[1]);
            args.insert(args.end(), item.args.begin() + 2, item.args.end());
            unified_matches.push_back(MatchMacroResult{item.pos, args});
        }
        matches = matchMacros(code, "SERVICE_PRIO_READY()");
        for (const auto& item : matches) {
            vector<string> args;
            args.push_back(item.args[0]);
            args.push_back(item.args[2]);
            args.push_back(item.args[1]);
            args.insert(args.end(), item.args.begin() + 3, item.args.end());
            unified_matches.push_back(MatchMacroResult{item.pos, args});
        }
        for (const auto& item : unified_matches) {
            VulErrorContextGuard _err{item.args[0]};
            VulStaticReqServ serv;
            serv.name = item.args[0];
            serv.has_handshake = (item.args[1] != "");
            _parseReqServArgs(item.args, 3, configlib, serv);
            module.services[serv.name] = serv;
            VulLogicBlock lb;
            lb.block_id = module.serv_logic_blocks.size() + 1; // 按照出现顺序分配 block_id
            lb.with_priority = (item.args[2] != "");
            lb.priority = lb.with_priority ? std::stoi(item.args[2]) : 0;
            if (serv.has_handshake) {
                lb.cond_codelines.push_back("return (" + item.args[1] + ");\n");
            }
            auto block = findNextBraceBlock(code, item.pos, '{', '}', true);
            if (block.end_pos.line == -1) {
                throw VulException("SERVICE_IMPL for service " + serv.name + " has no body");
            }
            lb.codelines = split(block.content, '\n');
            module.serv_logic_blocks[serv.name] = lb;
        }
    }
    const string tick_func_name = "tick";
    {
        uint32_t tick_impl_count = 0;
        VulErrorContextGuard _err{"parsing tick implementation"};
        // parse TICK_IMPL
        auto matches = matchMacros(code, "TICK_IMPL()");
        if (!matches.empty()) {
            for (const auto & match : matches) {
                auto block = findNextBraceBlock(code, match.pos, '{', '}', true);
                if (block.end_pos.line == -1) {
                    throw VulException("TICK_IMPL has no body");
                }
                VulTickBlock tick_block;
                tick_block.codelines = split(block.content, '\n');
                module.tick_blocks.push_back(std::move(tick_block));
            }
        } else {
            // 没有提供 TICK_IMPL，使用默认空实现
            VulTickBlock tick_block;
            tick_block.codelines = {};
            module.tick_blocks.push_back(std::move(tick_block));
        }
    }
    {
        // parse CHILD_INSTANCE(module, instance, param1=value1, param2=value2, ...)
        VulErrorContextGuard _err{"parsing child instances"};
        auto matches = matchMacros(code, "CHILD_INSTANCE()");
        for (const auto& item : matches) {
            if (item.args.size() < 2) {
                throw VulException("CHILD_INSTANCE requires at least two arguments (module name and instance name), at line " + std::to_string(item.pos.line));
            }
            VulErrorContextGuard _err{item.args[1]};
            ModuleName child_module = item.args[0];
            std::string instance_name = item.args[1];
            unordered_map<std::string, std::string> param_values;
            for (size_t i = 2; i < item.args.size(); ++i) {
                const std::string& arg = item.args[i];
                size_t eq_pos = arg.find('=');
                if (eq_pos == std::string::npos) {
                    throw VulException("Invalid parameter assignment in CHILD_INSTANCE: " + arg + ", at line " + std::to_string(item.pos.line));
                }
                std::string param_name = trim(arg.substr(0, eq_pos));
                std::string param_value = trim(arg.substr(eq_pos + 1));
                param_values[param_name] = param_value;
            }

            VulStaticInstanceDecl instance;
            instance.name = instance_name;
            instance.module_name = child_module;
            for (const auto& [param_name, param_value] : param_values) {
                ConfigRealValue value = calculateConstexprValue(param_value, configlib);
                instance.parameter_overrides[param_name] = value;
            }
            module.instances[instance.name] = instance;
        }
        // parse USE_CHILD_SERVICE_PORT(instance, serv, ret, ...)
        matches = matchMacros(code, "USE_CHILD_SERVICE_PORT()");
        for (const auto& item : matches) {
            if (item.args.size() < 2) {
                throw VulException("USE_CHILD_SERVICE_PORT requires at least two arguments (instance name and service name), at line " + std::to_string(item.pos.line));
            }
            std::string instance_name = item.args[0];
            std::string serv_name = item.args[1];
            auto instance_iter = module.instances.find(instance_name);
            if (instance_iter == module.instances.end()) {
                throw VulException("Instance not found for USE_CHILD_SERVICE_PORT: " + instance_name + ", at line " + std::to_string(item.pos.line));
            }
            instance_iter->second.referenced_services.insert(serv_name);
        }
    }
    {
        VulErrorContextGuard _err{"parsing request-service connections"};
        // parse CONNECT_CR_CS(srcmod, srcreq, dstmod, dstserv)
        auto matches = matchMacros(code, "CONNECT_CR_CS($,$,$,$)");
        for (const auto& item : matches) {
            VulReqServConnection conn;
            conn.req_instance = item.args[0];
            conn.req_name = item.args[1];
            conn.serv_instance = item.args[2];
            conn.serv_name = item.args[3];
            module.req_connections.push_back(conn);
        }
        // parse CONNECT_CR_S(srcmod, srcreq, dstserv)
        matches = matchMacros(code, "CONNECT_CR_S($,$,$)");
        for (const auto& item : matches) {
            VulReqServConnection conn;
            conn.req_instance = item.args[0];
            conn.req_name = item.args[1];
            conn.serv_instance = VulModule::TopInterface; // 连接到顶层接口
            conn.serv_name = item.args[2];
            module.req_connections.push_back(conn);
        }
        // parse CONNECT_CR_R(srcmod, srcreq, dstreq)
        matches = matchMacros(code, "CONNECT_CR_R($,$,$)");
        for (const auto& item : matches) {
            VulReqServConnection conn;
            conn.req_instance = item.args[0];
            conn.req_name = item.args[1];
            conn.serv_instance = VulModule::TopInterface; // 连接到顶层接口
            conn.serv_name = item.args[2];
            module.req_connections.push_back(conn);
        }
        // parse CONNECT_S_CS(srcserv, dstmod, dstserv)
        matches = matchMacros(code, "CONNECT_S_CS($,$,$)");
        for (const auto& item : matches) {
            VulReqServConnection conn;
            conn.req_instance = VulModule::TopInterface; // 连接到顶层接口
            conn.req_name = item.args[0];
            conn.serv_instance = item.args[1];
            conn.serv_name = item.args[2];
            module.req_connections.push_back(conn);
        }
    }
    {
        // parse BRAM
        VulErrorContextGuard _err{"parsing BRAMs"};
        auto matches = matchMacros(code, "BRAM($,$,$,$,$)");
        for (const auto& item : matches) {
            VulErrorContextGuard _err{"parsing BRAM " + item.args[0]};
            VulStaticBRAM bram;
            bram.name = item.args[0];
            parse_reg_uint_type(item.args[1], local_configlib, bram.data_type);
            bram.addr_width = calculateConstexprValue(item.args[2], local_configlib);
            bram.read_ports = calculateConstexprValue(item.args[3], local_configlib);
            bram.write_ports = calculateConstexprValue(item.args[4], local_configlib);
            module.brams.push_back(bram);
        }
        matches = matchMacros(code, "BRAM_1RW($,$,$)");
        for (const auto& item : matches) {
            VulErrorContextGuard _err{"parsing BRAM " + item.args[0]};
            VulStaticBRAM bram;
            bram.name = item.args[0];
            parse_reg_uint_type(item.args[1], local_configlib, bram.data_type);
            bram.addr_width = calculateConstexprValue(item.args[2], local_configlib);
            bram.read_ports = 0;
            bram.write_ports = 0;
            module.brams.push_back(bram);
        }
    }
    {
        // parse ROM
        VulErrorContextGuard _err{"parsing ROMs"};
        auto matches = matchMacros(code, "ROM($,$,$,$,$)");
        for (const auto& item : matches) {
            VulErrorContextGuard _err{"parsing ROM " + item.args[0]};
            VulStaticDigitalROM rom;
            rom.name = item.args[0];
            rom.data_width = calculateConstexprValue(item.args[1], local_configlib);
            rom.addr_width = calculateConstexprValue(item.args[2], local_configlib);
            rom.read_ports = calculateConstexprValue(item.args[3], local_configlib);
            rom.init_path = item.args[4];
            module.roms.push_back(rom);
        }
    }
    {
        // parse QUEUEs
        VulErrorContextGuard _err{"parsing queues"};
        auto matches = matchMacros(code, "QUEUE($,$,$)");
        for (const auto& item : matches) {
            VulErrorContextGuard _err{"parsing QUEUE " + item.args[0]};
            VulStaticQueue queue;
            queue.name = item.args[0];
            parse_reg_uint_type(item.args[1], local_configlib, queue.type);
            queue.depth = calculateConstexprValue(item.args[2], local_configlib);
            queue.enq_width = queue.deq_width = 1; // default width = 1
            module.queues.push_back(std::move(queue));
        }
        matches = matchMacros(code, "QUEUE_MP($,$,$,$,$)");
        for (const auto& item : matches) {
            VulErrorContextGuard _err{"parsing QUEUE_MP " + item.args[0]};
            VulStaticQueue queue;
            queue.name = item.args[0];
            parse_reg_uint_type(item.args[1], local_configlib, queue.type);
            queue.depth = calculateConstexprValue(item.args[2], local_configlib);
            queue.enq_width = calculateConstexprValue(item.args[3], local_configlib);
            queue.deq_width = calculateConstexprValue(item.args[4], local_configlib);
            module.queues.push_back(std::move(queue));
        }
    }
}

VulStaticTestHarnessModule _parseStaticTestHarnessModule(
    const std::vector<std::string>& code,
    const VulStaticConfigLib &configlib
) {
    VulStaticTestHarnessModule module;
    VulStaticConfigLib local_configlib = configlib;
    
    {
        // parse test codelines
        VulErrorContextGuard _err{"parsing simulation codelines"};
        auto matches = matchMacros(code, "SIMULATION()");
        if (!matches.empty()) {
            auto block = findNextBraceBlock(code, matches[0].pos, '{', '}', true);
            if (block.end_pos.line == -1) {
                throw VulException("SIMULATION has no body, at line " + std::to_string(matches[0].pos.line));
            }
            module.test_codelines = split(block.content, '\n');
        }
    }
    {
        // parse req
        VulErrorContextGuard _err{"parsing requests"};
        auto matches = matchMacros(code, "REQUEST()");
        for (const auto& item : matches) {
            VulErrorContextGuard _err{item.args[0]};
            VulStaticReqServ req;
            req.name = item.args[0];
            req.has_handshake = false;
            _parseReqServArgs(item.args, 1, configlib, req);
            module.requests[req.name] = req;
        }
        matches = matchMacros(code, "REQUEST_READY()");
        for (const auto& item : matches) {
            VulErrorContextGuard _err{item.args[0]};
            VulStaticReqServ req;
            req.name = item.args[0];
            req.has_handshake = true;
            _parseReqServArgs(item.args, 1, configlib, req);
            module.requests[req.name] = req;
        }
    }
    {
        // parse serv
        VulErrorContextGuard _err{"parsing services"};
        vector<MatchMacroResult> unified_matches;
        auto matches = matchMacros(code, "SERVICE()");
        for (const auto& item : matches) {
            vector<string> args;
            args.push_back(item.args[0]);
            args.push_back(""); // condition = None
            args.push_back(""); // priority = None
            args.insert(args.end(), item.args.begin() + 1, item.args.end());
            unified_matches.push_back(MatchMacroResult{item.pos, args});
        }
        matches = matchMacros(code, "SERVICE_READY()");
        for (const auto& item : matches) {
            vector<string> args;
            args.push_back(item.args[0]);
            args.push_back(item.args[1]);
            args.push_back(""); // priority = None
            args.insert(args.end(), item.args.begin() + 2, item.args.end());
            unified_matches.push_back(MatchMacroResult{item.pos, args});
        }
        for (const auto& item : unified_matches) {
            VulErrorContextGuard _err{item.args[0]};
            VulStaticReqServ serv;
            serv.name = item.args[0];
            serv.has_handshake = (item.args[1] != "");
            _parseReqServArgs(item.args, 3, configlib, serv);
            module.services[serv.name] = serv;
            if (serv.has_handshake) {
                vector<string> cond_lines;
                cond_lines.push_back("return (" + item.args[1] + ");\n");
                module.serv_cond_codelines[serv.name] = cond_lines;
            }
            auto block = findNextBraceBlock(code, item.pos, '{', '}', true);
            if (block.end_pos.line == -1) {
                throw VulException("SERVICE_IMPL for service " + serv.name + " has no body");
            }
            module.serv_codelines[serv.name] = split(block.content, '\n');
        }
    }
    {
        // parse PARAMETER(name, value)
        VulErrorContextGuard _err{"parsing parameters"};
        auto matches = matchMacros(code, "PARAMETER($,$)");
        for (const auto& item : matches) {
            if (item.args.size() < 2) {
                throw VulException("PARAMETER requires at least two arguments (name and value), at line " + std::to_string(item.pos.line));
            }
            VulErrorContextGuard _err{item.args[0] + " = " + item.args[1]};
            const string &param_name = item.args[0];
            const string &param_value_str = item.args[1];
            ConfigRealValue value = calculateConstexprValue(param_value_str, configlib);
            module.top_config_overrides[param_name] = value;
        }
    }
    {
        // parse #include "xxx"
        VulErrorContextGuard _err{"parsing includes"};
        auto includes = extractIncludes(code);
        // exclude internal includes (defhelper.hpp, header.hpp, testheader.hpp, run.hpp)
        includes.erase(std::remove_if(includes.begin(), includes.end(), [](const std::string& s) {
            return s == "defhelper.hpp" || s == "header.hpp" || s == "testheader.hpp" || s == "run.hpp";
        }), includes.end());
        module.includedHeaders = includes;
    }
    {
        // parse GLOBAL() { ...}
        VulErrorContextGuard _err{"parsing global declarations"};
        auto matches = matchMacros(code, "GLOBAL() {");
        for (const auto& item : matches) {
            auto block = findNextBraceBlock(code, item.pos, '{', '}', true);
            if (block.end_pos.line == -1) {
                continue;
            }
            auto codelines = split(block.content, '\n');
            module.globalCodes.insert(module.globalCodes.end(), codelines.begin(), codelines.end());
        }
    }

    return module;
}

VulStaticProject parseVcppStaticProject(
    const string &project_dir,
    const string &top_file_path,
    const string &main_file_path
) {
    VulStaticProject project;

    using namespace std::filesystem;

    bool is_sim = !main_file_path.empty();

    path proj_dir(project_dir);
    if (!exists(proj_dir) || !is_directory(proj_dir)) {
        throw VulException("Project directory does not exist or is not a directory: " + project_dir);
    }

    path header_path = proj_dir / "header.hpp";
    bool header_found = false;
    if (exists(header_path) && is_regular_file(header_path)) {
        header_found = true;
    } else if (exists(proj_dir / "header.h") && is_regular_file(proj_dir / "header.h")) {
        header_path = proj_dir / "header.h";
        header_found = true;
    }
    if (header_found) {
        VulErrorContextGuard _err{"parsing header file " + header_path.string()};
        auto header_lines = readFileLines(header_path.string());
        auto header_strip = stripComments(header_lines);
        auto consts = _parseConsts(header_strip.lines);
        for (const auto& item : consts) {
            VulErrorContextGuard _err{"evaluating constant " + item.name};
            ConfigRealValue value = calculateConstexprValue(item.value, project.global_configlib);
            project.global_configlib[item.name] = value;
        }
        auto bundles = _parseBundles(header_strip.lines);
        for (const auto& item : bundles) {
            VulErrorContextGuard _err{"staticalizing bundle " + item.name};
            VulStaticBundle static_bundle;
            static_bundle = staticalizeBundle(item, project.global_configlib);
            project.global_bundlelib.push_back(std::move(static_bundle));
        }
    }

    if (!main_file_path.empty()) {
        path main_path(main_file_path);
        if (!exists(main_path) || !is_regular_file(main_path)) {
            std::cerr << "Error: Main file does not exist or is not a regular file: " << main_file_path << std::endl;
            assert(0);
        }
        {
            VulErrorContextGuard _err{"reading test harness module from " + main_path.string()};
            auto main_lines = readFileLines(main_path.string());
            auto main_strip = stripComments(main_lines);
            project.test_harness = _parseStaticTestHarnessModule(main_strip.lines, project.global_configlib);
        }
    }

    unordered_map<ModuleName, path> module_file_path_cache;
    auto find_module_file = [&](const ModuleName& mod_name) -> std::optional<path> {
        auto iter = module_file_path_cache.find(mod_name);
        if (iter != module_file_path_cache.end()) {
            return iter->second;
        }
        vector<string> candidates = {
            mod_name + ".hpp",
            mod_name + ".cpp",
            mod_name + ".vul",
            mod_name + ".h"
        };
        for (const auto& candidate : candidates) {
            // find in project directory recursively
            vector<path> found_paths;
            for (auto& p : recursive_directory_iterator(proj_dir)) {
                if (p.is_regular_file() && p.path().filename() == candidate) {
                    found_paths.push_back(p.path());
                }
            }
            if (found_paths.size() > 1) {
                string err_str = "Error: Multiple files found for module " + mod_name + ":";
                for (const auto& p : found_paths) {
                    err_str += "\n  " + p.string();
                }
                throw VulException(err_str);
            } else if (!found_paths.empty()) {
                module_file_path_cache[mod_name] = found_paths.front();
                return found_paths.front();
            }
        }
        return std::nullopt;
    };

    struct InstanceToProcess {
        shared_ptr<VulStaticModuleInstance> ptr;
        VulStaticConfigLib config_overrides;
    };
    std::deque<InstanceToProcess> todo_queue;

    shared_ptr<VulStaticModuleInstance> top_instance = std::make_shared<VulStaticModuleInstance>();
    path top_path(top_file_path);
    if (!exists(top_path) || !is_regular_file(top_path)) {
        throw VulException("Top file does not exist or is not a regular file: " + top_file_path);
    }
    top_instance->module_name = top_path.stem().string();
    if (is_sim) {
        top_instance->instance_path = {"sim", "top"};
    } else {
        top_instance->instance_path = {"top"};
    }
    top_instance->filepath = top_path.string();
    todo_queue.push_back({top_instance, project.test_harness.top_config_overrides});

    uint32_t module_count = 0;

    while (!todo_queue.empty()) {
        auto [instance_ptr, config_overrides] = todo_queue.front();
        todo_queue.pop_front();

        VulErrorContextGuard _err{"parsing module instance " + instance_ptr->simClassName()};

        auto mod_file_opt = find_module_file(instance_ptr->module_name);
        if (!mod_file_opt.has_value()) {
            throw VulException("Module file not found for module: " + instance_ptr->module_name);
        }

        VulErrorContextGuard _err_file{"entering module file " + mod_file_opt.value().string()};

        path mod_path = mod_file_opt.value();
        instance_ptr->filepath = mod_path.string();
        auto mod_lines = readFileLines(mod_path.string());
        auto mod_strip = stripComments(mod_lines);
        _parseStaticModule(mod_strip.lines, project.global_configlib, config_overrides, *instance_ptr);
        detectRequestCallInLogicBlocks(*instance_ptr);

        instance_ptr->instance_id = module_count++;

        // process child instances
        for (const auto& [child_name, child_instance_decl] : instance_ptr->instances) {
            shared_ptr<VulStaticModuleInstance> child_instance = std::make_shared<VulStaticModuleInstance>();
            child_instance->instance_path = instance_ptr->instance_path;
            child_instance->instance_path.push_back(child_name);
            child_instance->module_name = child_instance_decl.module_name;
            child_instance->exported_services = child_instance_decl.referenced_services;
            child_instance->parent = instance_ptr;
            instance_ptr->children.push_back(child_instance);
            todo_queue.push_back({child_instance, child_instance_decl.parameter_overrides});
        }
    }
    project.top_module_instance = top_instance;


    printf("Successfully parsed project. Summary:\n");
    printf("Parse %ld modules:\n", module_file_path_cache.size());
    for (const auto& [mod_name, mod_path] : module_file_path_cache) {
        printf("  [%s]: %s\n", mod_name.c_str(), mod_path.string().c_str());
    }
    printf("Instance hierarchy:\n");
    vector<pair<shared_ptr<VulStaticModuleInstance>, size_t>> dfs_stack;
    dfs_stack.reserve(64);
    dfs_stack.push_back({top_instance, 1});
    while (!dfs_stack.empty()) {
        auto [node, depth] = dfs_stack.back();
        dfs_stack.pop_back();

        for (size_t i = 0; i < depth; ++i) {
            putchar(' ');
        }
        std::string instance_path_str;
        for (const auto& name : node->instance_path) {
            if (!instance_path_str.empty()) {
                instance_path_str += "::";
            }
            instance_path_str += name;
        }
        printf("%s [%s]\n", instance_path_str.c_str(), node->module_name.c_str());

        for (size_t i = node->children.size(); i > 0; --i) {
            dfs_stack.push_back({node->children[i - 1], depth + 1});
        }
    }
    printf("Total instances: %d\n", module_count);
    
    if (!is_sim) {
        printf("No main file provided, skipping simulation setup.\n");
        return project;
    }

    printf("Setting up simulation hierarchy and update sequence...\n");

    shared_ptr<VulStaticModuleInstance> fake_main = std::make_shared<VulStaticModuleInstance>();
    fake_main->instance_path = {"sim", "main"};
    fake_main->module_name = "TestMain";
    fake_main->instance_id = module_count + 1;
    fake_main->tick_blocks.push_back(VulTickBlock());

    shared_ptr<VulStaticModuleInstance> sim_top = std::make_shared<VulStaticModuleInstance>();
    sim_top->instance_path = {"sim"};
    sim_top->module_name = "SimTop";
    sim_top->filepath = main_file_path;
    sim_top->instance_id = module_count + 2;
    sim_top->tick_blocks.push_back(VulTickBlock());

    for (const auto &req_entry: project.top_module_instance->requests) {
        fake_main->services[req_entry.first] = req_entry.second;
        VulLogicBlock logic_block;
        logic_block.block_id = fake_main->services.size();
        logic_block.with_priority = false;
        fake_main->serv_logic_blocks[req_entry.first] = logic_block;
        VulReqServConnection conn;
        conn.req_instance = "top";
        conn.req_name = req_entry.first;
        conn.serv_instance = "main";
        conn.serv_name = req_entry.first;
        sim_top->req_connections.push_back(conn);
    }
    for (const auto &serv_entry: project.top_module_instance->services) {
        fake_main->requests[serv_entry.first] = serv_entry.second;
        LogicBlockCall call;
        call.instance = "";
        call.port = serv_entry.first;
        fake_main->tick_blocks[0].call_requests.push_back(call);
        VulReqServConnection conn;
        conn.req_instance = "main";
        conn.req_name = serv_entry.first;
        conn.serv_instance = "top";
        conn.serv_name = serv_entry.first;
        sim_top->req_connections.push_back(conn);
    }

    VulStaticInstanceDecl main_decl;
    main_decl.name = "main";
    main_decl.module_name = "TestMain";
    sim_top->instances["main"] = main_decl;
    VulStaticInstanceDecl top_decl;
    top_decl.name = "top";
    top_decl.module_name = project.top_module_instance->module_name;
    sim_top->instances["top"] = top_decl;

    sim_top->children.push_back(fake_main);
    sim_top->children.push_back(project.top_module_instance);

    fake_main->parent = sim_top;
    project.top_module_instance->parent = sim_top;

    setupUpdateSequence(sim_top);

    printf("Setup complete.\n");

    return project;
}

