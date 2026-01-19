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

#include "simman.h"
#include "logsocket.h"
#include "simgen.h"
#include "configuration.h"

#include "platform/time.hpp"

#include <filesystem>
#include <fstream>


void SimulationManager::_generation() {
    std::filesystem::path gen_dir(project_snapshot->dirpath);
    gen_dir /= "generation";

    std::filesystem::path gen_finish_file = gen_dir / ".finished";
    if (std::filesystem::exists(gen_finish_file)) {
        return;
    }

    if (std::filesystem::exists(gen_dir)) {
        std::filesystem::remove_all(gen_dir);
    }
    std::filesystem::create_directories(gen_dir);

    auto on_error = [&](const ErrorMsg &err) {
        logSocketError(LogSocketCategory::Generation, "Generation error: " + err.msg);
        std::filesystem::path gen_err_file = gen_dir / ".error";
        std::ofstream err_ofs(gen_err_file, std::ios::out | std::ios::app);
        if (err_ofs.is_open()) {
            err_ofs << "Error Code: " << err.code << "\n";
            err_ofs << "Error Message: " << err.msg << "\n";
            err_ofs.close();
        }
        std::lock_guard<std::mutex> lk(state_mtx);
        state.generation.errors.push_back(err);
        state.generation.finished = true;
        state.errored = true;
        state.running = false;
    };
    auto check_error_and_set = [&](const ErrorMsg &err, const string &prefix) -> bool {
        if (err.error()) {
            on_error(EStr(err.code, prefix + err.msg));
            return true;
        }
        return false;
    };
    auto write_file = [&](const std::filesystem::path &filepath, const vector<string> &lines) -> ErrorMsg {
        std::ofstream ofs(filepath, std::ios::out | std::ios::trunc);
        if (!ofs.is_open()) {
            return EStr(EGENWriteFileFailed, "Failed to open file for writing: " + filepath.string());
        }
        for (const auto &line : lines) {
            ofs << line << "\n";
        }
        ofs.close();
        return ErrorMsg();
    };

    unordered_set<ModuleName> all_modules;
    ModuleName top_module = project_snapshot->top_module;
    auto &modulelist = project_snapshot->modulelib->modules;
    if (modulelist.find(top_module) == modulelist.end()) {
        on_error(EStr(EGENTopNotFound, "Top module '" + top_module + "' not found in project."));
        return;
    }
    {
        all_modules.clear();
        all_modules.insert(top_module);
        std::deque<ModuleName> mod_todo;
        mod_todo.push_back(top_module);
        while (!mod_todo.empty()) {
            ModuleName mod_name = mod_todo.front();
            mod_todo.pop_front();
            if (all_modules.find(mod_name) != all_modules.end()) {
                continue;
            }
            auto mod_iter = modulelist.find(mod_name);
            if (mod_iter == modulelist.end()) {
                on_error(EStr(EGENModuleNotFound, "Module '" + mod_name + "' not found in project."));
                return;
            }
            shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_iter->second);
            if (!mod_ptr) {
                continue;
            }
            for (const auto &inst_pair : mod_ptr->instances) {
                const auto &inst = inst_pair.second;
                mod_todo.push_back(inst.module_name);
            }
        }
    }

    {
        std::lock_guard<std::mutex> lk(state_mtx);
        state.generation.total_modules = all_modules.size() + 3;
        state.generation.finished_modules = 0;
        state.generation.started = true;
        state.starttime_us = getCurrentTimeUs();
    }

    // generate modules
    for (const auto &mod_name : all_modules) {
        if (all_cancel_flag.load(std::memory_order_acquire)) {
            on_error(EStr(EGENCancel, "Generation cancelled."));
            return;
        }
        logSocketInfo(LogSocketCategory::Generation, "Generating module '" + mod_name + "'...");
        auto mod_iter = project_snapshot->modulelib->modules.find(mod_name);
        if (mod_iter == project_snapshot->modulelib->modules.end()) {
            on_error(EStr(EGENModuleNotFound, "Module '" + mod_name + "' not found in project."));
            continue;
        }
        shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_iter->second);
        if (!mod_ptr) {
            // skip external module
            continue;
        }
        vector<string> gen_lines;
        std::filesystem::path gen_file_path = gen_dir / (mod_name + ".hpp");
        ErrorMsg err = simgen::genModuleCodeHpp(*mod_ptr, gen_lines, project_snapshot->configlib, project_snapshot->modulelib);
        if (check_error_and_set(err, "Failed to generate module header for module '" + mod_name + "': ")) {
            continue;
        }
        err = write_file(gen_file_path, gen_lines);
        if (check_error_and_set(err, "Failed to write module header for module '" + mod_name + "': ")) {
            continue;
        }
        {
            std::lock_guard<std::mutex> lk(state_mtx);
            state.generation.finished_modules += 1;
        }
    }

    // generate configlib
    {
        vector<string> gen_lines;
        std::filesystem::path gen_file_path = gen_dir / "config.h";
        ErrorMsg err = simgen::genConfigHeaderCode(*project_snapshot->configlib, gen_lines);
        if (check_error_and_set(err, "Failed to generate config.h: ")) {
            return;
        }
        err = write_file(gen_file_path, gen_lines);
        if (check_error_and_set(err, "Failed to write config.h: ")) {
            return;
        }
        {
            std::lock_guard<std::mutex> lk(state_mtx);
            state.generation.finished_modules += 1;
        }
    }
    // generate bundle.h
    {
        vector<string> gen_lines;
        std::filesystem::path gen_file_path = gen_dir / "bundle.h";
        ErrorMsg err = simgen::genBundleHeaderCode(*project_snapshot->bundlelib, gen_lines);
        if (check_error_and_set(err, "Failed to generate bundle.h: ")) {
            return;
        }
        err = write_file(gen_file_path, gen_lines);
        if (check_error_and_set(err, "Failed to write bundle.h: ")) {
            return;
        }
        {
            std::lock_guard<std::mutex> lk(state_mtx);
            state.generation.finished_modules += 1;
        }
    }
    {
        vector<string> gen_lines;
        std::filesystem::path gen_file_path = gen_dir / "simulation.cpp";
        ErrorMsg err = simgen::genTopSimCpp(
            project_snapshot->top_module,
            vector<ConfigValue>(), // TODO: top module local configs
            gen_lines
        );
        if (check_error_and_set(err, "Failed to generate top simulation source: ")) {
            return;
        }
        err = write_file(gen_file_path, gen_lines);
        if (check_error_and_set(err, "Failed to write top simulation source: ")) {
            return;
        }
        // copy all files in vullib_path into generation_dir
        std::filesystem::path vullib_path(state.vullib_path);
        if (std::filesystem::exists(vullib_path) && std::filesystem::is_directory(vullib_path)) {
            for (const auto &entry : std::filesystem::directory_iterator(vullib_path)) {
                if (entry.is_regular_file()) {
                    std::filesystem::path dest_path = gen_dir / entry.path().filename();
                    std::error_code ec;
                    std::filesystem::copy_file(entry.path(), dest_path, std::filesystem::copy_options::overwrite_existing, ec);
                    if (ec) {
                        check_error_and_set(EStr(EGENWriteFileFailed, "Failed to copy file '" + entry.path().string() + "' to generation directory: " + ec.message()), "");
                    }
                }
            }
        } else {
            check_error_and_set(EStr(EGENWriteFileFailed, "Vullib path '" + vullib_path.string() + "' does not exist or is not a directory."), "");
        }
        {
            std::lock_guard<std::mutex> lk(state_mtx);
            state.generation.finished_modules += 1;
        }
    }

    {
        std::lock_guard<std::mutex> lk(state_mtx);
        state.generation.finished = true;
        state.generation.finished_us = getCurrentTimeUs();
    }
}



