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

#include "childproc.hpp"

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

bool ChildProcessRunner::start() {
    if (running.load()) {
        // already running
        return false;
    }

    int outpipe[2];
    int errpipe[2];
    if (pipe(outpipe) == -1 || pipe(errpipe) == -1) {
        // failed to create stdout or stderr pipe
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        // fork failed
        return false;
    } else if (pid == 0) {
        // child process
        setsid();
        dup2(outpipe[1], STDOUT_FILENO);
        dup2(errpipe[1], STDERR_FILENO);
        close(outpipe[0]);
        close(errpipe[0]);

        // set up argv
        vector<char *> argv;
        argv.push_back(const_cast<char *>(config.executable_path.c_str()));
        for (const auto &arg : config.arguments) {
            argv.push_back(const_cast<char *>(arg.c_str()));
        }
        argv.push_back(nullptr);
        // set up envp
        vector<string> env_strings;
        vector<char *> envp;
        for (const auto &env_var : config.environment) {
            env_strings.push_back(env_var.first + "=" + env_var.second);
            envp.push_back(const_cast<char *>(env_strings.back().c_str()));
        }
        envp.push_back(nullptr);

        execvpe(config.executable_path.c_str(), argv.data(), envp.data());
        // if execvpe returns, there was an error
        _exit(127);
    }

    // parent process
    child_pid = pid;
    pgid = pid;
    stdout_fd = outpipe[0];
    stderr_fd = errpipe[0];
    close(outpipe[1]);
    close(errpipe[1]);

    running.store(true, std::memory_order_release);
    force_killed.store(false, std::memory_order_release);
    stdout_thread = std::thread(&ChildProcessRunner::thread_function_stdout, this);
    stderr_thread = std::thread(&ChildProcessRunner::thread_function_stderr, this);
    return true;
}

void ChildProcessRunner::terminate() {
    if (!running.exchange(false, std::memory_order_acq_rel)) {
        // not running
        return;
    }
    force_killed.store(true, std::memory_order_release);
    // send SIGKILL to the process group
    if (pgid != -1) {
        kill(-pgid, SIGKILL);
    }
}

int32_t ChildProcessRunner::wait() {
    int status = 0;
    waitpid(child_pid, &status, 0);

    running.store(false, std::memory_order_release);

    if (stdout_thread.joinable()) {
        stdout_thread.join();
    }
    if (stderr_thread.joinable()) {
        stderr_thread.join();
    }

    close(stdout_fd);
    close(stderr_fd);
    
    return (WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

void ChildProcessRunner::thread_function_stdout() {
    string buf;
    char tmp[4096];

    while (running.load(std::memory_order_acquire)) {
        ssize_t n = read(stdout_fd, tmp, sizeof(tmp));
        if (n > 0) {
            buf.append(tmp, n);
            size_t pos = 0;
            while (true) {
                size_t newline_pos = buf.find('\n', pos);
                if (newline_pos == string::npos) break;
                string line = buf.substr(pos, newline_pos - pos);
                if (config.stdout_callback) config.stdout_callback(line);
                pos = newline_pos + 1;
            }
            buf.erase(0, pos);
            continue;
        }

        if (n == 0) {
            // EOF: peer closed write end
            break;
        }

        // n < 0: error
        if (errno == EINTR) {
            continue; // interrupted, retry
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue; // non-blocking no data, retry later
        }
        // unrecoverable error (e.g. EBADF) -> stop
        break;
    }
}

void ChildProcessRunner::thread_function_stderr() {
    string buf;
    char tmp[4096];

    while (running.load(std::memory_order_acquire)) {
        ssize_t n = read(stderr_fd, tmp, sizeof(tmp));
        if (n <= 0) {
            break;
        }
        buf.append(tmp, n);
        size_t pos = 0;
        while (true) {
            size_t newline_pos = buf.find('\n', pos);
            if (newline_pos == string::npos) {
                break;
            }
            string line = buf.substr(pos, newline_pos - pos);
            if (config.stderr_callback) {
                config.stderr_callback(line);
            }
            pos = newline_pos + 1;
        }
        buf.erase(0, pos);
    }
}
