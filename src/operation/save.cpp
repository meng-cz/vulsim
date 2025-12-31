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
#include "serialize.h"

#include <filesystem>

namespace operation_save {

using namespace std::filesystem;
using namespace serialize;

class SaveOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;
    
    virtual VulOperationResponse execute(VulProject &project) override;
};

VulOperationResponse SaveOperation::execute(VulProject &project) {
    if (!project.is_opened) {
        return EStr(EOPSaveNotOpened, "No project is opened to save.");
    }
    
    path project_dir(project.dirpath);
    path tmp_save_dir = project_dir / ".vul_tmp_save";
    path tmp_save_modules_dir = tmp_save_dir / "modules";
    try {
        if (exists(tmp_save_dir)) {
            std::filesystem::remove_all(tmp_save_dir);
        }
        create_directory(tmp_save_dir);
        create_directory(tmp_save_modules_dir);
    } catch (const std::exception &e) {
        return EStr(EOPSaveFileFailed, string("Failed to create temporary save directory: ") + e.what());
    }

    if (project.is_modified) {
        path project_file = tmp_save_dir / (project.name + ".vul");
        VulProjectRaw project_raw;
        project_raw.top_module = project.top_module;
        for (const auto &imp_pair : project.imports) {
            const ModuleName &mod_name = imp_pair.first;
            const VulImport &imp = imp_pair.second;
            VulImportRaw imp_raw;
            imp_raw.abspath = imp.abspath;
            imp_raw.name = mod_name;
            imp_raw.config_overrides = imp.config_overrides;
            project_raw.imports.push_back(imp_raw);
        }
        ErrorMsg err = writeProjectToXMLFile(project_file.string(), project_raw);
        if (err) {
            std::filesystem::remove_all(tmp_save_dir);
            return EStr(EOPSaveFileFailed, string("Failed to write project file: ") + err.msg);
        }
    }

    if (project.is_config_modified) {
        path config_file = tmp_save_dir / "configlib.xml";
        vector<VulConfigItem> configs;
        for (const auto &cfg_pair : project.configlib->config_items) {
            if (cfg_pair.second.group != project.configlib->DefaultGroupName) {
                continue; // only save default group items
            }
            configs.push_back(cfg_pair.second.item);
        }
        ErrorMsg err = writeConfigLibToXMLFile(config_file.string(), configs);
        if (err) {
            std::filesystem::remove_all(tmp_save_dir);
            return EStr(EOPSaveFileFailed, string("Failed to write config library file: ") + err.msg);
        }
    }

    if (project.is_bundle_modified) {
        path bundle_file = tmp_save_dir / "bundlelib.xml";
        vector<VulBundleItem> bundles;
        for (const auto &bnd_pair : project.bundlelib->bundles) {
            if (bnd_pair.second.tags.find(project.bundlelib->DefaultTag) == bnd_pair.second.tags.end()) {
                continue; // only save default tag bundles
            }
            bundles.push_back(bnd_pair.second.item);
        }
        ErrorMsg err = writeBundleLibToXMLFile(bundle_file.string(), bundles);
        if (err) {
            std::filesystem::remove_all(tmp_save_dir);
            return EStr(EOPSaveFileFailed, string("Failed to write bundle library file: ") + err.msg);
        }
    }

    unordered_set<ModuleName> deleted_modules;
    for (const auto &mod_name : project.modified_modules) {
        auto mod_iter = project.modulelib->modules.find(mod_name);
        if (mod_iter == project.modulelib->modules.end()) {
            deleted_modules.insert(mod_name);
            continue;
        }
        path module_file = tmp_save_modules_dir / (mod_name + ".xml");
        shared_ptr<VulModule> module_ptr = std::dynamic_pointer_cast<VulModule>(mod_iter->second);
        if (!module_ptr) {
            continue; // not a VulModule, skip
        }
        ErrorMsg err = writeModuleToXMLFile(module_file.string(), *module_ptr);
        if (err) {
            std::filesystem::remove_all(tmp_save_dir);
            return EStr(EOPSaveFileFailed, string("Failed to write module file '") + mod_name + "': " + err.msg);
        }
    }

    try {
        // move temp save files to project directory
        for (const auto &entry : std::filesystem::recursive_directory_iterator(tmp_save_dir, std::filesystem::directory_options::skip_permission_denied)) {
            path rel = entry.path().lexically_relative(tmp_save_dir);
            path dest = project_dir / rel;

            if (entry.is_directory()) {
                if (!exists(dest)) std::filesystem::create_directories(dest);
            } else if (entry.is_regular_file() || entry.is_symlink()) {
                if (!exists(dest.parent_path())) std::filesystem::create_directories(dest.parent_path());
                std::filesystem::copy_file(entry.path(), dest, std::filesystem::copy_options::overwrite_existing);
            } else {
                // fallback: try to copy other file types
                if (!exists(dest.parent_path())) std::filesystem::create_directories(dest.parent_path());
                std::filesystem::copy(entry.path(), dest, std::filesystem::copy_options::overwrite_existing);
            }
        }
        std::filesystem::remove_all(tmp_save_dir);
        for (const auto &mod_name : deleted_modules) {
            path module_file = project_dir / "modules" / (mod_name + ".xml");
            if (exists(module_file)) {
                std::filesystem::remove(module_file);
            }
        }
    } catch (const std::exception &e) {
        std::filesystem::remove_all(tmp_save_dir);
        return EStr(EOPSaveFileFailed, string("Failed to finalize save operation: ") + e.what());
    }

    return VulOperationResponse(); // Success
}

OperationFactory saveOperationFactory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<SaveOperation>(op);
};

struct RegisterSaveOperation {
    RegisterSaveOperation() {
        VulProject::registerOperation("save", saveOperationFactory);
    }
} registerSaveOperationInstance;

} // namespace operation_save
