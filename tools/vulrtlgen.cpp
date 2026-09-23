// Copyright (c) 2025 Chino
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

#include "vcpp.hpp"

#include "simgen.h"
#include "rtlgen.h"
#include "debugmap.hpp"
#include "argparse.hpp"
#include "vullib.hpp"
#include "output_dir.hpp"

#include <filesystem>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <deque>
#include <functional>
#include <memory>
#include <unordered_set>
#include <vector>
#include <string>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstring>
#ifndef _WIN32
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
unsigned positiveCount(const std::string& text, const char* option) {
    unsigned value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || !value)
        throw VulException(std::string(option) + " requires a positive integer");
    return value;
}

class TaskDashboard {
public:
    TaskDashboard(unsigned worker_count, unsigned total)
        : total_(total), workers_(worker_count) {
#ifndef _WIN32
        interactive_ = isatty(STDOUT_FILENO) != 0;
#endif
        if (interactive_) render();
        else printProgress();
    }

    void start(unsigned worker, const std::string &module, const std::string &stage) {
        workers_[worker] = {module, stage, true};
        refresh();
    }
    void update(unsigned worker, const std::string &stage) {
        if (worker < workers_.size() && workers_[worker].active) workers_[worker].stage = stage;
        refresh();
    }
    void complete(unsigned worker, bool failed) {
        ++completed_;
        if (failed) ++failed_;
        if (worker < workers_.size()) workers_[worker] = {{}, failed ? "failed" : "completed", false};
        refresh();
    }
    unsigned failed() const { return failed_; }

private:
    struct Worker { std::string module; std::string stage; bool active = false; };
    bool interactive_;
    unsigned total_;
    unsigned completed_ = 0;
    unsigned failed_ = 0;
    std::vector<Worker> workers_;

    std::string progress() const {
        return "[vulrtlgen] Tasks: " + std::to_string(completed_) + "/" + std::to_string(total_) +
               " completed, " + std::to_string(failed_) + " failed";
    }
    std::string workerLine(unsigned index) const {
        const Worker &worker = workers_[index];
        if (!worker.active) return "Worker " + std::to_string(index + 1) + ": idle";
        return "Worker " + std::to_string(index + 1) + ": module " + worker.module + " : + " + worker.stage;
    }
    void render() const {
        const unsigned rows = static_cast<unsigned>(workers_.size()) + 1;
        if (rendered_) std::cout << "\033[" << rows << "A";
        std::cout << "\r\033[2K" << progress() << '\n';
        for (unsigned index = 0; index < workers_.size(); ++index)
            std::cout << "\r\033[2K" << workerLine(index) << '\n';
        std::cout.flush();
        rendered_ = true;
    }
    void printProgress() const {
        std::cout << progress() << std::endl;
        for (unsigned index = 0; index < workers_.size(); ++index)
            if (workers_[index].active) std::cout << workerLine(index) << std::endl;
    }
    void refresh() const { if (interactive_) render(); else printProgress(); }
    mutable bool rendered_ = false;
};

class TaskReporter {
public:
    explicit TaskReporter(int fd = -1, std::string *direct_errors = nullptr)
        : fd_(fd), direct_errors_(direct_errors) {}
    void progress(const std::string &message) const { send('P', message); }
    void error(const std::string &message) const {
        if (fd_ < 0 && direct_errors_) *direct_errors_ += message;
        else if (fd_ < 0) std::cerr << message << std::flush;
        else send('E', message);
    }
private:
    int fd_;
    std::string *direct_errors_;
    void send(char kind, const std::string &message) const {
#ifdef _WIN32
        (void)kind;
        (void)message;
        return;
#else
        if (fd_ < 0) return;
        const std::uint32_t size = static_cast<std::uint32_t>(message.size());
        char header[5] = {kind, 0, 0, 0, 0};
        std::memcpy(header + 1, &size, sizeof(size));
        auto write_all = [&](const char *data, std::size_t remaining) {
            while (remaining) {
                const ssize_t written = write(fd_, data, remaining);
                if (written <= 0) return;
                data += written;
                remaining -= static_cast<std::size_t>(written);
            }
        };
        write_all(header, sizeof(header));
        write_all(message.data(), message.size());
#endif
    }
};

// Fork only from the single-threaded coordinator; workers own all RTLzz state.
class ModuleTasks {
public:
    struct Failure { std::string module; std::string output; };
    ModuleTasks(unsigned limit, unsigned total)
        : limit_(limit), dashboard_(limit > 1 ? std::make_unique<TaskDashboard>(limit, total) : nullptr) {
#ifdef _WIN32
        if (limit > 1) throw VulException("Parallel module processes require a POSIX platform");
#endif
    }
    ~ModuleTasks() {
#ifndef _WIN32
        for (auto &child : children_) {
            if (child.read_fd >= 0) close(child.read_fd);
            int status = 0;
            while (waitpid(child.pid, &status, 0) < 0 && errno == EINTR) {}
        }
#endif
    }

    template<class Function> void launch(const std::string &module, Function function) {
        if (limit_ == 1) {
            std::string errors;
            TaskReporter reporter(-1, &errors);
            const int status = function(reporter);
            if (status != 0) failures_.push_back({module, errors.empty() ? "Module task returned failure.\n" : std::move(errors)});
            return;
        }
#ifndef _WIN32
        while (children_.size() >= limit_) waitForEvent();
        int pipes[2];
        if (pipe(pipes) != 0) throw VulException("Failed to create module task progress pipe");
        std::cout.flush();
        std::cerr.flush();
        const pid_t pid = fork();
        if (pid < 0) {
            close(pipes[0]); close(pipes[1]);
            throw VulException("Failed to start module task");
        }
        const unsigned worker = next_worker_++ % limit_;
        if (pid == 0) {
            close(pipes[0]);
            int status = 1;
            TaskReporter reporter(pipes[1]);
            try { status = function(reporter); }
            catch (const std::exception& error) { reporter.error(std::string("ERROR: ") + error.what() + "\n"); }
            catch (...) { reporter.error("ERROR: Unknown module task failure\n"); }
            close(pipes[1]);
            _exit(status);
        }
        close(pipes[1]);
        const int flags = fcntl(pipes[0], F_GETFL, 0);
        if (flags >= 0) fcntl(pipes[0], F_SETFL, flags | O_NONBLOCK);
        children_.push_back({pid, pipes[0], worker, module});
        dashboard_->start(worker, module, "starting");
#endif
    }

    bool finish() {
#ifndef _WIN32
        while (!children_.empty()) waitForEvent();
#endif
        return failures_.empty();
    }
    const std::vector<Failure> &failures() const { return failures_; }

private:
    unsigned limit_;
    std::unique_ptr<TaskDashboard> dashboard_;
    std::vector<Failure> failures_;
#ifndef _WIN32
    struct Child {
        pid_t pid;
        int read_fd;
        unsigned worker;
        std::string module;
        std::vector<char> received;
        std::string errors;
    };
    std::vector<Child> children_;
    unsigned next_worker_ = 0;

    void consume(Child &child) {
        char buffer[4096];
        for (;;) {
            const ssize_t count = read(child.read_fd, buffer, sizeof(buffer));
            if (count > 0) child.received.insert(child.received.end(), buffer, buffer + count);
            else if (count == 0) { close(child.read_fd); child.read_fd = -1; break; }
            else if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            else if (errno == EINTR) continue;
            else { close(child.read_fd); child.read_fd = -1; break; }
        }
        std::size_t offset = 0;
        while (child.received.size() - offset >= 5) {
            std::uint32_t size = 0;
            std::memcpy(&size, child.received.data() + offset + 1, sizeof(size));
            if (size > 16 * 1024 * 1024) throw VulException("Invalid module task progress record");
            if (child.received.size() - offset < 5 + size) break;
            const std::string message(child.received.data() + offset + 5, size);
            if (child.received[offset] == 'P') dashboard_->update(child.worker, message);
            else if (child.received[offset] == 'E') child.errors += message;
            offset += 5 + size;
        }
        if (offset) child.received.erase(child.received.begin(), child.received.begin() + offset);
    }
    void consumeAll() { for (auto &child : children_) if (child.read_fd >= 0) consume(child); }
    void reapFinished() {
        for (;;) {
            int status = 0;
            const pid_t pid = waitpid(-1, &status, WNOHANG);
            if (pid == 0) return;
            if (pid < 0) { if (errno == EINTR) continue; if (errno == ECHILD) return; throw VulException("Failed to wait for module task"); }
            const auto found = std::find_if(children_.begin(), children_.end(), [pid](const Child &child) { return child.pid == pid; });
            if (found == children_.end()) continue;
            while (found->read_fd >= 0) consume(*found);
            const bool failed = !WIFEXITED(status) || WEXITSTATUS(status) != 0;
            if (failed) {
                if (WIFSIGNALED(status)) found->errors += "Terminated by signal " + std::to_string(WTERMSIG(status)) + "\n";
                if (found->errors.empty()) found->errors = "Module task returned failure without diagnostic output.\n";
                failures_.push_back({found->module, std::move(found->errors)});
            }
            dashboard_->complete(found->worker, failed);
            children_.erase(found);
        }
    }
    void waitForEvent() {
        consumeAll();
        reapFinished();
        if (children_.empty()) return;
        std::vector<pollfd> poll_fds;
        for (const auto &child : children_) if (child.read_fd >= 0) poll_fds.push_back({child.read_fd, POLLIN | POLLHUP, 0});
        if (!poll_fds.empty()) {
            int result;
            do { result = poll(poll_fds.data(), poll_fds.size(), 100); } while (result < 0 && errno == EINTR);
            if (result < 0) throw VulException("Failed to read module task progress");
        }
        consumeAll();
        reapFinished();
    }
#endif
};
} // namespace

inline static void writeLinesToFile(const std::vector<std::string> &lines, const std::string &filepath) {
    const std::filesystem::path target_path(filepath);
    const std::filesystem::path parent_dir = target_path.parent_path();
    if (!parent_dir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent_dir, ec);
        if (ec) {
            throw VulException("Failed to create directory: " + parent_dir.string() + ", reason: " + ec.message());
        }
    }
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw VulException("Failed to open file for writing: " + filepath);
    }
    for (const auto &line : lines) {
        file << line;
        if (line.empty() || line.back() != '\n') {
            file << "\n";
        }
    }
    file.close();
}

inline static std::string joinNames(const std::vector<std::string> &names) {
    if (names.empty()) {
        return "<not provided by RTLzz>";
    }
    std::string out;
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) {
            out += ", ";
        }
        out += names[i];
    }
    return out;
}

static int runVulRTLGen(int argc, char * argv[]) {

    argparse::ArgumentParser parser("vulrtlgen", "Vul RTL Generator");
    parser.add_argument("-t", "--top")
        .help("sets the top module file")
        .default_value(std::string(""));
    parser.add_argument("-m", "--main")
        .help("sets the TestMain file used to generate a Verilator main cpp")
        .default_value(std::string(""));
    parser.add_argument("-o", "--out")
        .help("sets the output directory for generated code (default: ./rtlout)")
        .default_value(std::string("./rtlout"));
    parser.add_argument("-l", "--lib")
        .help("sets the directory for runtime library files (default: ./vullib)")
        .default_value(std::string("./vullib"));
    parser.add_argument("-p", "--project")
        .help("sets the project directory (default: parent directory of the top module file)")
        .default_value(std::string(""));
    parser.add_argument("-j")
        .help("maximum concurrent module processes (default: 1)")
        .default_value(std::string("1"));
    parser.add_argument("-r", "--release")
        .help("emit RTL without intermediate C++ or debug files")
        .default_value(false)
        .implicit_value(true);
    parser.add_argument("-f", "--force")
        .help("overwrite a non-empty output directory without prompting")
        .default_value(false)
        .implicit_value(true);
    try {
        parser.parse_args(argc, argv);
    } catch (const std::exception &e) {
        std::cerr << "Argument parsing error: " << e.what() << std::endl;
        std::cerr << parser.help().str() << std::endl;
        return 1;
    }

    const bool release = parser.get<bool>("--release");
    string top_file = parser.get<std::string>("--top");
    string main_file = parser.get<std::string>("--main");
    string out_dir = parser.get<std::string>("--out");
    string proj_dir = parser.get<std::string>("--project");
    string lib_dir = parser.get<std::string>("--lib");
    bool force = parser.get<bool>("--force");
    const unsigned processes = positiveCount(parser.get<std::string>("-j"), "-j");

    if (top_file.empty() && main_file.empty()) {
        std::cerr << "Error: Specify -t/--top, -m/--main, or a TestMain with TOP(...)." << std::endl;
        return 1;
    }
    std::filesystem::path top_path(top_file);
    if (!top_file.empty() && (!std::filesystem::exists(top_path) || !std::filesystem::is_regular_file(top_path))) {
        std::cerr << "Error: Top module file does not exist: " << top_file << std::endl;
        return 1;
    }
    if (!main_file.empty()) {
        std::filesystem::path main_path(main_file);
        if (!std::filesystem::exists(main_path) || !std::filesystem::is_regular_file(main_path)) {
            std::cerr << "Error: TestMain file does not exist: " << main_file << std::endl;
            return 1;
        }
    }
    if (proj_dir.empty() && !top_file.empty()) {
        proj_dir = top_path.parent_path().string();
    }

    VulErrorContextGuard _err{"generating project from " + proj_dir};
    VulStaticProject project = parseVcppStaticProject(proj_dir, top_file, main_file);

    std::filesystem::path out_path(out_dir);
    if (!prepareOutputDirectory(out_path, out_dir, force)) {
        return 1;
    }

    // Determine the unique generated modules before launching workers so the
    // dashboard can display a stable total task count.
    std::deque<shared_ptr<VulStaticModuleInstance>> bfs_queue;
    std::unordered_set<std::string> generated_module_paths;
    std::unordered_set<std::string> copied_resources;
    std::vector<shared_ptr<VulStaticModuleInstance>> modules;
    bfs_queue.push_back(project.top_module_instance);
    while (!bfs_queue.empty()) {
        auto mod_instance = bfs_queue.front();
        bfs_queue.pop_front();
        for (const auto &child : mod_instance->children) {
            bfs_queue.push_back(child);
        }

        const std::string hls_path = mod_instance->rtlHlsPath();
        if (!generated_module_paths.insert(hls_path).second) {
            continue;
        }
        modules.push_back(std::move(mod_instance));
    }

    ModuleTasks tasks(processes, static_cast<unsigned>(modules.size()));
    for (const auto &mod_instance : modules) {
        const std::string hls_path = mod_instance->rtlHlsPath();

        VulErrorContextGuard _err("generating code for module instance: " + mod_instance->simClassName());

        if (processes == 1) {
            std::cout << "[vulrtlgen] module " << mod_instance->simClassName()
                      << ": generating RTL skeleton and API-inline logic\n";
        }
        auto codes = rtlgen::genModuleRTL(
            *mod_instance,
            project.global_configlib,
            project.global_bundlelib,
            project.global_helper_codes
        );
        // Shared resources are copied only by the coordinator, once per path.
        for (const auto& resource : codes.resource_files) {
            if (!copied_resources.insert(resource).second) continue;
            const auto source = std::filesystem::path(proj_dir) / resource;
            const auto destination = out_path / resource;
            std::filesystem::create_directories(destination.parent_path());
            std::filesystem::copy_file(source, destination);
        }
        tasks.launch(mod_instance->simClassName(), [&](TaskReporter &reporter) -> int {
        const auto hls_out_path = out_path / hls_path;
        // The frontend still needs a source file; remove it on every exit in release mode.
        struct IntermediateCleanup {
            std::filesystem::path path;
            bool enabled;
            ~IntermediateCleanup() {
                if (enabled) { std::error_code ec; std::filesystem::remove(path, ec); }
            }
        } cleanup{hls_out_path, release};
        writeLinesToFile(codes.logic_hls_codes, hls_out_path.string());
        if (!release) vulDebugWriteMapToFile(codes.logic_hls_debug_lines, (out_path / (hls_path + ".dbgmap")).string());
        const auto sv_path = mod_instance->rtlSvPath();
        rtlgen::LogicRTLResult rtlzz_result =
            rtlgen::appendLogicRTL(codes, *mod_instance, hls_out_path.string(), lib_dir,
                                  1024, release,
                                  processes > 1
                                      ? std::function<void(const std::string &)>([&reporter](const std::string &step) { reporter.progress(step); })
                                      : std::function<void(const std::string &)>{});
        if (!rtlzz_result.ok) {
            std::filesystem::path error_dbg_path;
            if (!release) {
                const std::filesystem::path sv_out_path = out_path / sv_path;
                error_dbg_path =
                    (sv_out_path.has_parent_path() ? sv_out_path.parent_path() : out_path) / "error.dbg";
                if (rtlzz_result.error_debug_codelines.empty()) {
                    rtlzz_result.error_debug_codelines.push_back(
                        "RTLzz did not provide an error debug snapshot for this failure.\n"
                    );
                }
                writeLinesToFile(rtlzz_result.error_debug_codelines, error_dbg_path.string());
            }
            std::string failure;
            if (!release) {
                failure += "RTLzz error signal(s): " + joinNames(rtlzz_result.error_signal_names) + "\n";
                failure += "RTLzz error debug file: " + error_dbg_path.string() + "\n";
                if (!rtlzz_result.error_signal_debug_text.empty()) {
                    failure += "RTLzz error signal debug:\n" + rtlzz_result.error_signal_debug_text;
                    if (failure.back() != '\n') failure += '\n';
                } else {
                    failure += "RTLzz error signal debug: <not provided by RTLzz>\n";
                }
            }
            failure += "Error: " + rtlzz_result.error + "\n";
            reporter.error(failure);
            return 1;
        }
        std::vector<std::string> rtlzz_debug_codelines =
            std::move(rtlzz_result.debug_codelines);
        writeLinesToFile(codes.rtl_skeleten_codes, (out_path / sv_path).string());
        if (!release) vulDebugWriteMapToFile(codes.rtl_skeleten_debug_lines, (out_path / (sv_path + ".dbgmap")).string());
        if (!release && !rtlzz_debug_codelines.empty()) {
            writeLinesToFile(rtlzz_debug_codelines, (out_path / (sv_path + ".dbg")).string());
        }
        return 0;
        });
    }
    if (!tasks.finish()) {
        std::cerr << "[vulrtlgen] Summary: " << tasks.failures().size() << " module task(s) failed\n";
        for (const auto &failure : tasks.failures()) {
            std::cerr << "[vulrtlgen] Failed module " << failure.module << ":\n" << failure.output;
            if (failure.output.empty() || failure.output.back() != '\n') std::cerr << '\n';
        }
        return 1;
    }

    std::cout << "[vulrtlgen] Summary:\n";

    if (!release && !main_file.empty()) {
        VulErrorContextGuard _err("generating Verilator TestMain cpp");
        vector<string> testmain_code = rtlgen::genVerilatorTestMainCpp(project);
        writeLinesToFile(testmain_code, (out_path / "VulTestMain.cpp").string());
    }

    // copy vullib files to output directory
    {
        VulErrorContextGuard _err("copying vul library files");
        std::cout << "[vulrtlgen] copying vul library files from " << lib_dir << " to " << out_dir << std::endl;

        std::filesystem::path lib_path(lib_dir);
        if (!std::filesystem::exists(lib_path) || !std::filesystem::is_directory(lib_path)) {
            throw VulException("Library directory does not exist: " + lib_dir);
        }
        for (const auto &filename : VulRTLLibFiles) {
            std::filesystem::path src_file = lib_path / filename;
            if (!std::filesystem::exists(src_file) || !std::filesystem::is_regular_file(src_file)) {
                throw VulException("Runtime library file does not exist: " + src_file.string());
            }
            std::filesystem::path dst_file = out_path / filename;
            std::filesystem::copy_file(src_file, dst_file);
        }
    }

    const std::filesystem::path absolute_out_path =
        std::filesystem::absolute(out_path).lexically_normal();
    const std::filesystem::path top_rtl_path =
        absolute_out_path / project.top_module_instance->rtlSvPath();
    std::cout << "[vulrtlgen] generation complete: output=\""
              << absolute_out_path.string() << "\"; top file=\""
              << top_rtl_path.string() << "\"; top module="
              << project.top_module_instance->simClassName() << std::endl;

    return 0;
}

int main(int argc, char * argv[]) {
    try {
        return runVulRTLGen(argc, argv);
    } catch (const VulException &e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "ERROR: Unknown internal error" << std::endl;
    }
    return 1;
}
