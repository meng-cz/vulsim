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

#include "simman.h"
#include "logsocket.h"
#include "simgen.h"
#include "configuration.h"
#include "projman.h"

#include "platform/time.hpp"

#include <filesystem>
#include <fstream>

shared_ptr<SimulationManager> SimulationManager::getInstance() {
    static shared_ptr<SimulationManager> instance(new SimulationManager());
    return instance;
}

ErrorMsg SimulationManager::startTask(
    const string &project_name,
    const string &run_id,
    const string &vullib_path,
    std::optional<GenerationConfig> generation_config,
    std::optional<CompilationConfig> compilation_config,
    std::optional<SimulationConfig> simulation_config
) {
    {
        std::lock_guard<std::mutex> lk(state_mtx);
        if (state.running) {
            return EStr(EGENAlreadyRunning, "A simulation task is already running.");
        }
    }

    auto projman = VulProjectPathManager::getInstance();
    if (!projman->getProjectPath(project_name).has_value()) {
        return EStr(EGENInvalidProject, "Project '" + project_name + "' not found.");
    }
    auto _run_path = projman->getProjectRunPath(project_name, run_id);
    if (!_run_path.has_value()) {
        _run_path = projman->createAndInitProjectRunPath(project_name, run_id);
        if (!_run_path.has_value()) {
            return EStr(EGENInvalidProject, "Failed to create run path for project '" + project_name + "', run ID '" + run_id + "'.");
        }
    }
    string proj_run_path = _run_path.value();

    project_snapshot = std::make_shared<VulProject>();
    ErrorMsg err = project_snapshot->load(proj_run_path, project_name);
    if (err.error()) {
        return EStr(EGENInvalidProject, "Failed to load project '" + project_name + "': " + err.msg);
    }

    bool do_gen = generation_config.has_value();
    bool do_comp = compilation_config.has_value();
    bool do_sim = simulation_config.has_value();
    if (!do_gen && !do_comp && !do_sim) {
        return EStr(EGENNoTaskSpecified, "No generation, compilation, or simulation task specified.");
    }
    if (do_sim && !do_comp) {
        // check whether compilation is already done (rundir/build/.finished)
        std::filesystem::path build_finish_file = std::filesystem::path(proj_run_path) / "build" / ".finished";
        if (!std::filesystem::exists(build_finish_file)) {
            return EStr(EGENRunFormerStage, "Simulation task requires compilation task to be specified.");
        }
    }
    if (do_comp && !do_gen) {
        // check whether generation is already done (rundir/generation/.finished)
        std::filesystem::path gen_finish_file = std::filesystem::path(proj_run_path) / "generation" / ".finished";
        if (!std::filesystem::exists(gen_finish_file)) {
            return EStr(EGENRunFormerStage, "Compilation task requires generation task to be specified.");
        }
    }

    state = SimStateInfo();
    state.project_name = project_name;
    state.run_id = run_id;
    state.vullib_path = vullib_path;
    if (generation_config.has_value()) {
        state.generation_config = generation_config.value();
        state.process_generation = true;
    } else {
        state.process_generation = false;
    }
    if (compilation_config.has_value()) {
        state.compilation_config = compilation_config.value();
        state.process_compilation = true;
    } else {
        state.process_compilation = false;
    }
    if (simulation_config.has_value()) {
        state.simulation_config = simulation_config.value();
        state.process_simulation = true;
    } else {
        state.process_simulation = false;
    }
    state.running = true;
    state.errored = false;
    state.starttime_us = getCurrentTimeUs();

    all_cancel_flag.store(false, std::memory_order_release);
    sim_thread = std::thread(&SimulationManager::simulationThreadFunc, this);
    return ErrorMsg();
}

ErrorMsg SimulationManager::cancelTask() {
    std::lock_guard<std::mutex> lk(state_mtx);
    all_cancel_flag.store(true, std::memory_order_release);
    if (subprocess) {
        subprocess->terminate();
    }
    return ErrorMsg();
}

void SimulationManager::simulationThreadFunc() {
    if (state.process_generation) {
        _generation();
    }
    if (all_cancel_flag.load(std::memory_order_acquire)) {
        return;
    }
    {
        std::lock_guard<std::mutex> lk(state_mtx);
        if (state.errored) {
            state.running = false;
            return;
        }
    }
    if (state.process_compilation) {
        _compilation();
    }
    if (all_cancel_flag.load(std::memory_order_acquire)) {
        return;
    }
    {
        std::lock_guard<std::mutex> lk(state_mtx);
        if (state.errored) {
            state.running = false;
            return;
        }
    }
    if (state.process_simulation) {
        _simulation();
    }
    {
        std::lock_guard<std::mutex> lk(state_mtx);
        state.running = false;
    }
}

uint64_t SimulationManager::_startChildProcess(const string &exe, const vector<string> &args, LogSocketCategory log_category) {
    uint64_t pid = getCurrentTimeUs();
    ChildProcessConfig config;
    config.executable_path = exe;
    config.arguments = args;
    config.stdout_callback = [log_category](const string &output_line) {
        logSocketInfo(log_category, output_line);
    };
    config.stderr_callback = [log_category](const string &error_line) {
        logSocketError(log_category, error_line);
    };
    {
        std::lock_guard<std::mutex> lk(state_mtx);
        subprocess = std::make_unique<ChildProcessRunner>(config);
        subprocess->start();
    }
    return pid;
}

int32_t SimulationManager::_waitChildProcess(uint64_t id) {
    std::lock_guard<std::mutex> lk(state_mtx);
    if (!subprocess) {
        return -1;
    }
    int32_t ret = subprocess->wait();
    subprocess.reset();
    return ret;
}

void SimulationManager::_generation() {
    std::filesystem::path gen_dir(project_snapshot->dirpath);
    gen_dir /= "generation";

    std::filesystem::path gen_finish_file = gen_dir / ".finished";
    if (std::filesystem::exists(gen_finish_file)) {
        std::lock_guard<std::mutex> lk(state_mtx);
        state.generation.finished = true;
        return;
    }

    if (std::filesystem::exists(gen_dir)) {
        std::filesystem::remove_all(gen_dir);
    }
    std::filesystem::create_directories(gen_dir);

    auto on_error = [&](const ErrorMsg &err) {
        logSocketError(LogSocketCategory::Generation, err.msg);
        std::filesystem::path gen_err_file = gen_dir / ".error";
        std::ofstream err_ofs(gen_err_file, std::ios::out | std::ios::app);
        if (err_ofs.is_open()) {
            err_ofs << "Error Code: " << err.code << "\n";
            err_ofs << "Error Message: " << err.msg << "\n";
            err_ofs.close();
        }
        std::lock_guard<std::mutex> lk(state_mtx);
        state.generation.errors.push_back(err);
        state.generation.finished = true;
        state.errored = true;
    };
    auto check_error_and_set = [&](const ErrorMsg &err, const string &prefix) -> bool {
        if (err.error()) {
            on_error(EStr(err.code, prefix + err.msg));
            return true;
        }
        return false;
    };
    auto write_file = [&](const std::filesystem::path &filepath, const vector<string> &lines) -> ErrorMsg {
        std::ofstream ofs(filepath, std::ios::out | std::ios::trunc);
        if (!ofs.is_open()) {
            return EStr(EGENWriteFileFailed, "Failed to open file for writing: " + filepath.string());
        }
        for (const auto &line : lines) {
            ofs << line << "\n";
        }
        ofs.close();
        return ErrorMsg();
    };

    unordered_set<ModuleName> all_modules;
    ModuleName top_module = project_snapshot->top_module;
    auto &modulelist = project_snapshot->modulelib->modules;
    if (modulelist.find(top_module) == modulelist.end()) {
        on_error(EStr(EGENTopNotFound, "Top module '" + top_module + "' not found in project."));
        return;
    }
    {
        all_modules.clear();
        all_modules.insert(top_module);
        std::deque<ModuleName> mod_todo;
        mod_todo.push_back(top_module);
        while (!mod_todo.empty()) {
            ModuleName mod_name = mod_todo.front();
            mod_todo.pop_front();
            if (all_modules.find(mod_name) != all_modules.end()) {
                continue;
            }
            auto mod_iter = modulelist.find(mod_name);
            if (mod_iter == modulelist.end()) {
                on_error(EStr(EGENModuleNotFound, "Module '" + mod_name + "' not found in project."));
                return;
            }
            shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_iter->second);
            if (!mod_ptr) {
                continue;
            }
            for (const auto &inst_pair : mod_ptr->instances) {
                const auto &inst = inst_pair.second;
                mod_todo.push_back(inst.module_name);
            }
        }
    }

    {
        std::lock_guard<std::mutex> lk(state_mtx);
        state.generation.total_modules = all_modules.size() + 3;
        state.generation.finished_modules = 0;
        state.generation.started = true;
        state.starttime_us = getCurrentTimeUs();
    }

    // generate modules
    for (const auto &mod_name : all_modules) {
        if (all_cancel_flag.load(std::memory_order_acquire)) {
            on_error(EStr(EGENCancel, "Generation cancelled."));
            return;
        }
        logSocketInfo(LogSocketCategory::Generation, "Generating module '" + mod_name + "'...");
        auto mod_iter = project_snapshot->modulelib->modules.find(mod_name);
        if (mod_iter == project_snapshot->modulelib->modules.end()) {
            on_error(EStr(EGENModuleNotFound, "Module '" + mod_name + "' not found in project."));
            continue;
        }
        shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_iter->second);
        if (!mod_ptr) {
            // skip external module
            continue;
        }
        vector<string> gen_lines;
        std::filesystem::path gen_file_path = gen_dir / (mod_name + ".hpp");
        ErrorMsg err = simgen::genModuleCodeHpp(*mod_ptr, gen_lines, project_snapshot->configlib, project_snapshot->modulelib);
        if (check_error_and_set(err, "Failed to generate module header for module '" + mod_name + "': ")) {
            continue;
        }
        err = write_file(gen_file_path, gen_lines);
        if (check_error_and_set(err, "Failed to write module header for module '" + mod_name + "': ")) {
            continue;
        }
        {
            std::lock_guard<std::mutex> lk(state_mtx);
            state.generation.finished_modules += 1;
        }
    }

    // generate configlib
    {
        vector<string> gen_lines;
        std::filesystem::path gen_file_path = gen_dir / "config.h";
        ErrorMsg err = simgen::genConfigHeaderCode(*project_snapshot->configlib, gen_lines);
        if (check_error_and_set(err, "Failed to generate config.h: ")) {
            return;
        }
        err = write_file(gen_file_path, gen_lines);
        if (check_error_and_set(err, "Failed to write config.h: ")) {
            return;
        }
        {
            std::lock_guard<std::mutex> lk(state_mtx);
            state.generation.finished_modules += 1;
        }
    }
    // generate bundle.h
    {
        vector<string> gen_lines;
        std::filesystem::path gen_file_path = gen_dir / "bundle.h";
        ErrorMsg err = simgen::genBundleHeaderCode(*project_snapshot->bundlelib, gen_lines);
        if (check_error_and_set(err, "Failed to generate bundle.h: ")) {
            return;
        }
        err = write_file(gen_file_path, gen_lines);
        if (check_error_and_set(err, "Failed to write bundle.h: ")) {
            return;
        }
        {
            std::lock_guard<std::mutex> lk(state_mtx);
            state.generation.finished_modules += 1;
        }
    }
    {
        vector<string> gen_lines;
        std::filesystem::path gen_file_path = gen_dir / "simulation.cpp";
        ErrorMsg err = simgen::genTopSimCpp(
            project_snapshot->top_module,
            vector<ConfigValue>(), // TODO: top module local configs
            gen_lines
        );
        if (check_error_and_set(err, "Failed to generate top simulation source: ")) {
            return;
        }
        err = write_file(gen_file_path, gen_lines);
        if (check_error_and_set(err, "Failed to write top simulation source: ")) {
            return;
        }
        // copy all files in vullib_path into generation_dir
        std::filesystem::path vullib_path(state.vullib_path);
        if (std::filesystem::exists(vullib_path) && std::filesystem::is_directory(vullib_path)) {
            for (const auto &entry : std::filesystem::directory_iterator(vullib_path)) {
                if (entry.is_regular_file()) {
                    std::filesystem::path dest_path = gen_dir / entry.path().filename();
                    std::error_code ec;
                    std::filesystem::copy_file(entry.path(), dest_path, std::filesystem::copy_options::overwrite_existing, ec);
                    if (ec) {
                        check_error_and_set(EStr(EGENWriteFileFailed, "Failed to copy file '" + entry.path().string() + "' to generation directory: " + ec.message()), "");
                    }
                }
            }
        } else {
            check_error_and_set(EStr(EGENWriteFileFailed, "Vullib path '" + vullib_path.string() + "' does not exist or is not a directory."), "");
        }
        {
            std::lock_guard<std::mutex> lk(state_mtx);
            state.generation.finished_modules += 1;
        }
    }

    {
        std::lock_guard<std::mutex> lk(state_mtx);
        state.generation.finished = true;
        state.generation.finished_us = getCurrentTimeUs();
    }
}

void SimulationManager::_compilation() {
    
    std::filesystem::path run_dir(project_snapshot->dirpath);
    std::filesystem::path gen_dir = run_dir / "generation";
    std::filesystem::path build_dir = run_dir / "build";

    auto on_error = [&](const ErrorMsg &err) {
        logSocketError(LogSocketCategory::Compilation, err.msg);
        std::filesystem::path gen_err_file = build_dir / ".error";
        std::ofstream err_ofs(gen_err_file, std::ios::out | std::ios::app);
        if (err_ofs.is_open()) {
            err_ofs << "Error Code: " << err.code << "\n";
            err_ofs << "Error Message: " << err.msg << "\n";
            err_ofs.close();
        }
        std::lock_guard<std::mutex> lk(state_mtx);
        state.compilation.errors.push_back(err);
        state.compilation.finished = true;
        state.errored = true;
    };
    auto check_error_and_set = [&](const ErrorMsg &err, const string &prefix) -> bool {
        if (err.error()) {
            on_error(EStr(err.code, prefix + err.msg));
            return true;
        }
        return false;
    };
    auto write_file = [&](const std::filesystem::path &filepath, const vector<string> &lines) -> ErrorMsg {
        std::ofstream ofs(filepath, std::ios::out | std::ios::trunc);
        if (!ofs.is_open()) {
            return EStr(EGENWriteFileFailed, "Failed to open file for writing: " + filepath.string());
        }
        for (const auto &line : lines) {
            ofs << line << "\n";
        }
        ofs.close();
        return ErrorMsg();
    };
    
    if (std::filesystem::exists(build_dir)) {
        std::filesystem::remove_all(build_dir);
    }
    std::filesystem::create_directories(build_dir);

    std::filesystem::path gen_finish_file = gen_dir / ".finished";
    if (!std::filesystem::exists(gen_finish_file)) {
        on_error(EStr(EGENRunFormerStage, "Generation step not finished."));
        return;
    }
    std::filesystem::path build_finish_file = build_dir / ".finished";
    if (std::filesystem::exists(build_finish_file)) {
        std::lock_guard<std::mutex> lk(state_mtx);
        state.compilation.finished = true;
        return;
    }

    vector<string> cpp_filenames;
    for (const auto &entry : std::filesystem::directory_iterator(gen_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".cpp") {
            cpp_filenames.push_back(entry.path().filename().string());
        }
    }

    string cxx = getConfigValue(string(EnvCXXPath), "g++");
    {
        // check cxx validation
        vector<string> check_cpp_lines;
        check_cpp_lines.push_back("#include <iostream>");
        check_cpp_lines.push_back("int main() { std::cout << \"CXX_CHECK\" << std::endl; return 0; }");
        ErrorMsg write_file_err = write_file(build_dir / "_check.cpp", check_cpp_lines);
        if (check_error_and_set(write_file_err, "Failed to write into build dir: ")) {
            return;
        }

        vector<string> check_args;
        check_args.push_back("-std=c++20");
        check_args.push_back("-O2");
        check_args.push_back("-c");
        check_args.push_back((build_dir / "_check.cpp").string());
        check_args.push_back("-o");
        check_args.push_back((build_dir / "_check.o").string());
        uint64_t pid = _startChildProcess(cxx, check_args, LogSocketCategory::Compilation);
        int32_t ret = _waitChildProcess(pid);
        if (ret != 0) {
            on_error(EStr(EGENInvalidCXX, "C++ compiler '" + cxx + "' failed to compile test file."));
            return;
        }
        // remove check files
        std::filesystem::remove(build_dir / "_check.cpp");
        std::filesystem::remove(build_dir / "_check.o");
    }

    {
        std::lock_guard<std::mutex> lk(state_mtx);
        state.compilation.total_files = cpp_filenames.size();
        state.compilation.finished_files = 0;
        state.compilation.started = true;
        state.starttime_us = getCurrentTimeUs();
    }

    // compile cpp files
    for (const auto &cpp_file : cpp_filenames) {
        if (all_cancel_flag.load(std::memory_order_acquire)) {
            on_error(EStr(EGENCancel, "Compilation cancelled."));
            return;
        }
        logSocketInfo(LogSocketCategory::Compilation, "Compiling source file '" + cpp_file + "'...");
        vector<string> compile_args;
        compile_args.push_back("-std=c++20");
        if (state.compilation_config.release_mode) {
            compile_args.push_back("-O3");
            compile_args.push_back("-DNDEBUG");
        } else {
            compile_args.push_back("-g");
            compile_args.push_back("-O2");
        }
        compile_args.push_back("-I" + gen_dir.string());
        compile_args.push_back("-c");
        compile_args.push_back((gen_dir / cpp_file).string());
        compile_args.push_back("-o");
        string obj_file = cpp_file;
        obj_file.replace(obj_file.end() - 4, obj_file.end(), ".o");
        compile_args.push_back((build_dir / obj_file).string());
        uint64_t pid = _startChildProcess(cxx, compile_args, LogSocketCategory::Compilation);
        int32_t ret = _waitChildProcess(pid);
        if (ret != 0) {
            on_error(EStr(EGENCompilationFailed, "Compilation of source file '" + cpp_file + "' failed."));
            continue;
        }
        {
            std::lock_guard<std::mutex> lk(state_mtx);
            state.compilation.finished_files += 1;
        }
    }

    // linking
    {
        vector<string> link_args;
        link_args.push_back("-std=c++20");
        if (state.compilation_config.release_mode) {
            link_args.push_back("-O3");
            link_args.push_back("-DNDEBUG");
        } else {
            link_args.push_back("-g");
            link_args.push_back("-O2");
        }
        for (const auto &cpp_file : cpp_filenames) {
            string obj_file = cpp_file;
            obj_file.replace(obj_file.end() - 4, obj_file.end(), ".o");
            link_args.push_back((build_dir / obj_file).string());
        }
        link_args.push_back("-o");
        link_args.push_back((build_dir / "nullsim").string());
        uint64_t pid = _startChildProcess(cxx, link_args, LogSocketCategory::Compilation);
        int32_t ret = _waitChildProcess(pid);
        if (ret != 0) {
            on_error(EStr(EGENCompilationFailed, "Linking of simulation executable failed."));
            return;
        }
    }

    {
        std::lock_guard<std::mutex> lk(state_mtx);
        state.compilation.finished = true;
        state.compilation.finished_us = getCurrentTimeUs();
    }

}

void SimulationManager::_simulation() {
    
    std::filesystem::path run_dir(project_snapshot->dirpath);
    std::filesystem::path build_dir = run_dir / "build";

    auto on_error = [&](const ErrorMsg &err) {
        logSocketError(LogSocketCategory::Simulation, err.msg);
        std::lock_guard<std::mutex> lk(state_mtx);
        state.simulation.errors.push_back(err);
        state.simulation.finished = true;
        state.errored = true;
    };
    auto check_error_and_set = [&](const ErrorMsg &err, const string &prefix) -> bool {
        if (err.error()) {
            on_error(EStr(err.code, prefix + err.msg));
            return true;
        }
        return false;
    };

    std::filesystem::path build_finish_file = build_dir / ".finished";
    if (!std::filesystem::exists(build_finish_file)) {
        on_error(EStr(EGENRunFormerStage, "Compilation step not finished."));
        return;
    }

    {
        std::lock_guard<std::mutex> lk(state_mtx);
        state.simulation.started = true;
        state.starttime_us = getCurrentTimeUs();
    }

    vector<string> sim_args;
    uint64_t pid = _startChildProcess((build_dir / "nullsim").string(), sim_args, LogSocketCategory::Simulation);
    int32_t ret = _waitChildProcess(pid);
    if (ret != 0) {
        on_error(EStr(EGENSimulationFailed, "Simulation execution failed."));
        return;
    }
    
    {
        std::lock_guard<std::mutex> lk(state_mtx);
        state.simulation.finished = true;
        state.simulation.finished_us = getCurrentTimeUs();
    }
}

