
#include "vcpp.hpp"

#include "simgen.h"
#include "rtlgen.h"
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

    argparse::ArgumentParser parser("vulsimgen", "VulSim Verilog Generator V1.0");
    parser.add_argument("-t", "--top")
        .help("sets the top module file")
        .required();
    parser.add_argument("-o", "--out")
        .help("sets the output directory for generated code (default: ./rtlout)")
        .default_value(std::string("./rtlout"));

    try {
        parser.parse_args(argc, argv);
    } catch (const std::exception &e) {
        std::cerr << "Argument parsing error: " << e.what() << std::endl;
        std::cerr << parser.help().str() << std::endl;
        return 1;
    }

    string top_file = parser.get<std::string>("--top");
    string out_dir = parser.get<std::string>("--out");

    std::filesystem::path top_path(top_file);
    if (!std::filesystem::exists(top_path) || !std::filesystem::is_regular_file(top_path)) {
        std::cerr << "Error: Top module file does not exist: " << top_file << std::endl;
        return 1;
    }
    std::filesystem::path proj_dir = top_path.parent_path();

    VulProject project = parseVcppProject(proj_dir.string(), top_path.filename().string(), "");

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

    // prepare data for RTL generation
    unordered_map<ConfigName, ConfigRealValue> calculated_configlib;
    BundleTable bundle_table;

    for (const auto &conf_entry : project.configlib->config_items) {
        const string &conf_name = conf_entry.first;
        const ConfigRealValue &real_value = conf_entry.second.real_value;
        calculated_configlib[conf_name] = real_value;
    }
    for (const auto &bundle_entry : project.bundlelib->bundles) {
        const string &bundle_name = bundle_entry.first;
        VulBundleItem item = bundle_entry.second.item;
        ErrorMsg err = calculateBundleConstexprValue(item, *project.configlib);
        if (err.error()) {
            std::cerr << "Error calculating constexpr value for bundle " << bundle_name << ": " << err.msg << std::endl;
            return 1;
        }
        bundle_table[bundle_name] = item;
    }
    
    for (const auto &module_entry : project.modulelib->modules) {
        const string &module_name = module_entry.first;
        std::cout << "Generating HLS Main Function for module: " << module_name << std::endl;
        shared_ptr<VulModule> mod_ptr = dynamic_pointer_cast<VulModule>(module_entry.second);
        if (!mod_ptr) {
            std::cerr << "Error: Module is not a VulModule: " << module_name << std::endl;
            return 1;
        }
        unordered_map<ConfigName, ConfigValue> config_overrides;
        unordered_map<ConfigName, ConfigRealValue> local_calculated_configlib = calculated_configlib;
        BundleTable local_bundle_table = bundle_table;
        for (const auto &local_conf_entry : mod_ptr->local_consts) {
            const string &conf_name = local_conf_entry.first;
            const ConfigValue &conf_value = local_conf_entry.second.value;
            ConfigRealValue real_value;
            unordered_set<ConfigName> seen_configs;
            ErrorMsg err = project.configlib->calculateConfigExpression(conf_value, local_calculated_configlib, real_value, seen_configs);
            if (err.error()) {
                std::cerr << "Error calculating config expression for local const " << conf_name << " in module " << module_name << ": " << err.msg << std::endl;
                return 1;
            }
            config_overrides[conf_name] = conf_value;
            local_calculated_configlib[conf_name] = real_value;
        }
        for (const auto &local_bundle_entry : mod_ptr->local_bundles) {
            const string &bundle_name = local_bundle_entry.first;
            VulBundleItem item = local_bundle_entry.second;
            ErrorMsg err = calculateBundleConstexprValue(item, *project.configlib, local_calculated_configlib);
            if (err.error()) {
                std::cerr << "Error calculating constexpr value for local bundle " << bundle_name << " in module " << module_name << ": " << err.msg << std::endl;
                return 1;
            }
            local_bundle_table[bundle_name] = item;
        }

        vector<string> hls_code_lines;
        ErrorMsg err = rtlgen::genHLSMainFunc(
            *mod_ptr,
            local_bundle_table,
            config_overrides,
            local_calculated_configlib,
            project,
            hls_code_lines
        );
        if (err.error()) {
            std::cerr << "Error generating HLS main function for module " << module_name << ": " << err.msg << std::endl;
            return 1;
        }

        std::filesystem::path module_out_path = out_path / (rtlgen::LogicSubModuleName(module_name) + ".cpp");
        writeLinesToFile(hls_code_lines, module_out_path.string());
    }
    

    return 0;

}

