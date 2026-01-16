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

#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <unordered_map>

using std::string;
using std::function;
using std::mutex;
using std::unique_lock;
using std::condition_variable;
using std::vector;
using std::unordered_map;

struct ChildProcessConfig {
    string executable_path;
    vector<string> arguments;
    unordered_map<string, string> environment;

    function<void(const string &output_line)> stdout_callback;
    function<void(const string &error_line)> stderr_callback;
};

class ChildProcessRunner {
public:
    ChildProcessRunner(const ChildProcessConfig &config) : config(config) {};
    ~ChildProcessRunner() {
        terminate();
        wait();
    };

    bool start();
    int32_t wait();
    void terminate();
    
private:
    ChildProcessConfig config;
    
    std::atomic<bool> running {false};
    std::atomic<bool> force_killed {false};

    std::thread stdout_thread;
    std::thread stderr_thread;

    int32_t child_pid = -1;
    int32_t pgid = -1;

    int32_t stdout_fd = -1;
    int32_t stderr_fd = -1;

    void thread_function_stdout();
    void thread_function_stderr();
};
