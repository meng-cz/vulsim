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

#include "tui.h"

#include <mutex>
using std::mutex;
using std::lock_guard;

#include <iostream>

mutex stdout_mutex;
mutex stderr_mutex;

void logStdout(const string &message) {
    lock_guard<mutex> lock(stdout_mutex);
    std::cout << message;
}
void logStdoutLine(const string &message) {
    lock_guard<mutex> lock(stdout_mutex);
    std::cout << message << std::endl;
}

void logStderr(const string &message) {
    lock_guard<mutex> lock(stderr_mutex);
    std::cerr << message;
}
void logStderrLine(const string &message) {
    lock_guard<mutex> lock(stderr_mutex);
    std::cerr << message << std::endl;
}


void cmd_loop() {
    logStdoutLine("Type 'exit' or 'quit' to terminate the program.");

    std::string line;
    while (true) {
        {
            lock_guard<mutex> lock(stdout_mutex);
            std::cout << "vulsim> ";
        }
        if (!std::getline(std::cin, line)) {
            break;
        }
        if (line == "exit" || line == "quit") {
            break;
        }
        // Process other commands here
        {
            lock_guard<mutex> lock(stdout_mutex);
            std::cout << "Unknown command: " << line << std::endl;
        }
    }
}

