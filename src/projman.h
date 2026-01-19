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


#pragma once

#include <string>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

using std::string;
using std::optional;
using std::shared_ptr;
using std::unordered_map;
using std::vector;

typedef string VulProjectName;
typedef string VulProjectPath; // path to project root directory that contains project.vul
typedef string VulProjectRunID;

class VulProjectPathManager {

public:

    static shared_ptr<VulProjectPathManager> getInstance();

    bool projectExists(const VulProjectName &name);
    optional<VulProjectPath> createNewProjectPath(const VulProjectName &name);
    bool removeProjectPath(const VulProjectName &name);
    
    vector<VulProjectName> getAllProjectNames();
    unordered_map<VulProjectName, VulProjectPath> getAllProjects();

    optional<VulProjectPath> getProjectPath(const VulProjectName &name);
    optional<vector<VulProjectRunID>> getProjectRunIDs(const VulProjectName &name);
    optional<VulProjectPath> getProjectRunPath(const VulProjectName &name, const VulProjectRunID &run_id);
    optional<VulProjectPath> createAndInitProjectRunPath(const VulProjectName &name, const VulProjectRunID &run_id);

    optional<VulProjectPath> getImportModulePath(const string &module_name);

    string getLibRootPath() const {
        return lib_root_path;
    }

protected:
    VulProjectPathManager();

    string lib_root_path;
    vector<string> import_path;

};

