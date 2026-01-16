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

#include "platform/time.hpp"
#include "platform/env.hpp"

#include <filesystem>
#include <fstream>

ErrorMsg SimulationManager::startTask(
    shared_ptr<VulProject> project,
    const string &generation_dir,
    std::optional<GenerationConfig> generation_config,
    std::optional<CompilationConfig> compilation_config,
    std::optional<SimulationConfig> simulation_config
) {
    {
        std::lock_guard<std::mutex> lk(state_mtx);
        if (state.running) {
            return EStr(EGENAlreadyRunning, "SimulationManager is already running a task.");
        }
        project_snapshot = std::make_shared<VulProject>(*project);
        state.project_name = project_snapshot->name;
        state.generation_dir = generation_dir;
        if (generation_config.has_value()) {
            state.generation_config = generation_config.value();
            state.process_generation = true;
        } else {
            state.process_generation = false;
        }
        if (compilation_config.has_value()) {
            state.compilation_config = compilation_config.value();
            state.process_compilation = true;
        } else {
            state.process_compilation = false;
        }
        if (simulation_config.has_value()) {
            state.simulation_config = simulation_config.value();
            state.process_simulation = true;
        } else {
            state.process_simulation = false;
        }
        if (state.process_compilation && !state.process_generation) {
            return EStr(EGENStepInvalid, "Compilation step requested without generation step.");
        }
        if (state.process_simulation && !state.process_compilation) {
            return EStr(EGENStepInvalid, "Simulation step requested without compilation step.");
        }
        auto vullibpath = getEnvVar(string(EnvVulLibraryPath));
        if (!vullibpath || !std::filesystem::exists(vullibpath.value()) || !std::filesystem::is_directory(vullibpath.value())) {
            return EStr(EGENInvalidLibrary, "VUL library path environment variable '" + string(EnvVulLibraryPath) + "' is invalid.");
        }
        state.generation = GenerationStepInfo();
        state.compilation = CompilationStepInfo();
        state.simulation = SimulationStepInfo();
        state.running = true;
        state.errored = false;
        state.starttime_us = getCurrentTimeUs();
    }

    all_cancel_flag.store(false, std::memory_order_release);
    if (gen_ctrl_thread.joinable()) {
        gen_ctrl_thread.join();
    }
    gen_ctrl_thread = std::thread(&SimulationManager::generationCtrlThreadFunc, this);
}


void SimulationManager::generationCtrlThreadFunc() {
    if (!state.process_generation) {
        return;
    }

    auto on_error = [&](const ErrorMsg &err) {
        logSocketError(LogSocketCategory::Generation, "Generation error: " + err.msg);
        std::lock_guard<std::mutex> lk(state_mtx);
        state.generation.errors.push_back(err);
        state.generation.finished = true;
        state.errored = true;
        state.running = false;
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

    // setup work dispatcher
    vector<GenThreadWork> gen_tasks;
    gen_tasks.reserve(all_modules.size() + 3);
    gen_tasks.push_back(GenThreadWork{GenThreadWork::WorkType::GenerateConfig, ""});
    gen_tasks.push_back(GenThreadWork{GenThreadWork::WorkType::GenerateBundle, ""});
    gen_tasks.push_back(GenThreadWork{GenThreadWork::WorkType::GenerateTop, ""});
    for (const auto &mod_name : all_modules) {
        gen_tasks.push_back(GenThreadWork{GenThreadWork::WorkType::GenerateModule, mod_name});
    }
    gen_work_dispather.clear();
    gen_work_dispather.addTasks(gen_tasks);

    // start worker threads
    if (!all_cancel_flag.load(std::memory_order_acquire)) {
        on_error(EStr(EGENCancel, "Generation cancelled before starting."));
        return;
    }
    {
        std::lock_guard<std::mutex> lk(state_mtx);
        state.generation.total_modules = all_modules.size();
        state.generation.finished_modules = 0;
        state.generation.started = true;
        state.starttime_us = getCurrentTimeUs();
    }
    gen_worker_threads.resize(state.generation_config.thread_num ? state.generation_config.thread_num : 1);
    for (uint32_t i = 0; i < gen_worker_threads.size(); i++) {
        gen_worker_threads[i] = std::make_unique<std::thread>(&SimulationManager::generationWorkerThreadFunc, this, i);
    }

    // wait for worker threads to finish
    for (uint32_t i = 0; i < gen_worker_threads.size(); i++) {
        if (gen_worker_threads[i]->joinable()) {
            gen_worker_threads[i]->join();
        }
    }

    // finalize
    gen_worker_threads.clear();
    {
        std::lock_guard<std::mutex> lk(state_mtx);
        state.generation.finished = true;
        state.generation.finished_us = getCurrentTimeUs();
        state.errored = !state.generation.errors.empty();
        if (state.process_compilation && !state.errored) {
            if (comp_ctrl_thread.joinable()) {
                comp_ctrl_thread.join();
            }
            comp_ctrl_thread = std::thread(&SimulationManager::compilationCtrlThreadFunc, this);
        } else {
            state.running = false;
        }
    }
}

void SimulationManager::generationWorkerThreadFunc(uint32_t thread_id) {
    while (!all_cancel_flag.load(std::memory_order_acquire)) {
        GenThreadWork work;
        if (!gen_work_dispather.getNextTask(work)) {
            // no more tasks
            break;
        }
        auto write_file = [](const std::filesystem::path &path, const vector<string> &lines) -> ErrorMsg {
            std::ofstream ofs(path, std::ios::out | std::ios::trunc);
            if (!ofs.is_open()) {
                return EStr(EGENWriteFileFailed, "Cannot open file '" + path.string() + "' for writing.");
            }
            for (const auto &line : lines) {
                if (line.empty() || line.back() != '\n') {
                    ofs << line << "\n";
                } else {
                    ofs << line;
                }
            }
            ofs.close();
            return ErrorMsg();
        };
        auto check_error_and_set = [&](const ErrorMsg &err, const string &context_msg_prefix) -> bool {
            if (err.error()) {
                ErrorMsg new_err = err;
                new_err.msg = context_msg_prefix + err.msg;
                {
                    std::lock_guard<std::mutex> lk(state_mtx);
                    state.generation.errors.push_back(new_err);
                }
                logSocketError(LogSocketCategory::Generation, new_err.msg);
                return true;
            }
            return false;
        };

        if (work.type == GenThreadWork::WorkType::GenerateConfig) {
            // generate config.h
            vector<string> gen_lines;
            std::filesystem::path gen_file_path = std::filesystem::path(state.generation_dir) / "config.h";
            ErrorMsg err = simgen::genConfigHeaderCode(*project_snapshot->configlib, gen_lines);
            if (check_error_and_set(err, "Failed to generate config.h: ")) {
                continue;
            }
            err = write_file(gen_file_path, gen_lines);
            if (check_error_and_set(err, "Failed to write config.h: ")) {
                continue;
            }
        } else if (work.type == GenThreadWork::WorkType::GenerateBundle) {
            // generate bundle.h
            vector<string> gen_lines;
            std::filesystem::path gen_file_path = std::filesystem::path(state.generation_dir) / "bundle.h";
            ErrorMsg err = simgen::genBundleHeaderCode(*project_snapshot->bundlelib, gen_lines);
            if (check_error_and_set(err, "Failed to generate bundle.h: ")) {
                continue;
            }
            err = write_file(gen_file_path, gen_lines);
            if (check_error_and_set(err, "Failed to write bundle.h: ")) {
                continue;
            }
        } else if (work.type == GenThreadWork::WorkType::GenerateModule) {
            ModuleName &mod_name = work.module_name;
            auto mod_iter = project_snapshot->modulelib->modules.find(mod_name);
            if (mod_iter == project_snapshot->modulelib->modules.end()) {
                std::lock_guard<std::mutex> lk(state_mtx);
                state.generation.errors.push_back(EStr(EGENModuleNotFound, "Module '" + mod_name + "' not found in project."));
                continue;
            }
            shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_iter->second);
            if (!mod_ptr) {
                // skip external module
                continue;
            }
            vector<string> gen_lines;
            std::filesystem::path gen_file_path = std::filesystem::path(state.generation_dir) / (mod_name + ".hpp");
            ErrorMsg err = simgen::genModuleCodeHpp(*mod_ptr, gen_lines, project_snapshot->configlib, project_snapshot->modulelib);
            if (check_error_and_set(err, "Failed to generate module header for module '" + mod_name + "': ")) {
                continue;
            }
            err = write_file(gen_file_path, gen_lines);
            if (check_error_and_set(err, "Failed to write module header for module '" + mod_name + "': ")) {
                continue;
            }
        } else if (work.type == GenThreadWork::WorkType::GenerateTop) {
            vector<string> gen_lines;
            std::filesystem::path gen_file_path = std::filesystem::path(state.generation_dir) / "simulation.cpp";
            ErrorMsg err = simgen::genTopSimCpp(
                project_snapshot->top_module,
                vector<ConfigValue>(), // TODO: top module local configs
                gen_lines
            );
            if (check_error_and_set(err, "Failed to generate top simulation source: ")) {
                continue;
            }
            err = write_file(gen_file_path, gen_lines);
            if (check_error_and_set(err, "Failed to write top simulation source: ")) {
                continue;
            }
            // copy all files in vullib_path into generation_dir
            std::filesystem::path vullib_path(state.vullib_path);
            if (std::filesystem::exists(vullib_path) && std::filesystem::is_directory(vullib_path)) {
                for (const auto &entry : std::filesystem::directory_iterator(vullib_path)) {
                    if (entry.is_regular_file()) {
                        std::filesystem::path dest_path = std::filesystem::path(state.generation_dir) / entry.path().filename();
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
        }
        {
            std::lock_guard<std::mutex> lk(state_mtx);
            state.generation.finished_modules++;
        }
    }
}

void SimulationManager::compilationCtrlThreadFunc() {
    
}

