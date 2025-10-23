/*
 * Copyright (c) 2025 Meng-CZ
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <unordered_set>
#include <functional>

#include "command.h"
#include "codehelper.h"

/**
 * @brief Check if the given name conflicts with existing names in the design.
 * Conflicting names are those that already exist in bundles, combines, instances, pipes, or config_lib.
 * @param design The VulDesign object to check against.
 * @param name The name to check for conflicts.
 * @return An empty string if there are no conflicts, or an error message if there is a conflict.
 */
string isNameConflict(VulDesign &design, const string &name) {
    return design.checkGlobalNameConflict(name);
}

/**
 * @brief Check if the given name conflicts with local names in the combine.
 * Conflicting names are those that already exist as pipein, pipeout, config, request, service, or all types of storage names.
 * @param vc The VulCombine object to check against.
 * @param name The name to check for conflicts.
 * @return An empty string if there are no conflicts, or an error message if there is a conflict.
 */
string isLocalNameConflict(const VulCombine &vc, const string &name) {
    for (const VulPipePort &p : vc.pipein) if (p.name == name) return "Input pipe port name conflict";
    for (const VulPipePort &p : vc.pipeout) if (p.name == name) return "Output pipe port name conflict";
    for (const VulConfig &c : vc.config) if (c.name == name) return "Config name conflict";
    for (const VulRequest &r : vc.request) if (r.name == name) return "Request name conflict";
    for (const VulService &s : vc.service) if (s.name == name) return "Service name conflict";
    for (const VulStorage &s : vc.storage) if (s.name == name) return "Storage name conflict";
    for (const VulStorage &s : vc.storagenext) if (s.name == name) return "Storagenext name conflict";
    for (const VulStorage &s : vc.storagetick) if (s.name == name) return "Storagetick name conflict";
    for (const VulStorageArray &s : vc.storagenextarray) if (s.name == name) return "Storagenextarray name conflict";
    return "";
}

/**
 * @brief Add or update a config item in the design's config library.
 * If the config item already exists, its value and comment will be updated.
 * @param design The VulDesign object to modify.
 * @param name The name of the config item.
 * @param value The value of the config item.
 * @param comment An optional comment for the config item.
 * @return An empty string on success, or an error message on failure.
 */
string cmdUpdateConfigItem(VulDesign &design, const string &name, const string &value, const string &comment) {
    if (name.empty()) return "#20001: Config item name cannot be empty";
    if (!isValidIdentifier(name)) return "#20002: Invalid config item name (must be a valid C++ identifier)";
    auto it = design.config_lib.find(name);
    if (it != design.config_lib.end()) {
        // Update existing config item
        it->second.value = value;
        it->second.comment = comment;
    } else {
        // Add new config item
        string conflict = isNameConflict(design, name);
        if (!conflict.empty()) {
            return string("#20003: Cannot add config item '") + name + "': " + conflict;
        }
        VulConfigItem vci;
        vci.name = name;
        vci.value = value;
        vci.comment = comment;
        design.config_lib[name] = vci;
    }
    design.dirty_config_lib = true;
    return "";
}

string _fullWordReplace(const string &s, const string &oldword, const string &newword) {
    if (oldword.empty()) return s; // nothing to do
    string out;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = (unsigned char)s[i];
        if (std::isalpha(c) || s[i] == '_') {
            size_t j = i + 1;
            while (j < s.size()) {
                unsigned char cc = (unsigned char)s[j];
                if (!(std::isalnum(cc) || cc == '_')) break;
                ++j;
            }
            string ident = s.substr(i, j - i);
            if (ident == oldword) {
                out += newword;
            } else {
                out += ident;
            }
            i = j;
        } else {
            out += s[i];
            ++i;
        }
    }
    return out;
}

/**
 * @brief Rename a config item in the design's config library.
 * All combines using this config item will also be updated.
 * If the new name is empty, it will be treated as a delete operation.
 * @param design The VulDesign object to modify.
 * @param oldname The old name of the config item.
 * @param newname The new name of the config item.
 * @return An empty string on success, or an error message on failure.
 */
string cmdRenameConfigItem(VulDesign &design, const string &oldname, const string &newname) {
    if (oldname.empty()) return "#20010: old config name cannot be empty";

    auto it = design.config_lib.find(oldname);
    if (it == design.config_lib.end()) return string("#20011: config item '") + oldname + "' not found";

    // If newname is empty -> treat as delete operation
    if (newname.empty()) {
        // ensure no combine references this config
        for (const auto &ck : design.combines) {
            const VulCombine &vc = ck.second;
            for (const VulConfig &cfg : vc.config) {
                if (cfg.references.find(oldname) != cfg.references.end()) {
                    return string("#20012: cannot delete config item '") + oldname + "' because combine '" + ck.first + "' references it";
                }
            }
        }

        // safe to delete
        design.config_lib.erase(it);
        design.dirty_config_lib = true;
        return string();
    }

    // Rename operation
    if (!isValidIdentifier(newname)) return string("#20013: Invalid new config name '") + newname + "'";

    if (newname == oldname) return string(); // nothing to do

    // check conflict with other names in design
    string conflict = isNameConflict(design, newname);
    if (!conflict.empty()) return string("#20014: cannot rename to '") + newname + "': " + conflict;

    // perform rename: insert new entry and remove old
    VulConfigItem vci = it->second;
    vci.name = newname;
    design.config_lib.erase(it);
    design.config_lib[newname] = vci;

    // update all combines that reference oldname
    for (auto &ck : design.combines) {
        VulCombine &vc = ck.second;
        bool changed = false;
        for (VulConfig &cfg : vc.config) {
            if (cfg.references.find(oldname) != cfg.references.end()) {
                cfg.value = _fullWordReplace(cfg.value, oldname, newname);
                // re-extract references
                extractConfigReferences(cfg);
                changed = true;
            }
        }
        if (changed) design.dirty_combines = true;
    }

    design.dirty_config_lib = true;
    return string();
}

/**
 * @brief Check that all referenced prefabs in the bundle are user-only.
 * @param ref_prefabs The list of referenced prefab names.
 * @return true if ref_prefabs contains only a "_user_" item, false otherwise.
 */
bool _checkBundleRefPrefabUserOnly(const vector<string> &ref_prefabs) {
    return (ref_prefabs.size() == 1 && ref_prefabs[0] == "_user_");
}

/**
 * @brief Add or update a bundle in the design.
 * If the bundle already exists, its comment will be updated.
 * @param design The VulDesign object to modify.
 * @param name The name of the bundle.
 * @param comment An optional comment for the bundle.
 * @return An empty string on success, or an error message on failure.
 */
string cmdUpdateBundle(VulDesign &design, const string &name, const string &comment) {
    if (name.empty()) return "#21001: Bundle name cannot be empty";
    if (!isValidIdentifier(name)) return "#21002: Invalid bundle name (must be a valid C++ identifier)";
    auto it = design.bundles.find(name);
    if (it != design.bundles.end()) {
        // Update existing bundle
        // check for only _user_ mask in bundle ref_prefabs
        if (!_checkBundleRefPrefabUserOnly(it->second.ref_prefabs)) {
            return "#21004: Cannot update bundle '" + name + "': references prefabs";
        }
        it->second.comment = comment;
    } else {
        // Add new bundle
        string conflict = isNameConflict(design, name);
        if (!conflict.empty()) {
            return string("#21003: Cannot add bundle '") + name + "': " + conflict;
        }
        VulBundle vb;
        vb.name = name;
        vb.comment = comment;
        design.bundles[name] = vb;
    }
    design.dirty_bundles = true;
    return "";
}

/**
 * @brief Rename a bundle in the design.
 * All combines and pipes using this bundle will also be updated.
 * If the new name is empty, it will be treated as a delete operation.
 * @param design The VulDesign object to modify.
 * @param oldname The old name of the bundle.
 * @param newname The new name of the bundle.
 * @return An empty string on success, or an error message on failure.
 */
string cmdRenameBundle(VulDesign &design, const string &oldname, const string &newname) {
    if (oldname.empty()) return "#21010: old bundle name cannot be empty";

    auto it = design.bundles.find(oldname);
    if (it == design.bundles.end()) return string("#21011: bundle '") + oldname + "' not found";

    if (!_checkBundleRefPrefabUserOnly(it->second.ref_prefabs)) {
        return "#21014: Cannot rename bundle '" + oldname + "': references prefabs";
    }

    // helper: tokenize and replace full identifier tokens equal to oldname
    auto replaceTokenInType = [&](const string &stype, string &out) -> bool {
        out.clear();
        bool changed = false;
        size_t i = 0;
        while (i < stype.size()) {
            unsigned char c = (unsigned char)stype[i];
            if (std::isalpha(c) || stype[i] == '_') {
                size_t j = i + 1;
                while (j < stype.size()) {
                    unsigned char cc = (unsigned char)stype[j];
                    if (!(std::isalnum(cc) || stype[j] == '_')) break;
                    ++j;
                }
                string tok = stype.substr(i, j - i);
                if (tok == oldname) {
                    out.append(newname);
                    changed = true;
                } else {
                    out.append(tok);
                }
                i = j;
            } else {
                out.push_back(stype[i]);
                ++i;
            }
        }
        return changed;
    };

    // DELETE operation if newname empty
    if (newname.empty()) {
        // check references in combines
        for (const auto &ck : design.combines) {
            const string &cname = ck.first;
            const VulCombine &vc = ck.second;
            // check pipein/pipeout
            for (const VulPipePort &p : vc.pipein) {
                string tmp; if (replaceTokenInType(p.type, tmp)) return string("#21012: cannot delete bundle '") + oldname + "' because combine '" + cname + "' pipein references it";
            }
            for (const VulPipePort &p : vc.pipeout) {
                string tmp; if (replaceTokenInType(p.type, tmp)) return string("#21012: cannot delete bundle '") + oldname + "' because combine '" + cname + "' pipeout references it";
            }
            // requests
            for (const VulRequest &r : vc.request) {
                for (const VulArgument &a : r.arg) { string tmp; if (replaceTokenInType(a.type, tmp)) return string("#21012: cannot delete bundle '") + oldname + "' because combine '" + cname + "' request '" + r.name + "' arg references it"; }
                for (const VulArgument &a : r.ret) { string tmp; if (replaceTokenInType(a.type, tmp)) return string("#21012: cannot delete bundle '") + oldname + "' because combine '" + cname + "' request '" + r.name + "' return references it"; }
            }
            // services
            for (const VulService &s : vc.service) {
                for (const VulArgument &a : s.arg) { string tmp; if (replaceTokenInType(a.type, tmp)) return string("#21012: cannot delete bundle '") + oldname + "' because combine '" + cname + "' service '" + s.name + "' arg references it"; }
                for (const VulArgument &a : s.ret) { string tmp; if (replaceTokenInType(a.type, tmp)) return string("#21012: cannot delete bundle '") + oldname + "' because combine '" + cname + "' service '" + s.name + "' return references it"; }
            }
            // storages
            for (const VulStorage &st : vc.storage) { string tmp; if (replaceTokenInType(st.type, tmp)) return string("#21012: cannot delete bundle '") + oldname + "' because combine '" + cname + "' storage '" + st.name + "' references it"; }
            for (const VulStorage &st : vc.storagenext) { string tmp; if (replaceTokenInType(st.type, tmp)) return string("#21012: cannot delete bundle '") + oldname + "' because combine '" + cname + "' storagenext '" + st.name + "' references it"; }
            for (const VulStorage &st : vc.storagetick) { string tmp; if (replaceTokenInType(st.type, tmp)) return string("#21012: cannot delete bundle '") + oldname + "' because combine '" + cname + "' storagetick '" + st.name + "' references it"; }
        }

        // check pipes
        for (const auto &pk : design.pipes) {
            const VulPipe &vp = pk.second;
            string tmp; if (replaceTokenInType(vp.type, tmp)) return string("#21012: cannot delete bundle '") + oldname + "' because pipe '" + pk.first + "' references it";
        }

        // check bundles referencing this bundle
        for (const auto &bk : design.bundles) {
            if (bk.first == oldname) continue;
            for (const VulArgument &m : bk.second.members) {
                string tmp; if (replaceTokenInType(m.type, tmp)) return string("#21012: cannot delete bundle '") + oldname + "' because bundle '" + bk.first + "' member '" + m.name + "' references it";
            }
        }

        // safe to delete
        design.bundles.erase(it);
        design.dirty_bundles = true;
        return string();
    }

    // RENAME operation
    if (!isValidIdentifier(newname)) return string("#21013: Invalid new bundle name '") + newname + "'";
    if (newname == oldname) return string();

    // check conflict with other names in design
    string conflict = isNameConflict(design, newname);
    if (!conflict.empty()) return string("#21014: cannot rename to '") + newname + "': " + conflict;

    // move bundle
    VulBundle vb = it->second;
    vb.name = newname;
    design.bundles.erase(it);
    design.bundles[newname] = vb;

    bool anyCombineChanged = false;
    // update combines
    for (auto &ck : design.combines) {
        VulCombine &vc = ck.second;
        string out;
        for (VulPipePort &p : vc.pipein) { if (replaceTokenInType(p.type, out)) { p.type = out; anyCombineChanged = true; } }
        for (VulPipePort &p : vc.pipeout) { if (replaceTokenInType(p.type, out)) { p.type = out; anyCombineChanged = true; } }
        for (VulRequest &r : vc.request) {
            for (VulArgument &a : r.arg) if (replaceTokenInType(a.type, out)) { a.type = out; anyCombineChanged = true; }
            for (VulArgument &a : r.ret) if (replaceTokenInType(a.type, out)) { a.type = out; anyCombineChanged = true; }
        }
        for (VulService &s : vc.service) {
            for (VulArgument &a : s.arg) if (replaceTokenInType(a.type, out)) { a.type = out; anyCombineChanged = true; }
            for (VulArgument &a : s.ret) if (replaceTokenInType(a.type, out)) { a.type = out; anyCombineChanged = true; }
        }
        for (VulStorage &st : vc.storage) if (replaceTokenInType(st.type, out)) { st.type = out; anyCombineChanged = true; }
        for (VulStorage &st : vc.storagenext) if (replaceTokenInType(st.type, out)) { st.type = out; anyCombineChanged = true; }
        for (VulStorage &st : vc.storagetick) if (replaceTokenInType(st.type, out)) { st.type = out; anyCombineChanged = true; }
    }

    bool anyPipeChanged = false;
    for (auto &pk : design.pipes) {
        VulPipe &vp = pk.second;
        string out;
        if (replaceTokenInType(vp.type, out)) { vp.type = out; anyPipeChanged = true; }
    }

    if (anyCombineChanged) design.dirty_combines = true;
    if (anyPipeChanged) design.dirty_combines = true; // no separate dirty_pipes flag
    design.dirty_bundles = true;

    return string();
}

/**
 * @brief Add, update, or remove a member in a bundle.
 * If the member already exists, its type and comment will be updated.
 * If membertype is empty, the member will be removed.
 * @param design The VulDesign object to modify.
 * @param bundlename The name of the bundle to modify.
 * @param membername The name of the member to add, update, or remove.
 * @param membertype The type of the member. If empty, the member will be removed.
 * @param comment An optional comment for the member.
 * @param value An optional default value for the member (not used currently).
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupBundleMember(VulDesign &design, const string &bundlename, const string &membername, const string &membertype, const string &comment, const string &value) {
    if (bundlename.empty()) return "#21020: bundle name cannot be empty";
    auto it = design.bundles.find(bundlename);
    if (it == design.bundles.end()) return string("#21021: bundle '") + bundlename + "' not found";

    if (membername.empty()) return "#21022: member name cannot be empty";
    if (!isValidIdentifier(membername)) return string("#21023: invalid member name '") + membername + "'";

    VulBundle &vb = it->second;
    if (!_checkBundleRefPrefabUserOnly(vb.ref_prefabs)) {
        return "#21026: Cannot modify bundle '" + bundlename + "': references prefabs";
    }

    // Remove member if membertype is empty
    if (membertype.empty()) {
        auto mit = std::find_if(vb.members.begin(), vb.members.end(), [&](const VulArgument &m){ return m.name == membername; });
        if (mit == vb.members.end()) return string("#21024: member '") + membername + "' not found in bundle '" + bundlename + "'";
        vb.members.erase(mit);
        design.dirty_bundles = true;
        return string();
    }

    // Validate member type using design's type checker
    if (!design.isValidTypeName(membertype)) {
        return string("#21025: invalid member type '") + membertype + "'";
    }

    // Add or update member
    for (VulArgument &m : vb.members) {
        if (m.name == membername) {
            m.type = membertype;
            m.comment = comment;
            design.dirty_bundles = true;
            return string();
        }
    }

    // not found -> add new member
    VulArgument na;
    na.name = membername;
    na.type = membertype;
    na.comment = comment;
    vb.members.push_back(na);
    design.dirty_bundles = true;
    return string();
}

/**
 * @brief Add or update a combine in the design.
 * If the combine already exists, its comment and stallable flag will be updated.
 * @param design The VulDesign object to modify.
 * @param name The name of the combine.
 * @param comment An optional comment for the combine.
 * @param stallable Whether the combine is stallable.
 * @return An empty string on success, or an error message on failure.
 */
string cmdUpdateCombine(VulDesign &design, const string &name, const string &comment, bool stallable) {
    if (name.empty()) return "#22001: Combine name cannot be empty";
    if (!isValidIdentifier(name)) return "#22002: Invalid combine name (must be a valid C++ identifier)";
    auto it = design.combines.find(name);
    if (it != design.combines.end()) {
        // if stallable changed from true to false, check no stalled-connection using this combine
        if (it->second.stallable && !stallable) {
            // any stalled connection that references any instance of this combine blocks clearing stallable
            for (const VulStalledConnection &sc : design.stalled_connections) {
                // check source instance
                auto isk = design.instances.find(sc.src_instance);
                if (isk != design.instances.end() && isk->second.combine == name) {
                    return string("#22004: cannot set stallable=false for combine '") + name + "' because instance '" + sc.src_instance + "' is source in a stalled connection";
                }
                // check dest instance
                isk = design.instances.find(sc.dest_instance);
                if (isk != design.instances.end() && isk->second.combine == name) {
                    return string("#22004: cannot set stallable=false for combine '") + name + "' because instance '" + sc.dest_instance + "' is destination in a stalled connection";
                }
            }
        }

        // Update existing combine
        it->second.comment = comment;
        it->second.stallable = stallable;
    } else {
        // Add new combine
        string conflict = isNameConflict(design, name);
        if (!conflict.empty()) {
            return string("#22003: Cannot add combine '") + name + "': " + conflict;
        }
        VulCombine vc;
        vc.name = name;
        vc.comment = comment;
        vc.stallable = stallable;
        design.combines[name] = vc;

        auto &nvc = design.combines[name];
        nvc.init.name = name + "_init";
        nvc.tick.name = name + "_tick";
        nvc.applytick.name = name + "_applytick";
        nvc.init.file = nvc.init.name + ".cpp";
        nvc.tick.file = nvc.tick.name + ".cpp";
        nvc.applytick.file = nvc.applytick.name + ".cpp";

        // init cpp function files for init, tick, applytick
        auto helper_lines = codeGenerateHelperLines(nvc);
        auto createCppFile = [&](const string &prefix) -> bool {
            // Create cpp directory if not exists
            std::filesystem::path cppdir = std::filesystem::path(design.project_dir) / "cpp";
            std::error_code ec;
            std::filesystem::create_directories(cppdir, ec);
            if (ec) return false; // cannot create directory
            std::filesystem::path filepath = cppdir / (name + "_" + prefix + ".cpp");
            std::ofstream ofs(filepath, std::ios::binary | std::ios::trunc);
            if (!ofs) return false; // cannot open for write
            for (const string &line : *helper_lines) {
                ofs << line << "\n";
            }
            vector<VulArgument> _dummy;
            string funcsig = codeGenerateFunctionSignature(name + "_" + prefix, _dummy, _dummy);
            ofs << funcsig << " {\n";
            ofs << "    // TODO: implement\n";
            ofs << "}\n";
            ofs.close();
            return true;
        };
        if (!createCppFile("init") || !createCppFile("tick") || !createCppFile("applytick")) {
            // failed to create one of the cpp files; remove any created files
            auto removeCppFile = [&](const string &prefix) {
                std::filesystem::path filepath = std::filesystem::path(design.project_dir) / "cpp" / (name + "_" + prefix + ".cpp");
                std::error_code ec;
                std::filesystem::remove(filepath, ec);
            };
            removeCppFile("init");
            removeCppFile("tick");
            removeCppFile("applytick");
            design.combines.erase(name);
            return string("#22005: failed to create cpp function file(s) for combine '") + name + "'";
        }
    }
    design.dirty_combines = true;
    return "";
}

/**
 * @brief Replace all occurrences of oldstr with newstr in the specified file.
 * This function reads the entire file, performs the replacement, and writes back the modified content.
 * @param filepath The path to the file to modify.
 * @param oldstr The string to be replaced.
 * @param newstr The string to replace with.
 */
void _fullWordReplaceInFile(const std::filesystem::path &filepath, const string &oldstr, const string &newstr) {
    if (oldstr.empty()) return;

    // Read entire file
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs) return;
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();

    bool changed = false;
    std::string out;
    out.reserve(content.size());

    size_t pos = 0;
    const size_t oldlen = oldstr.size();
    while (pos < content.size()) {
        size_t found = content.find(oldstr, pos);
        if (found == std::string::npos) {
            // append remainder
            out.append(content.data() + pos, content.size() - pos);
            break;
        }

        // Check whole-word boundary: left and right must not be alnum or '_'
        bool left_ok = (found == 0) || !(std::isalnum((unsigned char)content[found-1]) || content[found-1] == '_');
        bool right_ok = (found + oldlen >= content.size()) || !(std::isalnum((unsigned char)content[found+oldlen]) || content[found+oldlen] == '_');

        if (left_ok && right_ok) {
            // append up to found, then newstr
            out.append(content.data() + pos, found - pos);
            out.append(newstr);
            changed = true;
            pos = found + oldlen;
        } else {
            // not a whole-word match; append up to found+1 and continue search after found+1
            out.append(content.data() + pos, (found - pos) + 1);
            pos = found + 1;
        }
    }

    if (!changed) return; // nothing to do

    // write to temp file then atomically replace
    std::filesystem::path tmp = filepath;
    tmp += ".tmp";
    std::error_code ec;
    // remove any existing tmp
    std::filesystem::remove(tmp, ec);

    std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
    if (!ofs) {
        // cannot open tmp for write; abort
        return;
    }
    ofs.write(out.data(), static_cast<std::streamsize>(out.size()));
    ofs.close();

    // replace original with tmp
    std::filesystem::rename(tmp, filepath, ec);
    if (ec) {
        // if rename failed, try to remove tmp
        std::filesystem::remove(tmp, ec);
    }
}

/**
 * @brief Rename a combine in the design.
 * All instances using this combine will also be updated.
 * If the new name is empty, it will be treated as a delete operation.
 * @param design The VulDesign object to modify.
 * @param oldname The old name of the combine.
 * @param newname The new name of the combine.
 * @return An empty string on success, or an error message on failure.
 */
string cmdRenameCombine(VulDesign &design, const string &oldname, const string &newname) {
    if (oldname.empty()) return "#22010: old combine name cannot be empty";

    auto it = design.combines.find(oldname);
    if (it == design.combines.end()) return string("#22011: combine '") + oldname + "' not found";

    // If newname is empty -> treat as delete operation
    if (newname.empty()) {
        // ensure no instance references this combine
        for (const auto &ik : design.instances) {
            const VulInstance &vi = ik.second;
            if (vi.combine == oldname) {
                return string("#22012: cannot delete combine '") + oldname + "' because instance '" + ik.first + "' references it";
            }
        }

        // delete all cppfunc files associated with this combine
        auto &vc = it->second;
        auto deleteFuncFile = [&](const VulCppFunc &f) {
            if (!f.name.empty()) {
                // delete file in design.project_dir / cpp using filesystem paths (portable)
                std::filesystem::path filepath = std::filesystem::path(design.project_dir) / "cpp" / (f.name + ".cpp");
                std::error_code ec;
                std::filesystem::remove(filepath, ec);
            }
        };
        for (const auto &sv : vc.service) {
            deleteFuncFile(sv.cppfunc);
        }
        deleteFuncFile(vc.tick);
        deleteFuncFile(vc.applytick);
        deleteFuncFile(vc.init);

        // safe to delete
        design.combines.erase(it);
        design.dirty_combines = true;
        return string();
    }

    // Rename operation
    if (!isValidIdentifier(newname)) return string("#22013: Invalid new combine name '") + newname + "'";

    if (newname == oldname) return string(); // nothing to do

    // check conflict with other names in design
    string conflict = isNameConflict(design, newname);
    if (!conflict.empty()) return string("#22014: cannot rename to '") + newname + "': " + conflict;

    // perform rename: insert new entry and remove old
    VulCombine vc = it->second;
    vc.name = newname;
    design.combines.erase(it);
    design.combines[newname] = vc;

    // update all instances that reference oldname
    for (auto &ik : design.instances) {
        VulInstance &vi = ik.second;
        if (vi.combine == oldname) {
            vi.combine = newname;
        }
    }

    // update all cppfunc file name and function name as newname_servname or newname_tick ...
    auto &nvc = design.combines[newname];
    auto updateFuncName = [&](VulCppFunc &f, const string &suffix) {
        if (!f.name.empty()) {
            string oldfuncname = f.name;
            string newfuncname = newname + suffix;
            if (oldfuncname != newfuncname) {
                // rename file in design.project_dir / cpp using filesystem paths (portable)
                std::filesystem::path oldpath = std::filesystem::path(design.project_dir) / "cpp" / (oldfuncname + ".cpp");
                std::filesystem::path newpath = std::filesystem::path(design.project_dir) / "cpp" / (newfuncname + ".cpp");
                std::error_code ec;
                std::filesystem::rename(oldpath, newpath, ec);
                if (!ec) {
                    // also replace all occurrences of oldfuncname with newfuncname in the file (whole word only)
                    _fullWordReplaceInFile(newpath, oldfuncname, newfuncname);
                }
            }
            f.name = newfuncname;
            f.file = newfuncname + ".cpp";
        }
    };
    for (auto &sv : nvc.service) {
        updateFuncName(sv.cppfunc, "_serv_" + sv.name);
    }
    updateFuncName(nvc.tick, "_tick");
    updateFuncName(nvc.applytick, "_applytick");
    updateFuncName(nvc.init, "_init");

    design.dirty_combines = true;
    return string();
}

/**
 * @brief Duplicate a combine in the design.
 * All instances using the old combine will not be affected.
 * @param design The VulDesign object to modify.
 * @param oldname The name of the existing combine to duplicate.
 * @param newname The name of the new combine to create.
 * @return An empty string on success, or an error message on failure.
 */
string cmdDuplicateCombine(VulDesign &design, const string &oldname, const string &newname) {
    if (oldname.empty()) return "#22020: old combine name cannot be empty";
    if (newname.empty()) return "#22021: new combine name cannot be empty";
    if (!isValidIdentifier(newname)) return string("#22022: Invalid new combine name '") + newname + "'";

    auto it = design.combines.find(oldname);
    if (it == design.combines.end()) return string("#22023: combine '") + oldname + "' not found";

    if (newname == oldname) return string("#22024: new combine name must be different from old name");

    // check conflict with other names in design
    string conflict = isNameConflict(design, newname);
    if (!conflict.empty()) return string("#22025: cannot duplicate to '") + newname + "': " + conflict;

    // perform duplication: insert new entry
    VulCombine vc = it->second;
    vc.name = newname;
    design.combines[newname] = vc;

    // copy all cppfunc file
    auto &nvc = design.combines[newname];
    auto copyFuncFile = [&](VulCppFunc &f, const string &suffix) {
        if (!f.name.empty()) {
            string oldfuncname = f.name;
            string newfuncname = newname + suffix;
            // copy file in design.project_dir / cpp using filesystem paths (portable)
            std::filesystem::path oldpath = std::filesystem::path(design.project_dir) / "cpp" / (oldfuncname + ".cpp");
            std::filesystem::path newpath = std::filesystem::path(design.project_dir) / "cpp" / (newfuncname + ".cpp");
            std::error_code ec;
            std::filesystem::copy_file(oldpath, newpath, std::filesystem::copy_options::overwrite_existing, ec);
            f.name = newfuncname;
            f.file = newfuncname + ".cpp";
            if (!ec) {
                // also replace all occurrences of oldfuncname with newfuncname in the file (whole word only)
                _fullWordReplaceInFile(newpath, oldfuncname, newfuncname);
            }
        }
    };
    for (auto &sv : nvc.service) {
        copyFuncFile(sv.cppfunc, "_serv_" + sv.name);
    }
    copyFuncFile(nvc.tick, "_tick");
    copyFuncFile(nvc.applytick, "_applytick");
    copyFuncFile(nvc.init, "_init");

    design.dirty_combines = true;
    return string();
}


/**
 * @brief Add, update, or remove a config item in a combine.
 * If the config item already exists, its ref and comment will be updated.
 * If configref is empty, the config item will be removed.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param configname The name of the config item to add, update, or remove.
 * @param configvalue The value string for the config item. It can reference config items by name.
 *                  If empty, the config item will be removed.
 * @param comment An optional comment for the config item.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupCombineConfig(VulDesign &design, const string &combinename, const string &configname, const string &configvalue, const string &comment) {
    if (combinename.empty()) return "#22030: combine name cannot be empty";
    auto it = design.combines.find(combinename);
    if (it == design.combines.end()) return string("#22031: combine '") + combinename + "' not found";

    if (configname.empty()) return "#22032: config item name cannot be empty";
    if (!isValidIdentifier(configname)) return string("#22032: invalid config item name '") + configname + "'";

    VulCombine &vc = it->second;

    // Remove config item if configvalue is empty
    if (configvalue.empty()) {
        auto cit = std::find_if(vc.config.begin(), vc.config.end(), [&](const VulConfig &c){ return c.name == configname; });
        if (cit == vc.config.end()) return string("#22034: config item '") + configname + "' not found in combine '" + combinename + "'";
        vc.config.erase(cit);
        design.dirty_combines = true;
        return string();
    }

    VulConfig checking;
    checking.name = configname;
    checking.value = configvalue;
    checking.comment = comment;
    extractConfigReferences(checking);
    for (const string &refname : checking.references) {
        // Validate that configref exists in design's config library
        if (design.config_lib.find(refname) == design.config_lib.end()) {
            return string("#22035: config item reference '") + refname + "' not found in design's config library";
        }
    }

    // Add or update config item
    for (VulConfig &c : vc.config) {
        if (c.name == configname) {
            c.value = configvalue;
            c.comment = comment;
            c.references = checking.references; // update references
            design.dirty_combines = true;
            return string();
        }
    }

    string conflict = isLocalNameConflict(vc, configname);
    if (!conflict.empty()) return string("#22033: cannot add config item '") + configname + "': " + conflict;

    // not found -> add new config item
    vc.config.push_back(checking);
    design.dirty_combines = true;
    return string();
}

/**
 * @brief Add, update, or remove a storage item in specific storage category in a combine.
 * If the storage item already exists, its type and comment will be updated.
 * If storagetype is empty, the storage item will be removed.
 * @param storages Reference to the vector of VulStorage items in the combine (storage, storagenext, or storagetick).
 * @param storagename The name of the storage item to add, update, or remove.
 * @param storagetype The type of the storage item. If empty, the storage item will be removed.
 * @param comment An optional comment for the storage item.
 * @return An empty string on success, or an error message on failure.
 */
string _cmdSetupStorageCommon(VulCombine &vc, vector<VulStorage> &storages, const string &storagename, const string &storagetype, const string &comment) {
    if (storagename.empty()) return "#22040: storage name cannot be empty";

    // Remove storage if storagetype is empty
    if (storagetype.empty()) {
        for (auto it = storages.begin(); it != storages.end(); ++it) {
            if (it->name == storagename) {
                storages.erase(it);
                return string();
            }
        }
        return string("#22041: storage '") + storagename + "' not found";
    }

    // Add or update storage
    for (VulStorage &s : storages) {
        if (s.name == storagename) {
            s.type = storagetype;
            s.comment = comment;
            return string();
        }
    }

    // not found -> add new storage item

    string conflict = isLocalNameConflict(vc, storagename);
    if (!conflict.empty()) return string("#22039: cannot add storage '") + storagename + "': " + conflict;

    VulStorage ns;
    ns.name = storagename;
    ns.type = storagetype;
    ns.comment = comment;
    storages.push_back(ns);
    return string();
}

/**
 * @brief Add, update, or remove a storage item in a combine.
 * If the storage item already exists, its type, value, and comment will be updated.
 * If storagetype is empty, the storage item will be removed.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param storagename The name of the storage item to add, update, or remove.
 * @param storagetype The type of the storage item. If empty, the storage item will be removed.
 * @param value An optional initial value for the storage item.
 * @param comment An optional comment for the storage item.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupCombineStorage(VulDesign &design, const string &combinename, const string &storagename, const string &storagetype, const string &value, const string &comment) {
    if (combinename.empty()) return "#22042: combine name cannot be empty";
    auto it = design.combines.find(combinename);
    if (it == design.combines.end()) return string("#22043: combine '") + combinename + "' not found";

    VulCombine &vc = it->second;

    string err = _cmdSetupStorageCommon(vc, vc.storage, storagename, storagetype, comment);
    if (!err.empty()) return err;

    // set initial value when adding/updating
    if (!storagetype.empty()) {
        for (VulStorage &s : vc.storage) {
            if (s.name == storagename) { s.value = value; break; }
        }
    }

    design.dirty_combines = true;
    return string();
}

/**
 * @brief Add, update, or remove a storage-next item in a combine.
 * If the storage-next item already exists, its type, value, and comment will be updated.
 * If storagetype is empty, the storage-next item will be removed.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param storagename The name of the storage-next item to add, update, or remove.
 * @param storagetype The type of the storage-next item. If empty, the storage-next item will be removed.
 * @param value An optional initial value for the storage-next item.
 * @param comment An optional comment for the storage-next item.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupCombineStorageNext(VulDesign &design, const string &combinename, const string &storagename, const string &storagetype, const string &value, const string &comment) {
    if (combinename.empty()) return "#22044: combine name cannot be empty";
    auto it = design.combines.find(combinename);
    if (it == design.combines.end()) return string("#22045: combine '") + combinename + "' not found";

    VulCombine &vc = it->second;

    string err = _cmdSetupStorageCommon(vc, vc.storagenext, storagename, storagetype, comment);
    if (!err.empty()) return err;

    if (!storagetype.empty()) {
        for (VulStorage &s : vc.storagenext) {
            if (s.name == storagename) { s.value = value; break; }
        }
    }

    design.dirty_combines = true;
    return string();
}

/**
 * @brief Add, update, or remove a storage-tick item in a combine.
 * If the storage-tick item already exists, its type, value, and comment will be updated.
 * If storagetype is empty, the storage-tick item will be removed.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param storagename The name of the storage-tick item to add, update, or remove.
 * @param storagetype The type of the storage-tick item. If empty, the storage-tick item will be removed.
 * @param value An optional initial value for the storage-tick item.
 * @param comment An optional comment for the storage-tick item.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupCombineStorageTick(VulDesign &design, const string &combinename, const string &storagename, const string &storagetype, const string &value, const string &comment) {
    if (combinename.empty()) return "#22046: combine name cannot be empty";
    auto it = design.combines.find(combinename);
    if (it == design.combines.end()) return string("#22047: combine '") + combinename + "' not found";

    VulCombine &vc = it->second;

    string err = _cmdSetupStorageCommon(vc, vc.storagetick, storagename, storagetype, comment);
    if (!err.empty()) return err;

    if (!storagetype.empty()) {
        for (VulStorage &s : vc.storagetick) {
            if (s.name == storagename) { s.value = value; break; }
        }
    }

    design.dirty_combines = true;
    return string();
}


/**
 * @brief Add, update, or remove a storage-array item in specific storage category in a combine.
 * If the storage item already exists, its type and comment will be updated.
 * If storagetype is empty, the storage item will be removed.
 * @param storages Reference to the vector of VulStorageArray items in the combine.
 * @param storagename The name of the storage item to add, update, or remove.
 * @param storagetype The type of the storage item. If empty, the storage item will be removed.
 * @param storatesize The size of the storage item. Cannot be empty if storagetype is not empty.
 * @param comment An optional comment for the storage item.
 * @return An empty string on success, or an error message on failure.
 */
string _cmdSetupStorageArrayCommon(VulCombine &vc, vector<VulStorageArray> &storages, const string &storagename, const string &storagetype, const string &storatesize, const string &comment) {
    if (storagename.empty()) return "#22040: storage name cannot be empty";

    // Remove storage if storagetype is empty
    if (storagetype.empty()) {
        for (auto it = storages.begin(); it != storages.end(); ++it) {
            if (it->name == storagename) {
                storages.erase(it);
                return string();
            }
        }
        return string("#22041: storage '") + storagename + "' not found";
    }

    if (storatesize.empty()) {
        return string("#22042: storage size cannot be empty for storage '") + storagename + "'";
    }

    // Add or update storage
    for (VulStorageArray &s : storages) {
        if (s.name == storagename) {
            s.type = storagetype;
            s.size = storatesize;
            s.comment = comment;
            return string();
        }
    }

    // not found -> add new storage item

    string conflict = isLocalNameConflict(vc, storagename);
    if (!conflict.empty()) return string("#22039: cannot add storage '") + storagename + "': " + conflict;

    VulStorageArray ns;
    ns.name = storagename;
    ns.type = storagetype;
    ns.size = storatesize;
    ns.comment = comment;
    storages.push_back(ns);
    return string();
}

/**
 * @brief Add, update, or remove a storage-next item in a combine.
 * If the storage-next item already exists, its type, value, and comment will be updated.
 * If storagetype is empty, the storage-next item will be removed.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param storagename The name of the storage-next item to add, update, or remove.
 * @param storagetype The type of the storage-next item. If empty, the storage-next item will be removed.
 * @param storagesize The size of the storage-next item. Cannot be empty if storagetype is not empty.
 * @param value An optional initial value for the storage-next item.
 * @param comment An optional comment for the storage-next item.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupCombineStorageNextArray(VulDesign &design, const string &combinename, const string &storagename, const string &storagetype, const string &storagesize, const string &value, const string &comment) {
    if (combinename.empty()) return "#22048: combine name cannot be empty";
    auto it = design.combines.find(combinename);
    if (it == design.combines.end()) return string("#22049: combine '") + combinename + "' not found";

    VulCombine &vc = it->second;

    string err = _cmdSetupStorageArrayCommon(vc, vc.storagenextarray, storagename, storagetype, storagesize, comment);
    if (!err.empty()) return err;

    if (!storagetype.empty()) {
        for (VulStorageArray &s : vc.storagenextarray) {
            if (s.name == storagename) {
                s.value = value;
                s.size = storagesize;
                break;
            }
        }
    }

    design.dirty_combines = true;
    return string();
}


/**
 * @brief Set up the tick function for a combine.
 * If all parameters are empty, the tick function will be disabled.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param code The raw C++ code for the tick function.
 * @param file The file name containing the tick function.
 * @param funcname The name of the tick function.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupCombineTickFunction(VulDesign &design, const string &combinename, const string &code, const string &file, const string &funcname) {
    if (combinename.empty()) return "#22050: combine name cannot be empty";
    auto it = design.combines.find(combinename);
    if (it == design.combines.end()) return string("#22051: combine '") + combinename + "' not found";

    VulCombine &vc = it->second;

    if (code.empty() && file.empty() && funcname.empty()) {
        vc.tick.code.clear(); vc.tick.file.clear(); vc.tick.name.clear();
    } else if (!code.empty()) {
        vc.tick.code = code; vc.tick.file.clear(); vc.tick.name.clear();
    } else if (!file.empty() && !funcname.empty()) {
        vc.tick.file = file; vc.tick.name = funcname; vc.tick.code.clear();
    } else {
        return string("#22052: invalid tick specification for combine '") + combinename + "' (provide code or file+funcname)";
    }

    design.dirty_combines = true;
    return string();
}

/**
 * @brief Set up the applytick function for a combine.
 * If all parameters are empty, the applytick function will be disabled.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param code The raw C++ code for the applytick function.
 * @param file The file name containing the applytick function.
 * @param funcname The name of the applytick function.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupCombineApplyTickFunction(VulDesign &design, const string &combinename, const string &code, const string &file, const string &funcname) {
    if (combinename.empty()) return "#22054: combine name cannot be empty";
    auto it = design.combines.find(combinename);
    if (it == design.combines.end()) return string("#22055: combine '") + combinename + "' not found";

    VulCombine &vc = it->second;

    if (code.empty() && file.empty() && funcname.empty()) {
        vc.applytick.code.clear(); vc.applytick.file.clear(); vc.applytick.name.clear();
    } else if (!code.empty()) {
        vc.applytick.code = code; vc.applytick.file.clear(); vc.applytick.name.clear();
    } else if (!file.empty() && !funcname.empty()) {
        vc.applytick.file = file; vc.applytick.name = funcname; vc.applytick.code.clear();
    } else {
        return string("#22056: invalid applytick specification for combine '") + combinename + "' (provide code or file+funcname)";
    }

    design.dirty_combines = true;
    return string();
}

/**
 * @brief Set up the init function for a combine.
 * If all parameters are empty, the init function will be disabled.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param code The raw C++ code for the init function.
 * @param file The file name containing the init function.
 * @param funcname The name of the init function.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupCombineInitFunction(VulDesign &design, const string &combinename, const string &code, const string &file, const string &funcname) {
    if (combinename.empty()) return "#22060: combine name cannot be empty";
    auto it = design.combines.find(combinename);
    if (it == design.combines.end()) return string("#22061: combine '") + combinename + "' not found";

    VulCombine &vc = it->second;

    // Validation: allowed forms:
    // 1) all empty -> disable
    // 2) code != empty -> use code (file/name cleared)
    // 3) file != empty && funcname != empty -> use file+funcname (code cleared)
    if (code.empty() && file.empty() && funcname.empty()) {
        vc.init.code.clear(); vc.init.file.clear(); vc.init.name.clear();
    } else if (!code.empty()) {
        // prefer raw code; ignore file/funcname
        vc.init.code = code;
        vc.init.file.clear(); vc.init.name.clear();
    } else if (!file.empty() && !funcname.empty()) {
        vc.init.file = file; vc.init.name = funcname; vc.init.code.clear();
    } else {
        return string("#22062: invalid init specification for combine '") + combinename + "' (provide code or file+funcname)";
    }

    design.dirty_combines = true;
    return string();
}

/**
 * @brief Add, update, or remove a pipe input port in a combine.
 * If the pipe input port already exists, its type and comment will be updated.
 * If pipetype is empty, the pipe input port will be removed.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param pipename The name of the pipe input port to add, update, or remove.
 * @param pipetype The type of the pipe input port. If empty, the pipe input port will be removed.
 * @param comment An optional comment for the pipe input port.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupCombinePipeIn(VulDesign &design, const string &combinename, const string &pipename, const string &pipetype, const string &comment) {
    if (combinename.empty()) return "#22070: combine name cannot be empty";
    auto it = design.combines.find(combinename);
    if (it == design.combines.end()) return string("#22071: combine '") + combinename + "' not found";

    if (pipename.empty()) return "#22072: pipe input port name cannot be empty";
    if (!isValidIdentifier(pipename)) return string("#22072: invalid pipe input port name '") + pipename + "'";

    VulCombine &vc = it->second;

    // Remove pipe input port if pipetype is empty
    if (pipetype.empty()) {
        return cmdRenameCombinePipeIn(design, combinename, pipename, ""); // reuse rename function to remove
    }

    // Validate pipe type using design's type checker
    if (!design.isValidTypeName(pipetype)) {
        return string("#22075: invalid pipe input port type '") + pipetype + "'";
    }

    // Add or update pipe input port
    for (VulPipePort &p : vc.pipein) {
        if (p.name == pipename) {
            p.type = pipetype;
            p.comment = comment;
            design.dirty_combines = true;
            return string();
        }
    }

    // not found -> add new pipe input port

    string conflict = isLocalNameConflict(vc, pipename);
    if (!conflict.empty()) return string("#22073: cannot add pipe input port '") + pipename + "': " + conflict;

    VulPipePort np;
    np.name = pipename;
    np.type = pipetype;
    np.comment = comment;
    vc.pipein.push_back(np);
    design.dirty_combines = true;
    return string();
}

/**
 * @brief Rename a pipe input port in a combine.
 * All connections using this pipe input port will also be updated.
 * If the new name is empty, it will be treated as a delete operation.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param oldpipename The old name of the pipe input port.
 * @param newpipename The new name of the pipe input port.
 * @return An empty string on success, or an error message on failure.
 */
string cmdRenameCombinePipeIn(VulDesign &design, const string &combinename, const string &oldpipename, const string &newpipename) {
    if (combinename.empty()) return "#22070: combine name cannot be empty";
    auto it = design.combines.find(combinename);
    if (it == design.combines.end()) return string("#22071: combine '") + combinename + "' not found";

    if (oldpipename.empty()) return "#22072: pipe input port name cannot be empty";

    VulCombine &vc = it->second;

    // find the pipein port
    auto pit = std::find_if(vc.pipein.begin(), vc.pipein.end(), [&](const VulPipePort &p){ return p.name == oldpipename; });
    if (pit == vc.pipein.end()) return string("#22074: pipe input port '") + oldpipename + "' not found in combine '" + combinename + "'";

    // DELETE operation
    if (newpipename.empty()) {
        // ensure no pipe->module connections reference this port on instances of this combine
        for (const VulPipeModuleConnection &pm : design.pipe_mod_connections) {
            // check instance exists
            auto ik = design.instances.find(pm.instance);
            if (ik == design.instances.end()) continue;
            if (ik->second.combine == combinename && pm.pipeinport == oldpipename) {
                return string("#22076: cannot delete pipe input port '") + oldpipename + "' because instance '" + pm.instance + "' has connections";
            }
        }

        // safe to remove
        vc.pipein.erase(pit);
        design.dirty_combines = true;
        return string();
    }

    // RENAME operation
    if (!isValidIdentifier(newpipename)) return string("#22077: invalid new pipe input port name '") + newpipename + "'";
    if (newpipename == oldpipename) return string();

    // ensure no name conflict in this combine
    auto conflict = isLocalNameConflict(vc, newpipename);
    if (!conflict.empty()) return string("#22078: cannot rename pipe input port to '") + newpipename + "': " + conflict;

    // update port name
    pit->name = newpipename;

    // update all pipe->module connections referring to this port
    for (VulPipeModuleConnection &pm : design.pipe_mod_connections) {
        auto ik = design.instances.find(pm.instance);
        if (ik == design.instances.end()) continue;
        if (ik->second.combine == combinename && pm.pipeinport == oldpipename) {
            pm.pipeinport = newpipename;
        }
    }

    design.dirty_combines = true;
    return string();
}

/**
 * @brief Add, update, or remove a pipe output port in a combine.
 * If the pipe output port already exists, its type and comment will be updated.
 * If pipetype is empty, the pipe output port will be removed.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param pipename The name of the pipe output port to add, update, or remove.
 * @param pipetype The type of the pipe output port. If empty, the pipe output port will be removed.
 * @param comment An optional comment for the pipe output port.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupCombinePipeOut(VulDesign &design, const string &combinename, const string &pipename, const string &pipetype, const string &comment) {
    if (combinename.empty()) return "#22080: combine name cannot be empty";
    auto it = design.combines.find(combinename);
    if (it == design.combines.end()) return string("#22081: combine '") + combinename + "' not found";

    if (pipename.empty()) return "#22082: pipe output port name cannot be empty";
    if (!isValidIdentifier(pipename)) return string("#22082: invalid pipe output port name '") + pipename + "'";

    VulCombine &vc = it->second;

    // Remove pipe output port if pipetype is empty
    if (pipetype.empty()) {
        return cmdRenameCombinePipeOut(design, combinename, pipename, ""); // reuse rename function to remove
    }

    // Validate pipe type using design's type checker
    if (!design.isValidTypeName(pipetype)) {
        return string("#22085: invalid pipe output port type '") + pipetype + "'";
    }

    // Add or update pipe output port
    for (VulPipePort &p : vc.pipeout) {
        if (p.name == pipename) {
            p.type = pipetype;
            p.comment = comment;
            design.dirty_combines = true;
            return string();
        }
    }

    // not found -> add new pipe output port

    string conflict = isLocalNameConflict(vc, pipename);
    if (!conflict.empty()) return string("#22083: cannot add pipe output port '") + pipename + "': " + conflict;

    VulPipePort np;
    np.name = pipename;
    np.type = pipetype;
    np.comment = comment;
    vc.pipeout.push_back(np);
    design.dirty_combines = true;
    return string();
}

/**
 * @brief Rename a pipe output port in a combine.
 * All connections using this pipe output port will also be updated.
 * If the new name is empty, it will be treated as a delete operation.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param oldpipename The old name of the pipe output port.
 * @param newpipename The new name of the pipe output port.
 * @return An empty string on success, or an error message on failure.
 */
string cmdRenameCombinePipeOut(VulDesign &design, const string &combinename, const string &oldpipename, const string &newpipename) {
    if (combinename.empty()) return "#22080: combine name cannot be empty";
    auto it = design.combines.find(combinename);
    if (it == design.combines.end()) return string("#22081: combine '") + combinename + "' not found";

    if (oldpipename.empty()) return "#22082: pipe output port name cannot be empty";

    VulCombine &vc = it->second;

    // find the pipeout port
    auto pit = std::find_if(vc.pipeout.begin(), vc.pipeout.end(), [&](const VulPipePort &p){ return p.name == oldpipename; });
    if (pit == vc.pipeout.end()) return string("#22084: pipe output port '") + oldpipename + "' not found in combine '" + combinename + "'";

    // DELETE operation
    if (newpipename.empty()) {
        // ensure no module->pipe connections reference this port on instances of this combine
        for (const VulModulePipeConnection &mp : design.mod_pipe_connections) {
            auto ik = design.instances.find(mp.instance);
            if (ik == design.instances.end()) continue;
            if (ik->second.combine == combinename && mp.pipeoutport == oldpipename) {
                return string("#22086: cannot delete pipe output port '") + oldpipename + "' because module->pipe connection exists for instance '" + mp.instance + "'";
            }
        }

        // safe to remove
        vc.pipeout.erase(pit);
        design.dirty_combines = true;
        return string();
    }

    // RENAME operation
    if (!isValidIdentifier(newpipename)) return string("#22087: invalid new pipe output port name '") + newpipename + "'";
    if (newpipename == oldpipename) return string();

    // ensure no conflict in this combine
    auto conflict = isLocalNameConflict(vc, newpipename);
    if (!conflict.empty()) return string("#22088: cannot rename pipe output port to '") + newpipename + "': " + conflict;
    
    // update port name
    pit->name = newpipename;

    // update all module->pipe connections referring to this port
    for (VulModulePipeConnection &mp : design.mod_pipe_connections) {
        auto ik = design.instances.find(mp.instance);
        if (ik == design.instances.end()) continue;
        if (ik->second.combine == combinename && mp.pipeoutport == oldpipename) {
            mp.pipeoutport = newpipename;
        }
    }

    design.dirty_combines = true;
    return string();
}


/**
 * @brief Add or update a request port in a combine.
 * If the request port already exists, its arguments, return values, and comment will be updated.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param reqname The name of the request port to add, update, or remove.
 * @param args The arguments for the request port.
 * @param rets The return values for the request port.
 * @param comment An optional comment for the request port.
 * @return An empty string on success, or an error message on failure.
 */
string cmdUpdateCombineRequest(VulDesign &design, const string &combinename, const string &reqname, const vector<VulArgument> &args, const vector<VulArgument> &rets, const string &comment) {
    if (combinename.empty()) return "#22100: combine name cannot be empty";
    auto it = design.combines.find(combinename);
    if (it == design.combines.end()) return string("#22101: combine '") + combinename + "' not found";

    if (reqname.empty()) return "#22102: request name cannot be empty";
    if (!isValidIdentifier(reqname)) return string("#22103: invalid request name '") + reqname + "'";

    VulCombine &vc = it->second;

    // validate each type
    for (const VulArgument &a : args) {
        if (!a.type.empty() && !design.isValidTypeName(a.type)) return string("#22106: invalid argument type '") + a.type + "' for request '" + reqname + "'";
    }
    for (const VulArgument &r : rets) {
        if (!r.type.empty() && !design.isValidTypeName(r.type)) return string("#22107: invalid return type '") + r.type + "' for request '" + reqname + "'";
    }

    // find existing request
    for (VulRequest &r : vc.request) {
        if (r.name == reqname) {
            // update
            r.arg = args;
            r.ret = rets;
            r.comment = comment;
            design.dirty_combines = true;
            return string();
        }
    }

    // not found -> add new request

    string conflict = isLocalNameConflict(vc, reqname);
    if (!conflict.empty()) return string("#22108: cannot add request '") + reqname + "': " + conflict;

    VulRequest nr;
    nr.name = reqname;
    nr.comment = comment;
    nr.arg = args;
    nr.ret = rets;
    vc.request.push_back(nr);
    design.dirty_combines = true;
    return string();
}

/**
 * @brief Rename a request port in a combine.
 * If the new name is empty, it will be treated as a delete operation.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param oldreqname The old name of the request port.
 * @param newreqname The new name of the request port.
 * @return An empty string on success, or an error message on failure.
 */
string cmdRenameCombineRequest(VulDesign &design, const string &combinename, const string &oldreqname, const string &newreqname) {
    if (combinename.empty()) return "#22110: combine name cannot be empty";
    auto it = design.combines.find(combinename);
    if (it == design.combines.end()) return string("#22111: combine '") + combinename + "' not found";

    if (oldreqname.empty()) return "#22112: request name cannot be empty";

    VulCombine &vc = it->second;

    // find the request
    auto rit = std::find_if(vc.request.begin(), vc.request.end(), [&](const VulRequest &r){ return r.name == oldreqname; });
    if (rit == vc.request.end()) return string("#22114: request '") + oldreqname + "' not found in combine '" + combinename + "'";

    // DELETE operation
    if (newreqname.empty()) {
        // ensure no req->serv connections reference this request on instances of this combine
        for (const VulReqConnection &rc : design.req_connections) {
            // find the instance referenced by the request connection
            auto ik = design.instances.find(rc.req_instance);
            if (ik == design.instances.end()) continue;
            if (ik->second.combine == combinename && rc.req_name == oldreqname) {
                return string("#22116: cannot delete request '") + oldreqname + "' because instance '" + rc.req_instance + "' has connections";
            }
        }

        // safe to remove
        vc.request.erase(rit);
        design.dirty_combines = true;
        return string();
    }

    // RENAME operation
    if (!isValidIdentifier(newreqname)) return string("#22117: invalid new request name '") + newreqname + "'";
    if (newreqname == oldreqname) return string();

    // ensure no local conflict
    auto conflict = isLocalNameConflict(vc, newreqname);
    if (!conflict.empty()) return string("#22118: cannot rename request to '") + newreqname + "': " + conflict;

    // update request name
    rit->name = newreqname;

    // update all req_connections referring to this request for instances of this combine
    for (VulReqConnection &rc : design.req_connections) {
        auto ik = design.instances.find(rc.req_instance);
        if (ik == design.instances.end()) continue;
        if (ik->second.combine == combinename && rc.req_name == oldreqname) {
            rc.req_name = newreqname;
        }
    }

    design.dirty_combines = true;
    return string();
}

/**
 * @brief Add or update a service port in a combine.
 * If the service port already exists, its arguments, return values, and comment will be updated.
 * For a new service port, its associated cpp file and function name will be created automatically.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param servname The name of the service port to add, update, or remove.
 * @param args The arguments for the service port.
 * @param rets The return values for the service port.
 * @param comment An optional comment for the service port.
 * @return An empty string on success, or an error message on failure.
 */
string cmdUpdateCombineService(VulDesign &design, const string &combinename, const string &servname, const vector<VulArgument> &args, const vector<VulArgument> &rets, const string &comment) {
    if (combinename.empty()) return "#22120: combine name cannot be empty";
    auto it = design.combines.find(combinename);
    if (it == design.combines.end()) return string("#22121: combine '") + combinename + "' not found";

    if (servname.empty()) return "#22122: service name cannot be empty";
    if (!isValidIdentifier(servname)) return string("#22123: invalid service name '") + servname + "'";

    VulCombine &vc = it->second;

    // validate each type
    for (const VulArgument &a : args) {
        if (!a.type.empty() && !design.isValidTypeName(a.type)) return string("#22126: invalid argument type '") + a.type + "' for service '" + servname + "'";
    }
    for (const VulArgument &a : rets) {
        if (!a.type.empty() && !design.isValidTypeName(a.type)) return string("#22127: invalid return type '") + a.type + "' for service '" + servname + "'";
    }

    // find existing service
    for (VulService &s : vc.service) {
        if (s.name == servname) {
            // update
            s.arg.clear(); s.ret.clear();
            s.arg = args;
            s.ret = rets;
            s.comment = comment;
            design.dirty_combines = true;
            return string();
        }
    }

    // not found -> add new service
    string conflict = isLocalNameConflict(vc, servname);
    if (!conflict.empty()) return string("#22128: cannot add service '") + servname + "': " + conflict;

    VulService ns;
    ns.name = servname;
    ns.comment = comment;
    ns.arg = args;
    ns.ret = rets;

    // initialize cpp function file for this service: combinename + _serv_ + servname
    string funcname = combinename + string("_serv_") + servname;
    std::filesystem::path filepath = std::filesystem::path(design.project_dir) / "cpp" / (funcname + ".cpp");
    std::error_code ec;
    // create directory if needed
    std::filesystem::create_directories(std::filesystem::path(design.project_dir) / "cpp", ec);
    if (!std::filesystem::exists(filepath, ec)) {
        auto helper_lines = codeGenerateHelperLines(vc);
        std::ofstream ofs(filepath, std::ios::binary | std::ios::trunc);
        if (ofs) {
            if (helper_lines) {
                for (const string &line : *helper_lines) ofs << line << "\n";
            }
            string sig = codeGenerateFunctionSignature(funcname, ns.arg, ns.ret);
            ofs << sig << " {\n";
            ofs << "    // TODO: implement service '" << servname << "'\n";
            ofs << "}\n";
            ofs.close();
        }
    }

    ns.cppfunc.name = funcname;
    ns.cppfunc.file = funcname + ".cpp";

    vc.service.push_back(ns);
    design.dirty_combines = true;
    return string();
}

/**
 * @brief Rename a service port in a combine.
 * If the new name is empty, it will be treated as a delete operation.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to modify.
 * @param oldservname The old name of the service port.
 * @param newservname The new name of the service port.
 * @return An empty string on success, or an error message on failure.
 */
string cmdRenameCombineService(VulDesign &design, const string &combinename, const string &oldservname, const string &newservname) {
    if (combinename.empty()) return "#22130: combine name cannot be empty";
    auto it = design.combines.find(combinename);
    if (it == design.combines.end()) return string("#22131: combine '") + combinename + "' not found";

    if (oldservname.empty()) return "#22132: service name cannot be empty";

    VulCombine &vc = it->second;

    // find the service
    auto sit = std::find_if(vc.service.begin(), vc.service.end(), [&](const VulService &s){ return s.name == oldservname; });
    if (sit == vc.service.end()) return string("#22133: service '") + oldservname + "' not found in combine '" + combinename + "'";

    // DELETE operation
    if (newservname.empty()) {
        // ensure no req->serv connections reference this service on instances of this combine
        for (const VulReqConnection &rc : design.req_connections) {
            auto ik = design.instances.find(rc.serv_instance);
            if (ik == design.instances.end()) continue;
            if (ik->second.combine == combinename && rc.serv_name == oldservname) {
                return string("#22136: cannot delete service '") + oldservname + "' because instance '" + rc.serv_instance + "' has connections";
            }
        }

        // remove cpp file if exists
        if (!sit->cppfunc.name.empty()) {
            std::filesystem::path filepath = std::filesystem::path(design.project_dir) / "cpp" / (sit->cppfunc.name + ".cpp");
            std::error_code ec;
            std::filesystem::remove(filepath, ec);
        }

        // safe to remove
        vc.service.erase(sit);
        design.dirty_combines = true;
        return string();
    }

    // RENAME operation
    if (!isValidIdentifier(newservname)) return string("#22137: invalid new service name '") + newservname + "'";
    if (newservname == oldservname) return string();

    // ensure no local conflict
    auto conflict = isLocalNameConflict(vc, newservname);
    if (!conflict.empty()) return string("#22138: cannot rename service to '") + newservname + "': " + conflict;

    // ensure no other service uses new name
    for (const VulService &s : vc.service) if (s.name == newservname) return string("#22139: service '") + newservname + "' already exists in combine '" + combinename + "'";

    // perform rename: update service name and cppfunc if present
    string oldfuncname;
    if (!sit->cppfunc.name.empty()) oldfuncname = sit->cppfunc.name;

    sit->name = newservname;

    if (!oldfuncname.empty()) {
        string newfuncname = combinename + string("_serv_") + newservname;
        if (oldfuncname != newfuncname) {
            std::filesystem::path oldpath = std::filesystem::path(design.project_dir) / "cpp" / (oldfuncname + ".cpp");
            std::filesystem::path newpath = std::filesystem::path(design.project_dir) / "cpp" / (newfuncname + ".cpp");
            std::error_code ec;
            std::filesystem::rename(oldpath, newpath, ec);
            if (!ec) {
                // replace occurrences of oldfuncname with newfuncname inside the moved file
                _fullWordReplaceInFile(newpath, oldfuncname, newfuncname);
            }
            sit->cppfunc.name = newfuncname;
            sit->cppfunc.file = newfuncname + ".cpp";
        }
    }

    // update all req_connections referring to this service for instances of this combine
    for (VulReqConnection &rc : design.req_connections) {
        auto ik = design.instances.find(rc.serv_instance);
        if (ik == design.instances.end()) continue;
        if (ik->second.combine == combinename && rc.serv_name == oldservname) {
            rc.serv_name = newservname;
        }
    }

    design.dirty_combines = true;
    return string();
}


/**
 * @brief Add or update an instance in the design.
 * If the instance already exists, its combine and comment will be updated.
 * @param design The VulDesign object to modify.
 * @param name The name of the instance.
 * @param combine The name of the combine this instance refers to.
 * @param comment An optional comment for the instance.
 * @return An empty string on success, or an error message on failure.
 */
string cmdUpdateInstance(VulDesign &design, const string &name, const string &combine, const string &comment) {
    if (name.empty()) return "#23001: instance name cannot be empty";
    if (!isValidIdentifier(name)) return string("#23002: invalid instance name '") + name + "'";

    if (combine.empty()) return "#23003: combine name cannot be empty";
    // ensure combine exists
    auto cit = design.combines.find(combine);
    if (cit == design.combines.end()) return string("#23004: combine '") + combine + "' not found";

    auto it = design.instances.find(name);
    if (it != design.instances.end()) {
        // exists -> may update combine or comment
        VulInstance &vi = it->second;
        // if combine unchanged, just update comment
        if (vi.combine == combine) {
            vi.comment = comment;
            design.dirty_combines = true; // instances affect combines view
            return string();
        }

        // changing combine: must ensure no connections of any form reference this instance
        // check req connections (either as req_instance or serv_instance)
        for (const VulReqConnection &rc : design.req_connections) {
            if (rc.req_instance == name || rc.serv_instance == name) return string("#23005: cannot rebind instance '") + name + "' because it has request/service connections";
        }
        // check module->pipe connections (as instance)
        for (const VulModulePipeConnection &mp : design.mod_pipe_connections) {
            if (mp.instance == name) return string("#23006: cannot rebind instance '") + name + "' because it has module->pipe connections";
        }
        // check pipe->module connections (as instance)
        for (const VulPipeModuleConnection &pm : design.pipe_mod_connections) {
            if (pm.instance == name) return string("#23007: cannot rebind instance '") + name + "' because it has pipe->module connections";
        }
        // check stalled connections (as src or dest)
        for (const VulStalledConnection &sc : design.stalled_connections) {
            if (sc.src_instance == name || sc.dest_instance == name) return string("#23008: cannot rebind instance '") + name + "' because it participates in stalled connections";
        }
        // check update sequences (as former or latter)
        for (const VulUpdateSequence &us : design.update_constraints) {
            if (us.former_instance == name || us.latter_instance == name) return string("#23009: cannot rebind instance '") + name + "' because it participates in update sequences";
        }

        // safe to change combine
        vi.combine = combine;
        vi.comment = comment;
        design.dirty_combines = true;
        return string();
    }

    // not found -> add new instance

    // check name conflict
    string conflict = isNameConflict(design, name);
    if (!conflict.empty()) return string("#23002: cannot add instance '") + name + "': " + conflict;

    VulInstance ni;
    ni.name = name;
    ni.combine = combine;
    ni.comment = comment;
    design.instances[name] = ni;
    design.dirty_combines = true;
    return string();
}

/**
 * @brief Rename an instance in the design.
 * All connections involving this instance will also be updated.
 * If the new name is empty, it will be treated as a delete operation.
 * @param design The VulDesign object to modify.
 * @param oldname The old name of the instance.
 * @param newname The new name of the instance.
 * @return An empty string on success, or an error message on failure.
 */
string cmdRenameInstance(VulDesign &design, const string &oldname, const string &newname) {
    if (oldname.empty()) return "#23010: old instance name cannot be empty";

    auto it = design.instances.find(oldname);
    if (it == design.instances.end()) return string("#23011: instance '") + oldname + "' not found";

    // DELETE operation
    if (newname.empty()) {
        // ensure no connections reference this instance
        for (const VulReqConnection &rc : design.req_connections) {
            if (rc.req_instance == oldname || rc.serv_instance == oldname) return string("#23012: cannot delete instance '") + oldname + "' because request/service connections exist";
        }
        for (const VulModulePipeConnection &mp : design.mod_pipe_connections) {
            if (mp.instance == oldname) return string("#23013: cannot delete instance '") + oldname + "' because module->pipe connections exist";
        }
        for (const VulPipeModuleConnection &pm : design.pipe_mod_connections) {
            if (pm.instance == oldname) return string("#23014: cannot delete instance '") + oldname + "' because pipe->module connections exist";
        }
        for (const VulStalledConnection &sc : design.stalled_connections) {
            if (sc.src_instance == oldname || sc.dest_instance == oldname) return string("#23015: cannot delete instance '") + oldname + "' because stalled connections exist";
        }
        for (const VulUpdateSequence &us : design.update_constraints) {
            if (us.former_instance == oldname || us.latter_instance == oldname) return string("#23016: cannot delete instance '") + oldname + "' because update sequence connections exist";
        }

        // safe to delete
        design.instances.erase(it);
        design.dirty_combines = true;
        return string();
    }

    // RENAME operation
    if (!isValidIdentifier(newname)) return string("#23017: invalid new instance name '") + newname + "'";
    if (newname == oldname) return string();

    // check conflict with other names in design
    string conflict = isNameConflict(design, newname);
    if (!conflict.empty()) return string("#23018: cannot rename to '") + newname + "': " + conflict;

    // move instance entry (update its internal name too)
    VulInstance vi = it->second;
    vi.name = newname;
    design.instances.erase(it);
    design.instances[newname] = vi;

    // update all connections that reference this instance
    for (VulReqConnection &rc : design.req_connections) {
        if (rc.req_instance == oldname) rc.req_instance = newname;
        if (rc.serv_instance == oldname) rc.serv_instance = newname;
    }
    for (VulModulePipeConnection &mp : design.mod_pipe_connections) {
        if (mp.instance == oldname) mp.instance = newname;
    }
    for (VulPipeModuleConnection &pm : design.pipe_mod_connections) {
        if (pm.instance == oldname) pm.instance = newname;
    }
    for (VulStalledConnection &sc : design.stalled_connections) {
        if (sc.src_instance == oldname) sc.src_instance = newname;
        if (sc.dest_instance == oldname) sc.dest_instance = newname;
    }
    for (VulUpdateSequence &us : design.update_constraints) {
        if (us.former_instance == oldname) us.former_instance = newname;
        if (us.latter_instance == oldname) us.latter_instance = newname;
    }

    design.dirty_combines = true;
    return string();
}

/**
 * @brief Set up a config override for an instance.
 * If the config override already exists, its value will be updated.
 * If configvalue is empty, the config override will be removed.
 * @param design The VulDesign object to modify.
 * @param instancename The name of the instance to modify.
 * @param configname The name of the config item to add, update, or remove.
 * @param configvalue The override value for the config item. If empty, the config override will be removed.
 * @return An empty string on success, or an error message on failure.
 */
string cmdSetupInstanceConfigOverride(VulDesign &design, const string &instancename, const string &configname, const string &configvalue) {
    if (instancename.empty()) return "#23021: instance name cannot be empty";
    auto it = design.instances.find(instancename);
    if (it == design.instances.end()) return string("#23022: instance '") + instancename + "' not found";

    if (configname.empty()) return "#23023: config name cannot be empty";
    if (!isValidIdentifier(configname)) return string("#23024: invalid config name '") + configname + "'";

    VulInstance &vi = it->second;

    // If configvalue empty => remove override
    if (configvalue.empty()) {
        for (auto cur = vi.config_override.begin(); cur != vi.config_override.end(); ++cur) {
            if (cur->first == configname) {
                vi.config_override.erase(cur);
                design.dirty_combines = true;
                return string();
            }
        }
        return string("#23025: config override '") + configname + "' not found for instance '" + instancename + "'";
    }

    // Adding/updating: configname must exist in design.config_lib
    if (design.config_lib.find(configname) == design.config_lib.end()) return string("#23026: config item '") + configname + "' not found in design config library";

    for (auto &p : vi.config_override) {
        if (p.first == configname) {
            p.second = configvalue;
            design.dirty_combines = true;
            return string();
        }
    }

    // not found -> add
    vi.config_override.emplace_back(configname, configvalue);
    design.dirty_combines = true;
    return string();
}

/**
 * @brief Add, update or remove a pipe in the design.
 * If the pipe already exists, its type, comment, input size, output size, and buffer size will be updated.
 * If pipetype is empty, the pipe will be removed.
 * @param design The VulDesign object to modify.
 * @param name The name of the pipe.
 * @param type The data type of the pipe.
 * @param comment An optional comment for the pipe.
 * @param inputsize The input size of the pipe (0 for unlimited).
 * @param outputsize The output size of the pipe (0 for unlimited).
 * @param buffersize The buffer size of the pipe (0 for default).
 * @return An empty string on success, or an error message on failure.
 */
string cmdUpdatePipe(VulDesign &design, const string &name, const string &type, const string &comment, unsigned int inputsize, unsigned int outputsize, unsigned int buffersize) {
    if (name.empty()) return "#24001: pipe name cannot be empty";
    if (!isValidIdentifier(name)) return string("#24002: invalid pipe name '") + name + "'";

    auto it = design.pipes.find(name);
    // DELETE operation when type is empty -> reuse cmdRenamePipe
    if (type.empty()) {
        return cmdRenamePipe(design, name, "");
    }

    // validate type
    if (!design.isValidTypeName(type)) return string("#24003: invalid pipe type '") + type + "'";

    // validate input/output sizes as non-zero values
    if (inputsize == 0) return string("#24004: invalid input size for pipe '") + name + "'";
    if (outputsize == 0) return string("#24004: invalid output size for pipe '") + name + "'";

    if (it != design.pipes.end()) {
        // existing pipe -> may update
        VulPipe &vp = it->second;

        // if type changes, ensure no connections reference this pipe (changing type could break them)
        if (vp.type != type) {
            for (const VulModulePipeConnection &mp : design.mod_pipe_connections) {
                if (mp.pipe == name) return string("#24005: cannot change pipe type for '") + name + "' because module->pipe connections exist";
            }
            for (const VulPipeModuleConnection &pm : design.pipe_mod_connections) {
                if (pm.pipe == name) return string("#24005: cannot change pipe type for '") + name + "' because pipe->module connections exist";
            }
        }

        // if inputsize changes, ensure no connection index overflows
        if (inputsize != vp.inputsize) {
            for (const VulModulePipeConnection &mp : design.mod_pipe_connections) {
                if (mp.pipe == name) {
                    if (mp.portindex >= inputsize) return string("#24006: cannot set inputsize to ") + std::to_string(inputsize) + " for pipe '" + name + "' because connection port index out of range";
                }
            }
        }

        // if outputsize changes, ensure no connection index overflows
        if (outputsize != vp.outputsize) {
            for (const VulPipeModuleConnection &pm : design.pipe_mod_connections) {
                if (pm.pipe == name) {
                    if (pm.portindex >= outputsize) return string("#24007: cannot set outputsize to ") + std::to_string(outputsize) + " for pipe '" + name + "' because connection port index out of range";
                }
            }
        }

        // apply updates
        vp.type = type;
        vp.comment = comment;
        vp.inputsize = inputsize;
        vp.outputsize = outputsize;
        vp.buffersize = buffersize;
        design.dirty_combines = true; // pipe changes affect overall design checks
        return string();
    }

    // not found -> add new pipe
    string conflict = isNameConflict(design, name);
    if (!conflict.empty()) return string("#24008: cannot add pipe '") + name + "': " + conflict;

    VulPipe np;
    np.name = name;
    np.type = type;
    np.comment = comment;
    np.inputsize = inputsize;
    np.outputsize = outputsize;
    np.buffersize = buffersize;
    design.pipes[name] = np;
    design.dirty_combines = true; // no separate dirty_pipes flag
    return string();
}

/**
 * @brief Rename a pipe in the design.
 * All connections using this pipe will also be updated.
 * If the new name is empty, it will be treated as a delete operation.
 * @param design The VulDesign object to modify.
 * @param oldname The old name of the pipe.
 * @param newname The new name of the pipe.
 * @return An empty string on success, or an error message on failure.
 */
string cmdRenamePipe(VulDesign &design, const string &oldname, const string &newname) {
    if (oldname.empty()) return "#24010: old pipe name cannot be empty";

    auto it = design.pipes.find(oldname);
    if (it == design.pipes.end()) return string("#24011: pipe '") + oldname + "' not found";

    // DELETE operation
    if (newname.empty()) {
        // ensure no module->pipe or pipe->module connections reference this pipe
        for (const VulModulePipeConnection &mp : design.mod_pipe_connections) {
            if (mp.pipe == oldname) return string("#24012: cannot delete pipe '") + oldname + "' because module->pipe connections exist";
        }
        for (const VulPipeModuleConnection &pm : design.pipe_mod_connections) {
            if (pm.pipe == oldname) return string("#24013: cannot delete pipe '") + oldname + "' because pipe->module connections exist";
        }

        // safe to remove
        design.pipes.erase(it);
        design.dirty_combines = true;
        return string();
    }

    // RENAME operation
    if (!isValidIdentifier(newname)) return string("#24017: invalid new pipe name '") + newname + "'";
    if (newname == oldname) return string();

    // check conflict with other names in design
    string conflict = isNameConflict(design, newname);
    if (!conflict.empty()) return string("#24018: cannot rename to '") + newname + "': " + conflict;

    // move pipe entry (update internal name)
    VulPipe vp = it->second;
    vp.name = newname;
    design.pipes.erase(it);
    design.pipes[newname] = vp;

    // update all connections that reference this pipe
    for (VulModulePipeConnection &mp : design.mod_pipe_connections) {
        if (mp.pipe == oldname) mp.pipe = newname;
    }
    for (VulPipeModuleConnection &pm : design.pipe_mod_connections) {
        if (pm.pipe == oldname) pm.pipe = newname;
    }

    design.dirty_combines = true;
    return string();
}

/**
 * @brief Connect a module's output port to a pipe's input port.
 * @param design The VulDesign object to modify.
 * @param instancename The name of the instance (module) to connect from.
 * @param pipeoutport The name of the module's output port to connect from.
 * @param pipename The name of the pipe to connect to.
 * @param portindex The index of the pipe's input port to connect to (0-based).
 * @return An empty string on success, or an error message on failure.
 */
string cmdConnectModuleToPipe(VulDesign &design, const string &instancename, const string &pipeoutport, const string &pipename, unsigned int portindex) {
    if (instancename.empty()) return "#25001: instance name cannot be empty";
    if (pipeoutport.empty()) return "#25002: pipe output port name cannot be empty";
    if (pipename.empty()) return "#25003: pipe name cannot be empty";

    auto ik = design.instances.find(instancename);
    if (ik == design.instances.end()) return string("#25004: instance '") + instancename + "' not found";

    auto pk = design.pipes.find(pipename);
    if (pk == design.pipes.end()) return string("#25005: pipe '") + pipename + "' not found";

    // find the combine of the instance and the pipeout port definition
    const string &combname = ik->second.combine;
    auto cit = design.combines.find(combname);
    if (cit == design.combines.end()) return string("#25006: combine '") + combname + "' not found for instance '" + instancename + "'";

    const VulCombine &vc = cit->second;
    const VulPipe &vp = pk->second;

    // portindex must be within pipe inputsize if inputsize != 0
    if (vp.inputsize != 0 && portindex >= vp.inputsize) return string("#25007: port index out of range for pipe '") + pipename + "'";

    // ensure the instance's combine exposes the pipeout port
    const VulPipePort *outport = nullptr;
    for (const VulPipePort &p : vc.pipeout) {
        if (p.name == pipeoutport) { outport = &p; break; }
    }
    if (!outport) return string("#25008: pipe output port '") + pipeoutport + "' not found in combine '" + combname + "'";

    // type must match exactly between combine pipeout port and pipe.type
    if (outport->type != vp.type) return string("#25009: type mismatch between module output port '") + pipeoutport + "' and pipe '" + pipename + "'";

    // uniqueness: one module output port can connect to only one pipe input; also a pipe input index can be occupied by only one module output
    for (const VulModulePipeConnection &mp : design.mod_pipe_connections) {
        if (mp.instance == instancename && mp.pipeoutport == pipeoutport) {
            return string("#25010: module output port '") + pipeoutport + "' of instance '" + instancename + "' is already connected";
        }
        if (mp.pipe == pipename && mp.portindex == portindex) {
            return string("#25011: pipe '") + pipename + "' input port index " + std::to_string(portindex) + " is already connected";
        }
    }

    // all checks passed -> add connection
    VulModulePipeConnection nc;
    nc.instance = instancename;
    nc.pipeoutport = pipeoutport;
    nc.pipe = pipename;
    nc.portindex = portindex;
    design.mod_pipe_connections.push_back(nc);
    design.dirty_combines = true;
    return string();
}

/**
 * @brief Disconnect a module's output port from a pipe's input port.
 * @param design The VulDesign object to modify.
 * @param instancename The name of the instance (module) to disconnect from.
 * @param pipeoutport The name of the module's output port to disconnect from.
 * @param pipename The name of the pipe to disconnect from.
 * @param portindex The index of the pipe's input port to disconnect from (0-based).
 * @return An empty string on success, or an error message on failure.
 */
string cmdDisconnectModuleFromPipe(VulDesign &design, const string &instancename, const string &pipeoutport, const string &pipename, unsigned int portindex) {
    
    // Determine which key(s) the caller provided.
    bool hasModuleKey = !instancename.empty() && !pipeoutport.empty();
    bool hasPipeKey = !pipename.empty();

    // Must provide at least one valid key
    if (!hasModuleKey && !hasPipeKey) return string("#25020: must specify either instance+pipeoutport or pipename+portindex to disconnect");

    // If one side of module key is provided but not the other, complain
    if ((!instancename.empty() && pipeoutport.empty()) || (instancename.empty() && !pipeoutport.empty())) {
        return string("#25021: both instancename and pipeoutport must be provided together");
    }

    // Search for a matching connection. If both keys provided, require both to match the same entry.
    for (auto it = design.mod_pipe_connections.begin(); it != design.mod_pipe_connections.end(); ++it) {
        const VulModulePipeConnection &mp = *it;

        bool matchModule = hasModuleKey && (mp.instance == instancename && mp.pipeoutport == pipeoutport);
        bool matchPipe = hasPipeKey && (mp.pipe == pipename && mp.portindex == portindex);

        bool matched = false;
        if (hasModuleKey && hasPipeKey) {
            // both provided -> both must match
            matched = matchModule && matchPipe;
        } else if (hasModuleKey) {
            matched = matchModule;
        } else { // hasPipeKey only
            matched = matchPipe;
        }

        if (matched) {
            design.mod_pipe_connections.erase(it);
            design.dirty_combines = true;
            return string();
        }
    }

    // No matching connection found
    return string("#25024: specified module->pipe connection not found");
}

/**
 * @brief Connect a pipe's output port to a module's input port.
 * @param design The VulDesign object to modify.
 * @param instancename The name of the instance (module) to connect to.
 * @param pipeinport The name of the module's input port to connect to.
 * @param pipename The name of the pipe to connect from.
 * @param portindex The index of the pipe's output port to connect from (0-based).
 * @return An empty string on success, or an error message on failure.
 */
string cmdConnectPipeToModule(VulDesign &design, const string &instancename, const string &pipeinport, const string &pipename, unsigned int portindex) {
    if (instancename.empty()) return "#25030: instance name cannot be empty";
    if (pipeinport.empty()) return "#25031: pipe input port name cannot be empty";
    if (pipename.empty()) return "#25032: pipe name cannot be empty";

    auto ik = design.instances.find(instancename);
    if (ik == design.instances.end()) return string("#25033: instance '") + instancename + "' not found";

    auto pk = design.pipes.find(pipename);
    if (pk == design.pipes.end()) return string("#25034: pipe '") + pipename + "' not found";

    // find the combine of the instance and the pipein port definition
    const string &combname = ik->second.combine;
    auto cit = design.combines.find(combname);
    if (cit == design.combines.end()) return string("#25035: combine '") + combname + "' not found for instance '" + instancename + "'";

    const VulCombine &vc = cit->second;
    const VulPipe &vp = pk->second;

    // portindex must be within pipe outputsize if outputsize != 0
    if (vp.outputsize != 0 && portindex >= vp.outputsize) return string("#25036: port index out of range for pipe '") + pipename + "'";

    // ensure the instance's combine exposes the pipein port
    const VulPipePort *inport = nullptr;
    for (const VulPipePort &p : vc.pipein) {
        if (p.name == pipeinport) { inport = &p; break; }
    }
    if (!inport) return string("#25037: pipe input port '") + pipeinport + "' not found in combine '" + combname + "'";

    // type must match exactly between combine pipein port and pipe.type
    if (inport->type != vp.type) return string("#25038: type mismatch between module input port '") + pipeinport + "' and pipe '" + pipename + "'";

    // uniqueness: one module input port can connect to only one pipe output; also a pipe output index can be occupied by only one module input
    for (const VulPipeModuleConnection &pm : design.pipe_mod_connections) {
        if (pm.instance == instancename && pm.pipeinport == pipeinport) {
            return string("#25039: module input port '") + pipeinport + "' of instance '" + instancename + "' is already connected";
        }
        if (pm.pipe == pipename && pm.portindex == portindex) {
            return string("#25040: pipe '") + pipename + "' output port index " + std::to_string(portindex) + " is already connected";
        }
    }

    // all checks passed -> add connection
    VulPipeModuleConnection nc;
    nc.instance = instancename;
    nc.pipeinport = pipeinport;
    nc.pipe = pipename;
    nc.portindex = portindex;
    design.pipe_mod_connections.push_back(nc);
    design.dirty_combines = true;
    return string();
}

/**
 * @brief Disconnect a pipe's output port from a module's input port.
 * @param design The VulDesign object to modify.
 * @param instancename The name of the instance (module) to disconnect from.
 * @param pipeinport The name of the module's input port to disconnect from.
 * @param pipename The name of the pipe to disconnect from.
 * @param portindex The index of the pipe's output port to disconnect from (0-based).
 * @return An empty string on success, or an error message on failure.
 */
string cmdDisconnectPipeFromModule(VulDesign &design, const string &instancename, const string &pipeinport, const string &pipename, unsigned int portindex) {
    // Determine which key(s) the caller provided.
    bool hasModuleKey = !instancename.empty() && !pipeinport.empty();
    bool hasPipeKey = !pipename.empty();

    // Must provide at least one valid key
    if (!hasModuleKey && !hasPipeKey) return string("#25050: must specify either instance+pipeinport or pipename+portindex to disconnect");

    // If one side of module key is provided but not the other, complain
    if ((!instancename.empty() && pipeinport.empty()) || (instancename.empty() && !pipeinport.empty())) {
        return string("#25051: both instancename and pipeinport must be provided together");
    }

    // Search for a matching connection. If both keys provided, require both to match the same entry.
    for (auto it = design.pipe_mod_connections.begin(); it != design.pipe_mod_connections.end(); ++it) {
        const VulPipeModuleConnection &pm = *it;

        bool matchModule = hasModuleKey && (pm.instance == instancename && pm.pipeinport == pipeinport);
        bool matchPipe = hasPipeKey && (pm.pipe == pipename && pm.portindex == portindex);

        bool matched = false;
        if (hasModuleKey && hasPipeKey) {
            // both provided -> both must match
            matched = matchModule && matchPipe;
        } else if (hasModuleKey) {
            matched = matchModule;
        } else { // hasPipeKey only
            matched = matchPipe;
        }

        if (matched) {
            design.pipe_mod_connections.erase(it);
            design.dirty_combines = true;
            return string();
        }
    }

    // No matching connection found
    return string("#25054: specified pipe->module connection not found");
}


/**
 * @brief Connect a request port of one module to a service port of another module.
 * @param design The VulDesign object to modify.
 * @param reqinstname The name of the instance (module) with the request port.
 * @param reqportname The name of the request port to connect from.
 * @param servinstname The name of the instance (module) with the service port.
 * @param servportname The name of the service port to connect to.
 * @return An empty string on success, or an error message on failure.
 */
string cmdConnectReqToServ(VulDesign &design, const string &reqinstname, const string &reqportname, const string &servinstname, const string &servportname) {
    if (reqinstname.empty()) return "#25060: request instance name cannot be empty";
    if (reqportname.empty()) return "#25061: request port name cannot be empty";
    if (servinstname.empty()) return "#25062: service instance name cannot be empty";
    if (servportname.empty()) return "#25063: service port name cannot be empty";

    // cannot connect with itself
    if (reqinstname == servinstname) return string("#25076: cannot connect request and service within the same instance '") + reqinstname + "'";

    auto rik = design.instances.find(reqinstname);
    if (rik == design.instances.end()) return string("#25064: instance '") + reqinstname + "' not found";

    const string &reqcomb = rik->second.combine;
    auto rcit = design.combines.find(reqcomb);
    if (rcit == design.combines.end()) return string("#25066: combine '") + reqcomb + "' not found for instance '" + reqinstname + "'";

    const VulCombine &rvc = rcit->second;

    // find request definition
    const VulRequest *reqdef = nullptr;
    for (const VulRequest &r : rvc.request) {
        if (r.name == reqportname) { reqdef = &r; break; }
    }
    if (!reqdef) return string("#25068: request '") + reqportname + "' not found in combine '" + reqcomb + "'";

    auto sik = design.instances.find(servinstname);
    if (sik == design.instances.end()) {
        // pipe has a clear service without arg and return, allow this special case
        if (design.pipes.find(servinstname) != design.pipes.end() && servportname == "clear") {
            if (reqdef->ret.empty()) {
                // add connection
                VulReqConnection nc;
                nc.req_instance = reqinstname;
                nc.req_name = reqportname;
                nc.serv_instance = servinstname;
                nc.serv_name = servportname;
                design.req_connections.push_back(nc);
                design.dirty_combines = true;
                return string();
            } else {
                return string("#25077: request '") + reqportname + "' of instance '" + reqinstname + "' has return values and cannot connect to pipe 'clear' service";
            }
        } else {
            return string("#25065: instance '") + servinstname + "' not found";
        }
    }

    const string &servcomb = sik->second.combine;
    auto scop = design.combines.find(servcomb);
    if (scop == design.combines.end()) return string("#25067: combine '") + servcomb + "' not found for instance '" + servinstname + "'";

    const VulCombine &svc = scop->second;

    // find service definition
    const VulService *servdef = nullptr;
    for (const VulService &s : svc.service) {
        if (s.name == servportname) { servdef = &s; break; }
    }
    if (!servdef) return string("#25069: service '") + servportname + "' not found in combine '" + servcomb + "'";

    // check argument types and return types: sequences must match exactly (names ignored)
    // Except for: Service without arg and ret can be connected to Request with arg without ret.
    if (servdef->arg.empty() && servdef->ret.empty()) {
        if (!reqdef->ret.empty()) {
            return string("#25078: request '") + reqportname + "' of instance '" + reqinstname + "' has return values and cannot connect to service '" + servportname + "' without return";
        }
    } else {
        if (reqdef->arg.size() != servdef->arg.size()) return string("#25072: argument count mismatch between request '") + reqportname + "' and service '" + servportname + "'";
        for (size_t i = 0; i < reqdef->arg.size(); ++i) {
            if (reqdef->arg[i].type != servdef->arg[i].type) {
                return string("#25073: argument type mismatch at index ") + std::to_string(i) + " between request '" + reqportname + "' and service '" + servportname + "'";
            }
        }

        if (reqdef->ret.size() != servdef->ret.size()) return string("#25074: return count mismatch between request '") + reqportname + "' and service '" + servportname + "'";
        for (size_t i = 0; i < reqdef->ret.size(); ++i) {
            if (reqdef->ret[i].type != servdef->ret[i].type) {
                return string("#25075: return type mismatch at index ") + std::to_string(i) + " between request '" + reqportname + "' and service '" + servportname + "'";
            }
        }
    }

    // multiplicity rules
    bool reqHasReturn = !reqdef->ret.empty();

    if (reqHasReturn) {
        // a request with return can be connected to only one service
        for (const VulReqConnection &rc : design.req_connections) {
            if (rc.req_instance == reqinstname && rc.req_name == reqportname) {
                return string("#25070: request '") + reqportname + "' of instance '" + reqinstname + "' already has a connection";
            }
        }
    } else {
        // request without return may connect to many services, but identical connection must not exist
        for (const VulReqConnection &rc : design.req_connections) {
            if (rc.req_instance == reqinstname && rc.req_name == reqportname && rc.serv_instance == servinstname && rc.serv_name == servportname) {
                return string("#25071: identical request->service connection already exists");
            }
        }
    }

    // add connection
    VulReqConnection nc;
    nc.req_instance = reqinstname;
    nc.req_name = reqportname;
    nc.serv_instance = servinstname;
    nc.serv_name = servportname;
    design.req_connections.push_back(nc);
    design.dirty_combines = true;
    return string();
}

/**
 * @brief Disconnect a request port of one module from a service port of another module.
 * @param design The VulDesign object to modify.
 * @param reqinstname The name of the instance (module) with the request port.
 * @param reqportname The name of the request port to disconnect from.
 * @param servinstname The name of the instance (module) with the service port.
 * @param servportname The name of the service port to disconnect from.
 * @return An empty string on success, or an error message on failure.
 */
string cmdDisconnectReqFromServ(VulDesign &design, const string &reqinstname, const string &reqportname, const string &servinstname, const string &servportname) {
    // All four parameters must be provided for a precise match
    if (reqinstname.empty()) return string("#25080: request instance name cannot be empty");
    if (reqportname.empty()) return string("#25081: request port name cannot be empty");
    if (servinstname.empty()) return string("#25082: service instance name cannot be empty");
    if (servportname.empty()) return string("#25083: service port name cannot be empty");

    for (auto it = design.req_connections.begin(); it != design.req_connections.end(); ++it) {
        const VulReqConnection &rc = *it;
        if (rc.req_instance == reqinstname && rc.req_name == reqportname && rc.serv_instance == servinstname && rc.serv_name == servportname) {
            design.req_connections.erase(it);
            design.dirty_combines = true;
            return string();
        }
    }

    return string("#25084: specified request->service connection not found");
}

/**
 * @brief Internal helper to check for cycles when adding an update sequence.
 * This function checks all connection in stalled_connections and update_constraints.
 * @param design The VulDesign object to check.
 * @param newformer The former instance of the new update sequence to add.
 * @param newlatter The latter instance of the new update sequence to add.
 * @return true if a cycle would be created, false otherwise.
 */
bool _checkUpdateSequenceLoop(VulDesign &design, const string &newformer, const string &newlatter) {
    // Build adjacency list from existing update_constraints and stalled_connections
    std::unordered_map<string, vector<string>> adj;
    for (const VulUpdateSequence &us : design.update_constraints) {
        adj[us.former_instance].push_back(us.latter_instance);
    }
    for (const VulStalledConnection &sc : design.stalled_connections) {
        adj[sc.src_instance].push_back(sc.dest_instance);
    }

    // add the new update edge
    adj[newformer].push_back(newlatter);

    // DFS to detect cycles
    std::unordered_set<string> visited;
    std::unordered_set<string> onstack;

    std::function<bool(const string&, vector<string>&)> dfs = [&](const string &u, vector<string> &path) -> bool {
        visited.insert(u);
        onstack.insert(u);
        path.push_back(u);
        for (const string &v : adj[u]) {
            if (onstack.find(v) != onstack.end()) {
                return true; // cycle detected
            }
            if (visited.find(v) == visited.end()) {
                if (dfs(v, path)) return true;
            }
        }
        onstack.erase(u);
        path.pop_back();
        return false;
    };

    for (const auto &p : adj) {
        const string &node = p.first;
        if (visited.find(node) == visited.end()) {
            vector<string> path;
            if (dfs(node, path)) return true;
        }
    }
    return false;
}

/**
 * @brief Connect two instances in a stalled connection.
 * Both instances must be from stallable combines.
 * @param design The VulDesign object to modify.
 * @param srcinstname The name of the source instance.
 * @param destinstname The name of the destination instance.
 * @return An empty string on success, or an error message on failure.
 */
string cmdConnectStalled(VulDesign &design, const string &srcinstname, const string &destinstname) {
    if (srcinstname.empty()) return string("#25090: source instance name cannot be empty");
    if (destinstname.empty()) return string("#25091: destination instance name cannot be empty");

    // instances must exist
    auto sIt = design.instances.find(srcinstname);
    if (sIt == design.instances.end()) return string("#25092: instance '") + srcinstname + "' not found";
    auto dIt = design.instances.find(destinstname);
    if (dIt == design.instances.end()) return string("#25093: instance '") + destinstname + "' not found";

    // cannot stall to self
    if (srcinstname == destinstname) return string("#25094: cannot create stalled connection from instance '") + srcinstname + "' to itself";

    // both combines must be stallable
    const string &srcComb = sIt->second.combine;
    const string &destComb = dIt->second.combine;
    auto scIt = design.combines.find(srcComb);
    if (scIt == design.combines.end()) return string("#25095: combine '") + srcComb + "' not found for instance '" + srcinstname + "'";
    if (!scIt->second.stallable) return string("#25096: source instance '") + srcinstname + "' combine '" + srcComb + "' is not stallable";
    auto dcIt = design.combines.find(destComb);
    if (dcIt == design.combines.end()) return string("#25097: combine '") + destComb + "' not found for instance '" + destinstname + "'";
    if (!dcIt->second.stallable) return string("#25098: destination instance '") + destinstname + "' combine '" + destComb + "' is not stallable";

    // uniqueness: identical stalled connection must not already exist
    for (const VulStalledConnection &sc : design.stalled_connections) {
        if (sc.src_instance == srcinstname && sc.dest_instance == destinstname) {
            return string("#25099: stalled connection from '") + srcinstname + "' to '" + destinstname + "' already exists";
        }
    }

    // cycle detection: build adjacency of update_constraints + stalled_connections + this new edge
    if (_checkUpdateSequenceLoop(design, srcinstname, destinstname)) {
        return string("#25100: cannot create stalled connection from '") + srcinstname + "' to '" + destinstname + "' because it would create a cycle in update sequences";
    }

    // all good -> add stalled connection
    VulStalledConnection nc;
    nc.src_instance = srcinstname;
    nc.dest_instance = destinstname;
    design.stalled_connections.push_back(nc);
    design.dirty_combines = true;
    return string();
}

/**
 * @brief Disconnect two instances in a stalled connection.
 * @param design The VulDesign object to modify.
 * @param srcinstname The name of the source instance.
 * @param destinstname The name of the destination instance.
 * @return An empty string on success, or an error message on failure.
 */
string cmdDisconnectStalled(VulDesign &design, const string &srcinstname, const string &destinstname) {
    if (srcinstname.empty()) return string("#25110: source instance name cannot be empty");
    if (destinstname.empty()) return string("#25111: destination instance name cannot be empty");

    for (auto it = design.stalled_connections.begin(); it != design.stalled_connections.end(); ++it) {
        const VulStalledConnection &sc = *it;
        if (sc.src_instance == srcinstname && sc.dest_instance == destinstname) {
            design.stalled_connections.erase(it);
            design.dirty_combines = true;
            return string();
        }
    }

    return string("#25112: specified stalled connection not found");
}

/**
 * @brief Connect two instances in a sequence connection.
 * @param design The VulDesign object to modify.
 * @param formerinstname The name of the former instance.
 * @param latterinstname The name of the latter instance.
 * @return An empty string on success, or an error message on failure.
 */
string cmdConnectUpdateSequence(VulDesign &design, const string &formerinstname, const string &latterinstname) {
    if (formerinstname.empty()) return string("#25120: former instance name cannot be empty");
    if (latterinstname.empty()) return string("#25121: latter instance name cannot be empty");

    // instances must exist
    auto fIt = design.instances.find(formerinstname);
    if (fIt == design.instances.end()) return string("#25122: instance '") + formerinstname + "' not found";
    auto lIt = design.instances.find(latterinstname);
    if (lIt == design.instances.end()) return string("#25123: instance '") + latterinstname + "' not found";

    // cannot sequence to self
    if (formerinstname == latterinstname) return string("#25124: cannot create update sequence from instance '") + formerinstname + "' to itself";

    // uniqueness: identical update sequence must not already exist
    for (const VulUpdateSequence &us : design.update_constraints) {
        if (us.former_instance == formerinstname && us.latter_instance == latterinstname) {
            return string("#25125: update sequence from '") + formerinstname + "' to '" + latterinstname + "' already exists";
        }
    }

    // cycle detection: build adjacency of update_constraints + stalled_connections + this new edge
    if (_checkUpdateSequenceLoop(design, formerinstname, latterinstname)) {
        return string("#25126: cannot create update sequence from '") + formerinstname + "' to '" + latterinstname + "' because it would create a cycle in update sequences";
    }

    // all good -> add update sequence
    VulUpdateSequence nu;
    nu.former_instance = formerinstname;
    nu.latter_instance = latterinstname;
    design.update_constraints.push_back(nu);
    design.dirty_combines = true;
    return string();
}

/**
 * @brief Disconnect two instances in a sequence connection.
 * @param design The VulDesign object to modify.
 * @param formerinstname The name of the former instance.
 * @param latterinstname The name of the latter instance.
 * @return An empty string on success, or an error message on failure.
 */
string cmdDisconnectUpdateSequence(VulDesign &design, const string &formerinstname, const string &latterinstname) {
    if (formerinstname.empty()) return string("#25130: former instance name cannot be empty");
    if (latterinstname.empty()) return string("#25131: latter instance name cannot be empty");

    for (auto it = design.update_constraints.begin(); it != design.update_constraints.end(); ++it) {
        const VulUpdateSequence &us = *it;
        if (us.former_instance == formerinstname && us.latter_instance == latterinstname) {
            design.update_constraints.erase(it);
            design.dirty_combines = true;
            return string();
        }
    }

    return string("#25132: specified update sequence not found");
}

/**
 * @brief Internal helper to check and update a C++ source file for a function definition.
 * C++ file name is derived from funcname with .cpp extension.
 * This function looks for a function definition matching the given name, and moves the function body into a new file with helper lines and signature,
 * then replaces the original file with new file contents.
 * If the function is not found, no changes are made and false is returned.
 * @param helperlines Lines to insert before the function definition in the new file.
 * @param funccomments Comments to insert before the function definition in the new file.
 * @param filename The C++ source file to check and update.
 * @param funcname The name of the function to look for.
 * @param funcsig The function signature to insert in the new file.
 * @return true if the function was found and updated, false if not found.
 */
bool _checkAndUpdateCppFile(vector<string> &helperlines, vector<string> &funccomments, const string &filename, const string &funcname, const string &funcsig) {
    string err;
    // try to extract the function body from the file
    unique_ptr<vector<string>> body;
    if (std::filesystem::exists(filename)) {
        // file exists, try to extract
        body = codeExtractFunctionBodyFromFile(filename, funcname, err);
    }

    std::filesystem::path filepath(filename);
    std::filesystem::path tmp = filepath;
    tmp += ".tmp";
    std::error_code ec;
    // remove any pre-existing tmp
    std::filesystem::remove(tmp, ec);

    std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
    if (!ofs) {
        return false;
    }

    // write helper lines
    for (const string &line : helperlines) {
        ofs << line << "\n";
    }
    // write function comments
    for (const string &c : funccomments) {
        ofs << c << "\n";
    }

    // write signature and opening brace
    ofs << funcsig << " {\n";

    // write extracted body lines
    if (body) {
        for (const string &line : *body) {
            ofs << line << "\n";
        }
    } else {
        // no body extracted -> write a TODO comment
        ofs << "    // TODO: implement " << funcname << "\n";
    }

    // closing brace
    ofs << "}\n";
    ofs.close();

    // atomically replace original
    std::filesystem::rename(tmp, filepath, ec);
    if (ec) {
        // try to remove tmp if rename failed
        std::filesystem::remove(tmp, ec);
        return false;
    }

    return true;
}

/**
 * @brief Update C++ file helpers for init/tick/applytick/service ports in a specific combine.
 * This function will create or update C++ files and function names for all service ports
 * in the specified combine that do not have them set up yet. It will also check for any broken files and report them.
 * @param design The VulDesign object to modify.
 * @param combinename The name of the combine to update.
 * @param outputbrokenfiles A vector to store the names of any broken files found during the update.
 * @return An empty string on success, or an error message on failure.
 */
string cmdUpdateCombineCppFileHelpers(VulDesign &design, const string &combinename, vector<string> &outputbrokenfiles) {
    if (combinename.empty()) return string("#22200: combine name cannot be empty");
    auto it = design.combines.find(combinename);
    if (it == design.combines.end()) return string("#22201: combine '") + combinename + "' not found";

    VulCombine &vc = it->second;

    // generate helper lines once for this combine
    auto helper_lines_ptr = codeGenerateHelperLines(vc);
    vector<string> helperlines;
    if (helper_lines_ptr) helperlines = *helper_lines_ptr;

    // prepare cpp directory base
    std::filesystem::path cppdir = std::filesystem::path(design.project_dir) / "cpp";

    // helper to build filepath string from function name
    auto filepath_for = [&](const string &fname) -> string {
        std::filesystem::path p = cppdir / (fname + ".cpp");
        return p.string();
    };

    // check init/tick/applytick functions (these have no args/rets)
    vector<VulCppFunc*> special = { &vc.init, &vc.tick, &vc.applytick };
    for (VulCppFunc *pf : special) {
        if (!pf->name.empty()) {
            string funcname = pf->name;
            string filename = filepath_for(funcname);
            vector<string> funccomments; // empty per spec
            vector<VulArgument> empty_args; vector<VulArgument> empty_rets;
            string funcsig = codeGenerateFunctionSignature(funcname, empty_args, empty_rets);
            // try to extract and update function body; record broken files
            if (!_checkAndUpdateCppFile(helperlines, funccomments, filename, funcname, funcsig)) {
                outputbrokenfiles.push_back(filename);
            }
        }
    }

    // check service functions
    for (const VulService &s : vc.service) {
        if (!s.cppfunc.name.empty()) {
            string funcname = s.cppfunc.name;
            string filename = filepath_for(funcname);
            // build function comments from service properties
            vector<string> funccomments;
            funccomments.push_back(string("// Service: ") + s.name);
            if (!s.comment.empty()) funccomments.push_back(string("// ") + s.comment);
            // arguments and returns as comments
            for (const VulArgument &a : s.arg) {
                funccomments.push_back(string("// @arg ") + a.name + string(": ") + a.comment);
            }
            for (const VulArgument &r : s.ret) {
                funccomments.push_back(string("// @ret ") + r.name + string(": ") + r.comment);
            }
            string funcsig = codeGenerateFunctionSignature(funcname, s.arg, s.ret);
            if (!_checkAndUpdateCppFile(helperlines, funccomments, filename, funcname, funcsig)) {
                outputbrokenfiles.push_back(filename);
            }
        }
    }

    return string();
}

/**
 * @brief Update all C++ file helpers for init/tick/applytick/service ports in the design.
 * This function will create or update C++ files and function names for all service ports
 * that do not have them set up yet. It will also check for any broken files and report them.
 * @param design The VulDesign object to modify.
 * @param outputbrokenfiles A vector to store the names of any broken files found during the update.
 * @return An empty string on success, or an error message on failure.
 */
string cmdUpdateAllCppFileHelpers(VulDesign &design, vector<string> &outputbrokenfiles) {
    for (const auto &p : design.combines) {
        const string &combname = p.first;
        string err = cmdUpdateCombineCppFileHelpers(design, combname, outputbrokenfiles);
        if (!err.empty()) return err;
    }
    return string();
}































