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
    uint32_t thread_num = 1;
};

struct CompilationConfig {
    uint32_t thread_num = 1;
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
    ProjectName project_name;
    string vullib_path;
    string generation_dir;
    uint64_t starttime_us = 0;
};

class SimulationManager {
public:

    SimStateInfo getState() {
        std::lock_guard<std::mutex> lk(state_mtx);
        return state;
    }

    ErrorMsg startTask(
        shared_ptr<VulProject> project,
        const string &generation_dir,
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
        if (gen_ctrl_thread.joinable()) {
            gen_ctrl_thread.join();
        }
        if (comp_ctrl_thread.joinable()) {
            comp_ctrl_thread.join();
        }
    }

protected:

    shared_ptr<VulProject> project_snapshot;

    SimStateInfo state;
    std::mutex state_mtx;

    unordered_map<uint64_t, ChildProcessRunner *> subprocesses;
    std::mutex subprocesses_mtx;

    std::atomic<bool> all_cancel_flag = false;

    template<typename T>
    struct WorkDispather {
        vector<T> tasks;
        uint64_t next_task = 0;
        std::mutex mtx;

        void addTask(const T &task) {
            std::lock_guard<std::mutex> lk(mtx);
            tasks.push_back(task);
        }
        void addTasks(const vector<T> &new_tasks) {
            std::lock_guard<std::mutex> lk(mtx);
            tasks.insert(tasks.end(), new_tasks.begin(), new_tasks.end());
        }
        bool getNextTask(T &task) {
            std::lock_guard<std::mutex> lk(mtx);
            if (next_task >= tasks.size()) {
                return false;
            }
            task = tasks[next_task++];
            return true;
        }
        void clear() {
            std::lock_guard<std::mutex> lk(mtx);
            tasks.clear();
            next_task = 0;
        }
    };

    
    struct GenThreadWork {
        enum class WorkType {
            GenerateModule,
            GenerateConfig,
            GenerateBundle,
            GenerateTop
        } type;
        ModuleName module_name;
    };
    
    std::thread gen_ctrl_thread;
    vector<unique_ptr<std::thread>> gen_worker_threads;
    WorkDispather<GenThreadWork> gen_work_dispather;
    void generationCtrlThreadFunc();
    void generationWorkerThreadFunc(uint32_t thread_id);

    struct CompInfo {
        vector<string> include_paths;
        vector<string> cxx_flags;
        vector<string> ld_flags;
    } compile_information;

    struct CompThreadWork {
        string source_file_path;
        string object_file_path;
    };

    std::thread comp_ctrl_thread;
    vector<unique_ptr<std::thread>> comp_worker_threads;
    WorkDispather<CompThreadWork> comp_work_dispather;
    void compilationCtrlThreadFunc();
    void compilationWorkerThreadFunc(uint32_t thread_id);

};



