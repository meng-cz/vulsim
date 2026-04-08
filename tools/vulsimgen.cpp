
#include "vcpp.hpp"

#include "simgen.h"
#include "argparse.hpp"

#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

inline static void writeLinesToFile(const std::vector<std::string> &lines, const std::string &filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Failed to open file for writing: " << filepath << std::endl;
        return;
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

    argparse::ArgumentParser parser("vulsimgen", "VulSim Simulation Generator V1.0");
    parser.add_argument("-t", "--top")
        .help("sets the top module file")
        .required();
    parser.add_argument("-m", "--main")
        .help("sets the main test file for test case generation (optional)")
        .default_value(std::string(""));
    parser.add_argument("-o", "--out")
        .help("sets the output directory for generated code (default: ./simout)")
        .default_value(std::string("./simout"));
    parser.add_argument("-l", "--lib")
        .help("sets the directory for runtime library files (default: ./vullib)")
        .default_value(std::string("./vullib"));
    
    try {
        parser.parse_args(argc, argv);
    } catch (const std::exception &e) {
        std::cerr << "Argument parsing error: " << e.what() << std::endl;
        std::cerr << parser.help().str() << std::endl;
        return 1;
    }
    
    string top_file = parser.get<std::string>("--top");
    string main_file = parser.get<std::string>("--main");
    string out_dir = parser.get<std::string>("--out");
    string lib_dir = parser.get<std::string>("--lib");
    std::filesystem::path top_path(top_file);
    if (!std::filesystem::exists(top_path) || !std::filesystem::is_regular_file(top_path)) {
        std::cerr << "Error: Top module file does not exist: " << top_file << std::endl;
        return 1;
    }
    std::filesystem::path proj_dir = top_path.parent_path();

    std::filesystem::path main_path;
    if (!main_file.empty()) {
        main_path = std::filesystem::path(main_file);
        if (!std::filesystem::exists(main_path) || !std::filesystem::is_regular_file(main_path)) {
            std::cerr << "Error: Main test file does not exist: " << main_file << std::endl;
            return 1;
        }
        // main file must be in the same directory as the top module file
        if (main_path.parent_path() != proj_dir) {
            std::cerr << "Error: Main test file must be in the same directory as the top module file" << std::endl;
            return 1;
        }
    }

    VulProject project = parseVcppProject(proj_dir.string(), top_path.filename().string(), main_path.filename().string());

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

    vector<string> code_lines;
    ErrorMsg err = simgen::genConfigHeaderCode(*project.configlib, code_lines);
    if (err.error()) {
        std::cerr << "Error generating config header code: " << err.msg << std::endl;
        return 1;
    }
    writeLinesToFile(code_lines, (out_path / "config.h").string());

    err = simgen::genBundleHeaderCode(*project.bundlelib, code_lines);
    if (err.error()) {
        std::cerr << "Error generating bundle header code: " << err.msg << std::endl;
        return 1;
    }
    writeLinesToFile(code_lines, (out_path / "bundle.h").string());

    // gen module
    for (const auto &mod_entry : project.modulelib->modules) {
        shared_ptr<VulModule> mod_ptr = dynamic_pointer_cast<VulModule>(mod_entry.second);
        if (!mod_ptr) {
            std::cerr << "Error: Module is not a VulModule: " << mod_entry.first << std::endl;
            return 1;
        }
        err = simgen::genModuleCodeHpp(*mod_ptr, code_lines, project.configlib, project.modulelib);
        if (err.error()) {
            std::cerr << "Error generating module code for module " << mod_entry.first << ": " << err.msg << std::endl;
            return 1;
        }
        writeLinesToFile(code_lines, (out_path / (mod_entry.first + ".hpp")).string());
    }

    // gen test harness module
    auto iter = project.modulelib->modules.find(project.top_module);
    if (iter == project.modulelib->modules.end()) {
        std::cerr << "Error: Top module not found in module library: " << project.top_module << std::endl;
        return 1;
    }
    shared_ptr<VulModule> top_module_ptr = dynamic_pointer_cast<VulModule>(iter->second);
    if (!project.test_module.empty()) {
        auto test_iter = project.test_harness.find(project.test_module);
        if (test_iter == project.test_harness.end()) {
            std::cerr << "Error: Test module not found: " << project.test_module << std::endl;
            return 1;
        }
        err = simgen::genTestHarnessHpp(test_iter->second, *top_module_ptr, code_lines);
        if (err.error()) {
            std::cerr << "Error generating test module code: " << err.msg << std::endl;
            return 1;
        }
        writeLinesToFile(code_lines, (out_path / ("VulTestMain.hpp")).string());
    }

    // copy vullib runtime files to output directory
    vector<std::string> runtime_files = {
        "common.h",
        "vullib.h",
        "main.cpp",
        "pipe.hpp",
        "storage.hpp",
        "uint.hpp",
    };
    std::filesystem::path lib_path(lib_dir);
    if (!std::filesystem::exists(lib_path) || !std::filesystem::is_directory(lib_path)) {
        std::cerr << "Error: Library directory does not exist: " << lib_dir << std::endl;
        return 1;
    }
    for (const auto &filename : runtime_files) {
        std::filesystem::path src_file = lib_path / filename;
        if (!std::filesystem::exists(src_file) || !std::filesystem::is_regular_file(src_file)) {
            std::cerr << "Error: Runtime library file does not exist: " << src_file << std::endl;
            return 1;
        }
        std::filesystem::path dst_file = out_path / filename;
        std::filesystem::copy_file(src_file, dst_file);
    }

    // generate build script
    string build_cmd = "g++ -std=c++20 -O2 main.cpp -o " + project.name;
    std::ofstream build_script((out_path / "build.sh").string());
    if (!build_script.is_open()) {
        std::cerr << "Error: Failed to create build script." << std::endl;
        return 1;
    }
    build_script << "#!/bin/bash\necho \"Building " << project.name << "\"\n" << build_cmd << "\n";
    build_script.close();

    return 0;
}

