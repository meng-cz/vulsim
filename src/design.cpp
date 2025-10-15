#include "design.h"
#include "pugixml.hpp"

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
unique_ptr<VulDesign> VulDesign::loadFromFile(const string &filename, VulPrefabMetaDataMap &prefabs, string &err) {
    auto design = std::unique_ptr<VulDesign>(new VulDesign());
    design->prefab_map = prefabs;
    namespace fs = std::filesystem;

    fs::path filePath(filename);
    if (!fs::exists(filePath)) {
        err = "#10001: project file not found: " + filename;
        return nullptr;
    }

    fs::path dir = filePath.parent_path();
    design->project_dir = dir.string();
    design->project_name = filePath.stem().string();

    // load version from .vul file
    {
        pugi::xml_document doc;
        pugi::xml_parse_result res = doc.load_file(filePath.string().c_str());
        if (!res) {
            err = string("#10002: Failed to load project file: ") + filename + "; parser error: " + res.description();
            return nullptr;
        }

        pugi::xml_node root = doc.child("project");
        if (!root) {
            err = string("#10003: Missing root <project> in file: ") + filename;
            return nullptr;
        }

        pugi::xml_node versionNode = root.child("version");
        if (versionNode) {
            string verStr = versionNode.text().as_string();
            size_t pos1 = verStr.find('.');
            size_t pos2 = verStr.rfind('.');
            if (pos1 != string::npos && pos2 != string::npos && pos1 != pos2) {
                try {
                    int major = std::stoi(verStr.substr(0, pos1));
                    int minor = std::stoi(verStr.substr(pos1 + 1, pos2 - pos1 - 1));
                    int patch = std::stoi(verStr.substr(pos2 + 1));
                    design->version = {major, minor, patch};
                } catch (...) {
                    // ignore parse errors, keep default version
                }
            }
        }
    }

    // load config.xml if present
    fs::path configFile = dir / "config.xml";
    if (fs::exists(configFile) && fs::is_regular_file(configFile)) {
        design->_loadConfigLibFromFile(configFile.string(), err);
        if (!err.empty()) return nullptr;
    }

    // load all bundle xml files from bundle/ folder
    fs::path bundleDir = dir / "bundle";
    if (fs::exists(bundleDir) && fs::is_directory(bundleDir)) {
        for (auto &entry : fs::directory_iterator(bundleDir)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() == ".xml") {
                design->_loadBundleFromFile(entry.path().string(), err);
                if (!err.empty()) return nullptr;
            }
        }
    }

    // load all combine xml files from combine/ folder
    fs::path combineDir = dir / "combine";
    if (fs::exists(combineDir) && fs::is_directory(combineDir)) {
        for (auto &entry : fs::directory_iterator(combineDir)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() == ".xml") {
                design->_loadCombineFromFile(entry.path().string(), err);
                if (!err.empty()) return nullptr;
            }
        }
    }

    // load design.xml (required)
    fs::path designFile = dir / "design.xml";
    if (fs::exists(designFile) && fs::is_regular_file(designFile)) {
        design->_loadDesignFromFile(designFile.string(), err);
        if (!err.empty()) return nullptr;
    } else {
        err = "#10002: design.xml not found in project folder: " + dir.string();
        return nullptr;
    }

    return std::move(design);
}

void VulDesign::_loadConfigLibFromFile(const string &filename, string &err) {
    err.clear();

    pugi::xml_document doc;
    pugi::xml_parse_result res = doc.load_file(filename.c_str());
    if (!res) {
        err = string("#10003: Failed to load config file: ") + filename + "; parser error: " + res.description();
        return;
    }

    pugi::xml_node root = doc.child("configlib");
    if (!root) {
        err = string("#10004: Missing root <configlib> in file: ") + filename;
        return;
    }

    // clear existing config lib before loading
    config_lib.clear();

    for (pugi::xml_node item : root.children("configitem")) {
        pugi::xml_node nameNode = item.child("name");
        pugi::xml_node valueNode = item.child("value");
        pugi::xml_node commentNode = item.child("comment");

        if (!nameNode || !valueNode) {
            err = string("#10005: configitem missing <name> or <value> in file: ") + filename;
            return;
        }

        string name = nameNode.text().as_string();
        string value = valueNode.text().as_string();
        string comment = commentNode ? commentNode.text().as_string() : string();

        if (name.empty()) {
            err = string("#10006: configitem has empty <name> in file: ") + filename;
            return;
        }

        VulConfigItem ci;
        ci.name = name;
        ci.value = value;
        ci.comment = comment;

        config_lib[ci.name] = ci;
    }

}

void VulDesign::_loadBundleFromFile(const string &filename, string &err) {
    err.clear();

    pugi::xml_document doc;
    pugi::xml_parse_result res = doc.load_file(filename.c_str());
    if (!res) {
        err = string("#10007: Failed to load bundle file: ") + filename + "; parser error: " + res.description();
        return;
    }

    pugi::xml_node root = doc.child("bundle");
    if (!root) {
        err = string("#10008: Missing root <bundle> in file: ") + filename;
        return;
    }

    pugi::xml_node nameNode = root.child("name");
    if (!nameNode) {
        err = string("#10009: bundle missing <name> in file: ") + filename;
        return;
    }

    VulBundle vb;
    vb.name = nameNode.text().as_string();
    pugi::xml_node commentNode = root.child("comment");
    vb.comment = commentNode ? commentNode.text().as_string() : string();

    if (vb.name.empty()) {
        err = string("#10010: bundle has empty <name> in file: ") + filename;
        return;
    }

    // parse members
    for (pugi::xml_node m : root.children("member")) {
        pugi::xml_node mtype = m.child("type");
        pugi::xml_node mname = m.child("name");
        if (!mtype || !mname) {
            err = string("#10011: member missing <type> or <name> in bundle file: ") + filename;
            return;
        }

        VulArgument va;
        va.type = mtype.text().as_string();
        va.name = mname.text().as_string();
        pugi::xml_node mcomment = m.child("comment");
        va.comment = mcomment ? mcomment.text().as_string() : string();

        if (va.name.empty()) {
            err = string("#10012: member has empty <name> in bundle file: ") + filename;
            return;
        }

        vb.members.push_back(va);
    }

    // insert or overwrite existing bundle
    bundles[vb.name] = vb;
}

void VulDesign::_loadCombineFromFile(const string &filename, string &err) {
    err.clear();

    pugi::xml_document doc;
    pugi::xml_parse_result res = doc.load_file(filename.c_str());
    if (!res) {
        err = string("#10013: Failed to load combine file: ") + filename + "; parser error: " + res.description();
        return;
    }

    pugi::xml_node root = doc.child("combine");
    if (!root) {
        err = string("#10014: Missing root <combine> in file: ") + filename;
        return;
    }

    pugi::xml_node nameNode = root.child("name");
    if (!nameNode) {
        err = string("#10015: combine missing <name> in file: ") + filename;
        return;
    }

    VulCombine vc;
    vc.name = nameNode.text().as_string();
    pugi::xml_node commentNode = root.child("comment");
    vc.comment = commentNode ? commentNode.text().as_string() : string();

    if (vc.name.empty()) {
        err = string("#10016: combine has empty <name> in file: ") + filename;
        return;
    }

    // config entries
    for (pugi::xml_node c : root.children("config")) {
        pugi::xml_node cname = c.child("name");
        if (!cname) {
            err = string("#10017: config missing <name> in combine file: ") + filename;
            return;
        }
        VulConfig cfg;
        cfg.name = cname.text().as_string();
        pugi::xml_node cref = c.child("ref");
        cfg.ref = cref ? cref.text().as_string() : string();
        pugi::xml_node ccomment = c.child("comment");
        cfg.comment = ccomment ? ccomment.text().as_string() : string();
        if (cfg.name.empty()) {
            err = string("#10018: config has empty <name> in combine file: ") + filename;
            return;
        }
        vc.config.push_back(cfg);
    }

    // pipein and pipeout
    for (pugi::xml_node p : root.children("pipein")) {
        pugi::xml_node pname = p.child("name");
        pugi::xml_node ptype = p.child("type");
        pugi::xml_node pcomment = p.child("comment");
        VulPipePort vpp;
        vpp.name = pname ? pname.text().as_string() : string();
        vpp.type = ptype ? ptype.text().as_string() : string();
        vpp.comment = pcomment ? pcomment.text().as_string() : string();
        vc.pipein.push_back(vpp);
    }
    for (pugi::xml_node p : root.children("pipeout")) {
        pugi::xml_node pname = p.child("name");
        pugi::xml_node ptype = p.child("type");
        pugi::xml_node pcomment = p.child("comment");
        VulPipePort vpp;
        vpp.name = pname ? pname.text().as_string() : string();
        vpp.type = ptype ? ptype.text().as_string() : string();
        vpp.comment = pcomment ? pcomment.text().as_string() : string();
        vc.pipeout.push_back(vpp);
    }

    // requests
    for (pugi::xml_node r : root.children("request")) {
        VulRequest vr;
        pugi::xml_node rname = r.child("name");
        if (rname) vr.name = rname.text().as_string();
        pugi::xml_node rcomment = r.child("comment");
        vr.comment = rcomment ? rcomment.text().as_string() : string();

        // args
        for (pugi::xml_node a : r.children("arg")) {
            pugi::xml_node aname = a.child("name");
            pugi::xml_node atype = a.child("type");
            if (!aname || !atype) {
                err = string("#10019: request arg missing <name> or <type> in combine file: ") + filename;
                return;
            }
            VulArgument va;
            va.name = aname.text().as_string();
            va.type = atype.text().as_string();
            pugi::xml_node acomment = a.child("comment");
            va.comment = acomment ? acomment.text().as_string() : string();
            vr.arg.push_back(va);
        }

        // returns
        for (pugi::xml_node ret : r.children("return")) {
            pugi::xml_node rname2 = ret.child("name");
            pugi::xml_node rtype2 = ret.child("type");
            if (!rname2 || !rtype2) {
                err = string("#10020: request return missing <name> or <type> in combine file: ") + filename;
                return;
            }
            VulArgument va;
            va.name = rname2.text().as_string();
            va.type = rtype2.text().as_string();
            pugi::xml_node rcomment2 = ret.child("comment");
            va.comment = rcomment2 ? rcomment2.text().as_string() : string();
            vr.ret.push_back(va);
        }

        vc.request.push_back(vr);
    }

    // services
    for (pugi::xml_node s : root.children("service")) {
        VulService vs;
        pugi::xml_node sname = s.child("name");
        if (sname) vs.name = sname.text().as_string();
        pugi::xml_node scomment = s.child("comment");
        vs.comment = scomment ? scomment.text().as_string() : string();

        for (pugi::xml_node a : s.children("arg")) {
            pugi::xml_node aname = a.child("name");
            pugi::xml_node atype = a.child("type");
            if (!aname || !atype) {
                err = string("#10021: service arg missing <name> or <type> in combine file: ") + filename;
                return;
            }
            VulArgument va;
            va.name = aname.text().as_string();
            va.type = atype.text().as_string();
            pugi::xml_node acomment = a.child("comment");
            va.comment = acomment ? acomment.text().as_string() : string();
            vs.arg.push_back(va);
        }

        for (pugi::xml_node ret : s.children("return")) {
            pugi::xml_node rname2 = ret.child("name");
            pugi::xml_node rtype2 = ret.child("type");
            if (!rname2 || !rtype2) {
                err = string("#10022: service return missing <name> or <type> in combine file: ") + filename;
                return;
            }
            VulArgument va;
            va.name = rname2.text().as_string();
            va.type = rtype2.text().as_string();
            pugi::xml_node rcomment2 = ret.child("comment");
            va.comment = rcomment2 ? rcomment2.text().as_string() : string();
            vs.ret.push_back(va);
        }

        // cppfunc
        pugi::xml_node cpp = s.child("cppfunc");
        if (cpp) {
            pugi::xml_node code = cpp.child("code");
            pugi::xml_node file = cpp.child("file");
            pugi::xml_node name = cpp.child("name");
            vs.cppfunc.code = code ? code.text().as_string() : string();
            vs.cppfunc.file = file ? file.text().as_string() : string();
            vs.cppfunc.name = name ? name.text().as_string() : string();
        }

        vc.service.push_back(vs);
    }

    // storage / storagenext / storagetick
    auto parseStorageList = [&](const char *tag, vector<VulStorage> &out) {
        for (pugi::xml_node st : root.children(tag)) {
            pugi::xml_node sname = st.child("name");
            pugi::xml_node stype = st.child("type");
            if (!sname || !stype) {
                err = string("#10023: ") + string(tag) + string(" item missing <name> or <type> in combine file: ") + filename;
                return false;
            }
            VulStorage vs;
            vs.name = sname.text().as_string();
            vs.type = stype.text().as_string();
            pugi::xml_node sval = st.child("value");
            vs.value = sval ? sval.text().as_string() : string();
            pugi::xml_node scomment = st.child("comment");
            vs.comment = scomment ? scomment.text().as_string() : string();
            out.push_back(vs);
        }
        return true;
    };

    if (!parseStorageList("storage", vc.storage)) return;
    if (!parseStorageList("storagenext", vc.storagenext)) return;
    if (!parseStorageList("storagetick", vc.storagetick)) return;

    // stallable
    pugi::xml_node stall = root.child("stallable");
    vc.stallable = false;
    if (stall) {
        vc.stallable = true;
    }

    // tick / applytick / init cppfuncs
    auto parseCppFunc = [&](const char *tag, VulCppFunc &out) {
        pugi::xml_node node = root.child(tag);
        if (!node) return;
        pugi::xml_node cpp = node.child("cppfunc");
        if (!cpp) return;
        pugi::xml_node code = cpp.child("code");
        pugi::xml_node file = cpp.child("file");
        pugi::xml_node name = cpp.child("name");
        out.code = code ? code.text().as_string() : string();
        out.file = file ? file.text().as_string() : string();
        out.name = name ? name.text().as_string() : string();
    };

    parseCppFunc("tick", vc.tick);
    parseCppFunc("applytick", vc.applytick);
    parseCppFunc("init", vc.init);

    // insert into map
    combines[vc.name] = vc;
}

void VulDesign::_loadDesignFromFile(const string &filename, string &err) {
    err.clear();

    pugi::xml_document doc;
    pugi::xml_parse_result res = doc.load_file(filename.c_str());
    if (!res) {
        err = string("#10024: Failed to load design file: ") + filename + "; parser error: " + res.description();
        return;
    }

    pugi::xml_node root = doc.child("design");
    if (!root) {
        err = string("#10025: Missing root <design> in file: ") + filename;
        return;
    }

    auto parseVisual = [&](pugi::xml_node vnode, VulVisualization &vis) {
        if (!vnode) return;
        pugi::xml_node n;
        n = vnode.child("x"); vis.x = n ? (long)n.text().as_llong() : 0;
        n = vnode.child("y"); vis.y = n ? (long)n.text().as_llong() : 0;
        n = vnode.child("w"); vis.w = n ? (long)n.text().as_llong() : 0;
        n = vnode.child("h"); vis.h = n ? (long)n.text().as_llong() : 0;

        vis.visible = false;
        vis.enabled = false;
        vis.selected = false;
        n = vnode.child("visible"); if (n) { string v = n.text().as_string(); vis.visible = (v=="1"||v=="true"||v=="True"); }
        n = vnode.child("enabled"); if (n) { string v = n.text().as_string(); vis.enabled = (v=="1"||v=="true"||v=="True"); }
        n = vnode.child("selected"); if (n) { string v = n.text().as_string(); vis.selected = (v=="1"||v=="true"||v=="True"); }

        n = vnode.child("color"); vis.color = n ? n.text().as_string() : string();
        n = vnode.child("bordercolor"); vis.bordercolor = n ? n.text().as_string() : string();
        n = vnode.child("borderwidth"); vis.borderwidth = n ? (unsigned int)n.text().as_llong() : 0u;
    };

    // instances
    for (pugi::xml_node inst : root.children("instance")) {
        pugi::xml_node nameNode = inst.child("name");
        pugi::xml_node combineNode = inst.child("combine");
        if (!nameNode || !combineNode) {
            err = string("#10026: instance missing <name> or <combine> in design file: ") + filename;
            return;
        }
        VulInstance vi;
        vi.name = nameNode.text().as_string();
        vi.combine = combineNode.text().as_string();
        pugi::xml_node commentNode = inst.child("comment");
        vi.comment = commentNode ? commentNode.text().as_string() : string();

        // config overrides
        for (pugi::xml_node c : inst.children("config")) {
            pugi::xml_node cname = c.child("name");
            pugi::xml_node cval = c.child("value");
            if (!cname || !cval) {
                err = string("#10027: instance config missing <name> or <value> in design file: ") + filename;
                return;
            }
            vi.config_override.emplace_back(cname.text().as_string(), cval.text().as_string());
        }

        // visual
        pugi::xml_node visNode = inst.child("visual");
        parseVisual(visNode, vi.vis);

        instances[vi.name] = vi;
    }

    // pipes
    for (pugi::xml_node pnode : root.children("pipe")) {
        pugi::xml_node nameNode = pnode.child("name");
        pugi::xml_node typeNode = pnode.child("type");
        if (!nameNode || !typeNode) {
            err = string("#10028: pipe missing <name> or <type> in design file: ") + filename;
            return;
        }
        VulPipe vp;
        vp.name = nameNode.text().as_string();
        vp.type = typeNode.text().as_string();
        pugi::xml_node commentNode = pnode.child("comment");
        vp.comment = commentNode ? commentNode.text().as_string() : string();
        pugi::xml_node inNode = pnode.child("inputsize"); vp.inputsize = inNode ? (unsigned int)inNode.text().as_llong() : 0u;
        pugi::xml_node outNode = pnode.child("outputsize"); vp.outputsize = outNode ? (unsigned int)outNode.text().as_llong() : 0u;
        pugi::xml_node bufNode = pnode.child("buffersize"); vp.buffersize = bufNode ? (unsigned int)bufNode.text().as_llong() : 0u;
        pugi::xml_node visNode = pnode.child("visual"); parseVisual(visNode, vp.vis);
        pipes[vp.name] = vp;
    }

    // reqconn
    for (pugi::xml_node rc : root.children("reqconn")) {
        pugi::xml_node reqport = rc.child("reqport");
        if (!reqport) {
            err = string("#10029: reqconn missing <reqport> in design file: ") + filename;
            return;
        }
        pugi::xml_node instn = reqport.child("instname");
        pugi::xml_node portn = reqport.child("portname");
        if (!instn || !portn) {
            err = string("#10029: reqport missing <instname> or <portname> in design file: ") + filename;
            return;
        }
        VulReqConnection vrc;
        vrc.req_instance = instn.text().as_string();
        vrc.req_name = portn.text().as_string();

        pugi::xml_node servport = rc.child("servport");
        if (!servport) {
            err = string("#10030: reqconn missing <servport> in design file: ") + filename;
            return;
        }
        pugi::xml_node sinst = servport.child("instname");
        pugi::xml_node sport = servport.child("portname");
        if (!sinst || !sport) {
            err = string("#10030: reqconn missing <instname> or <portname> in design file: ") + filename;
            return;
        }
        vrc.serv_instance = sinst.text().as_string();
        vrc.serv_name = sport.text().as_string();

        pugi::xml_node visNode = rc.child("visual"); parseVisual(visNode, vrc.vis);
        req_connections.push_back(vrc);
    }

    // modpipe and pipemod
    for (pugi::xml_node mp : root.children("modpipe")) {
        pugi::xml_node instn = mp.child("instname");
        pugi::xml_node portn = mp.child("portname");
        pugi::xml_node pipen = mp.child("pipename");
        pugi::xml_node pindex = mp.child("portindex");
        if (!instn || !portn || !pipen || !pindex) {
            err = string("#10032: modpipe missing fields in design file: ") + filename;
            return;
        }
        VulModulePipeConnection vmp;
        vmp.instance = instn.text().as_string();
    vmp.pipeoutport = portn.text().as_string();
        vmp.pipe = pipen.text().as_string();
        vmp.portindex = (unsigned int)pindex.text().as_llong();
        pugi::xml_node visNode = mp.child("visual"); parseVisual(visNode, vmp.vis);
        mod_pipe_connections.push_back(vmp);
    }

    for (pugi::xml_node pm : root.children("pipemod")) {
        pugi::xml_node instn = pm.child("instname");
        pugi::xml_node portn = pm.child("portname");
        pugi::xml_node pipen = pm.child("pipename");
        pugi::xml_node pindex = pm.child("portindex");
        if (!instn || !portn || !pipen || !pindex) {
            err = string("#10033: pipemod missing fields in design file: ") + filename;
            return;
        }
        VulPipeModuleConnection vpm;
        vpm.instance = instn.text().as_string();
    vpm.pipeinport = portn.text().as_string();
        vpm.pipe = pipen.text().as_string();
        vpm.portindex = (unsigned int)pindex.text().as_llong();
        pugi::xml_node visNode = pm.child("visual"); parseVisual(visNode, vpm.vis);
        pipe_mod_connections.push_back(vpm);
    }

    // stalledconn
    for (pugi::xml_node sc : root.children("stalledconn")) {
        pugi::xml_node src = sc.child("src");
        pugi::xml_node dest = sc.child("dest");
        if (!src || !dest) {
            err = string("#10034: stalledconn missing <src> or <dest> in design file: ") + filename;
            return;
        }
        pugi::xml_node sname = src.child("instname");
        pugi::xml_node dname = dest.child("instname");
        if (!sname || !dname) {
            err = string("#10035: stalledconn src/dest missing <instname> in design file: ") + filename;
            return;
        }
        VulStalledConnection vsc;
        vsc.src_instance = sname.text().as_string();
        vsc.dest_instance = dname.text().as_string();
        pugi::xml_node visNode = sc.child("visual"); parseVisual(visNode, vsc.vis);
        stalled_connections.push_back(vsc);
    }

    // update constraints
    for (pugi::xml_node uc : root.children("updconstraint")) {
        pugi::xml_node former = uc.child("former");
        pugi::xml_node later = uc.child("later");
        if (!former || !later) {
            err = string("#10036: updconstraint missing <former> or <later> in design file: ") + filename;
            return;
        }
        pugi::xml_node fname = former.child("instname");
        pugi::xml_node lname = later.child("instname");
        if (!fname || !lname) {
            err = string("#10037: updconstraint former/later missing <instname> in design file: ") + filename;
            return;
        }
        VulUpdateSequence vus;
        vus.former_instance = fname.text().as_string();
        vus.latter_instance = lname.text().as_string();
        pugi::xml_node visNode = uc.child("visual"); parseVisual(visNode, vus.vis);
        update_constraints.push_back(vus);
    }

    // blocks and texts
    for (pugi::xml_node b : root.children("block")) {
        pugi::xml_node nameNode = b.child("name");
        if (!nameNode) {
            err = string("#10038: block missing <name> in design file: ") + filename;
            return;
        }
        VulVisBlock vb;
        vb.name = nameNode.text().as_string();
        pugi::xml_node visNode = b.child("visual"); parseVisual(visNode, vb.vis);
        vis_blocks.push_back(vb);
    }

    for (pugi::xml_node t : root.children("text")) {
        pugi::xml_node textNode = t.child("text");
        if (!textNode) {
            err = string("#10039: text missing <text> in design file: ") + filename;
            return;
        }
        VulVisText vt;
        vt.text = textNode.text().as_string();
        pugi::xml_node visNode = t.child("visual"); parseVisual(visNode, vt.vis);
        vis_texts.push_back(vt);
    }
}






/**
 * @brief Check for name conflicts:
 * (1) No duplicate names within: bundles, combines, instances, pipes, and config_lib.
 * (2) All instances must refer to an existing combine.
 * (3) All names must be valid C++ identifiers and not C++ reserved keywords.
 * (4) All config reference names in combines must exist in config_lib.
 */
string VulDesign::_checkNameError() {
    // collect C++ reserved keywords (subset common to C++11+)
    static const std::unordered_set<string> cpp_keywords = {
        "alignas","alignof","and","and_eq","asm","atomic_cancel","atomic_commit","atomic_noexcept",
        "auto","bitand","bitor","bool","break","case","catch","char","char16_t","char32_t","class",
        "compl","concept","const","constexpr","const_cast","continue","co_return","co_await","co_yield",
        "decltype","default","delete","do","double","dynamic_cast","else","enum","explicit","export",
        "extern","false","float","for","friend","goto","if","inline","int","long","mutable","namespace",
        "new","noexcept","not","not_eq","nullptr","operator","or","or_eq","private","protected","public",
        "register","reinterpret_cast","return","short","signed","sizeof","static","static_assert","static_cast",
        "struct","switch","template","this","thread_local","throw","true","try","typedef","typeid","typename",
        "union","unsigned","using","virtual","void","volatile","wchar_t","while","xor","xor_eq",
        "uint8", "uint16", "uint32", "uint64", "uint128", "int8", "int16", "int32", "int64", "int128"
    };

    auto is_valid_ident = [&](const string &s) {
        if (s.empty()) return false;
        // first char: letter or underscore
        unsigned char c0 = (unsigned char)s[0];
        if (!(std::isalpha(c0) || s[0] == '_')) return false;
        for (size_t i = 1; i < s.size(); ++i) {
            unsigned char c = (unsigned char)s[i];
            if (!(std::isalnum(c) || s[i] == '_')) return false;
        }
        if (cpp_keywords.find(s) != cpp_keywords.end()) return false;
        return true;
    };

    // (1) No duplicate names within: bundles, combines, instances, pipes, and config_lib.
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
        if (!is_valid_ident(o.first)) {
            return string("#11004: invalid identifier name: '") + o.first + "'";
        }
    }

    // (4) All config reference names in combines must exist in config_lib.
    for (const auto &kv : combines) {
        const string &cname = kv.first;
        const VulCombine &vc = kv.second;
        for (const VulConfig &cfg : vc.config) {
            if (!cfg.ref.empty()) {
                if (config_lib.find(cfg.ref) == config_lib.end()) {
                    return string("#11005: combine '") + cname + " has config ref to unknown config '" + cfg.ref + "'";
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
        auto combIt = combines.find(reqCombineName);
        if (combIt == combines.end()) {
            return string("#13002: req connection instance '") + rc.req_instance + "' refers to unknown combine '" + reqCombineName + "'";
        }

        // find request definition in combine
        const VulCombine &reqCombine = combIt->second;
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
            if(pipes.find(servInst) != pipes.end() && servName == "clear" && reqDef->arg.empty() && reqDef->ret.empty()) {
                    continue;
            }

            return string("#13004: req connection references unknown service instance '") + servInst + "' (for request '" + rc.req_name + "')";
        }

        // find service combine
        const string &servCombineName = sInstIt->second.combine;
        auto sCombIt = combines.find(servCombineName);
        if (sCombIt == combines.end()) {
            return string("#13005: service instance '") + servInst + "' refers to unknown combine '" + servCombineName + "'";
        }

        // find service definition
        const VulCombine &servCombine = sCombIt->second;
        const VulService *servDef = nullptr;
        for (const VulService &s : servCombine.service) {
            if (s.name == servName) { servDef = &s; break; }
        }
        if (!servDef) {
            return string("#13006: service '") + servName + "' not found in combine '" + servCombineName + "' for instance '" + servInst + "'";
        }

        // (3) type matching: args and returns must match exactly in type and order
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
        auto combIt = combines.find(combName);
        if (combIt == combines.end()) continue; // already checked elsewhere
        const VulCombine &vc = combIt->second;
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
        auto combIt = combines.find(combName);
        if (combIt == combines.end()) {
            return string("#14002: modpipe instance '") + mpc.instance + "' refers to unknown combine '" + combName + "'";
        }

        // module pipeout port exists in combine
        const VulCombine &vc = combIt->second;
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
        auto combIt = combines.find(combName);
        if (combIt == combines.end()) {
            return string("#14008: pipemod instance '") + pmc.instance + "' refers to unknown combine '" + combName + "'";
        }

        // module pipein port exists in combine
        const VulCombine &vc = combIt->second;
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
        auto combIt = combines.find(destComb);
        if (combIt == combines.end()) {
            return string("#15005: stalled connection dest instance '") + sc.dest_instance + "' refers to unknown combine '" + destComb + "'";
        }
        if (!combIt->second.stallable) {
            return string("#15006: stalled connection dest instance '") + sc.dest_instance + "' combine '" + destComb + "' is not stallable";
        }

        // src combine must be stallable
        const string &srcComb = instances.at(sc.src_instance).combine;
        combIt = combines.find(srcComb);
        if (combIt == combines.end()) {
            return string("#15007: stalled connection src instance '") + sc.src_instance + "' refers to unknown combine '" + srcComb + "'";
        }
        if (!combIt->second.stallable) {
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



// Helper to write visualization node
static void writeVisualNode(pugi::xml_node parent, const VulVisualization &vis) {
    pugi::xml_node vnode = parent.append_child("visual");
    vnode.append_child("x").text().set(std::to_string(vis.x).c_str());
    vnode.append_child("y").text().set(std::to_string(vis.y).c_str());
    vnode.append_child("w").text().set(std::to_string(vis.w).c_str());
    vnode.append_child("h").text().set(std::to_string(vis.h).c_str());
    vnode.append_child("visible").text().set(vis.visible ? "1" : "0");
    vnode.append_child("enabled").text().set(vis.enabled ? "1" : "0");
    vnode.append_child("selected").text().set(vis.selected ? "1" : "0");
    if (!vis.color.empty()) vnode.append_child("color").text().set(vis.color.c_str());
    if (!vis.bordercolor.empty()) vnode.append_child("bordercolor").text().set(vis.bordercolor.c_str());
    vnode.append_child("borderwidth").text().set(std::to_string(vis.borderwidth).c_str());
}

void VulDesign::_saveConfigLibToFile(const string &filename, string &err) {
    err.clear();
    pugi::xml_document doc;
    pugi::xml_node root = doc.append_child("configlib");
    for (const auto &kv : config_lib) {
        const VulConfigItem &ci = kv.second;
        pugi::xml_node item = root.append_child("configitem");
        item.append_child("name").text().set(ci.name.c_str());
        item.append_child("value").text().set(ci.value.c_str());
        if (!ci.comment.empty()) item.append_child("comment").text().set(ci.comment.c_str());
    }

    bool ok = doc.save_file(filename.c_str(), "  ");
    if (!ok) err = string("#16001: failed to save configlib to file: ") + filename;
}

void VulDesign::_saveBundleToFile(const string &filename, const VulBundle &vb, string &err) {
    err.clear();
    pugi::xml_document doc;
    pugi::xml_node root = doc.append_child("bundle");
    root.append_child("name").text().set(vb.name.c_str());
    if (!vb.comment.empty()) root.append_child("comment").text().set(vb.comment.c_str());

    for (const VulArgument &m : vb.members) {
        pugi::xml_node mn = root.append_child("member");
        mn.append_child("name").text().set(m.name.c_str());
        mn.append_child("type").text().set(m.type.c_str());
        if (!m.comment.empty()) mn.append_child("comment").text().set(m.comment.c_str());
    }

    bool ok = doc.save_file(filename.c_str(), "  ");
    if (!ok) err = string("#16002: failed to save bundle to file: ") + filename;
}

void VulDesign::_saveCombineToFile(const string &filename, const VulCombine &vc, string &err) {
    err.clear();
    pugi::xml_document doc;
    pugi::xml_node root = doc.append_child("combine");
    root.append_child("name").text().set(vc.name.c_str());
    if (!vc.comment.empty()) root.append_child("comment").text().set(vc.comment.c_str());

    // config entries
    for (const VulConfig &cfg : vc.config) {
        pugi::xml_node cn = root.append_child("config");
        cn.append_child("name").text().set(cfg.name.c_str());
        cn.append_child("ref").text().set(cfg.ref.c_str());
        if (!cfg.comment.empty()) cn.append_child("comment").text().set(cfg.comment.c_str());
    }

    // pipein
    for (const VulPipePort &p : vc.pipein) {
        pugi::xml_node pn = root.append_child("pipein");
        pn.append_child("name").text().set(p.name.c_str());
        pn.append_child("type").text().set(p.type.c_str());
        if (!p.comment.empty()) pn.append_child("comment").text().set(p.comment.c_str());
    }
    // pipeout
    for (const VulPipePort &p : vc.pipeout) {
        pugi::xml_node pn = root.append_child("pipeout");
        pn.append_child("name").text().set(p.name.c_str());
        pn.append_child("type").text().set(p.type.c_str());
        if (!p.comment.empty()) pn.append_child("comment").text().set(p.comment.c_str());
    }

    // requests
    for (const VulRequest &r : vc.request) {
        pugi::xml_node rn = root.append_child("request");
        if (!r.name.empty()) rn.append_child("name").text().set(r.name.c_str());
        if (!r.comment.empty()) rn.append_child("comment").text().set(r.comment.c_str());
        for (const VulArgument &a : r.arg) {
            pugi::xml_node an = rn.append_child("arg");
            an.append_child("name").text().set(a.name.c_str());
            an.append_child("type").text().set(a.type.c_str());
            if (!a.comment.empty()) an.append_child("comment").text().set(a.comment.c_str());
        }
        for (const VulArgument &rt : r.ret) {
            pugi::xml_node ret = rn.append_child("return");
            ret.append_child("name").text().set(rt.name.c_str());
            ret.append_child("type").text().set(rt.type.c_str());
            if (!rt.comment.empty()) ret.append_child("comment").text().set(rt.comment.c_str());
        }
    }

    // services
    for (const VulService &s : vc.service) {
        pugi::xml_node sn = root.append_child("service");
        if (!s.name.empty()) sn.append_child("name").text().set(s.name.c_str());
        if (!s.comment.empty()) sn.append_child("comment").text().set(s.comment.c_str());
        for (const VulArgument &a : s.arg) {
            pugi::xml_node an = sn.append_child("arg");
            an.append_child("name").text().set(a.name.c_str());
            an.append_child("type").text().set(a.type.c_str());
            if (!a.comment.empty()) an.append_child("comment").text().set(a.comment.c_str());
        }
        for (const VulArgument &rt : s.ret) {
            pugi::xml_node ret = sn.append_child("return");
            ret.append_child("name").text().set(rt.name.c_str());
            ret.append_child("type").text().set(rt.type.c_str());
            if (!rt.comment.empty()) ret.append_child("comment").text().set(rt.comment.c_str());
        }
        if (!s.cppfunc.code.empty() || !s.cppfunc.file.empty() || !s.cppfunc.name.empty()) {
            pugi::xml_node cpp = sn.append_child("cppfunc");
            if (!s.cppfunc.code.empty()) cpp.append_child("code").text().set(s.cppfunc.code.c_str());
            if (!s.cppfunc.file.empty()) cpp.append_child("file").text().set(s.cppfunc.file.c_str());
            if (!s.cppfunc.name.empty()) cpp.append_child("name").text().set(s.cppfunc.name.c_str());
        }
    }

    // storage lists
    auto writeStorageList = [&](const char *tag, const vector<VulStorage> &lst) {
        for (const VulStorage &st : lst) {
            pugi::xml_node sn = root.append_child(tag);
            sn.append_child("name").text().set(st.name.c_str());
            sn.append_child("type").text().set(st.type.c_str());
            if (!st.value.empty()) sn.append_child("value").text().set(st.value.c_str());
            if (!st.comment.empty()) sn.append_child("comment").text().set(st.comment.c_str());
        }
    };

    writeStorageList("storage", vc.storage);
    writeStorageList("storagenext", vc.storagenext);
    writeStorageList("storagetick", vc.storagetick);

    if (vc.stallable) root.append_child("stallable");

    auto writeCppFunc = [&](const char *tag, const VulCppFunc &cf) {
        if (cf.code.empty() && cf.file.empty() && cf.name.empty()) return;
        pugi::xml_node tn = root.append_child(tag);
        pugi::xml_node cpp = tn.append_child("cppfunc");
        if (!cf.code.empty()) cpp.append_child("code").text().set(cf.code.c_str());
        if (!cf.file.empty()) cpp.append_child("file").text().set(cf.file.c_str());
        if (!cf.name.empty()) cpp.append_child("name").text().set(cf.name.c_str());
    };

    writeCppFunc("tick", vc.tick);
    writeCppFunc("applytick", vc.applytick);
    writeCppFunc("init", vc.init);

    bool ok = doc.save_file(filename.c_str(), "  ");
    if (!ok) err = string("#16003: failed to save combine to file: ") + filename;
}

void VulDesign::_saveDesignToFile(const string &filename, string &err) {
    err.clear();
    pugi::xml_document doc;
    pugi::xml_node root = doc.append_child("design");

    // instances
    for (const auto &kv : instances) {
        const VulInstance &vi = kv.second;
        pugi::xml_node in = root.append_child("instance");
        in.append_child("name").text().set(vi.name.c_str());
        in.append_child("combine").text().set(vi.combine.c_str());
        if (!vi.comment.empty()) in.append_child("comment").text().set(vi.comment.c_str());
        for (const auto &co : vi.config_override) {
            pugi::xml_node cn = in.append_child("config");
            cn.append_child("name").text().set(co.first.c_str());
            cn.append_child("value").text().set(co.second.c_str());
        }
        writeVisualNode(in, vi.vis);
    }

    // pipes
    for (const auto &kv : pipes) {
        const VulPipe &vp = kv.second;
        pugi::xml_node pn = root.append_child("pipe");
        pn.append_child("name").text().set(vp.name.c_str());
        pn.append_child("type").text().set(vp.type.c_str());
        if (!vp.comment.empty()) pn.append_child("comment").text().set(vp.comment.c_str());
        pn.append_child("inputsize").text().set(std::to_string(vp.inputsize).c_str());
        pn.append_child("outputsize").text().set(std::to_string(vp.outputsize).c_str());
        pn.append_child("buffersize").text().set(std::to_string(vp.buffersize).c_str());
        writeVisualNode(pn, vp.vis);
    }

    // reqconn
    for (const VulReqConnection &rc : req_connections) {
        pugi::xml_node rcn = root.append_child("reqconn");
        pugi::xml_node rp = rcn.append_child("reqport");
        rp.append_child("instname").text().set(rc.req_instance.c_str());
        rp.append_child("portname").text().set(rc.req_name.c_str());
        pugi::xml_node sn = rcn.append_child("servport");
        sn.append_child("instname").text().set(rc.serv_instance.c_str());
        sn.append_child("portname").text().set(rc.serv_name.c_str());
        writeVisualNode(rcn, rc.vis);
    }

    // modpipe
    for (const VulModulePipeConnection &mp : mod_pipe_connections) {
        pugi::xml_node mn = root.append_child("modpipe");
        mn.append_child("instname").text().set(mp.instance.c_str());
        mn.append_child("portname").text().set(mp.pipeoutport.c_str());
        mn.append_child("pipename").text().set(mp.pipe.c_str());
        mn.append_child("portindex").text().set(std::to_string(mp.portindex).c_str());
        writeVisualNode(mn, mp.vis);
    }

    // pipemod
    for (const VulPipeModuleConnection &pm : pipe_mod_connections) {
        pugi::xml_node pn = root.append_child("pipemod");
        pn.append_child("instname").text().set(pm.instance.c_str());
        pn.append_child("portname").text().set(pm.pipeinport.c_str());
        pn.append_child("pipename").text().set(pm.pipe.c_str());
        pn.append_child("portindex").text().set(std::to_string(pm.portindex).c_str());
        writeVisualNode(pn, pm.vis);
    }

    // stalledconn
    for (const VulStalledConnection &sc : stalled_connections) {
        pugi::xml_node sn = root.append_child("stalledconn");
        pugi::xml_node src = sn.append_child("src");
        src.append_child("instname").text().set(sc.src_instance.c_str());
        pugi::xml_node dest = sn.append_child("dest");
        dest.append_child("instname").text().set(sc.dest_instance.c_str());
        writeVisualNode(sn, sc.vis);
    }

    // update constraints (tag used in load: updconstraint)
    for (const VulUpdateSequence &us : update_constraints) {
        pugi::xml_node un = root.append_child("updconstraint");
        pugi::xml_node former = un.append_child("former");
        former.append_child("instname").text().set(us.former_instance.c_str());
        pugi::xml_node later = un.append_child("later");
        later.append_child("instname").text().set(us.latter_instance.c_str());
        writeVisualNode(un, us.vis);
    }

    // blocks
    for (const VulVisBlock &vb : vis_blocks) {
        pugi::xml_node bn = root.append_child("block");
        bn.append_child("name").text().set(vb.name.c_str());
        writeVisualNode(bn, vb.vis);
    }

    // texts
    for (const VulVisText &vt : vis_texts) {
        pugi::xml_node tn = root.append_child("text");
        tn.append_child("text").text().set(vt.text.c_str());
        writeVisualNode(tn, vt.vis);
    }

    bool ok = doc.save_file(filename.c_str(), "  ");
    if (!ok) err = string("#16004: failed to save design to file: ") + filename;
}

string VulDesign::saveProject() {
    namespace fs = std::filesystem;
    string err;

    if (project_dir.empty()) return string("#16005: project_dir is empty");
    fs::path proj(project_dir);
    if (!fs::exists(proj) || !fs::is_directory(proj)) return string("#16005: project dir does not exist: ") + project_dir;

    // create bak directory with timestamp to avoid collisions
    std::time_t t = std::time(nullptr);
    string bakName = string("bak_") + std::to_string((long long)t);
    fs::path bakDir = proj / bakName;

    try {
        fs::create_directories(bakDir);
    } catch (const std::exception &e) {
        return string("#16006: failed to create bak directory: ") + e.what();
    }

    // prepare subdirs
    if (dirty_bundles) {
        try { fs::create_directories(bakDir / "bundle"); } catch (const std::exception &e) { fs::remove_all(bakDir); return string("#16006: failed to create bak bundle dir: ") + e.what(); }
    }
    if (dirty_combines) {
        try { fs::create_directories(bakDir / "combine"); } catch (const std::exception &e) { fs::remove_all(bakDir); return string("#16006: failed to create bak combine dir: ") + e.what(); }
    }

    // Save version info into {projectname}.vul XML file
    {
        pugi::xml_document doc;
        pugi::xml_node root = doc.append_child("project");
        root.append_child("version").text().set((std::to_string(std::get<0>(version)) + "." + std::to_string(std::get<1>(version)) + "." + std::to_string(std::get<2>(version))).c_str());
        string vulFileName = (bakDir / (project_name + ".vul")).string();
        doc.save_file(vulFileName.c_str());
    }

    // Always save design.xml (top-level)
    _saveDesignToFile((bakDir / "design.xml").string(), err);
    if (!err.empty()) { fs::remove_all(bakDir); return err; }

    // Save config lib if dirty
    if (dirty_config_lib) {
        _saveConfigLibToFile((bakDir / "config.xml").string(), err);
        if (!err.empty()) { fs::remove_all(bakDir); return err; }
    }

    // Save all bundles if dirty (one file per bundle)
    if (dirty_bundles) {
        for (const auto &kv : bundles) {
            const string fname = (bakDir / "bundle" / (kv.first + ".xml")).string();
            _saveBundleToFile(fname, kv.second, err);
            if (!err.empty()) { fs::remove_all(bakDir); return err; }
        }
    }

    // Save all combines if dirty
    if (dirty_combines) {
        for (const auto &kv : combines) {
            const string fname = (bakDir / "combine" / (kv.first + ".xml")).string();
            _saveCombineToFile(fname, kv.second, err);
            if (!err.empty()) { fs::remove_all(bakDir); return err; }
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
        return string("#16007: failed to replace project files: ") + e.what();
    }

    return string();
}

