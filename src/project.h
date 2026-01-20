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

/**
 * 项目操作保证以下内容的一致性：
 * - 配置库（VulConfigLib）
 *   1. 配置项名称唯一
 *   2. 配置项引用关系正确
 *   3. 所有配置项都能被正确解析和计算
 * - Bundle 库（VulBundleLib）
 *   1. Bundle 名称唯一
 *   2. Bundle 引用关系正确
 *   3. Bundle 引用的 Config 均存在且正确
 * - Module 库（VulModuleLib）
 *   1. 模块名称唯一
 *   2. 模块子模块引用关系正确
 *   3. 模块内部的组件连接关系均唯一且正确
 *   4. 模块内部的代码段和组件的绑定关系正确
 * 
 * 项目操作不保证以下内容的一致性：
 * - Module 库（VulModuleLib）
 *   1. 模块内部引用的 Config 和 Bundle 引用均存在且正确
 *   2. 模块代码内部引用的 Config 和 Bundle 或其他任意可访问组件 引用均存在且正确
 *   
 */

using std::function;

typedef string ProjectName;
typedef string ProjectPath;

typedef string OperationName;
typedef string OperationArgName;
typedef string OperationArg;

class VulProjectOperation;

struct VulImport {
    ProjectPath     abspath;
    ModuleName      name;
    unordered_map<ConfigName, VulConfigItem> configs;
    unordered_map<BundleName, VulBundleItem> bundles;
    unordered_map<ConfigName, ConfigValue> config_overrides;
};

struct VulImportRaw {
    string abspath;
    string name;
    unordered_map<ConfigName, ConfigValue> config_overrides;
};
struct VulProjectRaw {
    ModuleName                      top_module;
    vector<VulImportRaw>            imports;
    vector<ModuleName>              modules;
};

struct VulOperationArg {
    uint32_t            index;
    OperationArgName    name;
    OperationArg        value;
};

class VulOperationPackage {
public:
    OperationName       name;
    vector<VulOperationArg>    arg_list;

    std::unique_ptr<OperationArg> getArg(const OperationArgName &arg_name, uint32_t index) const {
        for (const auto &arg : arg_list) {
            if ((!arg.name.empty() && arg.name != arg_name) || (arg.index != -1 && arg.index != index)) {
                continue;
            }
            return std::make_unique<OperationArg>(arg.value);
        }
        return nullptr;
    };
    bool getBoolArg(const OperationArgName &arg_name, uint32_t index) const {
        auto arg_ptr = getArg(arg_name, index);
        if (!arg_ptr) {
            return false;
        }
        string val_str = *arg_ptr;
        return (val_str == "true" || val_str == "True" || val_str == "TRUE" || val_str == "1");
    }
};

struct VulOperationResponse {
    uint32_t            code;
    string              msg;
    unordered_map<OperationArgName, string>  results;
    unordered_map<OperationArgName, vector<string>>  list_results;

    VulOperationResponse() : code(0), msg("") {};
    VulOperationResponse(const ErrorMsg & err) : code(err.code), msg(err.msg) {};
    VulOperationResponse(const uint32_t c, const string &m) : code(c), msg(m) {};
};

using OperationFactory = function<unique_ptr<VulProjectOperation>(const VulOperationPackage &)>;

class VulProject {
public:
    
    static bool registerOperation(const OperationName &op_name, const OperationFactory &factory);
    static vector<OperationName> listAllRegisteredOperations();

    VulProject() {
        configlib = std::make_shared<VulConfigLib>();
        bundlelib = std::make_shared<VulBundleLib>();
        modulelib = std::make_shared<VulModuleLib>();
    };

    ProjectName                 name;
    ProjectPath                 dirpath;

    ErrorMsg load(const ProjectPath &path, const ProjectName &project_name);
    ErrorMsg save() const;

    ModuleName                  top_module;

    bool is_opened = false;
    bool is_modified = false;

    bool is_config_modified = false;
    bool is_bundle_modified = false;
    unordered_set<ModuleName> modified_modules;

    VulOperationResponse doOperation(const VulOperationPackage &op);
    string undoLastOperation();
    string redoLastOperation();

    vector<string> listHelpForOperation(const OperationName &op_name) const;

    shared_ptr<VulConfigLib> configlib;
    shared_ptr<VulBundleLib> bundlelib;
    shared_ptr<VulModuleLib> modulelib;

    unordered_map<ModuleName, VulImport>   imports;

    std::deque<unique_ptr<VulProjectOperation>>   operation_undo_history;
    std::deque<unique_ptr<VulProjectOperation>>   operation_redo_history;

    inline void closeAndFinalize() {
        is_modified = false;
        is_opened = false;
        is_config_modified = false;
        is_bundle_modified = false;
        modified_modules.clear();
        name = "";
        dirpath = "";
        top_module = "";
        configlib->clear();
        bundlelib->clear();
        modulelib->clear();
        imports.clear();
        operation_undo_history.clear();
        operation_redo_history.clear();
    }

    inline bool globalNameConflictCheck(const string &name) const {
        return configlib->config_items.find(name) != configlib->config_items.end() ||
               bundlelib->bundles.find(name) != bundlelib->bundles.end() ||
               modulelib->modules.find(name) != modulelib->modules.end();
    }
};

class VulProjectOperation {
public:

    VulProjectOperation(const VulOperationPackage &op);

    virtual VulOperationResponse execute(VulProject &project) = 0;
    virtual string undo(VulProject &project) { return  ""; }; // not supported by default
    virtual bool is_undoable() const { return false; }; // not undoable by default
    virtual bool is_modify() const { return false; }; // does not modify project by default

    virtual vector<string> help() const { return vector<string>(); };
    
    VulOperationPackage    op;
    string timestamp;
};

