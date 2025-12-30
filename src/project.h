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

#include "type.h"

#include "module.h"
#include "configlib.h"
#include "bundlelib.h"

using std::function;

typedef string ProjectName;
typedef string ProjectPath;

typedef string OperationName;
typedef string OperationArg;

class VulProjectOperation;

using OperationFactory = function<unique_ptr<VulProjectOperation>()>;

typedef struct {
    ProjectPath     abspath;
    ModuleName      name;
    unordered_map<ConfigName, VulConfigItem> configs;
    unordered_map<BundleName, VulBundleItem> bundles;
    unordered_map<ConfigName, ConfigValue> config_overrides;
} VulImport;

class VulProject {
public:
    
    static bool registerOperation(const OperationName &op_name, const OperationFactory &factory);

    VulProject(const vector<ProjectPath> &import_paths) : import_paths(import_paths) {};

    ProjectName                 name;
    ProjectPath                 path;

    ModuleName                  top_module;

    bool is_opened = false;
    bool is_modified = false;

    ErrorMsg doOperation(const OperationName &op_name, const vector<OperationArg> &op_args);

    shared_ptr<VulConfigLib> configlib;
    shared_ptr<VulBundleLib> bundlelib;
    shared_ptr<VulModuleLib> modulelib;

    vector<ProjectPath>     import_paths;
    unordered_map<ModuleName, VulImport>   imports;
};

class VulProjectOperation {
public:
    virtual ErrorMsg execute(VulProject &project, const vector<OperationArg> &op_args) = 0;
};

