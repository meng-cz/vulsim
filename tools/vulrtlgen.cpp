
#include "vcpp.hpp"

#include "simgen.h"
#include "rtlgen.h"
#include "argparse.hpp"
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

int main(int argc, char * argv[]) {

    argparse::ArgumentParser parser("vulsimgen", "VulSim Verilog Generator V1.0");
    parser.add_argument("-t", "--top")
        .help("sets the top module file")
        .required();
    parser.add_argument("-o", "--out")
        .help("sets the output directory for generated code (default: ./rtlout)")
        .default_value(std::string("./rtlout"));
    parser.add_argument("-l", "--lib")
        .help("sets the directory for runtime library files (default: ./vullib)")
        .default_value(std::string("./vullib"));
    parser.add_argument("-p", "--project")
        .help("sets the project directory (default: parent directory of the top module file)")
        .default_value(std::string(""));

    try {
        parser.parse_args(argc, argv);
    } catch (const std::exception &e) {
        std::cerr << "Argument parsing error: " << e.what() << std::endl;
        std::cerr << parser.help().str() << std::endl;
        return 1;
    }

    string top_file = parser.get<std::string>("--top");
    string out_dir = parser.get<std::string>("--out");
    string proj_dir = parser.get<std::string>("--project");
    string lib_dir = parser.get<std::string>("--lib");

    std::filesystem::path top_path(top_file);
    if (!std::filesystem::exists(top_path) || !std::filesystem::is_regular_file(top_path)) {
        std::cerr << "Error: Top module file does not exist: " << top_file << std::endl;
        return 1;
    }
    if (proj_dir.empty()) {
        proj_dir = top_path.parent_path().string();
    }

    VulErrorContextGuard _err{"generating project from " + proj_dir};
    VulStaticProject project = parseVcppStaticProject(proj_dir, top_path.string(), "");

    std::filesystem::path out_path(out_dir);
    if (!std::filesystem::exists(out_path)) {
        std::filesystem::create_directories(out_path);
    } else if (!std::filesystem::is_directory(out_path)) {
        std::cerr << "Error: Output path is not a directory: " << out_dir << std::endl;
        return 1;
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
            std::cerr << "Error: Output directory is not empty. Please clear the output directory or choose a different output directory." << std::endl;
            return 1;
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

        const std::string hls_path = mod_instance->rtlHlsPath();
        if (!generated_module_paths.insert(hls_path).second) {
            continue;
        }

        VulErrorContextGuard _err("generating code for module instance: " + mod_instance->simClassName());

        auto codes = rtlgen::genModuleRTL(*mod_instance, project.global_configlib, project.global_bundlelib);
        writeLinesToFile(codes.logic_hls_codes, (out_path / hls_path).string());
        writeLinesToFile(codes.rtl_skeleten_codes, (out_path / (mod_instance->rtlSvPath())).string());
        for (const auto &res_file : codes.resource_files) {
            std::filesystem::path src_file = std::filesystem::path(proj_dir) / res_file;
            if (!std::filesystem::exists(src_file) || !std::filesystem::is_regular_file(src_file)) {
                throw VulException("Resource file does not exist: " + src_file.string() + ", used in module: " + mod_instance->module_name);
            }
            std::filesystem::path dst_file = out_path / res_file;
            std::filesystem::create_directories(dst_file.parent_path());
            std::filesystem::copy_file(src_file, dst_file);
        }
    }

    // copy vullib files to output directory
    {
        VulErrorContextGuard _err("copying vul library files");

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

    return 0;
}
