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
        }
        matches = matchMacros(code, "REGISTER_INIT($,$,$)");
        for (const auto& item : matches) {
            VulStorage storage;
            storage.name = item.args[0];
            storage.comment = "";
            storage.type = item.args[1];
            storage.value = item.args[2];
            module.storagenexts[storage.name] = storage;
        }
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
        // parse TICK_IMPL
        auto matches = matchMacros(code, "TICK_IMPL()");
        if (!matches.empty()) {
            auto block = findNextBraceBlock(code, matches[0].pos, '{', '}', true);
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




