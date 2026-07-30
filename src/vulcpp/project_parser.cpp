// Copyright (c) 2025 Meng Chengzhen, in Shandong University
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "project_parser.hpp"

#include "module_parser.hpp"
#include "test_parser.hpp"
#include "../configexpr.hpp"
#include "../cppparse.hpp"
#include "../stringop.hpp"
#include "../toposort.hpp"
#include "../validate.hpp"

#include <algorithm>
#include <assert.h>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

using namespace cppparse;
using namespace stringop;

static bool isHeaderSourceFile(const std::filesystem::path &p) {
    const string ext = p.extension().string();
    return ext == ".h" || ext == ".hpp";
}

static string pathInHeaderDirKey(
    const std::filesystem::path &p,
    const std::filesystem::path &header_dir_canon
) {
    using namespace std::filesystem;
    path canon = canonical(p);
    path rel = relative(canon, header_dir_canon);
    if (rel.empty()) {
        return "";
    }
    for (const auto &part : rel) {
        if (part == "..") {
            return "";
        }
    }
    return rel.generic_string();
}

static vector<string> scanHeaderIncludes(const std::filesystem::path &header_file) {
    vector<string> out;
    vector<string> code_lines = stripComments(readFileLines(header_file.string())).lines;
    const string prefix = "#include";

    for (const auto &line_raw : code_lines) {
        string line = trim(line_raw);
        if (line.rfind(prefix, 0) != 0) {
            continue;
        }

        size_t open_pos = line.find_first_of("\"<", prefix.size());
        if (open_pos == string::npos) {
            continue;
        }
        char open = line[open_pos];
        char close = (open == '"') ? '"' : '>';
        size_t close_pos = line.find(close, open_pos + 1);
        if (close_pos == string::npos || close_pos <= open_pos + 1) {
            continue;
        }

        string included = trim(line.substr(open_pos + 1, close_pos - open_pos - 1));
        if (!included.empty()) {
            out.push_back(std::move(included));
        }
    }
    return out;
}

static vector<std::filesystem::path> collectHeaderParseOrder(const std::filesystem::path &header_dir) {
    using namespace std::filesystem;

    path header_dir_canon = canonical(header_dir);
    unordered_set<string> all_items;
    unordered_map<string, path> item_to_path;

    for (const auto &entry : recursive_directory_iterator(header_dir_canon)) {
        if (!entry.is_regular_file() || !isHeaderSourceFile(entry.path())) {
            continue;
        }
        string key = pathInHeaderDirKey(entry.path(), header_dir_canon);
        if (key.empty()) {
            continue;
        }
        all_items.insert(key);
        item_to_path[key] = entry.path();
    }

    if (all_items.empty()) {
        throw VulException("Header directory contains no .h or .hpp files: " + header_dir.string());
    }

    unordered_map<string, unordered_set<string>> edges_former_to_latter;
    for (const auto &[key, file_path] : item_to_path) {
        for (const auto &included_raw : scanHeaderIncludes(file_path)) {
            vector<path> candidates;
            path included_path(included_raw);
            if (included_path.is_absolute()) {
                candidates.push_back(included_path);
            } else {
                candidates.push_back(file_path.parent_path() / included_path);
                candidates.push_back(header_dir_canon / included_path);
            }

            string included_key;
            for (const auto &candidate : candidates) {
                std::error_code ec;
                if (!exists(candidate, ec) || !is_regular_file(candidate, ec) || !isHeaderSourceFile(candidate)) {
                    continue;
                }
                string maybe_key = pathInHeaderDirKey(candidate, header_dir_canon);
                if (!maybe_key.empty() && all_items.find(maybe_key) != all_items.end()) {
                    included_key = maybe_key;
                    break;
                }
            }
            if (!included_key.empty()) {
                edges_former_to_latter[included_key].insert(key);
            }
        }
    }

    vector<string> loop_nodes;
    auto sorted_keys = topologicalSort(all_items, edges_former_to_latter, loop_nodes);
    if (!sorted_keys) {
        string err = "Cycle detected in header include graph:";
        std::sort(loop_nodes.begin(), loop_nodes.end());
        for (const auto &node : loop_nodes) {
            err += "\n  " + node;
        }
        throw VulException(err);
    }

    vector<path> out;
    out.reserve(sorted_keys->size());
    for (const auto &key : *sorted_keys) {
        out.push_back(item_to_path.at(key));
    }
    return out;
}

static void importGlobalHeaderModule(
    VulStaticProject &project,
    const VulTempModule &header_module
) {
    for (const auto& item : header_module.configs) {
        VulErrorContextGuard _err{"evaluating global header constant " + item.name};
        ConfigRealValue value = calculateConstexprValue(item.value, project.global_configlib);
        project.global_configlib[item.name] = value;
    }
    for (const auto& item : header_module.bundles) {
        VulErrorContextGuard _err{"staticalizing global header bundle " + item.name};
        VulStaticBundle static_bundle = staticalizeBundle(item, project.global_configlib);
        project.global_bundlelib.push_back(std::move(static_bundle));
    }
    project.global_helper_codes.insert(
        project.global_helper_codes.end(),
        header_module.helper_codes.begin(),
        header_module.helper_codes.end()
    );
}

static void parseProjectHeaders(
    VulStaticProject &project,
    const std::filesystem::path &proj_dir
) {
    using namespace std::filesystem;

    const path header_dir = proj_dir / "header";
    const path header_hpp = proj_dir / "header.hpp";
    const path header_h = proj_dir / "header.h";
    const bool has_header_dir = exists(header_dir) && is_directory(header_dir);
    const bool has_header_hpp = exists(header_hpp) && is_regular_file(header_hpp);
    const bool has_header_h = exists(header_h) && is_regular_file(header_h);

    if (has_header_hpp && has_header_h) {
        throw VulException("Both header.hpp and header.h exist in project root; keep only one single-header file");
    }
    if (has_header_dir && (has_header_hpp || has_header_h)) {
        throw VulException("Both header directory and single-header file exist in project root; keep only one header system");
    }
    if (!has_header_dir && !has_header_hpp && !has_header_h) {
        throw VulException("Project root must contain either a header directory or a header.hpp/header.h file: " + proj_dir.string());
    }

    vector<path> header_files;
    if (has_header_dir) {
        header_files = collectHeaderParseOrder(header_dir);
    } else {
        header_files.push_back(has_header_hpp ? header_hpp : header_h);
    }

    for (const auto &header_path : header_files) {
        VulErrorContextGuard _err{"parsing global header file " + header_path.string()};
        VulTempModule fake_module = parseTempModule(
            header_path.stem().string(),
            header_path.string(),
            &project.global_names,
            true
        );
        importGlobalHeaderModule(project, fake_module);
    }
}


VulStaticProject parseVcppStaticProjectImpl(
    const string &project_dir,
    const string &top_file_path,
    const string &main_file_path
) {
    VulStaticProject project;

    using namespace std::filesystem;

    bool is_sim = !main_file_path.empty();

    path main_path;
    string scanned_top_module_path;
    string scanned_project_dir_path;
    if (!main_file_path.empty()) {
        main_path = path(main_file_path);
        if (!exists(main_path) || !is_regular_file(main_path)) {
            std::cerr << "Error: Main file does not exist or is not a regular file: " << main_file_path << std::endl;
            assert(0);
        }
        std::tie(scanned_top_module_path, scanned_project_dir_path) = scanTestModuleBindingPaths(main_path.string());
    }

    auto resolve_from_main = [&](const string &raw_path, const string &field_name) -> path {
        if (raw_path.empty()) {
            return {};
        }
        path p(raw_path);
        if (p.is_absolute()) {
            return p.lexically_normal();
        }
        if (main_path.empty()) {
            throw VulException(field_name + " in TestMain requires a main file");
        }
        return (main_path.parent_path() / p).lexically_normal();
    };

    path top_path;
    if (!top_file_path.empty()) {
        top_path = path(top_file_path);
    } else if (!scanned_top_module_path.empty()) {
        top_path = resolve_from_main(scanned_top_module_path, "TOP");
    }
    if (top_path.empty()) {
        throw VulException("Top module path is not specified. Use -t/--top or TOP(...) in TestMain.");
    }
    if (!exists(top_path) || !is_regular_file(top_path)) {
        throw VulException("Top file does not exist or is not a regular file: " + top_path.string());
    }

    path proj_dir;
    if (!project_dir.empty()) {
        proj_dir = path(project_dir);
    } else if (!scanned_project_dir_path.empty()) {
        proj_dir = resolve_from_main(scanned_project_dir_path, "PROJECT");
    } else {
        proj_dir = top_path.parent_path();
    }
    if (!exists(proj_dir) || !is_directory(proj_dir)) {
        throw VulException("Project directory does not exist or is not a directory: " + proj_dir.string());
    }

    parseProjectHeaders(project, proj_dir);

    if (!main_file_path.empty()) {
        {
            VulErrorContextGuard _err{"reading test harness module from " + main_path.string()};
            project.test_harness = parseTestModule(main_path.string(), project.global_configlib, project.global_bundlelib);
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
    top_instance->module_name = top_path.stem().string();
    if (is_sim) {
        top_instance->instance_path = {"sim", "top"};
    } else {
        top_instance->instance_path = {"top"};
    }
    top_instance->filepath = top_path.string();
    todo_queue.push_back({top_instance, project.test_harness.top_config_overrides});

    uint32_t instance_count = 0;
    VulTempModuleCache temp_module_cache;

    while (!todo_queue.empty()) {
        auto [instance_ptr, config_overrides] = todo_queue.front();
        todo_queue.pop_front();

        VulErrorContextGuard _err{"parsing instance " + instance_ptr->simClassName() + " of module " + instance_ptr->module_name};
        const ModuleName& mod_name = instance_ptr->module_name;
        VulTempModule *temp_mod_ptr = nullptr;

        auto temp_mod_iter = temp_module_cache.find(mod_name);
        if (temp_mod_iter != temp_module_cache.end()) {
            temp_mod_ptr = &temp_mod_iter->second;
        } else {

            auto mod_file_opt = find_module_file(mod_name);
            if (!mod_file_opt.has_value()) {
                throw VulException("Module file not found for module: " + instance_ptr->module_name);
            }
            const path& mod_file = mod_file_opt.value();
            string mod_file_str = mod_file.string();

            VulErrorContextGuard _err_file{"entering module file " + mod_file_str};

            temp_module_cache[mod_name] = parseTempModule(
                mod_name,
                mod_file_str,
                &project.global_names,
                false
            );
            temp_mod_ptr = &temp_module_cache[mod_name];
        }

        instantiateModule(
            *instance_ptr,
            *temp_mod_ptr,
            config_overrides,
            project.global_configlib,
            project.global_bundlelib
        );
        detectRequestCallInLogicBlocks(*instance_ptr);

        instance_ptr->instance_id = instance_count++;

        // process child instances
        for (const auto& [child_name, child_instance_decl] : instance_ptr->instances) {
            shared_ptr<VulStaticModuleInstance> child_instance = std::make_shared<VulStaticModuleInstance>();
            child_instance->instance_path = instance_ptr->instance_path;
            child_instance->instance_path.push_back(child_name);
            child_instance->module_name = child_instance_decl.module_name;
            child_instance->parent = instance_ptr;
            instance_ptr->children.push_back(child_instance);
            todo_queue.push_back({child_instance, child_instance_decl.parameter_overrides});

            if (child_instance_decl.array_dims.size() > 2) {
                throw VulException("Only up to 2 child instance array dimensions are currently supported");
            }
        }
    }
    project.top_module_instance = top_instance;

    validateStaticProject(project);

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
    printf("Total instances: %d\n", instance_count);

    if (!is_sim) {
        printf("No main file provided, skipping simulation setup.\n");
        return project;
    }

    printf("Setting up simulation hierarchy and update sequence...\n");

    shared_ptr<VulStaticModuleInstance> fake_main = std::make_shared<VulStaticModuleInstance>();
    fake_main->instance_path = {"sim", "main"};
    fake_main->module_name = "TestMain";
    fake_main->instance_id = instance_count + 1;
    fake_main->tick_blocks.push_back(VulTickBlock());

    shared_ptr<VulStaticModuleInstance> sim_top = std::make_shared<VulStaticModuleInstance>();
    sim_top->instance_path = {"sim"};
    sim_top->module_name = "SimTop";
    sim_top->filepath = main_file_path;
    sim_top->instance_id = instance_count + 2;
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
