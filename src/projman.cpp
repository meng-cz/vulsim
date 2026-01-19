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

#include "projman.h"
#include "configuration.h"

#include <filesystem>

namespace fs = std::filesystem;

VulProjectPathManager::VulProjectPathManager() {
    lib_root_path = getConfigValue(string(EnvVulProjectPath), "projects");
    string import_path_str = getConfigValue(string(EnvVulImportPath), "import");
    size_t start = 0;
    while (start < import_path_str.size()) {
        size_t i = import_path_str.find_first_of(";,:", start);
        if (i == string::npos) {
            import_path.push_back(import_path_str.substr(start));
            break;
        } else {
            import_path.push_back(import_path_str.substr(start, i - start));
            start = i + 1;
        }
    }
}

shared_ptr<VulProjectPathManager> VulProjectPathManager::getInstance() {
    static shared_ptr<VulProjectPathManager> instance = shared_ptr<VulProjectPathManager>(new VulProjectPathManager());
    return instance;
}

bool VulProjectPathManager::projectExists(const VulProjectName &name) {
    fs::path project_path = fs::path(lib_root_path) / name;
    fs::path project_file = project_path / (name + ".vul");
    return fs::exists(project_file);
}

optional<VulProjectPath> VulProjectPathManager::createNewProjectPath(const VulProjectName &name) {
    if (projectExists(name)) {
        return std::nullopt;
    }
    fs::path project_path = fs::path(lib_root_path) / name;
    if (fs::exists(project_path)) {
        // directory exists but no project.vul file, overwrite
        fs::remove_all(project_path);
    }
    fs::create_directories(project_path);
    return project_path.string();
}

bool VulProjectPathManager::removeProjectPath(const VulProjectName &name) {
    if (!projectExists(name)) {
        return false;
    }
    fs::path project_path = fs::path(lib_root_path) / name;
    fs::remove_all(project_path);
    return true;
}

vector<VulProjectName> VulProjectPathManager::getAllProjectNames() {
    vector<VulProjectName> project_names;
    for (const auto &entry : fs::directory_iterator(lib_root_path)) {
        if (entry.is_directory()) {
            VulProjectName name = entry.path().filename().string();
            if (projectExists(name)) {
                project_names.push_back(name);
            }
        }
    }
    return project_names;
}

unordered_map<VulProjectName, VulProjectPath> VulProjectPathManager::getAllProjects() {
    unordered_map<VulProjectName, VulProjectPath> projects;
    for (const auto &name : getAllProjectNames()) {
        auto path_opt = getProjectPath(name);
        if (path_opt.has_value()) {
            projects[name] = path_opt.value();
        }
    }
    return projects;
}

optional<VulProjectPath> VulProjectPathManager::getProjectPath(const VulProjectName &name) {
    if (!projectExists(name)) {
        return std::nullopt;
    }
    fs::path project_path = fs::path(lib_root_path) / name;
    return project_path.string();
}

optional<vector<VulProjectRunID>> VulProjectPathManager::getProjectRunIDs(const VulProjectName &name) {
    if (!projectExists(name)) {
        return std::nullopt;
    }
    fs::path project_path = fs::path(lib_root_path) / name / "runs";
    if (!fs::exists(project_path) || !fs::is_directory(project_path)) {
        return vector<VulProjectRunID>();
    }
    vector<VulProjectRunID> run_ids;
    for (const auto &entry : fs::directory_iterator(project_path)) {
        if (entry.is_directory()) {
            run_ids.push_back(entry.path().filename().string());
        }
    }
    return run_ids;
}

optional<VulProjectPath> VulProjectPathManager::getProjectRunPath(const VulProjectName &name, const VulProjectRunID &run_id) {
    if (!projectExists(name)) {
        return std::nullopt;
    }
    fs::path run_path = fs::path(lib_root_path) / name / "runs" / run_id;
    if (!fs::exists(run_path) || !fs::is_directory(run_path)) {
        return std::nullopt;
    }
    return run_path.string();
}

optional<VulProjectPath> VulProjectPathManager::createAndInitProjectRunPath(const VulProjectName &name, const VulProjectRunID &run_id) {
    if (!projectExists(name)) {
        return std::nullopt;
    }
    fs::path project_path = fs::path(lib_root_path) / name;
    fs::path run_path = fs::path(lib_root_path) / name / "runs" / run_id;
    if (fs::exists(run_path)) {
        return std::nullopt;
    }
    fs::create_directories(run_path);
    // copy project.vul, configlib.xml, bundlelib.xml, modules/ into run_path
    fs::copy_file(project_path / (name + ".vul"), run_path / (name + ".vul"));
    fs::copy_file(project_path / "configlib.xml", run_path / "configlib.xml");
    fs::copy_file(project_path / "bundlelib.xml", run_path / "bundlelib.xml");
    fs::path modules_src_path = project_path / "modules";
    fs::path modules_dst_path = run_path / "modules";
    if (fs::exists(modules_src_path) && fs::is_directory(modules_src_path)) {
        fs::create_directories(modules_dst_path);
        for (const auto &entry : fs::directory_iterator(modules_src_path)) {
            if (entry.is_regular_file()) {
                fs::copy_file(entry.path(), modules_dst_path / entry.path().filename());
            }
        }
    }
    return run_path.string();
}

optional<VulProjectPath> VulProjectPathManager::getImportModulePath(const string &module_name) {
    for (const auto &import_root_path : import_path) {
        fs::path import_module_path = fs::path(import_root_path) / module_name / (module_name + ".vulmod");
        if (!fs::exists(import_module_path) || !fs::is_regular_file(import_module_path)) {
            continue;
        }
        return import_module_path.string();
    }
    return std::nullopt;
}
