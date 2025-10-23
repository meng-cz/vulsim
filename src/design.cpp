/*
 * Copyright (c) 2025 Meng-CZ
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "design.h"
#include "serialize.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <unordered_set>
#include <filesystem>
#include <ctime>
#include <fstream>

/**
 * @brief Initialize a new VulDesign with given project name and directory.
 * This creates the necessary directory structure and an initial design.xml file.
 * @param project_name The name of the new project.
 * @param project_dir The directory to create the project in.
 * @param err Error message in case of failure.
 * @return A unique_ptr to the initialized VulDesign, or nullptr on failure.
 */
unique_ptr<VulDesign> VulDesign::initNewDesign(const string &project_name, const string &project_dir, string &err) {
    err.clear();

    if (project_name.empty()) { err = string("#10040: project_name is empty"); return nullptr; }
    if (project_dir.empty()) { err = string("#10005: project_dir is empty"); return nullptr; }

    namespace fs = std::filesystem;
    fs::path proj(project_dir);
    if (fs::exists(proj)) {
        err = string("#10041: project path exists: ") + project_dir;
        return nullptr;
    }
    // create project directory
    bool created_proj = false;
    try {
        fs::create_directories(proj);
        fs::create_directories(proj / "cpp");
        fs::create_directories(proj / "bundle");
        fs::create_directories(proj / "combine");
        // init cpp/global.h
        std::filesystem::path p = proj / "cpp" / "global.h";
        std::ofstream f(p);
        if (f) {
            f << "#pragma once\n\n";
            f << "// Global header file for VulSim project '" << project_name << "'\n\n";
            f.close();
            created_proj = true;
        }
    } catch (const std::exception &e) {
        err = string("Error: cannot create project-dir: ") + e.what();
        return nullptr;
    } catch (...) {
        err = string("Error: unknown exception creating project-dir");
        return nullptr;
    }

    // construct design and set basic fields
    auto design = std::unique_ptr<VulDesign>(new VulDesign());
    design->project_dir = proj.string();
    design->project_name = project_name;

    // mark as dirty to ensure files are written on first save if needed
    design->dirty_bundles = true;
    design->dirty_combines = true;
    design->dirty_config_lib = true;

    // attempt to save initial project files
    string res = design->saveProject();
    if (!res.empty()) {
        if (created_proj) {
            try { fs::remove_all(proj); } catch(...) {}
        }
        err = res;
        return nullptr;
    }

    return design;
}

/**
 * @brief Check for global name conflicts across bundles, combines, instances, pipes, prefabs, and config items.
 * @param name The name to check for conflicts.
 * @return An empty string if no conflict, or an error message if there is a conflict.
 */
string VulDesign::checkGlobalNameConflict(const string &name) {
    if (bundles.find(name) != bundles.end()) {
        return string("Name conflict: bundle with name '") + name + "' already exists.";
    }
    if (combines.find(name) != combines.end()) {
        return string("Name conflict: combine with name '") + name + "' already exists.";
    }
    if (instances.find(name) != instances.end()) {
        return string("Name conflict: instance with name '") + name + "' already exists.";
    }
    if (pipes.find(name) != pipes.end()) {
        return string("Name conflict: pipe with name '") + name + "' already exists.";
    }
    if (prefabs.find(name) != prefabs.end()) {
        return string("Name conflict: prefab with name '") + name + "' already exists.";
    }
    if (config_lib.find(name) != config_lib.end()) {
        return string("Name conflict: config item with name '") + name + "' already exists.";
    }
    return "";
}

/**
 * Check if the given type is a valid VulSim Data Type.
 * List of valid types:
 * Basic types; Bundles;
 * vector<Valid types>; array<Valid types, N>; set<Valid types>; map<Valid types, Valid types>
 *  @param type The type string to check.
 *  @return true if the type is a valid type, false otherwise.
 */
bool VulDesign::isValidTypeName(const string &type, vector<string> &seen_bundles) {
    // Recursive descent parser for simple C++-like template type syntax.
    // Accepts:
    //  - basic types (isBasicVulType)
    //  - bundles (present in this->bundles)
    //  - vector<T>, set<T>
    //  - array<T, N>  (N must be a positive integer)
    //  - map<K, V>

    string s = type;
    auto skipSpaces = [&](size_t &i) {
        while (i < s.size() && std::isspace((unsigned char)s[i])) ++i;
    };

    std::function<bool(size_t&)> parseType;
    parseType = [&](size_t &i) -> bool {
        skipSpaces(i);
        if (i >= s.size()) return false;

        // read token (identifier)
        size_t start = i;
        while (i < s.size() && s[i] != '<' && s[i] != '>' && s[i] != ',' && !std::isspace((unsigned char)s[i])) ++i;
        string token = s.substr(start, i - start);
        if (token.empty()) return false;

        // template types
        if (token == "vector" || token == "set") {
            skipSpaces(i);
            if (i >= s.size() || s[i] != '<') return false;
            ++i; // consume '<'
            if (!parseType(i)) return false;
            skipSpaces(i);
            if (i >= s.size() || s[i] != '>') return false;
            ++i; // consume '>'
            return true;
        }

        if (token == "array") {
            skipSpaces(i);
            if (i >= s.size() || s[i] != '<') return false;
            ++i; // consume '<'
            if (!parseType(i)) return false;
            skipSpaces(i);
            if (i >= s.size() || s[i] != ',') return false;
            ++i; // consume ','
            skipSpaces(i);
            // parse integer literal for array size
            size_t numStart = i;
            if (i >= s.size() || !std::isdigit((unsigned char)s[i])) return false;
            while (i < s.size() && std::isdigit((unsigned char)s[i])) ++i;
            string num = s.substr(numStart, i - numStart);
            if (num.empty()) return false;
            skipSpaces(i);
            if (i >= s.size() || s[i] != '>') return false;
            ++i; // consume '>'
            return true;
        }

        if (token == "map") {
            skipSpaces(i);
            if (i >= s.size() || s[i] != '<') return false;
            ++i; // consume '<'
            if (!parseType(i)) return false;
            skipSpaces(i);
            if (i >= s.size() || s[i] != ',') return false;
            ++i; // consume ','
            if (!parseType(i)) return false;
            skipSpaces(i);
            if (i >= s.size() || s[i] != '>') return false;
            ++i; // consume '>'
            return true;
        }

        // base types: basic vul types, bundles
        if (isBasicVulType(token)) return true;
        if (!isValidIdentifier(token)) return false;
        // check for bundle
        if (bundles.find(token) != bundles.end()) {
            seen_bundles.push_back(token);
            return true;
        }

        return false;
    };

    size_t idx = 0;
    bool ok = parseType(idx);
    skipSpaces(idx);
    return ok && idx == s.size();
}


/// @brief Load a VulDesign from a project file.
/// @param filename The name of the file to load.
/// @param err Error message in case of failure.
/// @return A unique_ptr to the loaded VulDesign, or nullptr on failure.
unique_ptr<VulDesign> VulDesign::loadFromFile(const string &filename, string &err) {
    auto design = std::unique_ptr<VulDesign>(new VulDesign());
    namespace fs = std::filesystem;

    fs::path filePath(filename);
    if (!fs::exists(filePath)) {
        err = "#10001: project file not found: " + filename;
        return nullptr;
    }

    fs::path dir = filePath.parent_path();
    design->project_dir = dir.string();
    design->project_name = filePath.stem().string();

    // load project info from .vul file
    VulProject vp;
    err = serializeParseProjectInfoFromFile(filePath.string(), vp);
    if (!err.empty()) {
        err = string("#10002: Failed to load project file: ") + filename + "; parser error: " + err;
        return nullptr;
    }
    if (vp.version != std::make_tuple(0,0,0)) {
        design->version = vp.version;
    }


    // load config.xml if present
    fs::path configFile = dir / "config.xml";
    if (fs::exists(configFile) && fs::is_regular_file(configFile)) {
        err = serializeParseConfigLibFromFile(configFile.string(), design->config_lib);
        if (!err.empty()) {
            err = string("#10003: Failed to load config lib file: ") + configFile.string() + "; parser error: " + err;
            return nullptr;
        }
    }

    // load all bundle xml files from bundle/ folder
    fs::path bundleDir = dir / "bundle";
    if (fs::exists(bundleDir) && fs::is_directory(bundleDir)) {
        for (auto &entry : fs::directory_iterator(bundleDir)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() == ".xml") {
                VulBundle vb;
                err = serializeParseBundleFromFile(entry.path().string(), vb);
                if (!err.empty()) {
                    err = string("#10004: Failed to load bundle file: ") + entry.path().string() + "; parser error: " + err;
                    return nullptr;
                }
                vb.ref_prefabs.clear();
                vb.ref_prefabs.push_back("_user_"); // special marker to indicate user-defined bundle
                design->bundles[vb.name] = vb;
            }
        }
    }

    // load all combine xml files from combine/ folder
    fs::path combineDir = dir / "combine";
    if (fs::exists(combineDir) && fs::is_directory(combineDir)) {
        for (auto &entry : fs::directory_iterator(combineDir)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() == ".xml") {
                VulCombine vc;
                err = serializeParseCombineFromFile(entry.path().string(), vc);
                if (!err.empty()) {
                    err = string("#10005: Failed to load combine file: ") + entry.path().string() + "; parser error: " + err;
                    return nullptr;
                }
                design->combines[vc.name] = vc;
            }
        }
    }

    // load design.xml (required)
    fs::path designFile = dir / "design.xml";
    if (fs::exists(designFile) && fs::is_regular_file(designFile)) {
        err = serializeParseDesignFromFile(designFile.string(),
            design->instances,
            design->pipes,
            design->req_connections,
            design->mod_pipe_connections,
            design->pipe_mod_connections,
            design->stalled_connections,
            design->update_constraints,
            design->vis_blocks,
            design->vis_texts
        );
        if (!err.empty()) {
            err = string("#10006: Failed to load design file: ") + designFile.string() + "; parser error: " + err;
            return nullptr;
        }
    } else {
        err = "#10007: design.xml not found in project folder: " + dir.string();
        return nullptr;
    }

    // load prefabs
    for (auto &pentry : vp.prefabs) {
        const string &prefab_name = pentry.first;
        const string &prefab_dir = pentry.second;
        fs::path prefab_dir_path = prefab_dir;
        fs::path prefab_file = prefab_dir_path / (prefab_name + ".vulp");
        VulPrefab prefab;
        err = serializeParsePrefabFromFile(prefab_file.string(), prefab);
        if (!err.empty()) {
            err = string("#10008: Failed to load prefab file: ") + prefab_file.string() + "; parser error: " + err;
            return nullptr;
        }
        prefab.path = prefab_dir_path.string();
        prefab.name = prefab_name;
        vector<VulBundle> dep_bundles;
        fs::path prefab_bundle_dir = prefab_dir_path / "bundle";
        // load dependent bundles in prefab's bundle/ folder
        if (fs::exists(prefab_bundle_dir) && fs::is_directory(prefab_bundle_dir)) {
            for (auto &entry : fs::directory_iterator(prefab_bundle_dir)) {
                if (!entry.is_regular_file()) continue;
                if (entry.path().extension() == ".xml") {
                    VulBundle vb;
                    err = serializeParseBundleFromFile(entry.path().string(), vb);
                    if (!err.empty()) {
                        err = string("#10009: Failed to load prefab bundle file: ") + entry.path().string() + "; parser error: " + err;
                        return nullptr;
                    }
                    dep_bundles.push_back(vb);
                }
            }
        }
        err = design->addPrefab(prefab, dep_bundles);
        if (!err.empty()) {
            err = string("#10010: Failed to load prefab '") + prefab_name + "' to design: " + err;
            return nullptr;
        }
    }

    return std::move(design);
}

/**
 * @brief Add a prefab to the design, along with its dependent bundles.
 * @param prefab The VulPrefab to add.
 * @param dep_bundles The list of VulBundles that the prefab depends on.
 * @return An empty string on success, or an error message on failure (e.g. name conflict).
 */
string VulDesign::addPrefab(VulPrefab &prefab, vector<VulBundle> &dep_bundles) {
    // check for name conflict
    string err = checkGlobalNameConflict(prefab.name);
    if (!err.empty()) {
        return string("Prefab name conflict: '") + prefab.name + "':" + err;
    }

    // add dependent bundles to design if not already present
    for (const VulBundle &vb : dep_bundles) {
        if (bundles.find(vb.name) == bundles.end()) {
            bundles[vb.name] = vb;
        } else {
            // check for consistency
            const VulBundle &existing = bundles[vb.name];
            bool match = (existing.members.size() == vb.members.size());
            if (match) {
                for (size_t i = 0; i < existing.members.size(); ++i) {
                    if (existing.members[i].name != vb.members[i].name ||
                        existing.members[i].type != vb.members[i].type) {
                        match = false;
                        break;
                    }
                }
            }
            if (!match) {
                return string("Bundle definition conflict for '") + vb.name + "' required by prefab '" + prefab.name + "'";
            }
            // update comments
            bundles[vb.name].comment = vb.comment;
        }
        // add reference to prefab in bundle
        auto &bundle_ref_prefabs = bundles[vb.name].ref_prefabs;
        if (std::find(bundle_ref_prefabs.begin(), bundle_ref_prefabs.end(), prefab.name) == bundle_ref_prefabs.end()) {
            bundle_ref_prefabs.push_back(prefab.name);
        }
    }

    // add prefab to design
    prefabs[prefab.name] = prefab;

    return string("");
}

string VulDesign::saveProject() {
    namespace fs = std::filesystem;
    string err;

    fs::path proj(project_dir);
    if (fs::exists(proj) && !fs::is_directory(proj)) return string("#16001: project dir path exists but is not a directory: ") + project_dir;
    if (!fs::exists(proj)) {
        try {
            fs::create_directories(proj);
        } catch (const std::exception &e) {
            return string("#16002: failed to create project directory: ") + e.what();
        }
    }

    // create bak directory with timestamp to avoid collisions
    std::time_t t = std::time(nullptr);
    string bakName = string("bak_") + std::to_string((long long)t);
    fs::path bakDir = proj / bakName;

    try {
        fs::create_directories(bakDir);
    } catch (const std::exception &e) {
        return string("#16003: failed to create bak directory: ") + e.what();
    }

    // prepare subdirs
    if (dirty_bundles) {
        try { fs::create_directories(bakDir / "bundle"); } catch (const std::exception &e) { fs::remove_all(bakDir); return string("#16004: failed to create bak bundle dir: ") + e.what(); }
    }
    if (dirty_combines) {
        try { fs::create_directories(bakDir / "combine"); } catch (const std::exception &e) { fs::remove_all(bakDir); return string("#16005: failed to create bak combine dir: ") + e.what(); }
    }

    // Save version info into {projectname}.vul XML file
    VulProject vp;
    vp.project_name = project_name;
    vp.version = version;
    vp.project_dir = project_dir;
    // collect prefab dirs
    for (const auto &kv : prefabs) {
        vp.prefabs.push_back(std::make_pair(kv.first, kv.second.path));
    }
    err = serializeSaveProjectInfoToFile((bakDir / (project_name + ".vul")).string(), vp);
    if (!err.empty()) {
        err = string("#16006: failed to save project file: ") + err;
        fs::remove_all(bakDir);
        return err;
    }

    // Always save design.xml (top-level)
    err = serializeSaveDesignToFile((bakDir / "design.xml").string(), 
        instances,
        pipes,
        req_connections,
        mod_pipe_connections,
        pipe_mod_connections,
        stalled_connections,
        update_constraints,
        vis_blocks,
        vis_texts
    );
    if (!err.empty()) {
        err = string("#16007: failed to save design file: ") + err;
        fs::remove_all(bakDir);
        return err;
    }

    // Save config lib if dirty
    if (dirty_config_lib) {
        err = serializeSaveConfigLibToFile((bakDir / "config.xml").string(), config_lib);
        if (!err.empty()) {
            err = string("#16008: failed to save config lib file: ") + err;
            fs::remove_all(bakDir);
            return err;
        }
    }

    // Save all bundles if dirty (one file per bundle)
    if (dirty_bundles) {
        for (const auto &kv : bundles) {
            // only save user-defined bundles (those referenced by "_user_" prefab)
            if (std::find(kv.second.ref_prefabs.begin(), kv.second.ref_prefabs.end(), "_user_") == kv.second.ref_prefabs.end()) {
                continue;
            }
            const string fname = (bakDir / "bundle" / (kv.first + ".xml")).string();
            err = serializeSaveBundleToFile(fname, kv.second);
            if (!err.empty()) {
                err = string("#16009: failed to save bundle '") + kv.first + "': " + err;
                fs::remove_all(bakDir);
                return err;
            }
        }
    }

    // Save all combines if dirty
    if (dirty_combines) {
        for (const auto &kv : combines) {
            const string fname = (bakDir / "combine" / (kv.first + ".xml")).string();
            err = serializeSaveCombineToFile(fname, kv.second);
            if (!err.empty()) {
                err = string("#16010: failed to save combine '") + kv.first + "': " + err;
                fs::remove_all(bakDir);
                return err;
            }
        }
    }

    // At this point, all files are written to bakDir. Now atomically replace originals.
    try {
        // .vul file
        fs::path origVul = proj / (project_name + ".vul");
        if (fs::exists(origVul)) fs::remove(origVul);
        fs::copy_file(bakDir / (project_name + ".vul"), origVul, fs::copy_options::overwrite_existing);

        // design.xml
        fs::path origDesign = proj / "design.xml";
        if (fs::exists(origDesign)) fs::remove(origDesign);
        fs::copy_file(bakDir / "design.xml", origDesign, fs::copy_options::overwrite_existing);

        // config.xml
        if (dirty_config_lib) {
            if (fs::exists(proj / "config.xml")) fs::remove(proj / "config.xml");
            fs::copy_file(bakDir / "config.xml", proj / "config.xml", fs::copy_options::overwrite_existing);
            dirty_config_lib = false;
        }

        // bundles
        if (dirty_bundles) {
            fs::path origBundleDir = proj / "bundle";
            if (fs::exists(origBundleDir)) fs::remove_all(origBundleDir);
            // copy directory
            fs::copy(bakDir / "bundle", origBundleDir, fs::copy_options::overwrite_existing | fs::copy_options::recursive);
            dirty_bundles = false;
        }

        // combines
        if (dirty_combines) {
            fs::path origCombineDir = proj / "combine";
            if (fs::exists(origCombineDir)) fs::remove_all(origCombineDir);
            // copy directory
            fs::copy(bakDir / "combine", origCombineDir, fs::copy_options::overwrite_existing | fs::copy_options::recursive);
            dirty_combines = false;
        }

        // cleanup bakDir
        fs::remove_all(bakDir);
    } catch (const std::exception &e) {
        // try to cleanup bakDir, but do not remove anything else
        try { fs::remove_all(bakDir); } catch (...) {}
        return string("#16012: failed to replace project files: ") + e.what();
    }

    return string();
}

/**
 * @brief Get the VulCombine definition for a given combine name.
 * If the combine does not exist, create a fake combine from a prefab with the same name.
 * @param combine_name The name of the combine to get.
 * @param is_fake Output parameter indicating whether the returned combine is fake (from prefab) or real.
 * @return A unique_ptr to the VulCombine definition.
 */
unique_ptr<VulCombine> VulDesign::getOrFakeCombineReadonly(const string &combine_name, bool &is_fake) {
    auto it = combines.find(combine_name);
    if (it != combines.end()) {
        is_fake = false;
        return std::make_unique<VulCombine>(it->second);
    }

    // check if there is a prefab with this name
    auto pit = prefabs.find(combine_name);
    if (pit != prefabs.end()) {
        const VulPrefab &vp = pit->second;
        VulCombine fake_combine;
        fakeCombineFromPrefab(vp, fake_combine);
        is_fake = true;
        return std::make_unique<VulCombine>(fake_combine);
    }

    // not found
    is_fake = false;
    return nullptr;
}

/**
 * @brief Check for name conflicts:
 * (1) No duplicate names within: bundles, combines, instances, pipes, and config_lib.
 * (2) All instances must refer to an existing combine.
 * (3) All names must be valid C++ identifiers and not C++ reserved keywords.
 * (4) All config reference names in combines must exist in config_lib.
 */
string VulDesign::_checkNameError() {
    
    // (1) No duplicate names within: bundles, combines, instances, pipes, prefabs, and config_lib.
    unordered_map<string, string> owner; // name -> category

    auto check_and_insert = [&](const string &name, const string &cat) -> string {
        if (name.empty()) return string("#11001: empty name in ") + cat;
        auto it = owner.find(name);
        if (it != owner.end()) {
            return string("#11002: duplicate name '") + name + "' found in " + cat + " and " + it->second;
        }
        owner[name] = cat;
        return string();
    };

    for (const auto &p : bundles) {
        string err = check_and_insert(p.first, "bundle");
        if (!err.empty()) return err;
    }
    for (const auto &p : combines) {
        string err = check_and_insert(p.first, "combine");
        if (!err.empty()) return err;
    }
    for (const auto &p : instances) {
        string err = check_and_insert(p.first, "instance");
        if (!err.empty()) return err;
    }
    for (const auto &p : pipes) {
        string err = check_and_insert(p.first, "pipe");
        if (!err.empty()) return err;
    }
    for (const auto &p : config_lib) {
        string err = check_and_insert(p.first, "config_lib");
        if (!err.empty()) return err;
    }
    for (const auto &p : prefabs) {
        string err = check_and_insert(p.first, "prefab");
        if (!err.empty()) return err;
    }

    // (2) All instances must refer to an existing combine.
    for (const auto &kv : instances) {
        const string &instname = kv.first;
        const VulInstance &vi = kv.second;
        if (combines.find(vi.combine) == combines.end()) {
            return string("#11003: instance '") + instname + " refers to unknown combine '" + vi.combine + "'";
        }
    }

    // (3) All names must be valid C++ identifiers and not C++ reserved keywords.
    for (const auto &o : owner) {
        if (!isValidIdentifier(o.first)) {
            return string("#11004: invalid identifier name: '") + o.first + "'";
        }
    }

    // (4) All config reference names in combines must exist in config_lib.
    for (auto &kv : combines) {
        const string &cname = kv.first;
        VulCombine &vc = kv.second;
        for (VulConfig &cfg : vc.config) {
            extractConfigReferences(cfg); // ensure cfg.ref is trimmed
            for (const string &refname : cfg.references) {
                if (!refname.empty()) {
                    if (config_lib.find(refname) == config_lib.end()) {
                        return string("#11005: combine '") + cname + " has config ref to unknown config '" + refname + "'";
                    }
                }
            }
        }
    }

    return string();
}

/**
 * @brief Check for type errors:
 * (1) All types must be valid VulSim data types within: bundles, combines, and pipes.
 * (2) All types must be valid VulSim Basic Data Types within: storagetick
 * (3) All bundle types must not be looped (i.e. a bundle cannot contain itself directly or indirectly).
 */
string VulDesign::_checkTypeError() {
    // All type dependencies of bundles have been pre-extracted into their vb.nested_bundles

    // (1) Validate bundle member types
    for (auto &bk : bundles) {
        VulBundle &vb = bk.second;
        vb.nested_bundles.clear();
        for (const VulArgument &m : vb.members) {
            if (!isValidTypeName(m.type, vb.nested_bundles)) {
                return string("#12001: bundle '") + bk.first + " member '" + m.name + "' has invalid type '" + m.type + "'";
            }
        }
    }

    // (1) Validate combines types (pipe ports, requests, services, storages)
    for (const auto &ck : combines) {
        const string &cname = ck.first;
        const VulCombine &vc = ck.second;

        for (const VulPipePort &p : vc.pipein) {
            if (!isValidTypeName(p.type)) {
                return string("#12002: combine '") + cname + " pipein '" + p.name + "' has invalid type '" + p.type + "'";
            }
        }
        for (const VulPipePort &p : vc.pipeout) {
            if (!isValidTypeName(p.type)) {
                return string("#12003: combine '") + cname + " pipeout '" + p.name + "' has invalid type '" + p.type + "'";
            }
        }

        for (const VulRequest &r : vc.request) {
            for (const VulArgument &a : r.arg) {
                if (!isValidTypeName(a.type)) {
                    return string("#12004: combine '") + cname + " request '" + r.name + "' arg '" + a.name + "' has invalid type '" + a.type + "'";
                }
            }
            for (const VulArgument &ret : r.ret) {
                if (!isValidTypeName(ret.type)) {
                    return string("#12005: combine '") + cname + " request '" + r.name + "' return '" + ret.name + "' has invalid type '" + ret.type + "'";
                }
            }
        }

        for (const VulService &s : vc.service) {
            for (const VulArgument &a : s.arg) {
                if (!isValidTypeName(a.type)) {
                    return string("#12006: combine '") + cname + " service '" + s.name + "' arg '" + a.name + "' has invalid type '" + a.type + "'";
                }
            }
            for (const VulArgument &ret : s.ret) {
                if (!isValidTypeName(ret.type)) {
                    return string("#12007: combine '") + cname + " service '" + s.name + "' return '" + ret.name + "' has invalid type '" + ret.type + "'";
                }
            }
        }

        for (const VulStorage &st : vc.storage) {
            if (!isValidTypeName(st.type)) {
                return string("#12008: combine '") + cname + " storage '" + st.name + "' has invalid type '" + st.type + "'";
            }
        }
        for (const VulStorage &st : vc.storagenext) {
            if (!isValidTypeName(st.type)) {
                return string("#12009: combine '") + cname + " storagenext '" + st.name + "' has invalid type '" + st.type + "'";
            }
        }
        for (const VulStorage &st : vc.storagetick) {
            if (!isBasicVulType(st.type)) {
                return string("#12010: combine '") + cname + " storagetick '" + st.name + "' must be a basic Vul type, got '" + st.type + "'";
            }
        }
    }

    // (1) Validate pipes
    for (const auto &pk : pipes) {
        const string &pname = pk.first;
        const VulPipe &vp = pk.second;
        if (!isValidTypeName(vp.type)) {
            return string("#12011: pipe '") + pname + " has invalid type '" + vp.type + "'";
        }
    }

    // (3) detect bundle cycles using vb.nested_bundles
    // build adjacency
    unordered_map<string, vector<string>> adj;
    for (auto &bk : bundles) {
        const string &bname = bk.first;
        VulBundle &vb = bk.second;
        for (const string &nb : vb.nested_bundles) {
            if (!nb.empty()) adj[bname].push_back(nb);
        }
    }

    // dfs
    std::unordered_set<string> visited;
    std::unordered_set<string> onstack;
    string cycle_msg;
    std::function<bool(const string&, vector<string>&)> dfs = [&](const string &u, vector<string> &path) -> bool {
        visited.insert(u);
        onstack.insert(u);
        path.push_back(u);
        for (const string &v : adj[u]) {
            if (onstack.find(v) != onstack.end()) {
                // build cycle path
                vector<string> cycle;
                auto it = std::find(path.begin(), path.end(), v);
                if (it != path.end()) {
                    for (; it != path.end(); ++it) cycle.push_back(*it);
                    cycle.push_back(v);
                } else {
                    cycle = {u, v, u};
                }
                cycle_msg = "#12012: bundle cycle detected: ";
                for (size_t i = 0; i < cycle.size(); ++i) {
                    if (i) cycle_msg += " -> ";
                    cycle_msg += cycle[i];
                }
                return true;
            }
            if (visited.find(v) == visited.end()) {
                if (dfs(v, path)) return true;
            }
        }
        onstack.erase(u);
        path.pop_back();
        return false;
    };

    for (const auto &bk : bundles) {
        const string &bname = bk.first;
        if (visited.find(bname) == visited.end()) {
            vector<string> path;
            if (dfs(bname, path)) return cycle_msg.empty() ? string("#12012: bundle cycle detected") : cycle_msg;
        }
    }

    return string();
}

/**
 * @brief Check for request-service connection errors:
 * (1) All requests connections must refer to existing instances and requests.
 * (2) All services connections must refer to existing instances and services.
 * (3) All connections must match in argument and return types exactly in type and sequence.
 * (4) Request without return can connect to multiple services or not connect at all.
 * (5) All requests with return must be connected to one service.
 * (6) Except for: Service without arg and ret can be connected to Request with arg without ret.
 */
string VulDesign::_checkReqConnectionError() {

    // For each request connection, validate existence and type compatibility
    for (const VulReqConnection &rc : req_connections) {
        // (1) request instance exists
        auto instIt = instances.find(rc.req_instance);
        if (instIt == instances.end()) {
            return string("#13001: req connection refers to unknown instance '") + rc.req_instance + "'";
        }

        // find the combine of the request instance
        const string &reqCombineName = instIt->second.combine;
        unique_ptr<VulCombine> reqCombinePtr = getOrFakeCombineReadonly(reqCombineName);
        if (!reqCombinePtr) {
            return string("#13002: req connection instance '") + rc.req_instance + "' refers to unknown combine '" + reqCombineName + "'";
        }

        // find request definition in combine
        const VulCombine &reqCombine = *reqCombinePtr;
        const VulRequest *reqDef = nullptr;
        for (const VulRequest &r : reqCombine.request) {
            if (r.name == rc.req_name) { reqDef = &r; break; }
        }
        if (!reqDef) {
            return string("#13003: request '") + rc.req_name + "' not found in combine '" + reqCombineName + "' for instance '" + rc.req_instance + "'";
        }

        const string &servInst = rc.serv_instance;
        const string &servName = rc.serv_name;

        // (2) service instance exists
        auto sInstIt = instances.find(servInst);
        if (sInstIt == instances.end()) {
            // pipe has a clear service without arg and return, allow this special case
            // (6) Except for: Service without arg and ret can be connected to Request with arg without ret.
            if(pipes.find(servInst) != pipes.end() && servName == "clear" && reqDef->ret.empty()) {
                    continue;
            }

            return string("#13004: req connection references unknown service instance '") + servInst + "' (for request '" + rc.req_name + "')";
        }

        // find service combine
        const string &servCombineName = sInstIt->second.combine;
        unique_ptr<VulCombine> servCombinePtr = getOrFakeCombineReadonly(servCombineName);
        if (!servCombinePtr) {
            return string("#13005: service instance '") + servInst + "' refers to unknown combine '" + servCombineName + "'";
        }

        // find service definition
        const VulCombine &servCombine = *servCombinePtr;
        const VulService *servDef = nullptr;
        for (const VulService &s : servCombine.service) {
            if (s.name == servName) { servDef = &s; break; }
        }
        if (!servDef) {
            return string("#13006: service '") + servName + "' not found in combine '" + servCombineName + "' for instance '" + servInst + "'";
        }

        // (3) type matching: args and returns must match exactly in type and order
        // (6) Except for: Service without arg and ret can be connected to Request with arg without ret.
        if (servDef->arg.empty() && servDef->ret.empty() && reqDef->ret.empty()) {
            // allow any args on request
            continue;
        }
        // args
        if (reqDef->arg.size() != servDef->arg.size()) {
            return string("#13007: argument count mismatch between request '") + rc.req_name + "' (" + rc.req_instance + ") and service '" + servName + "' (" + servInst + ")";
        }
        for (size_t i = 0; i < reqDef->arg.size(); ++i) {
            if (reqDef->arg[i].type != servDef->arg[i].type) {
                return string("#13008: argument type mismatch at index ") + std::to_string(i) + string(" between request '") + rc.req_name + "' (" + rc.req_instance + ") and service '" + servName + "' (" + servInst + "): '" + reqDef->arg[i].type + "' vs '" + servDef->arg[i].type + "'";
            }
        }

        // returns
        if (reqDef->ret.size() != servDef->ret.size()) {
            return string("#13009: return count mismatch between request '") + rc.req_name + "' (" + rc.req_instance + ") and service '" + servName + "' (" + servInst + ")";
        }
        for (size_t i = 0; i < reqDef->ret.size(); ++i) {
            if (reqDef->ret[i].type != servDef->ret[i].type) {
                return string("#13010: return type mismatch at index ") + std::to_string(i) + string(" between request '") + rc.req_name + "' (" + rc.req_instance + ") and service '" + servName + "' (" + servInst + "): '" + reqDef->ret[i].type + "' vs '" + servDef->ret[i].type + "'";
            }
        }
    }

    // (5) All requests with return in all instances must be connected to exactly one service

    // Build a lookup of how many service endpoints each instance.request is connected to
    std::unordered_map<string, std::unordered_map<string, size_t>> connCounts;
    for (const VulReqConnection &rc : req_connections) {
        // accumulate number of service endpoints referenced for this req
        if (!rc.req_instance.empty() && !rc.req_name.empty()) {
            connCounts[rc.req_instance][rc.req_name] ++;
        }
    }

    // For every instance, for every request that has returns, ensure exactly one connected service
    for (const auto &instKv : instances) {
        const string &instName = instKv.first;
        const string &combName = instKv.second.combine;
        unique_ptr<VulCombine> combPtr = getOrFakeCombineReadonly(combName);
        if (!combPtr) continue; // already checked elsewhere
        const VulCombine &vc = *combPtr;
        for (const VulRequest &r : vc.request) {
            if (r.ret.empty()) continue; // only care about requests with return

            size_t connected = 0;
            auto it1 = connCounts.find(instName);
            if (it1 != connCounts.end()) {
                auto it2 = it1->second.find(r.name);
                if (it2 != it1->second.end()) connected = it2->second;
            }

            if (connected == 0) {
                return string("#13011: request '") + r.name + "' on instance '" + instName + "' has return values and is not connected to any service";
            }
            if (connected > 1) {
                return string("#13012: request '") + r.name + "' on instance '" + instName + "' has return values and is connected to multiple services";
            }
        }
    }

    // All checks passed
    return string();
}

/**
 * @brief Check for pipe connection errors:
 * (1) Module-Pipe connections must refer to existing instance with valid pipeout port, to a valid pipe input potr index.
 * (2) Pipe-Module connections must refer to existing instance with valid pipein port, to a valid pipe output port index.
 * (3) The pipe port type must match the connected module port type exactly.
 */
string VulDesign::_checkPipeConnectionError() {
    // Validate Module -> Pipe connections
    for (const VulModulePipeConnection &mpc : mod_pipe_connections) {
        // instance exists
        auto instIt = instances.find(mpc.instance);
        if (instIt == instances.end()) {
            return string("#14001: modpipe refers to unknown instance '") + mpc.instance + "'";
        }

        // combine exists
        const string &combName = instIt->second.combine;
        unique_ptr<VulCombine> combPtr = getOrFakeCombineReadonly(combName);
        if (!combPtr) {
            return string("#14002: modpipe instance '") + mpc.instance + "' refers to unknown combine '" + combName + "'";
        }

        // module pipeout port exists in combine
        const VulCombine &vc = *combPtr;
        const VulPipePort *portDef = nullptr;
        for (const VulPipePort &p : vc.pipeout) {
            if (p.name == mpc.pipeoutport) { portDef = &p; break; }
        }
        if (!portDef) {
            return string("#14003: modpipe instance '") + mpc.instance + "' refers to unknown pipeout port '" + mpc.pipeoutport + "' in combine '" + combName + "'";
        }

        // pipe exists
        auto pipeIt = pipes.find(mpc.pipe);
        if (pipeIt == pipes.end()) {
            return string("#14004: modpipe instance '") + mpc.instance + "' references unknown pipe '" + mpc.pipe + "'";
        }

        const VulPipe &pp = pipeIt->second;
        // portindex must be a valid input index on the pipe
        if (mpc.portindex >= pp.inputsize) {
            return string("#14005: modpipe instance '") + mpc.instance + "' connects to pipe '" + mpc.pipe + "' input index " + std::to_string(mpc.portindex) + " out of range (inputsize=" + std::to_string(pp.inputsize) + ")";
        }

        // type must match exactly
        if (portDef->type != pp.type) {
            return string("#14006: type mismatch between module pipeout '") + portDef->name + "' (type '" + portDef->type + "') on instance '" + mpc.instance + "' and pipe '" + mpc.pipe + "' (type '" + pp.type + "')";
        }
    }

    // Validate Pipe -> Module connections
    for (const VulPipeModuleConnection &pmc : pipe_mod_connections) {
        // instance exists
        auto instIt = instances.find(pmc.instance);
        if (instIt == instances.end()) {
            return string("#14007: pipemod refers to unknown instance '") + pmc.instance + "'";
        }

        // combine exists
        const string &combName = instIt->second.combine;
        unique_ptr<VulCombine> combPtr = getOrFakeCombineReadonly(combName);
        if (!combPtr) {
            return string("#14008: pipemod instance '") + pmc.instance + "' refers to unknown combine '" + combName + "'";
        }

        // module pipein port exists in combine
        const VulCombine &vc = *combPtr;
        const VulPipePort *portDef = nullptr;
        for (const VulPipePort &p : vc.pipein) {
            if (p.name == pmc.pipeinport) { portDef = &p; break; }
        }
        if (!portDef) {
            return string("#14009: pipemod instance '") + pmc.instance + "' refers to unknown pipein port '" + pmc.pipeinport + "' in combine '" + combName + "'";
        }

        // pipe exists
        auto pipeIt = pipes.find(pmc.pipe);
        if (pipeIt == pipes.end()) {
            return string("#14010: pipemod instance '") + pmc.instance + "' references unknown pipe '" + pmc.pipe + "'";
        }

        const VulPipe &pp = pipeIt->second;
        // portindex must be a valid output index on the pipe
        if (pmc.portindex >= pp.outputsize) {
            return string("#14011: pipemod instance '") + pmc.instance + "' connects to pipe '" + pmc.pipe + "' output index " + std::to_string(pmc.portindex) + " out of range (outputsize=" + std::to_string(pp.outputsize) + ")";
        }

        // type must match exactly
        if (portDef->type != pp.type) {
            return string("#14012: type mismatch between module pipein '") + portDef->name + "' (type '" + portDef->type + "') on instance '" + pmc.instance + "' and pipe '" + pmc.pipe + "' (type '" + pp.type + "')";
        }
    }

    return string();
}

/**
 * @brief Check for update sequence connection errors:
 * (1) All update sequence connections must refer to existing instances.
 * (2) All stalled connections must refer to existing instances with stallable flag in its combine as true.
 * (3) No loops in the combined update graph of:
 *   (a) Update sequence connections: former_instance -> latter_instance
 *   (b) Stalled connections: src_instance -> dest_instance
 */
string VulDesign::_checkSeqConnectionError() {
    // (1) All update sequence connections must refer to existing instances.
    for (const VulUpdateSequence &us : update_constraints) {
        if (instances.find(us.former_instance) == instances.end()) {
            return string("#15001: update sequence refers to unknown former instance '") + us.former_instance + "'";
        }
        if (instances.find(us.latter_instance) == instances.end()) {
            return string("#15002: update sequence refers to unknown latter instance '") + us.latter_instance + "'";
        }
    }

    // (2) All stalled connections must refer to existing instances with stallable flag in its combine as true.
    for (const VulStalledConnection &sc : stalled_connections) {
        if (instances.find(sc.src_instance) == instances.end()) {
            return string("#15003: stalled connection refers to unknown src instance '") + sc.src_instance + "'";
        }
        if (instances.find(sc.dest_instance) == instances.end()) {
            return string("#15004: stalled connection refers to unknown dest instance '") + sc.dest_instance + "'";
        }

        // dest combine must be stallable
        const string &destComb = instances.at(sc.dest_instance).combine;
        unique_ptr<VulCombine> destCombPtr = getOrFakeCombineReadonly(destComb);
        if (!destCombPtr) {
            return string("#15005: stalled connection dest instance '") + sc.dest_instance + "' refers to unknown combine '" + destComb + "'";
        }
        if (!destCombPtr->stallable) {
            return string("#15006: stalled connection dest instance '") + sc.dest_instance + "' combine '" + destComb + "' is not stallable";
        }

        // src combine must be stallable
        const string &srcComb = instances.at(sc.src_instance).combine;
        unique_ptr<VulCombine> srcCombPtr = getOrFakeCombineReadonly(srcComb);
        if (!srcCombPtr) {
            return string("#15007: stalled connection src instance '") + sc.src_instance + "' refers to unknown combine '" + srcComb + "'";
        }
        if (!srcCombPtr->stallable) {
            return string("#15008: stalled connection src instance '") + sc.src_instance + "' combine '" + srcComb + "' is not stallable";
        }
    }

    // (3) No loops in the combined update graph of update sequences and stalled connections.
    // Build adjacency list for instances
    unordered_map<string, vector<string>> adj;
    for (const VulUpdateSequence &us : update_constraints) {
        adj[us.former_instance].push_back(us.latter_instance);
    }
    for (const VulStalledConnection &sc : stalled_connections) {
        adj[sc.src_instance].push_back(sc.dest_instance);
    }

    // DFS to detect cycles
    std::unordered_set<string> visited;
    std::unordered_set<string> onstack;
    string cycle_msg;
    std::function<bool(const string&, vector<string>&)> dfs = [&](const string &u, vector<string> &path) -> bool {
        visited.insert(u);
        onstack.insert(u);
        path.push_back(u);
        for (const string &v : adj[u]) {
            if (onstack.find(v) != onstack.end()) {
                // found cycle, build path
                vector<string> cycle;
                auto it = std::find(path.begin(), path.end(), v);
                if (it != path.end()) {
                    for (; it != path.end(); ++it) cycle.push_back(*it);
                    cycle.push_back(v);
                } else {
                    cycle = {u, v, u};
                }
                cycle_msg = "#15009: update graph cycle detected: ";
                for (size_t i = 0; i < cycle.size(); ++i) {
                    if (i) cycle_msg += " -> ";
                    cycle_msg += cycle[i];
                }
                return true;
            }
            if (visited.find(v) == visited.end()) {
                if (dfs(v, path)) return true;
            }
        }
        onstack.erase(u);
        path.pop_back();
        return false;
    };

    // Run DFS starting from all instances present in the adjacency or instances list
    for (const auto &kv : instances) {
        const string &iname = kv.first;
        if (visited.find(iname) == visited.end()) {
            vector<string> path;
            if (dfs(iname, path)) return cycle_msg.empty() ? string("#15009: update graph cycle detected") : cycle_msg;
        }
    }

    return string();
}

