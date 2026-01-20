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
#include "projman.h"
#include "serialize.h"
#include "configexpr.hpp"
#include "toposort.hpp"
#include "platform/env.hpp"

#include <stdexcept>
#include <filesystem>

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



ErrorMsg loadImports(VulProject &project, const string &abspath, const string &name, VulImport &out_import, shared_ptr<VulModuleBase> &out_module_base) {
    using namespace serialize;
    using namespace std::filesystem;

    auto _import_path = VulProjectPathManager::getInstance()->getImportModulePath(name);
    if (!_import_path.has_value()) {
        return EStr(EOPLoadImportInvalidPath, "Import module path for '" + name + "' not found in project import paths.");
    }
    string realpath = _import_path.value();
    path import_abs_path = absolute(realpath) / (name + ".vulmod");
    path import_dir = import_abs_path.parent_path();

    out_import.abspath = import_abs_path.string();
    out_import.name = name;
    out_import.configs.clear();
    out_import.bundles.clear();
    out_import.config_overrides.clear();

    shared_ptr<VulExternalModule> import_module = std::make_shared<VulExternalModule>();
    {
        ErrorMsg err = parseModuleBaseFromXMLFile(import_abs_path.string(), *import_module);
        if (err) {
            return err;
        }
    }
    out_module_base = import_module;

    path configlib_path = import_dir / "configlib.xml";
    if (!exists(configlib_path) || !is_regular_file(configlib_path)) {
        return EStr(EOPLoadImportInvalidPath, "Config library file '" + configlib_path.string() + "' for import module '" + name + "' does not exist.");
    }
    {
        vector<VulConfigItem> config_items_raw;
        ErrorMsg err = parseConfigLibFromXMLFile(configlib_path.string(), config_items_raw);
        if (err) {
            return err;
        }
        for (const auto &config_item_raw : config_items_raw) {
            out_import.configs[config_item_raw.name] = config_item_raw;
        }
    }

    path bundlelib_path = import_dir / "bundlelib.xml";
    if (!exists(bundlelib_path) || !is_regular_file(bundlelib_path)) {
        return EStr(EOPLoadImportInvalidPath, "Bundle library file '" + bundlelib_path.string() + "' for import module '" + name + "' does not exist.");
    }
    {
        vector<VulBundleItem> bundle_items_raw;
        ErrorMsg err = parseBundleLibFromXMLFile(bundlelib_path.string(), bundle_items_raw);
        if (err) {
            return err;
        }
        for (const auto &bundle_item : bundle_items_raw) {
            out_import.bundles[bundle_item.name] = bundle_item;
        }
    }

    return ErrorMsg(); // Success
}

ErrorMsg loadModule(VulProject &project, const string &module_path, shared_ptr<VulModuleBase> &out_module_base) {
    using namespace serialize;
    using namespace std::filesystem;

    path module_abs_path = absolute(module_path);
    if (!exists(module_abs_path) || !is_regular_file(module_abs_path)) {
        return EStr(EOPLoadInvalidPath, "Module path '" + module_abs_path.string() + "' is invalid or does not exist.");
    }

    shared_ptr<VulModule> module_ptr = std::make_shared<VulModule>();
    ErrorMsg err = parseModuleFromXMLFile(module_abs_path.string(), *module_ptr);
    if (err) {
        return err;
    }
    out_module_base = module_ptr;
    return ErrorMsg(); // Success
}

ErrorMsg VulProject::load(const ProjectPath &path, const ProjectName &project_name) {

    namespace fs = std::filesystem;
    using namespace serialize;

    if (is_opened) {
        return EStr(EOPLoadNotClosed, "Another project is already opened. Please close it before loading a new project.");
    }

    fs::path project_dir = fs::absolute(path);
    string projname = project_name;
    fs::path project_xml_abs_path = project_dir / (projname + ".vul");

    VulProjectRaw project_raw;
    ErrorMsg err = serialize::parseProjectFromXMLFile(project_xml_abs_path.string(), project_raw);
    if (err) {
        return err;
    }

    struct ImportTmp {
        VulImport      meta;
        shared_ptr<VulModuleBase>   ptr;
    };
    vector<ImportTmp> imports;
    
    imports.reserve(project_raw.imports.size());
    for (const auto &import_raw : project_raw.imports) {
        imports.emplace_back();
        ImportTmp &import = imports.back();
        ErrorMsg err = loadImports(*this, import_raw.abspath, import_raw.name, import.meta, import.ptr);
        if (err) {
            return err;
        }
        import.meta.config_overrides = import_raw.config_overrides;
    }

    unordered_map<ConfigName, VulConfigItem> all_configs_from_lib;
    {
        fs::path configlib_path = project_dir / "configlib.xml";
        if (!fs::exists(configlib_path) || !fs::is_regular_file(configlib_path)) {
            return EStr(EOPLoadInvalidPath, "Config library file '" + configlib_path.string() + "' does not exist.");
        }
        vector<VulConfigItem> config_items_raw;
        ErrorMsg err = parseConfigLibFromXMLFile(configlib_path.string(), config_items_raw);
        if (err) {
            return err;
        }
        for (const auto &config_item : config_items_raw) {
            all_configs_from_lib[config_item.name] = config_item;
        }
    }

    unordered_map<BundleName, VulBundleItem> all_bundles_from_lib;
    {
        fs::path bundlelib_path = project_dir / "bundlelib.xml";
        if (!fs::exists(bundlelib_path) || !fs::is_regular_file(bundlelib_path)) {
            return EStr(EOPLoadInvalidPath, "Bundle library file '" + bundlelib_path.string() + "' does not exist.");
        }
        vector<VulBundleItem> bundle_items_raw;
        ErrorMsg err = parseBundleLibFromXMLFile(bundlelib_path.string(), bundle_items_raw);
        if (err) {
            return err;
        }
        for (const auto &bundle_item : bundle_items_raw) {
            all_bundles_from_lib[bundle_item.name] = bundle_item;
        }
    }

    unordered_map<ModuleName, shared_ptr<VulModuleBase>> all_modules;
    {
        fs::path modules_dir = project_dir / "modules";
        if (!fs::exists(modules_dir) || !fs::is_directory(modules_dir)) {
            return EStr(EOPLoadInvalidPath, "Modules directory '" + modules_dir.string() + "' does not exist.");
        }
        for (const auto &module_name : project_raw.modules) {
            fs::path module_path = modules_dir / (module_name + ".xml");
            if (!fs::exists(module_path) || !fs::is_regular_file(module_path)) {
                return EStr(EOPLoadInvalidPath, "Module file '" + module_path.string() + "' does not exist.");
            }
            shared_ptr<VulModuleBase> module_base;
            ErrorMsg err = loadModule(*this, module_path.string(), module_base);
            if (err) {
                return err;
            }
            all_modules[module_base->name] = module_base;
        }
    }

    unordered_map<ConfigName, VulConfigItem> final_configs;
    unordered_map<ConfigName, GroupName> final_config_to_group;
    bool config_override_modified = false;
    {
        // first, add all configs from imports
        for (auto &import : imports) {
            for (const auto &config_pair : import.meta.configs) {
                auto iter = final_configs.find(config_pair.first);
                final_configs[config_pair.first] = config_pair.second;
                final_config_to_group[config_pair.first] = import.meta.name;
            }
            // apply overrides
            for (const auto &override_pair : import.meta.config_overrides) {
                if (final_configs.find(override_pair.first) != final_configs.end()) {
                    final_configs[override_pair.first].value = override_pair.second;
                }
            }
            for (const auto &config_pair : import.meta.configs) {
                auto iter = all_configs_from_lib.find(config_pair.first);
                if (iter != all_configs_from_lib.end()) {
                    import.meta.config_overrides[config_pair.first] = iter->second.value;
                    all_configs_from_lib.erase(iter);
                    config_override_modified = true;
                }
            }
        }
        // then, add configs from main configlib
        for (const auto &config_pair : all_configs_from_lib) {
            auto iter = final_configs.find(config_pair.first);
            if (iter != final_configs.end()) [[unlikely]] {
                continue; // already defined by imports, skip
            }
            final_configs[config_pair.first] = config_pair.second;
            final_config_to_group[config_pair.first] = configlib->DefaultGroupName;
        }
    }

    unordered_map<BundleName, VulBundleItem> final_bundles;
    unordered_map<BundleName, unordered_set<BundleTag>> final_bundle_to_tags;
    {
        // first, add all bundles from imports
        for (const auto &import : imports) {
            for (const auto &bundle_pair : import.meta.bundles) {
                final_bundles[bundle_pair.first] = bundle_pair.second;
                final_bundle_to_tags[bundle_pair.first].insert(import.meta.name);
            }
        }
        // then, add/override with bundles from main bundlelib
        for (const auto &bundle_pair : all_bundles_from_lib) {
            if (final_bundles.find(bundle_pair.first) != final_bundles.end()) {
                if (bundle_pair.second != final_bundles[bundle_pair.first]) {
                    return EStr(EOPLoadBundleConflict, "Bundle item '" + bundle_pair.first + "' from main bundle library conflicts with bundle item previously defined.");
                }
            } else {
                final_bundles[bundle_pair.first] = bundle_pair.second;
            }
            final_bundle_to_tags[bundle_pair.first].insert(bundlelib->DefaultTag);
        }
    }

    unordered_map<ModuleName, shared_ptr<VulModuleBase>> final_modules = all_modules;
    for (const auto &import : imports) {
        if (final_modules.find(import.ptr->name) != final_modules.end()) {
            return EStr(EOPLoadModuleConflict, "Module '" + import.ptr->name + "' from import '" + import.meta.name + "' conflicts with module previously defined in main module library.");
        }
        final_modules[import.ptr->name] = import.ptr;
    }

    unordered_map<ConfigName, VulConfigLib::ConfigEntry> final_config_entries;

    // validate and calculate config values
    {
        unordered_set<ConfigName> all_config_nodes;
        unordered_map<ConfigName, unordered_set<ConfigName>> config_refby_edges;
        using ConfE = VulConfigLib::ConfigEntry;
        for (const auto &config_pair : final_configs) {
            ConfE entry;
            entry.item = config_pair.second;
            entry.group = final_config_to_group[config_pair.first];
            final_config_entries[config_pair.first] = entry;
            all_config_nodes.insert(config_pair.first);
            config_refby_edges[config_pair.first] = {};
        }
        using namespace config_parser;
        for (auto &confige_pair : final_config_entries) {
            uint32_t errpos = 0;
            string errinfo;
            auto refered_confs = parseReferencedIdentifier(confige_pair.second.item.value, errpos, errinfo);
            if (!refered_confs) {
                return EStr(EOPLoadConfigInvalidValue, "Config item '" + confige_pair.first + "' has invalid value: " + errinfo + " at position " + std::to_string(errpos) + ". Expression: " + confige_pair.second.item.value);
            }
            for (const auto &ref_conf : *refered_confs) {
                confige_pair.second.references.insert(ref_conf);
                config_refby_edges[ref_conf].insert(confige_pair.first);
                final_config_entries[ref_conf].reverse_references.insert(confige_pair.first);
            }
        }
        vector<string> loop_nodes;
        auto sorted_configs = topologicalSort(all_config_nodes, config_refby_edges, loop_nodes);
        if (!sorted_configs) {
            string loop_info = "Configuration items have circular references: ";
            for (const auto &node : loop_nodes) {
                loop_info += "'" + node + "' ";
            }
            return EStr(EOPLoadConfigLoopRef, loop_info);
        }
        unordered_map<ConfigName, ConfigRealValue> calculated_values;
        for (const auto &conf_name : *sorted_configs) {
            ConfigRealValue real_value;
            unordered_set<ConfigName> visited;
            auto &confe = final_config_entries[conf_name];
            ErrorMsg err = configlib->calculateConfigExpression(
                confe.item.value,
                calculated_values,
                real_value,
                visited
            );
            if (err) {
                return EStr(EOPLoadConfigInvalidValue, "Config item '" + conf_name + "' has invalid value during calculation: " + err.msg);
            }
            calculated_values[conf_name] = real_value;
            confe.real_value = real_value;
        }
    }
    unordered_map<BundleName, VulBundleLib::BundleEntry> final_bundle_entries;
    {
        unordered_set<BundleName> all_bundle_nodes;
        unordered_map<BundleName, unordered_set<BundleName>> bundle_refby_edges;

        for (const auto &bundle_pair : final_bundles) {
            VulBundleLib::BundleEntry entry;
            entry.item = bundle_pair.second;
            entry.tags = final_bundle_to_tags[bundle_pair.first];
            string err = bundle_pair.second.checkAndExtractReferences(entry.references, entry.confs);
            if (!err.empty()) {
                return EStr(EOPLoadBundleInvalidValue, "Bundle item '" + bundle_pair.first + "' has invalid value during reference extraction: " + err);
            }
            for (const auto &conf : entry.confs) {
                if (final_config_entries.find(conf) == final_config_entries.end()) {
                    return EStr(EOPLoadBundleMissingConf, "Bundle item '" + bundle_pair.first + "' refers to missing config item '" + conf + "'.");
                }
            }
            final_bundle_entries[bundle_pair.first] = entry;
            all_bundle_nodes.insert(bundle_pair.first);
            bundle_refby_edges[bundle_pair.first] = {};
        }
        for (const auto &bundle_pair : final_bundle_entries) {
            for (const auto &ref_bundle : bundle_pair.second.references) {
                bundle_refby_edges[ref_bundle].insert(bundle_pair.first);
                final_bundle_entries[ref_bundle].reverse_references.insert(bundle_pair.first);
            }
        }
        vector<string> loop_nodes;
        auto sorted_bundles = topologicalSort(all_bundle_nodes, bundle_refby_edges, loop_nodes);
        if (!sorted_bundles) {
            string loop_info = "Bundle items have circular references: ";
            for (const auto &node : loop_nodes) {
                loop_info += "'" + node + "' ";
            }
            return EStr(EOPLoadBundleLoopRef, loop_info);
        }
    }
    {
        unordered_set<ModuleName> all_module_nodes;
        unordered_map<ModuleName, unordered_set<ModuleName>> module_refby_edges;
        for (const auto &module_pair : final_modules) {
            all_module_nodes.insert(module_pair.first);
            module_refby_edges[module_pair.first] = {};
        }
        for (const auto &module_pair : all_modules) {
            auto module_ptr = std::dynamic_pointer_cast<VulModule>(module_pair.second);
            if (!module_ptr) continue;
            for (const auto &inst_pair : module_ptr->instances) {
                const auto &inst = inst_pair.second;
                if (final_modules.find(inst.module_name) == final_modules.end()) {
                    return EStr(EOPLoadModuleMissingInst, "Module '" + module_pair.first + "' has instance '" + inst.name + "' referring to missing module '" + inst.module_name + "'.");
                }
                module_refby_edges[inst.module_name].insert(module_pair.first);
            }

        }
        vector<string> loop_nodes;
        auto sorted_modules = topologicalSort(all_module_nodes, module_refby_edges, loop_nodes);
        if (!sorted_modules) {
            string loop_info = "Modules have circular instance references: ";
            for (const auto &node : loop_nodes) {
                loop_info += "'" + node + "' ";
            }
            return EStr(EOPLoadModuleLoopRef, loop_info);
        }
    }

    // finalize
    this->name = projname;
    this->dirpath = project_dir.string();
    this->top_module = project_raw.top_module;
    this->imports.clear();
    for (const auto &import : imports) {
        this->imports[import.meta.name] = import.meta;
    }

    configlib->config_items = std::move(final_config_entries);
    bundlelib->bundles = std::move(final_bundle_entries);
    modulelib->modules = std::move(final_modules);

    this->is_modified = config_override_modified;
    this->is_config_modified = config_override_modified;
    this->is_bundle_modified = false;
    this->modified_modules.clear();
    this->is_opened = true;

    operation_undo_history.clear();
    operation_redo_history.clear();

    return {}; // Success
}

ErrorMsg VulProject::save() const {
    if (!is_opened) {
        return EStr(EOPSaveNotOpened, "No project is opened to save.");
    }

    using namespace std::filesystem;
    using namespace serialize;

    path project_dir(dirpath);
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

    if (is_modified) {
        path project_file = tmp_save_dir / (name + ".vul");
        VulProjectRaw project_raw;
        project_raw.top_module = top_module;
        project_raw.imports.clear();
        project_raw.modules.clear();
        for (const auto &mod_pair : modulelib->modules) {
            const ModuleName &mod_name = mod_pair.first;
            project_raw.modules.push_back(mod_name);
        }
        for (const auto &imp_pair : imports) {
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

    if (is_config_modified) {
        path config_file = tmp_save_dir / "configlib.xml";
        vector<VulConfigItem> configs;
        for (const auto &cfg_pair : configlib->config_items) {
            if (cfg_pair.second.group != configlib->DefaultGroupName) {
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

    if (is_bundle_modified) {
        path bundle_file = tmp_save_dir / "bundlelib.xml";
        vector<VulBundleItem> bundles;
        for (const auto &bnd_pair : bundlelib->bundles) {
            if (bnd_pair.second.tags.find(bundlelib->DefaultTag) == bnd_pair.second.tags.end()) {
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
    for (const auto &mod_name : modified_modules) {
        auto mod_iter = modulelib->modules.find(mod_name);
        if (mod_iter == modulelib->modules.end()) {
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

    return {}; // Success
}
