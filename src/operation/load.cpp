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
#include "configexpr.hpp"
#include "toposort.hpp"

#include <stdexcept>
#include <filesystem>

namespace operation_load {

/**
 * Load Operation:
 * Load a project from disk, including its modules, configs, and bundles.
 * If a project is already opened, return an error.
 * 
 * Arguments:
 * - "name": The project name to load.
 * - "import_paths": (optional) A colon-separated list of import paths to search for import modules.
 */


class LoadOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual bool is_modify() const override { return true; }; // modifies project

    ErrorMsg loadImports(VulProject &project, const vector<string> &import_paths, const string &abspath, const string &name, VulImport &out_import, shared_ptr<VulModuleBase> &out_module_base);

    ErrorMsg loadModule(VulProject &project, const string &module_path, shared_ptr<VulModuleBase> &out_module_base);

    inline VulOperationResponse resp(const ErrorMsg &err) {
        VulOperationResponse ret(err);
        ret.list_results["logs"].swap(logs);
        return ret;
    }
    inline VulOperationResponse resp(const uint32_t code, const string &msg) {
        VulOperationResponse ret(code, msg);
        ret.list_results["logs"].swap(logs);
        return ret;
    }
    inline VulOperationResponse resp() {
        VulOperationResponse ret;
        ret.list_results["logs"].swap(logs);
        return ret;
    }

    vector<string> logs;
};


ErrorMsg LoadOperation::loadImports(VulProject &project, const vector<string> &import_paths, const string &abspath, const string &name, VulImport &out_import, shared_ptr<VulModuleBase> &out_module_base) {
    using namespace serialize;
    using namespace std::filesystem;

    string realpath = abspath;
    if (realpath.empty()) {
        // use import_paths list to search for the file
        string search_target = name + ".vulmod";
        bool found = false;
        for (const auto &import_path : import_paths) {
            path base(import_path);
            if (!exists(base)) continue;
            try {
                for (const auto &ent : recursive_directory_iterator(base, directory_options::skip_permission_denied)) {
                    if (!ent.is_regular_file()) continue;
                    if (ent.path().filename() == search_target) {
                        realpath = absolute(ent.path()).string();
                        found = true;
                        break;
                    }
                }
            } catch (const std::exception &) {
                // ignore and try next import_path
            }
            if (found) break;
        }
    }
    if (realpath.empty()) {
        return EStr(EOPLoadImportNotFound, "Cannot find import module '" + name + "' in import paths.");
    }
    logs.emplace_back("Found import module '" + name + "' at '" + realpath + "'.");

    path import_abs_path = absolute(realpath);
    if (!exists(import_abs_path) || !is_regular_file(import_abs_path)) {
        return EStr(EOPLoadImportInvalidPath, "Import module path '" + import_abs_path.string() + "' is invalid or does not exist.");
    }
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
    logs.emplace_back("Import module '" + name + "' parsed successfully.");
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
    logs.emplace_back("Config library for import module '" + name + "' loaded successfully.");

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
    logs.emplace_back("Bundle library for import module '" + name + "' loaded successfully.");

    return ErrorMsg(); // Success
}

ErrorMsg LoadOperation::loadModule(VulProject &project, const string &module_path, shared_ptr<VulModuleBase> &out_module_base) {
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

VulOperationResponse LoadOperation::execute(VulProject &project) {
    logs.clear();

    auto &configlib = project.configlib;
    auto &bundlelib = project.bundlelib;
    auto &modulelib = project.modulelib;

    if (!project.name.empty()) {
        return resp(EOPLoadNotClosed, "Project already opened, cannot load another project without closing first.");
    }

    project.closeAndFinalize();
    

    using namespace serialize;
    using namespace std::filesystem;

    string path_str;
    {
        auto it = op.args.find("name");
        if (it == op.args.end()) {
            return resp(EOPLoadMissArg, "Load operation missing 'name' argument.");
        }
        path_str = project.findProjectPathInLocalLibrary(it->second);
        if (path_str.empty()) {
            return resp(EOPLoadInvalidPath, "Cannot find project '" + it->second + "' in local project library.");
        }
    }
    path project_abs_path = absolute(path_str);
    if (!exists(project_abs_path) || !is_regular_file(project_abs_path)) {
        return resp(EOPLoadInvalidPath, "Load operation path '" + project_abs_path.string() + "' is invalid or does not exist.");
    }
    logs.emplace_back("Loading project from '" + project_abs_path.string() + "'...");
    path project_dir = project_abs_path.parent_path();

    VulProjectRaw project_raw;
    ErrorMsg err = serialize::parseProjectFromXMLFile(project_abs_path.string(), project_raw);
    if (err) {
        return resp(err);
    }
    logs.emplace_back("Project parsed successfully. Found " + std::to_string(project_raw.imports.size()) + " imports.");

    vector<string> additional_import_paths = project.import_paths;
    {
        auto iter = op.args.find("import_paths");;
        if (iter != op.args.end()) {
            string import_paths_str = iter->second;
            size_t start = 0;
            size_t end = import_paths_str.find(':');
            while (end != string::npos) {
                string import_path = import_paths_str.substr(start, end - start);
                if (!import_path.empty()) {
                    additional_import_paths.push_back(import_path);
                }
                start = end + 1;
                end = import_paths_str.find(':', start);
            }
            string import_path = import_paths_str.substr(start);
            if (!import_path.empty()) {
                additional_import_paths.push_back(import_path);
            }
        }
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
        ErrorMsg err = loadImports(project, additional_import_paths, import_raw.abspath, import_raw.name, import.meta, import.ptr);
        if (err) {
            return resp(err);
        }
        import.meta.config_overrides = import_raw.config_overrides;
    }
    logs.emplace_back("All imports loaded successfully.");

    unordered_map<ConfigName, VulConfigItem> all_configs_from_lib;
    {
        path configlib_path = project_dir / "configlib.xml";
        if (!exists(configlib_path) || !is_regular_file(configlib_path)) {
            return resp(EOPLoadInvalidPath, "Config library file '" + configlib_path.string() + "' does not exist.");
        }
        vector<VulConfigItem> config_items_raw;
        ErrorMsg err = parseConfigLibFromXMLFile(configlib_path.string(), config_items_raw);
        if (err) {
            return resp(err);
        }
        for (const auto &config_item : config_items_raw) {
            all_configs_from_lib[config_item.name] = config_item;
        }
    }
    logs.emplace_back("Config library loaded successfully with " + std::to_string(all_configs_from_lib.size()) + " config items.");

    unordered_map<BundleName, VulBundleItem> all_bundles_from_lib;
    {
        path bundlelib_path = project_dir / "bundlelib.xml";
        if (!exists(bundlelib_path) || !is_regular_file(bundlelib_path)) {
            return resp(EOPLoadInvalidPath, "Bundle library file '" + bundlelib_path.string() + "' does not exist.");
        }
        vector<VulBundleItem> bundle_items_raw;
        ErrorMsg err = parseBundleLibFromXMLFile(bundlelib_path.string(), bundle_items_raw);
        if (err) {
            return resp(err);
        }
        for (const auto &bundle_item : bundle_items_raw) {
            all_bundles_from_lib[bundle_item.name] = bundle_item;
        }
    }
    logs.emplace_back("Bundle library loaded successfully with " + std::to_string(all_bundles_from_lib.size()) + " bundle items.");

    unordered_map<ModuleName, shared_ptr<VulModuleBase>> all_modules;
    {
        path modules_dir = project_dir / "modules";
        if (!exists(modules_dir) || !is_directory(modules_dir)) {
            return resp(EOPLoadInvalidPath, "Modules directory '" + modules_dir.string() + "' does not exist.");
        }
        for (const auto &ent : recursive_directory_iterator(modules_dir, directory_options::skip_permission_denied)) {
            if (!ent.is_regular_file() || ent.path().extension() != ".xml") continue;
            shared_ptr<VulModuleBase> module_base;
            ErrorMsg err = loadModule(project, ent.path().string(), module_base);
            if (err) {
                return resp(err);
            }
            all_modules[module_base->name] = module_base;
            logs.emplace_back("Module '" + module_base->name + "' loaded successfully from '" + ent.path().string() + "'.");
        }
    }
    logs.emplace_back("All modules loaded successfully. Total " + std::to_string(all_modules.size()) + " modules.");

    logs.emplace_back("Merging imports into project...");

    unordered_map<ConfigName, VulConfigItem> final_configs;
    unordered_map<ConfigName, GroupName> final_config_to_group;
    {
        // first, add all configs from imports
        for (auto &import : imports) {
            for (const auto &config_pair : import.meta.configs) {
                auto iter = final_configs.find(config_pair.first);
                if (iter != final_configs.end()) {
                    logs.emplace_back("Warning: Config item '" + config_pair.first + "' from import '" + import.meta.name + "' overrides config item previously defined by '" + final_config_to_group[config_pair.first] + "'.");
                }
                final_configs[config_pair.first] = config_pair.second;
                final_config_to_group[config_pair.first] = import.meta.name;
            }
            // apply overrides
            for (const auto &override_pair : import.meta.config_overrides) {
                if (final_configs.find(override_pair.first) != final_configs.end()) {
                    final_configs[override_pair.first].value = override_pair.second;
                } else {
                    logs.emplace_back("Warning: Config override '" + override_pair.first + "' from import '" + import.meta.name + "' does not match any existing config item.");
                }
            }
            for (const auto &config_pair : import.meta.configs) {
                auto iter = all_configs_from_lib.find(config_pair.first);
                if (iter != all_configs_from_lib.end()) {
                    logs.emplace_back("Warning: Config item '" + config_pair.first + "' from main config library overrides config item previously defined by '" + final_config_to_group[config_pair.first] + "'.");
                    import.meta.config_overrides[config_pair.first] = iter->second.value;
                    all_configs_from_lib.erase(iter);
                    project.is_modified = true;
                    project.is_config_modified = true;
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
                if (final_bundles.find(bundle_pair.first) != final_bundles.end()) {
                    logs.emplace_back("Warning: Bundle item '" + bundle_pair.first + "' from import '" + import.meta.name + "' overrides bundle item previously defined.");
                }
                final_bundles[bundle_pair.first] = bundle_pair.second;
                final_bundle_to_tags[bundle_pair.first].insert(import.meta.name);
            }
        }
        // then, add/override with bundles from main bundlelib
        for (const auto &bundle_pair : all_bundles_from_lib) {
            if (final_bundles.find(bundle_pair.first) != final_bundles.end()) {
                if (!bundlelib->_isBundleSameDefinition(bundle_pair.second, final_bundles[bundle_pair.first])) {
                    return resp(EOPLoadBundleConflict, "Bundle item '" + bundle_pair.first + "' from main bundle library conflicts with bundle item previously defined.");
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
            return resp(EOPLoadModuleConflict, "Module '" + import.ptr->name + "' from import '" + import.meta.name + "' conflicts with module previously defined in main module library.");
        }
        final_modules[import.ptr->name] = import.ptr;
    }

    logs.emplace_back("Merge completed. Load summary: " + 
        std::to_string(final_configs.size()) + " config items, " +
        std::to_string(final_bundles.size()) + " bundle items, " +
        std::to_string(final_modules.size()) + " modules."
    );

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
                return resp(EOPLoadConfigInvalidValue, "Config item '" + confige_pair.first + "' has invalid value: " + errinfo + " at position " + std::to_string(errpos) + ". Expression: " + confige_pair.second.item.value);
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
            return resp(EOPLoadConfigLoopRef, loop_info);
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
                return resp(EOPLoadConfigInvalidValue, "Config item '" + conf_name + "' has invalid value during calculation: " + err.msg);
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
            ErrorMsg err = bundlelib->extractBundleReferencesAndConfs(
                bundle_pair.second,
                entry.references,
                entry.confs
            );
            if (err) {
                return resp(EOPLoadBundleInvalidValue, "Bundle item '" + bundle_pair.first + "' has invalid value during reference extraction: " + err.msg);
            }
            for (const auto &conf : entry.confs) {
                if (final_config_entries.find(conf) == final_config_entries.end()) {
                    return resp(EOPLoadBundleMissingConf, "Bundle item '" + bundle_pair.first + "' refers to missing config item '" + conf + "'.");
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
            return resp(EOPLoadBundleLoopRef, loop_info);
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
                    return resp(EOPLoadModuleMissingInst, "Module '" + module_pair.first + "' has instance '" + inst.name + "' referring to missing module '" + inst.module_name + "'.");
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
            return resp(EOPLoadModuleLoopRef, loop_info);
        }
    }

    // finalize
    project.name = project_abs_path.stem().string();
    project.dirpath = project_dir.string();
    project.top_module = project_raw.top_module;
    project.is_opened = true;
    project.imports.clear();
    for (const auto &import : imports) {
        project.imports[import.meta.name] = import.meta;
    }

    configlib->config_items.swap(final_config_entries);
    bundlelib->bundles.swap(final_bundle_entries);
    modulelib->modules.swap(final_modules);

    return resp(); // Success
}

OperationFactory loadOperationFactory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<LoadOperation>(op);
};

struct RegisterLoadOperation {
    RegisterLoadOperation() {
        VulProject::registerOperation("load", loadOperationFactory);
    }
} registerLoadOperationInstance;

} // namespace operation_load