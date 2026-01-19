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

#pragma once

#include "project.h"

#include "platform/childproc.hpp"

#include <optional>
#include <thread>
#include <condition_variable>

struct GenerationStepInfo {
    uint32_t total_modules = 0;
    uint32_t finished_modules = 0;
    ModuleName current_module;
    vector<ErrorMsg> errors;
    uint64_t finished_us = 0;
    bool started = false;
    bool finished = false;
};

struct CompilationStepInfo {
    uint32_t total_files = 0;
    uint32_t finished_files = 0;
    string current_file;
    vector<ErrorMsg> errors;
    uint64_t finished_us = 0;
    bool started = false;
    bool finished = false;
};

struct SimulationStepInfo {
    vector<ErrorMsg> errors;
    uint64_t finished_us = 0;
    bool started = false;
    bool finished = false;
};

struct GenerationConfig {
    uint32_t __pad = 0;
};

struct CompilationConfig {
    uint32_t __pad = 0;
};

struct SimulationConfig {
    uint32_t __pad = 0;
};

struct SimStateInfo {
    GenerationStepInfo generation;
    CompilationStepInfo compilation;
    SimulationStepInfo simulation;
    GenerationConfig generation_config;
    CompilationConfig compilation_config;
    SimulationConfig simulation_config;
    bool process_generation = false;
    bool process_compilation = false;
    bool process_simulation = false;
    bool running = false;
    bool errored = false;
    string project_name;
    string vullib_path;
    string run_id;
    uint64_t starttime_us = 0;
};

class SimulationManager {
public:

    SimStateInfo getState() {
        std::lock_guard<std::mutex> lk(state_mtx);
        return state;
    }

    ErrorMsg startTask(
        const string &project_name,
        const string &run_id,
        const string &vullib_path,
        std::optional<GenerationConfig> generation_config,
        std::optional<CompilationConfig> compilation_config,
        std::optional<SimulationConfig> simulation_config
    );

    ErrorMsg cancelTask() {
        all_cancel_flag.store(true, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lk(subprocesses_mtx);
            for (auto &pair : subprocesses) {
                pair.second->terminate();
            }
        }
        return ErrorMsg();
    }

    ~SimulationManager() {
        cancelTask();
        if (sim_thread.joinable()) {
            sim_thread.join();
        }
    }

protected:

    shared_ptr<VulProject> project_snapshot;

    SimStateInfo state;
    std::mutex state_mtx;

    unordered_map<uint64_t, ChildProcessRunner *> subprocesses;
    std::mutex subprocesses_mtx;

    std::atomic<bool> all_cancel_flag = false;

    std::thread sim_thread;
    void simulationThreadFunc();

    void _generation();
    void _compilation();
    void _simulation();

};



