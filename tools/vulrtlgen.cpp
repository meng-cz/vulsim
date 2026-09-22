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
#include <iostream>
#include <fstream>
#include <deque>
#include <unordered_set>
#include <vector>
#include <string>
#include <cerrno>
#include <charconv>
#ifndef _WIN32
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

// Fork only from the single-threaded coordinator; workers own all RTLzz state.
class ModuleTasks {
    unsigned limit_;
    bool failed_ = false;
#ifndef _WIN32
    std::unordered_set<pid_t> children_;
    void reap() {
        int status = 0;
        pid_t child;
        do { child = waitpid(-1, &status, 0); } while (child < 0 && errno == EINTR);
        if (child < 0) throw VulException("Failed to wait for module task");
        if (!children_.erase(child)) return;
        failed_ |= !WIFEXITED(status) || WEXITSTATUS(status) != 0;
        if (WIFSIGNALED(status))
            std::cerr << "Module task " << child << " terminated by signal " << WTERMSIG(status) << '\n';
    }
#endif
public:
    explicit ModuleTasks(unsigned limit) : limit_(limit) {
#ifdef _WIN32
        if (limit > 1) throw VulException("Parallel module processes require a POSIX platform");
#endif
    }
    ~ModuleTasks() {
#ifndef _WIN32
        // Also join already-started jobs when the coordinator throws.
        for (auto child : children_) {
            int status;
            while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
        }
#endif
    }
    template<class Function> bool launch(Function function) {
        if (limit_ == 1) { failed_ |= function() != 0; return !failed_; }
#ifndef _WIN32
        while (children_.size() >= limit_) reap();
        if (failed_) return false;
        std::cout.flush();
        std::cerr.flush();
        const auto child = fork();
        if (child < 0) throw VulException("Failed to start module task");
        if (child == 0) {
            int status = 1;
            try { status = function(); }
            catch (const std::exception& error) { std::cerr << "ERROR: " << error.what() << '\n'; }
            catch (...) { std::cerr << "ERROR: Unknown module task failure\n"; }
            std::cout.flush();
            std::cerr.flush();
            _exit(status);
        }
        children_.insert(child);
#endif
        return true;
    }
    bool finish() {
#ifndef _WIN32
        while (!children_.empty()) reap();
#endif
        return !failed_;
    }
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
    parser.add_argument("--project")
        .help("sets the project directory (default: parent directory of the top module file)")
        .default_value(std::string(""));
    parser.add_argument("-p", "--process")
        .help("maximum concurrent module tasks (default: 1)")
        .default_value(std::string("1"));
    parser.add_argument("-j")
        .help("threads per module for S6, S10 and BEOPT (default: 1)")
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
    const unsigned processes = positiveCount(parser.get<std::string>("--process"), "--process");
    const unsigned threads = positiveCount(parser.get<std::string>("-j"), "-j");
    ModuleTasks tasks(processes);

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

    // gen module
    std::deque<shared_ptr<VulStaticModuleInstance>> bfs_queue;
    std::unordered_set<std::string> generated_module_paths;
    std::unordered_set<std::string> copied_resources;
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

        VulErrorContextGuard _err("generating code for module instance: " + mod_instance->simClassName());

        std::cout << "[vulrtlgen] module " << mod_instance->simClassName()
                  << ": generating RTL skeleton and API-inline logic\n";
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
        if (!tasks.launch([&]() -> int {
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
                                  1024, release, threads, processes > 1);
        if (!rtlzz_result.ok) {
            if (!release) {
                const std::filesystem::path sv_out_path = out_path / sv_path;
                const std::filesystem::path error_dbg_path =
                    (sv_out_path.has_parent_path() ? sv_out_path.parent_path() : out_path) / "error.dbg";
                if (rtlzz_result.error_debug_codelines.empty()) {
                    rtlzz_result.error_debug_codelines.push_back(
                        "RTLzz did not provide an error debug snapshot for this failure.\n"
                    );
                }
                writeLinesToFile(rtlzz_result.error_debug_codelines, error_dbg_path.string());
                std::cerr << "RTLzz error signal(s): " << joinNames(rtlzz_result.error_signal_names) << std::endl;
                std::cerr << "RTLzz error debug file: " << error_dbg_path.string() << std::endl;
                if (!rtlzz_result.error_signal_debug_text.empty()) {
                    std::cerr << "RTLzz error signal debug:" << std::endl;
                    std::cerr << rtlzz_result.error_signal_debug_text;
                    if (rtlzz_result.error_signal_debug_text.back() != '\n') {
                        std::cerr << std::endl;
                    }
                } else {
                    std::cerr << "RTLzz error signal debug: <not provided by RTLzz>" << std::endl;
                }
            }
            std::cerr << "Error: " << rtlzz_result.error << std::endl;
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
        })) return 1;
    }
    if (!tasks.finish()) return 1;

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
