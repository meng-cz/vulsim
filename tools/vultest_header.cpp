
#include "vcpp.hpp"

#include "simgen.h"

int main(int argc, char * argv[]) {

    if (argc < 3) {
        std::cerr << "Usage: vultest <header.hpp> <out_dir>" << std::endl;
        return 1;
    }

    std::string header_path = argv[1];
    std::string out_dir = argv[2];

    if (!std::filesystem::exists(out_dir)) {
        std::filesystem::create_directories(out_dir);
    }

    shared_ptr<VulConfigLib> configlib = std::make_shared<VulConfigLib>();
    shared_ptr<VulBundleLib> bundlelib = std::make_shared<VulBundleLib>();

    vector<string> code = _readFileLines(header_path);
    auto strip = _stripComments(code);
    _parseHeader(strip.lines, configlib, bundlelib);

    for (const auto &entry : configlib->config_items) {
        const auto &item = entry.second;
        std::cout << "Config item: " << item.item.name << ", value: " << item.item.value << ", real value: " << item.real_value << std::endl;
    }

    for (const auto &entry : bundlelib->bundles) {
        const auto &item = entry.second;
        std::cout << "Bundle item: " << item.item.name << ", is_alias: " << item.item.is_alias << ", members: " << item.item.members.size() << std::endl;
        for (const auto &member : item.item.members) {
            std::cout << "  Member: " << member.typeString() << " " << member.name << ", value: " << member.value << std::endl;
        }
    }

    vector<string> code_lines;
    ErrorMsg err;

    err = simgen::genConfigHeaderCode(*configlib, code_lines);
    if (err.error()) {
        std::cerr << "Error generating config header code: " << err.msg << std::endl;
        return 1;
    }
    std::string config_header_path = out_dir + "/config.h";
    std::ofstream config_header_file(config_header_path);
    if (!config_header_file.is_open()) {
        std::cerr << "Error: Failed to open file for writing: " << config_header_path << std::endl;
        return 1;
    }
    for (const auto &line : code_lines) {
        config_header_file << line;
        if (line.empty() || line.back() != '\n') {
            config_header_file << "\n";
        }
    }
    config_header_file.close();

    err = simgen::genBundleHeaderCode(*bundlelib, code_lines);
    if (err.error()) {
        std::cerr << "Error generating bundle header code: " << err.msg << std::endl;
        return 1;
    }
    std::string bundle_header_path = out_dir + "/bundle.h";
    std::ofstream bundle_header_file(bundle_header_path);
    if (!bundle_header_file.is_open()) {
        std::cerr << "Error: Failed to open file for writing: " << bundle_header_path << std::endl;
        return 1;
    }
    for (const auto &line : code_lines) {
        bundle_header_file << line;
        if (line.empty() || line.back() != '\n') {
            bundle_header_file << "\n";
        }
    }
    bundle_header_file.close();

    return 0;
}
