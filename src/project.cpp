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

#include "json.hpp"

using nlohmann::json;

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
    const char *pv = std::getenv("VUL_PROJECT_PATH");
    if (pv && *pv) {
        std::string s(pv);
        try {
            project_local_path = std::filesystem::absolute(s).string();
        } catch (...) {
            project_local_path = s;
        }
    }

    // load import paths
    import_paths.clear();
    const char *pi = std::getenv("VUL_IMPORT_PATH");
    if (!pi || !*pi) return;
    std::string raw(pi);

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

VulOperationPackage serializeOperationPackageFromJSON(const string &json_str) {
    VulOperationPackage op;
    json j = json::parse(json_str);
    op.name = j.at("name").get<OperationName>();
    if (j.contains("args")) {
        for (auto& [key, value] : j.at("args").items()) {
            op.args[key] = value.get<OperationArg>();
        }
    }
    return op;
}

string serializeOperationResponseToJSON(const VulOperationResponse &response) {
    json j;
    j["code"] = response.code;
    j["msg"] = response.msg;
    json args_json;
    for (const auto& [key, value] : response.results) {
        args_json[key] = value;
    }
    j["results"] = args_json;
    json list_args_json;
    for (const auto& [key, value] : response.list_results) {
        list_args_json[key] = value;
    }
    j["list_results"] = list_args_json;
    return j.dump(1, ' ');
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

void VulProject::undoLastOperation() {
    if (operation_undo_history.empty()) {
        return;
    }
    auto &operation = operation_undo_history.back();
    if (operation->is_undoable()) {
        operation->undo(*this);
        operation_redo_history.push_back(std::move(operation));
        operation_undo_history.pop_back();
    } else {
        // should not happen
        operation_undo_history.clear();
        operation_redo_history.clear();
    }
}
void VulProject::redoLastOperation() {
    if (operation_redo_history.empty()) {
        return;
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
    }
}




