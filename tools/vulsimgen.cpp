
#include "vcpp.hpp"

#include "simgen.h"
#include "argparse.hpp"
#include "breakpoint.hpp"
#include "trace.hpp"
#include "vullib.hpp"

#include <filesystem>
#include <iostream>
#include <fstream>
#include <deque>
#include <unordered_set>
#include <vector>
#include <string>

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

struct SimGenArgs {
    std::string top_file;
    std::string main_file;
    std::string proj_dir;
    std::string out_dir;
    std::string lib_dir;
    std::string trace_file;
    std::string trace_line;
    std::string break_file;
    std::string break_line;
    uint64_t break_cycles = 1024;
};

int simgenStatic(const SimGenArgs &args) {

    const string &top_file = args.top_file;
    const string &main_file = args.main_file;
    const string &proj_dir = args.proj_dir;
    const string &out_dir = args.out_dir;
    const string &lib_dir = args.lib_dir;
    const string &trace_file = args.trace_file;
    const string &trace_line = args.trace_line;
    const string &break_file = args.break_file;
    const string &break_line = args.break_line;

    std::filesystem::path main_path(main_file);
    if (!std::filesystem::exists(main_path) || !std::filesystem::is_regular_file(main_path)) {
        throw VulException("Main file does not exist: " + main_file);
    }

    auto resolve_from_main = [&](const string &raw_path) -> std::filesystem::path {
        std::filesystem::path p(raw_path);
        if (p.is_absolute()) {
            return p.lexically_normal();
        }
        return (main_path.parent_path() / p).lexically_normal();
    };

    VulErrorContextGuard _err{"generating project from " + main_path.parent_path().string()};
    VulStaticProject project = parseVcppStaticProject(proj_dir, top_file, main_path.string());

    std::filesystem::path effective_top_path;
    if (!top_file.empty()) {
        effective_top_path = std::filesystem::path(top_file);
    } else if (!project.test_harness.top_module_path.empty()) {
        effective_top_path = resolve_from_main(project.test_harness.top_module_path);
    }
    if (effective_top_path.empty()) {
        throw VulException("Top module path is not specified. Use -t/--top or TOP(...) in TestMain.");
    }

    std::filesystem::path proj_path;
    if (!proj_dir.empty()) {
        proj_path = std::filesystem::path(proj_dir);
    } else if (!project.test_harness.project_dir_path.empty()) {
        proj_path = resolve_from_main(project.test_harness.project_dir_path);
    } else {
        proj_path = effective_top_path.parent_path();
    }
    if (!std::filesystem::exists(proj_path) || !std::filesystem::is_directory(proj_path)) {
        throw VulException("Project directory does not exist or is not a directory: " + proj_path.string());
    }

    std::filesystem::path out_path(out_dir);
    if (!std::filesystem::exists(out_path)) {
        std::filesystem::create_directories(out_path);
    } else if (!std::filesystem::is_directory(out_path)) {
        throw VulException("Output path is not a directory: " + out_dir);
    } else {
        // ask user to confirm before deleting existing files in the output directory
        std::cout << "Output directory already exists: " << out_dir << std::endl;
        std::cout << "Do you want to clear the output directory before generating code? (y/n) ";
        char choice;
        std::cin >> choice;
        if (choice == 'y' || choice == 'Y') {
            std::filesystem::remove_all(out_path);
            std::filesystem::create_directories(out_path);
        } else {
            std::cout << "Output directory is not empty. Please clear the output directory or choose a different output directory." << std::endl;
            exit(1);
        }
    }

    {
        VulErrorContextGuard _err("generating config and bundle header code");
        writeLinesToFile(simgen::genStaticConfigHeaderCode(project.global_configlib), (out_path / "config.h").string());
    }
    {
        VulErrorContextGuard _err("generating bundle header code");
        writeLinesToFile(simgen::genStaticBundleHeaderCode(project.global_bundlelib), (out_path / "bundle.h").string());
    }

    vector<VulTraceMatcher> trace_matchers;
    if (trace_file.size() > 0) {
        VulErrorContextGuard _err("parsing trace matcher file: " + trace_file);

        std::filesystem::path trace_path(trace_file);
        if (!std::filesystem::exists(trace_path) || !std::filesystem::is_regular_file(trace_path)) {
            throw VulException("Trace matcher file does not exist: " + trace_file);
        }
        // parse trace matcher file
        // each line is a matcher string
        std::ifstream trace_file_stream(trace_path.string());
        if (!trace_file_stream.is_open()) {
            throw VulException("Failed to open trace matcher file: " + trace_file);
        }
        string line;
        while (std::getline(trace_file_stream, line)) {
            // skip empty lines and lines starting with # or //
            uint64_t commentpos = 0;
            if ((commentpos = line.find('#')) != string::npos) {
                line = line.substr(0, commentpos);
            }
            if ((commentpos = line.find("//")) != string::npos) {
                line = line.substr(0, commentpos);
            }
            if (line.empty()) {
                continue;
            }
            trace_matchers.push_back(parseTraceMatcher(line));
        }
    }
    if (trace_line.size() > 0) {
        // parse trace matcher line
        // each matcher string is seperated by comma
        VulErrorContextGuard _err("parsing trace matcher");

        std::stringstream ss(trace_line);
        string matcher_str;
        while (std::getline(ss, matcher_str, ',')) {
            if (matcher_str.empty()) {
                continue;
            }
            trace_matchers.push_back(parseTraceMatcher(matcher_str));
        }
    }

    auto trace_table = parseTraceOptions(project, trace_matchers);

    if ((!break_file.empty() || !break_line.empty()) && trace_matchers.empty()) {
        throw VulException("Breakpoints require tracing to be enabled with --trace or --tracefile");
    }

    std::vector<VulBreakPointSpec> break_specs;
    if (!break_file.empty() || !break_line.empty()) {
        VulErrorContextGuard _err("parsing breakpoint descriptions");
        break_specs = parseBreakSpecs(break_file, break_line, collectTracedSignalWidths(project, trace_table));
        if (args.break_cycles == 0) {
            throw VulException("Breakpoint history cycles must be greater than 0");
        }
    }

    // gen module
    std::deque<shared_ptr<VulStaticModuleInstance>> bfs_queue;
    std::unordered_set<std::string> generated_module_paths;
    bfs_queue.push_back(project.top_module_instance);
    while (!bfs_queue.empty()) {
        auto mod_instance = bfs_queue.front();
        bfs_queue.pop_front();
        for (const auto &child : mod_instance->children) {
            bfs_queue.push_back(child);
        }

        const std::string decl_path = mod_instance->simDeclPath();
        if (!generated_module_paths.insert(decl_path).second) {
            continue;
        }

        VulErrorContextGuard _err("generating code for module instance: " + mod_instance->simClassName());

        auto codes = simgen::genStaticModuleCodeHpp(*mod_instance, trace_table[mod_instance->instance_id]);
        writeLinesToFile(codes.decl, (out_path / decl_path).string());
        writeLinesToFile(codes.impl, (out_path / (mod_instance->simImplPath())).string());
        for (const auto &res_file : codes.resource_files) {
            std::filesystem::path src_file = proj_path / res_file;
            if (!std::filesystem::exists(src_file) || !std::filesystem::is_regular_file(src_file)) {
                throw VulException("Resource file does not exist: " + src_file.string() + ", used in module: " + mod_instance->module_name);
            }
            std::filesystem::path dst_file = out_path / res_file;
            std::filesystem::create_directories(dst_file.parent_path());
            std::filesystem::copy_file(src_file, dst_file);
        }
    }

    // gen test harness module
    {
        VulErrorContextGuard _err("generating test harness code");

        vector<string> testharness_code = simgen::genStaticTestHarnessHpp(
            project.test_harness, *project.top_module_instance,
            /*enable_tracing=*/trace_matchers.size() > 0,
            break_specs,
            args.break_cycles
        );
        writeLinesToFile(testharness_code, (out_path / project.top_module_instance->parent->simDeclPath()).string());
    }

    // gen VulTestMain.hpp
    {
        VulErrorContextGuard _err("generating VulTestMain.hpp");

        vector<string> testmain_code = simgen::genStaticTestMainHpp(project.top_module_instance);
        writeLinesToFile(testmain_code, (out_path / "VulTestMain.hpp").string());
    }

    // copy vullib runtime files to output directory
    {
        VulErrorContextGuard _err("copying runtime library files");

        std::filesystem::path lib_path(lib_dir);
        if (!std::filesystem::exists(lib_path) || !std::filesystem::is_directory(lib_path)) {
            throw VulException("Library directory does not exist: " + lib_dir);
        }
        for (auto filename : VulLibFiles) {
            std::filesystem::path src_file = lib_path / filename;
            if (!std::filesystem::exists(src_file) || !std::filesystem::is_regular_file(src_file)) {
                throw VulException("Runtime library file does not exist: " + src_file.string());
            }
            std::filesystem::path dst_file = out_path / filename;
            std::filesystem::copy_file(src_file, dst_file);
        }
    }

    // generate build script
    string projname = main_path.stem().string();
    string build_cmd = "g++ -std=c++20 -g -O2 main.cpp -I. -o " + projname;
    std::ofstream build_script((out_path / "build.sh").string());
    if (!build_script.is_open()) {
        throw VulException("Failed to create build script.");
    }
    build_script << "#!/bin/bash\necho \"Building " << projname << "\"\n";
    build_script << "SCRIPT_DIR=\"$(cd \"$(dirname \"${BASH_SOURCE[0]}\")\" && pwd)\"\n";
    build_script << "pushd \"$SCRIPT_DIR\"\n";
    build_script << build_cmd << "\n";
    build_script << "popd\n";
    build_script.close();

    string build_cmd_o3 = "g++ -std=c++20 -O3 main.cpp -I. -o " + projname + "_O3";
    std::ofstream build_script_o3((out_path / "release.sh").string());
    if (!build_script_o3.is_open()) {
        throw VulException("Failed to create O3 build script.");
    }
    build_script_o3 << "#!/bin/bash\necho \"Building " << projname << " with O3 optimization\"\n";
    build_script_o3 << "SCRIPT_DIR=\"$(cd \"$(dirname \"${BASH_SOURCE[0]}\")\" && pwd)\"\n";
    build_script_o3 << "pushd \"$SCRIPT_DIR\"\n";
    build_script_o3 << build_cmd_o3 << "\n";
    build_script_o3 << "popd\n";
    build_script_o3.close();

    string debug_cmd =
        "g++ -std=c++20 -g -O1 "
        "-fsanitize=address,undefined,leak "
        "-fno-omit-frame-pointer "
        "-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion "
        "-Wshadow -Wnull-dereference -Wdouble-promotion -Wformat=2 "
        "-Wundef -Wuninitialized -Werror "
        "main.cpp -I. -o " + projname + "_debug";
    std::ofstream debug_script((out_path / "debug.sh").string());
    if (!debug_script.is_open()) {
        throw VulException("Failed to create debug build script.");
    }
    debug_script << "#!/bin/bash\necho \"Building " << projname << " for debug with sanitizers\"\n";
    debug_script << "SCRIPT_DIR=\"$(cd \"$(dirname \"${BASH_SOURCE[0]}\")\" && pwd)\"\n";
    debug_script << "pushd \"$SCRIPT_DIR\"\n";
    debug_script << debug_cmd << "\n";
    debug_script << "popd\n";
    debug_script.close();

    return 0;
}


int main(int argc, char * argv[]) {

    argparse::ArgumentParser parser("vulsimgen", "VulSim Simulation Generator V1.0");
    parser.add_argument("-t", "--top")
        .help("overrides the top module file path declared by TOP(...) in the main test file")
        .default_value(std::string(""));
    parser.add_argument("-m", "--main")
        .help("sets the main test file for test case generation")
        .default_value(std::string(""))
        .required();
    parser.add_argument("-p", "--project")
        .help("overrides the project directory declared by PROJECT(...) in the main test file")
        .default_value(std::string(""));
    parser.add_argument("-o", "--out")
        .help("sets the output directory for generated code (default: ./simout)")
        .default_value(std::string("./simout"));
    parser.add_argument("-l", "--lib")
        .help("sets the directory for runtime library files (default: ./vullib)")
        .default_value(std::string("./vullib"));
    parser.add_argument("-T", "--tracefile")
        .help("tracing signal matcher file for generating tracing code (optional)")
        .default_value(std::string(""));
    parser.add_argument("--trace")
        .help("tracing signal matcher strings seperated by commas (optional)")
        .default_value(std::string(""));
    parser.add_argument("-B", "--breakfile")
        .help("breakpoint description file, one breakpoint expression per line (optional)")
        .default_value(std::string(""));
    parser.add_argument("--break")
        .help("breakpoint expression string joined by && inside one breakpoint (optional)")
        .default_value(std::string(""));
    parser.add_argument("--breakcycles")
        .help("number of recent cycles buffered for breakpoint waveform dump")
        .scan<'u', uint64_t>()
        .default_value(uint64_t(64));
    parser.add_argument("--dynamic")
        .help("generate dynamic simulation code instead of static code")
        .default_value(false)
        .implicit_value(true);
    
    try {
        parser.parse_args(argc, argv);
    } catch (const std::exception &e) {
        std::cerr << "Argument parsing error: " << e.what() << "\n" << parser.help().str() << std::endl;
        return 1;
    }
    
    string top_file = parser.get<std::string>("--top");
    string main_file = parser.get<std::string>("--main");
    string proj_dir = parser.get<std::string>("--project");
    string out_dir = parser.get<std::string>("--out");
    string lib_dir = parser.get<std::string>("--lib");
    string trace_file = parser.get<std::string>("--tracefile");
    string trace_line = parser.get<std::string>("--trace");
    string break_file = parser.get<std::string>("--breakfile");
    string break_line = parser.get<std::string>("--break");
    uint64_t break_cycles = parser.get<uint64_t>("--breakcycles");
    SimGenArgs args{top_file, main_file, proj_dir, out_dir, lib_dir, trace_file, trace_line, break_file, break_line, break_cycles};

    try{
        return simgenStatic(args);
    } catch (const std::exception &e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
