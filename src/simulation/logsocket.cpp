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

#include "logsocket.h"

#include "platform/tcpsocket.hpp"
#include "tui/tui.h"

#include "json.hpp"

using nlohmann::json;

#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <ctime>
#include <sstream>
#include <iomanip>


using std::unique_ptr;
using std::thread;
using std::mutex;
using std::lock_guard;
using std::condition_variable;

struct LogLine {
    LogSocketLevel level;
    LogSocketCategory category;
    string message;
    uint64_t timestampus = 0;
};

class LogSocketQueue {
public:
    LogSocketQueue(uint64_t size) : max_size(size) {
        buffer_.resize(size);
    }

    void push(const LogLine &s) {
        const uint64_t tail = tail_.load(std::memory_order_relaxed);
        const uint64_t next_tail = (tail + 1) % max_size;
        uint64_t head = head_.load(std::memory_order_acquire);
        if (next_tail == head) {
            // full, drop oldest
            head = (head + 1) % max_size;
            head_.store(head, std::memory_order_release);
        }
        buffer_[tail] = s;
        tail_.store(next_tail, std::memory_order_release);
        notify();
    }

    LogLine pop_blocked() {
        std::unique_lock<mutex> lock(mtx_);
        cv_.wait(lock, [&]() {
            return terminated_.load(std::memory_order_acquire) || head_.load(std::memory_order_acquire) != tail_.load(std::memory_order_acquire);
        });
        if (terminated_.load(std::memory_order_acquire) && head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire)) {
            return LogLine{};
        }
        const uint64_t head = head_.load(std::memory_order_relaxed);
        const LogLine s = std::move(buffer_[head]);
        head_.store((head + 1) % max_size, std::memory_order_release);
        return s;
    }

    void terminate() {
        terminated_.store(true, std::memory_order_release);
        notify();
    }


protected:
    void notify() {
        lock_guard<mutex> lock(mtx_);
        cv_.notify_all();
    }

    const uint64_t max_size;
    vector<LogLine> buffer_;

    std::atomic<uint64_t> head_{0};
    std::atomic<uint64_t> tail_{0};
    mutex mtx_;
    condition_variable cv_;

    std::atomic<bool> terminated_{false};
};

unique_ptr<TCPSocket> log_socket = nullptr;
unique_ptr<LogSocketQueue> log_queue = nullptr;
unique_ptr<thread> log_thread = nullptr;

std::atomic<bool> log_thread_running = true;

LogSocketLevel current_level = LogSocketLevel::Info;

function<void(const string &)> echo_function = nullptr;
function<void(const string &)> error_echo_function = nullptr;

string levelToString(const LogSocketLevel level) {
    switch (level) {
        case LogSocketLevel::Debug:
            return "Debug";
        case LogSocketLevel::Info:
            return "Info";
        case LogSocketLevel::Warning:
            return "Warning";
        case LogSocketLevel::Error:
            return "Error";
        case LogSocketLevel::Critical:
            return "Critical";
        default:
            return "Unknown";
    }
}
string categoryToString(const LogSocketCategory category) {
    switch (category) {
        case LogSocketCategory::General:
            return "General";
        case LogSocketCategory::Generation:
            return "Generation";
        case LogSocketCategory::Compilation:
            return "Compilation";
        case LogSocketCategory::Simulation:
            return "Simulation";
        default:
            return "Unknown";
    }
}

bool levelEnabled(const LogSocketLevel level) {
    return static_cast<int>(level) >= static_cast<int>(current_level);
}

void logThreadFunction() {
    while (log_thread_running.load(std::memory_order_acquire)) {
        LogLine line = log_queue->pop_blocked();
        if (line.timestampus == 0) {
            // terminated
            break;
        }
        string level_str = levelToString(line.level);
        string category_str = categoryToString(line.category);
        // construct log message
        string log_msg;
        if (levelEnabled(line.level) && ((error_echo_function && (line.level == LogSocketLevel::Error || line.level == LogSocketLevel::Critical)) || echo_function)) {
            std::time_t t = line.timestampus / 1000000;
            std::tm *tm_info = std::localtime(&t);
            char time_buffer[26];
            std::strftime(time_buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
            uint64_t ms = (line.timestampus % 1000000) / 1000;

            std::ostringstream oss;
            oss << "[" << time_buffer << "." << std::setfill('0') << std::setw(3) << ms << "] ";
            oss << "[" << level_str << "] ";
            oss << "[" << category_str << "] ";
            oss << line.message;
            log_msg = oss.str();
        }
        // send to socket
        json log_json;
        log_json["timestamp"] = line.timestampus;
        log_json["level"] = level_str;
        log_json["category"] = category_str;
        log_json["message"] = line.message;
        if (log_socket) {
            log_socket->send_blocked(log_json.dump(), true);
        }
    }
}

void logSocketInit(const uint32_t port, const bool listen_global) {
    if (log_socket) {
        // already initialized
        return;
    }
    log_socket = std::make_unique<TCPSocket>(port, listen_global, logStdoutLine);
    log_queue = std::make_unique<LogSocketQueue>(4096);
    log_thread_running.store(true, std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_acq_rel);
    log_thread = std::make_unique<thread>(logThreadFunction);
}
void logSocketTerminate() {
    if (!log_socket) {
        // not initialized
        return;
    }
    log_thread_running.store(false, std::memory_order_release);
    log_queue->terminate();
    if (log_thread->joinable()) {
        log_thread->join();
    }
    log_thread.reset();
    log_queue.reset();
    log_socket.reset();
}

void logSocketSetLevel(const LogSocketLevel level) {
    current_level = level;
}
void logSocketSetLevel(const string & level) {
    if (level == "Debug") {
        current_level = LogSocketLevel::Debug;
    } else if (level == "Info") {
        current_level = LogSocketLevel::Info;
    } else if (level == "Warning") {
        current_level = LogSocketLevel::Warning;
    } else if (level == "Error") {
        current_level = LogSocketLevel::Error;
    } else if (level == "Critical") {
        current_level = LogSocketLevel::Critical;
    }
}

void logSocketSetEcho(function<void(const string &)> echo_func) {
    echo_function = echo_func;
}
void logSocketSetErrorEcho(function<void(const string &)> echo_func) {
    error_echo_function = echo_func;
}

void logSocketFlush() {
    // no-op, as the log thread sends messages immediately
}
void logSocketClearBuffer() {
    // no-op, as the log queue does not support clearing
}

void logSocketMessage(const LogSocketLevel level, const LogSocketCategory category, const string &message, const bool newline) {
    if (!log_queue || !log_thread_running.load(std::memory_order_acquire)) {
        return;
    }
    LogLine line;
    line.level = level;
    line.category = category;
    line.message = message;
    if (newline) {
        line.message += "\n";
    }
    // get current timestamp in microseconds
    line.timestampus = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    log_queue->push(line);
}


