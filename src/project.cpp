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

#include "project.h"
#include "platform/env.hpp"

#include <filesystem>

void VulProject::initEnvs() {

    import_paths.clear();
#ifdef _WIN32
    project_local_path = ".\\vul_project_local";
    import_paths.push_back(".\\import");
#else
    project_local_path = "~/.vul_project_local";
    import_paths.push_back("./import");
#endif

    // load project local path
    auto pv = getEnvVar(std::string(EnvVulProjectPath));
    if (pv.has_value()) {
        const std::string &s = pv.value();
        try {
            project_local_path = std::filesystem::absolute(s).string();
        } catch (...) {
            project_local_path = s;
        }
    }

    // load import paths
    import_paths.clear();
    auto pi = getEnvVar(std::string(EnvVulImportPath));
    if (!pi.has_value()) return;
    const std::string &raw = pi.value();

    auto is_delim = [](char c) { return c == ';' || c == ':' || c == ','; };

    size_t start = 0;
    while (start < raw.size()) {
        size_t i = start;
        while (i < raw.size() && !is_delim(raw[i])) ++i;
        std::string part = raw.substr(start, i - start);
        // trim whitespace
        const char *ws = " \t\r\n";
        size_t left = part.find_first_not_of(ws);
        if (left != std::string::npos) {
            size_t right = part.find_last_not_of(ws);
            part = part.substr(left, right - left + 1);
            if (!part.empty()) {
                try {
                    import_paths.push_back(std::filesystem::absolute(part).string());
                } catch (...) {
                    import_paths.push_back(part);
                }
            }
        }
        start = i + 1;
    }
}

string VulProject::findProjectPathInLocalLibrary(const ProjectName &name) {
    try {
        std::filesystem::path local_path = std::filesystem::absolute(project_local_path);
        string target_file = name + ".vul";
        for (const auto &entry : std::filesystem::recursive_directory_iterator(local_path)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().filename() == target_file) {
                return entry.path().string();
            }
        }
    } catch (...) {
        // ignore errors
    }
    return "";
}

static unordered_map<OperationName, OperationFactory> operationFactories;

bool VulProject::registerOperation(const OperationName &op_name, const OperationFactory &factory) {
    auto iter = operationFactories.find(op_name);
    if (iter != operationFactories.end()) {
        return false;
    }
    operationFactories[op_name] = factory;
    return true;
}

vector<OperationName> VulProject::listAllRegisteredOperations() {
    vector<OperationName> op_names;
    for (const auto& [name, factory] : operationFactories) {
        op_names.push_back(name);
    }
    return op_names;
}

VulOperationResponse VulProject::doOperation(const VulOperationPackage &op) {
    auto iter = operationFactories.find(op.name);
    if (iter == operationFactories.end()) {
        return EStr(EItemOPInvalid, string("Unsupported operation: ") + op.name);
    }
    auto &factory = iter->second;
    auto operation = factory(op);
    if (!operation) {
        return EStr(EItemOPInvalid, string("Failed to create operation instance for: ") + op.name);
    }
    VulOperationResponse resp = operation->execute(*this);
    if (resp.code == 0 && operation->is_modify()) {
        // record operation for undo/redo
        if (operation->is_undoable()) {
            operation_undo_history.push_back(std::move(operation));
            operation_redo_history.clear();
        } else {
            operation_undo_history.clear();
            operation_redo_history.clear();
        }
    }
    return resp;
}

string VulProject::undoLastOperation() {
    if (operation_undo_history.empty()) {
        return "";
    }
    auto &operation = operation_undo_history.back();
    if (operation->is_undoable()) {
        string err = operation->undo(*this);
        if (!err.empty()) {
            return err;
        }
        operation_redo_history.push_back(std::move(operation));
        operation_undo_history.pop_back();
    } else {
        // should not happen
        operation_undo_history.clear();
        operation_redo_history.clear();
        return "Last operation is not undoable.";
    }
    return "";
}
string VulProject::redoLastOperation() {
    if (operation_redo_history.empty()) {
        return "";
    }
    auto &operation = operation_redo_history.back();
    VulOperationResponse resp = operation->execute(*this);
    if (resp.code == 0) {
        operation_undo_history.push_back(std::move(operation));
        operation_redo_history.pop_back();
    } else {
        // should not happen
        operation_undo_history.clear();
        operation_redo_history.clear();
        return resp.msg;
    }
    return "";
}

vector<string> VulProject::listHelpForOperation(const OperationName &op_name) const {
    auto iter = operationFactories.find(op_name);
    if (iter == operationFactories.end()) {
        vector<string> ret;
        ret.push_back("No help available for unknown operation: " + op_name);
        return ret;
    }
    auto &factory = iter->second;
    auto operation = factory(VulOperationPackage());
    if (!operation) {
        vector<string> ret;
        ret.push_back("Failed to create operation instance for: " + op_name);
        return ret;
    }
    return operation->help();
}

VulProjectOperation::VulProjectOperation(const VulOperationPackage &op) : op(op) {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm* ptm = std::localtime(&tt);
    std::ostringstream oss;
    oss << std::put_time(ptm, "%Y-%m-%d %H:%M:%S");
    timestamp = oss.str();
}
