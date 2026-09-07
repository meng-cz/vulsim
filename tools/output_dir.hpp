#pragma once

#include <filesystem>
#include <iostream>
#include <string>

inline bool prepareOutputDirectory(
    const std::filesystem::path &out_path,
    const std::string &out_dir,
    bool force
) {
    if (!std::filesystem::exists(out_path)) {
        std::filesystem::create_directories(out_path);
        return true;
    }
    if (!std::filesystem::is_directory(out_path)) {
        std::cerr << "Error: Output path is not a directory: " << out_dir << std::endl;
        return false;
    }
    if (std::filesystem::is_empty(out_path)) {
        return true;
    }

    if (!force) {
        std::cout << "Output directory is not empty: " << out_dir << std::endl;
        std::cout << "Do you want to clear the output directory before generating code? (y/n) ";
        char choice = '\0';
        if (!(std::cin >> choice) || (choice != 'y' && choice != 'Y')) {
            std::cerr << "Error: Output directory is not empty. Use -f/--force, clear it, or choose a different output directory." << std::endl;
            return false;
        }
    }

    std::filesystem::remove_all(out_path);
    std::filesystem::create_directories(out_path);
    return true;
}
