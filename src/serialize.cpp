/*
 * Copyright (c) 2025 Meng-CZ
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "serialize.h"
#include "pugixml.hpp"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;
using std::to_string;

/**
 * @brief Parse project info from an XML file.
 * @param filename The name of the XML file to load.
 * @param vp The VulProject object to populate.
 * @return An error message string, empty on success.
 */
string serializeParseProjectInfoFromFile(const string &filename, VulProject &vp) {
    pugi::xml_document doc;
    pugi::xml_parse_result res = doc.load_file(filename.c_str());
    if (!res) {
        return res.description();
    }

    pugi::xml_node root = doc.child("project");
    if (!root) {
        return string("Missing root <project> in file: ") + filename;
    }

    fs::path filePath(filename);
    vp.project_name = filePath.stem().string();
    vp.project_dir = filePath.parent_path().string();

    vp.version = {0, 0, 0};
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
                vp.version = {major, minor, patch};
            } catch (...) {
                // ignore parse errors, keep default version
            }
        }
    }

    for (pugi::xml_node prefabNode : root.children("prefab")) {
        pugi::xml_node nameNode = prefabNode.child("name");
        pugi::xml_node pathNode = prefabNode.child("path");
        string name = nameNode.text().as_string();
        string path = "";
        if (pathNode) path = pathNode.text().as_string();
        if (path.empty()) {
            fs::path def = "prefabs";
            path = (def / name).string();
        }
        vp.prefabs.push_back({name, path});
    }

    return string("");
}

/**
 * @brief Save project info to an XML file.
 * @param filename The name of the XML file to save.
 * @param vp The VulProject object to serialize.
 * @return An error message string, empty on success.
 */
string serializeSaveProjectInfoToFile(const string &filename, const VulProject &vp) {
    pugi::xml_document doc;
    pugi::xml_node root = doc.append_child("project");

    // version
    int major = 0, minor = 0, patch = 0;
    try {
        major = std::get<0>(vp.version);
        minor = std::get<1>(vp.version);
        patch = std::get<2>(vp.version);
    } catch(...) {
        // ignore, keep zeros
    }
    pugi::xml_node ver = root.append_child("version");
    ver.append_child(pugi::node_pcdata).text().set((to_string(major) + "." + to_string(minor) + "." + to_string(patch)).c_str());

    // prefabs
    for (const auto &p : vp.prefabs) {
        pugi::xml_node prefab = root.append_child("prefab");
        prefab.append_child("name").text().set(p.first.c_str());
        prefab.append_child("path").text().set(p.second.c_str());
    }

    // save to file
    bool ok = doc.save_file(filename.c_str(), PUGIXML_TEXT("\t"));
    if (!ok) {
        return string("Failed to write project file: ") + filename;
    }
    return string("");
}


/**
 * @brief Parse a VulBundle from an XML file.
 * @param filename The name of the XML file to load.
 * @param vb The VulBundle object to populate.
 * @return An error message string, empty on success.
 */
string serializeParseBundleFromFile(const string &filename, VulBundle &vb) {
    pugi::xml_document doc;
    pugi::xml_parse_result res = doc.load_file(filename.c_str());
    if (!res) {
        return res.description();
    }

    pugi::xml_node root = doc.child("bundle");
    if (!root) {
        return string("Missing root <bundle> in file: ") + filename;
    }

    pugi::xml_node nameNode = root.child("name");
    if (!nameNode) {
        return string("bundle missing <name> in file: ") + filename;
    }

    vb.name = nameNode.text().as_string();
    pugi::xml_node commentNode = root.child("comment");
    vb.comment = commentNode ? commentNode.text().as_string() : string();

    if (vb.name.empty()) {
        return string("bundle has empty <name> in file: ") + filename;
    }

    // parse members
    for (pugi::xml_node m : root.children("member")) {
        pugi::xml_node mtype = m.child("type");
        pugi::xml_node mname = m.child("name");
        if (!mtype || !mname) {
            return string("member missing <type> or <name> in bundle file: ") + filename;
        }

        VulArgument va;
        va.type = mtype.text().as_string();
        va.name = mname.text().as_string();
        pugi::xml_node mcomment = m.child("comment");
        va.comment = mcomment ? mcomment.text().as_string() : string();

        if (va.name.empty()) {
            return string("member has empty <name> in bundle file: ") + filename;
        }

        vb.members.push_back(va);
    }
    return string("");
}

/**
 * @brief Save a VulBundle to an XML file.
 * @param filename The name of the XML file to save.
 * @param vb The VulBundle object to serialize.
 * @return An error message string, empty on success.
 */
string serializeSaveBundleToFile(const string &filename, const VulBundle &vb) {
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
    if (!ok) return string("failed to save bundle to file: ") + filename;
    return string("");
}


/**
 * @brief Parse VulConfigLib from an XML file.
 * @param filename The name of the XML file to load.
 * @param config_lib The unordered_map to populate with VulConfigItem objects.
 * @return An error message string, empty on success.
 */
string serializeParseConfigLibFromFile(const string &filename, unordered_map<string, VulConfigItem> &config_lib) {
    pugi::xml_document doc;
    pugi::xml_parse_result res = doc.load_file(filename.c_str());
    if (!res) {
        return string("Failed to load config file: ") + filename + "; parser error: " + res.description();
    }

    pugi::xml_node root = doc.child("configlib");
    if (!root) {
        return string("Missing root <configlib> in file: ") + filename;
    }

    for (pugi::xml_node item : root.children("configitem")) {
        pugi::xml_node nameNode = item.child("name");
        pugi::xml_node valueNode = item.child("value");
        pugi::xml_node commentNode = item.child("comment");

        if (!nameNode || !valueNode) {
            return string("configitem missing <name> or <value> in file: ") + filename;
        }

        string name = nameNode.text().as_string();
        string value = valueNode.text().as_string();
        string comment = commentNode ? commentNode.text().as_string() : string();

        if (name.empty()) {
            return string("configitem has empty <name> in file: ") + filename;
        }

        VulConfigItem ci;
        ci.name = name;
        ci.value = value;
        ci.comment = comment;

        config_lib[ci.name] = ci;
    }
    return string("");
}

/**
 * @brief Save VulConfigLib to an XML file.
 * @param filename The name of the XML file to save.
 * @param config_lib The unordered_map to serialize.
 * @return An error message string, empty on success.
 */
string serializeSaveConfigLibToFile(const string &filename, const unordered_map<string, VulConfigItem> &config_lib) {
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
    if (!ok) return string("failed to save configlib to file: ") + filename;
    return string("");
}

/**
 * @brief Parse VulCombine from an XML file.
 * @param filename The name of the XML file to load.
 * @param vc The VulCombine object to populate.
 * @return An error message string, empty on success.
 */
string serializeParseCombineFromFile(const string &filename, VulCombine &vc) {
    pugi::xml_document doc;
    pugi::xml_parse_result res = doc.load_file(filename.c_str());
    if (!res) {
        return string("Failed to load combine file: ") + filename + "; parser error: " + res.description();
    }

    pugi::xml_node root = doc.child("combine");
    if (!root) {
        return string("Missing root <combine> in file: ") + filename;
    }

    pugi::xml_node nameNode = root.child("name");
    if (!nameNode) {
        return string("combine missing <name> in file: ") + filename;
    }

    vc.name = nameNode.text().as_string();
    pugi::xml_node commentNode = root.child("comment");
    vc.comment = commentNode ? commentNode.text().as_string() : string();

    if (vc.name.empty()) {
        return string("combine has empty <name> in file: ") + filename;
    }

    // config entries
    for (pugi::xml_node c : root.children("config")) {
        pugi::xml_node cname = c.child("name");
        if (!cname) {
            return string("config missing <name> in combine file: ") + filename;
        }
        VulConfig cfg;
        cfg.name = cname.text().as_string();
        pugi::xml_node cvalue = c.child("value");
        cfg.value = cvalue ? cvalue.text().as_string() : string();
        extractConfigReferences(cfg); // populate cfg.references
        pugi::xml_node ccomment = c.child("comment");
        cfg.comment = ccomment ? ccomment.text().as_string() : string();
        if (cfg.name.empty()) {
            return string("config has empty <name> in combine file: ") + filename;
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
                return string("request arg missing <name> or <type> in combine file: ") + filename;
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
                return string("request return missing <name> or <type> in combine file: ") + filename;
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
                return string("service arg missing <name> or <type> in combine file: ") + filename;
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
                return string("service return missing <name> or <type> in combine file: ") + filename;
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
    auto parseStorageList = [&](const char *tag, vector<VulStorage> &out) -> string {
        for (pugi::xml_node st : root.children(tag)) {
            pugi::xml_node sname = st.child("name");
            pugi::xml_node stype = st.child("type");
            if (!sname || !stype) {
                return string("missing <name> or <type> in combine file: ") + filename;
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
        return string("");
    };

    string e1 = parseStorageList("storage", vc.storage);
    string e2 = parseStorageList("storagenext", vc.storagenext);
    string e3 = parseStorageList("storagetick", vc.storagetick);
    if (!e1.empty()) {
        e1 = string("storage parse error: ") + e1;
        return e1;
    }
    if (!e2.empty()) {
        e2 = string("storagenext parse error: ") + e2;
        return e2;
    }
    if (!e3.empty()) {
        e3 = string("storagetick parse error: ") + e3;
        return e3;
    }

    // storagenextarray / storagearray(un-implemented)
    auto parseStorageArrayList = [&](const char *tag, vector<VulStorageArray> &out) -> string {
        for (pugi::xml_node st : root.children(tag)) {
            pugi::xml_node sname = st.child("name");
            pugi::xml_node stype = st.child("type");
            pugi::xml_node ssize = st.child("size");
            if (!sname || !stype || !ssize) {
                return string("missing <name> or <type> or <size> in combine file: ") + filename;
            }
            VulStorageArray vsa;
            vsa.name = sname.text().as_string();
            vsa.type = stype.text().as_string();
            vsa.size = ssize.text().as_string();
            pugi::xml_node sval = st.child("value");
            vsa.value = sval ? sval.text().as_string() : string();
            pugi::xml_node scomment = st.child("comment");
            vsa.comment = scomment ? scomment.text().as_string() : string();
            out.push_back(vsa);
        }
        return string("");
    };

    string e4 = parseStorageArrayList("storagenextarray", vc.storagenextarray);
    if (!e4.empty()) {
        e4 = string("storagenextarray parse error: ") + e4;
        return e4;
    }

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

    return string("");
}

/**
 * @brief Save VulCombine to an XML file.
 * @param filename The name of the XML file to save.
 * @param vc The VulCombine object to serialize.
 * @return An error message string, empty on success.
 */
string serializeSaveCombineToFile(const string &filename, const VulCombine &vc) {
    pugi::xml_document doc;
    pugi::xml_node root = doc.append_child("combine");
    root.append_child("name").text().set(vc.name.c_str());
    if (!vc.comment.empty()) root.append_child("comment").text().set(vc.comment.c_str());

    // config entries
    for (const VulConfig &cfg : vc.config) {
        pugi::xml_node cn = root.append_child("config");
        cn.append_child("name").text().set(cfg.name.c_str());
        cn.append_child("value").text().set(cfg.value.c_str());
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

    // storage array lists
    auto writeStorageArrayList = [&](const char *tag, const vector<VulStorageArray> &lst) {
        for (const VulStorageArray &st : lst) {
            pugi::xml_node sn = root.append_child(tag);
            sn.append_child("name").text().set(st.name.c_str());
            sn.append_child("type").text().set(st.type.c_str());
            sn.append_child("size").text().set(st.size.c_str());
            if (!st.value.empty()) sn.append_child("value").text().set(st.value.c_str());
            if (!st.comment.empty()) sn.append_child("comment").text().set(st.comment.c_str());
        }
    };
    
    writeStorageArrayList("storagenextarray", vc.storagenextarray);

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
    if (!ok) return string("failed to save combine to file: ") + filename;
    return string("");
}

/**
 * @brief Parse VulPrefab from an XML file.
 * @param filename The name of the XML file to load.
 * @param prefab The VulPrefab object to populate.
 * @return An error message string, empty on success.
 */
string serializeParsePrefabFromFile(const string &filename, VulPrefab &prefab) {
    fs::path filePath(filename);
    prefab.path = filePath.parent_path().string();
    prefab.name = filePath.stem().string();

    pugi::xml_document doc;
    pugi::xml_parse_result res = doc.load_file(filename.c_str());
    if (!res) {
        return string("Failed to load prefab file: ") + filename + "; parser error: " + res.description();
    }

    pugi::xml_node root = doc.child("prefab");
    if (!root) {
        return string("Missing root <prefab> in file: ") + filename;
    }

    pugi::xml_node commentNode = root.child("comment");
    prefab.comment = commentNode ? commentNode.text().as_string() : string();

    // load configs
    for (pugi::xml_node c : root.children("config")) {
        pugi::xml_node cname = c.child("name");
        if (!cname) {
            return string("Missing <name> in config file: ") + filename;
        }
        VulConfig cfg;
        cfg.name = cname.text().as_string();
        pugi::xml_node cvalue = c.child("value");
        cfg.value = cvalue ? cvalue.text().as_string() : string();
        extractConfigReferences(cfg); // populate cfg.references
        pugi::xml_node ccomment = c.child("comment");
        cfg.comment = ccomment ? ccomment.text().as_string() : string();
        if (cfg.name.empty()) {
            return string("Missing <name> in config file: ") + filename;
        }
        prefab.config.push_back(cfg);
    }

    // load pipein and pipeout
    for (pugi::xml_node p : root.children("pipein")) {
        pugi::xml_node pname = p.child("name");
        pugi::xml_node ptype = p.child("type");
        pugi::xml_node pcomment = p.child("comment");
        VulPipePort vpp;
        vpp.name = pname ? pname.text().as_string() : string();
        vpp.type = ptype ? ptype.text().as_string() : string();
        vpp.comment = pcomment ? pcomment.text().as_string() : string();
        prefab.pipein.push_back(vpp);
    }
    for (pugi::xml_node p : root.children("pipeout")) {
        pugi::xml_node pname = p.child("name");
        pugi::xml_node ptype = p.child("type");
        pugi::xml_node pcomment = p.child("comment");
        VulPipePort vpp;
        vpp.name = pname ? pname.text().as_string() : string();
        vpp.type = ptype ? ptype.text().as_string() : string();
        vpp.comment = pcomment ? pcomment.text().as_string() : string();
        prefab.pipeout.push_back(vpp);
    }

    // load requests
    
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
                return string("Missing <name> or <type> in request arg file: ") + filename;
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
                return string("Missing <name> or <type> in request return file: ") + filename;
            }
            VulArgument va;
            va.name = rname2.text().as_string();
            va.type = rtype2.text().as_string();
            pugi::xml_node rcomment2 = ret.child("comment");
            va.comment = rcomment2 ? rcomment2.text().as_string() : string();
            vr.ret.push_back(va);
        }

        prefab.request.push_back(vr);
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
                return string("Missing <name> or <type> in service arg file: ") + filename;
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
                return string("Missing <name> or <type> in service return file: ") + filename;
            }
            VulArgument va;
            va.name = rname2.text().as_string();
            va.type = rtype2.text().as_string();
            pugi::xml_node rcomment2 = ret.child("comment");
            va.comment = rcomment2 ? rcomment2.text().as_string() : string();
            vs.ret.push_back(va);
        }

        prefab.service.push_back(vs);
    }

    return string("");
}

/**
 * @brief Save VulPrefab to an XML file.
 * @param filename The name of the XML file to save.
 * @param prefab The VulPrefab object to serialize.
 * @return An error message string, empty on success.
 */
string serializeSavePrefabToFile(const string &filename, const VulPrefab &prefab) {
    pugi::xml_document doc;
    pugi::xml_node root = doc.append_child("prefab");
    root.append_child("name").text().set(prefab.name.c_str());
    if (!prefab.comment.empty()) root.append_child("comment").text().set(prefab.comment.c_str());

    // configs
    for (const VulConfig &c : prefab.config) {
        pugi::xml_node cn = root.append_child("config");
        cn.append_child("name").text().set(c.name.c_str());
        cn.append_child("value").text().set(c.value.c_str());
        if (!c.comment.empty()) cn.append_child("comment").text().set(c.comment.c_str());
    }

    // pipein
    for (const VulPipePort &p : prefab.pipein) {
        pugi::xml_node pn = root.append_child("pipein");
        pn.append_child("name").text().set(p.name.c_str());
        pn.append_child("type").text().set(p.type.c_str());
        if (!p.comment.empty()) pn.append_child("comment").text().set(p.comment.c_str());
    }

    // pipeout
    for (const VulPipePort &p : prefab.pipeout) {
        pugi::xml_node pn = root.append_child("pipeout");
        pn.append_child("name").text().set(p.name.c_str());
        pn.append_child("type").text().set(p.type.c_str());
        if (!p.comment.empty()) pn.append_child("comment").text().set(p.comment.c_str());
    }

    // requests
    for (const VulRequest &r : prefab.request) {
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
    for (const VulService &s : prefab.service) {
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
    }

    // prefab has no visualization member
    bool ok = doc.save_file(filename.c_str(), "  ");
    if (!ok) return string("failed to save prefab to file: ") + filename;
    return string("");
}


// Helper to write visualization node
static void _serializeWriteVisualNode(pugi::xml_node parent, const VulVisualization &vis) {
    // pugi::xml_node vnode = parent.append_child("visual");
    // vnode.append_child("x").text().set(std::to_string(vis.x).c_str());
    // vnode.append_child("y").text().set(std::to_string(vis.y).c_str());
    // vnode.append_child("w").text().set(std::to_string(vis.w).c_str());
    // vnode.append_child("h").text().set(std::to_string(vis.h).c_str());
    // vnode.append_child("visible").text().set(vis.visible ? "1" : "0");
    // vnode.append_child("enabled").text().set(vis.enabled ? "1" : "0");
    // vnode.append_child("selected").text().set(vis.selected ? "1" : "0");
    // if (!vis.color.empty()) vnode.append_child("color").text().set(vis.color.c_str());
    // if (!vis.bordercolor.empty()) vnode.append_child("bordercolor").text().set(vis.bordercolor.c_str());
    // vnode.append_child("borderwidth").text().set(std::to_string(vis.borderwidth).c_str());
}

static void _serializeParseVisualNode(const pugi::xml_node &vnode, VulVisualization &vis) {
    if (!vnode) return;
    // pugi::xml_node n;
    // n = vnode.child("x"); vis.x = n ? (long)n.text().as_llong() : 0;
    // n = vnode.child("y"); vis.y = n ? (long)n.text().as_llong() : 0;
    // n = vnode.child("w"); vis.w = n ? (long)n.text().as_llong() : 0;
    // n = vnode.child("h"); vis.h = n ? (long)n.text().as_llong() : 0;

    // vis.visible = false;
    // vis.enabled = false;
    // vis.selected = false;
    // n = vnode.child("visible"); if (n) { string v = n.text().as_string(); vis.visible = (v=="1"||v=="true"||v=="True"); }
    // n = vnode.child("enabled"); if (n) { string v = n.text().as_string(); vis.enabled = (v=="1"||v=="true"||v=="True"); }
    // n = vnode.child("selected"); if (n) { string v = n.text().as_string(); vis.selected = (v=="1"||v=="true"||v=="True"); }

    // n = vnode.child("color"); vis.color = n ? n.text().as_string() : string();
    // n = vnode.child("bordercolor"); vis.bordercolor = n ? n.text().as_string() : string();
    // n = vnode.child("borderwidth"); vis.borderwidth = n ? (unsigned int)n.text().as_llong() : 0u;
}

/**
 * @brief Parse VulDesign from an XML file.
 * @param filename The name of the XML file to load.
 * @param instances The unordered_map to populate with VulInstance objects.
 * @param pipes The unordered_map to populate with VulPipe objects.
 * @param req_connections The vector to populate with VulReqConnection objects.
 * @param mod_pipe_connections The vector to populate with VulModulePipeConnection objects.
 * @param pipe_mod_connections The vector to populate with VulPipeModuleConnection objects.
 * @param stalled_connections The vector to populate with VulStalledConnection objects.
 * @param update_constraints The vector to populate with VulUpdateSequence objects.
 * @param vis_blocks The vector to populate with VulVisBlock objects.
 * @param vis_texts The vector to populate with VulVisText objects.
 * @return An error message string, empty on success.
 */
string serializeParseDesignFromFile(const string &filename,
    unordered_map<string, VulInstance>  &instances,
    unordered_map<string, VulPipe>      &pipes,
    vector<VulReqConnection>            &req_connections,
    vector<VulModulePipeConnection>     &mod_pipe_connections,
    vector<VulPipeModuleConnection>     &pipe_mod_connections,
    vector<VulStalledConnection>        &stalled_connections,
    vector<VulUpdateSequence>           &update_constraints,
    vector<VulVisBlock>                 &vis_blocks,
    vector<VulVisText>                  &vis_texts
) {
    pugi::xml_document doc;
    pugi::xml_parse_result res = doc.load_file(filename.c_str());
    if (!res) {
        return string("Failed to load design file: ") + filename + "; parser error: " + res.description();
    }

    pugi::xml_node root = doc.child("design");
    if (!root) {
        return string("Missing root <design> in file: ") + filename;
    }

    // instances
    for (pugi::xml_node inst : root.children("instance")) {
        pugi::xml_node nameNode = inst.child("name");
        pugi::xml_node combineNode = inst.child("combine");
        if (!nameNode || !combineNode) {
            return string("instance missing <name> or <combine> in design file: ") + filename;
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
                return string("instance config missing <name> or <value> in design file: ") + filename;
            }
            vi.config_override.emplace_back(cname.text().as_string(), cval.text().as_string());
        }

        // visual
        pugi::xml_node visNode = inst.child("visual");
        _serializeParseVisualNode(visNode, vi.vis);

        instances[vi.name] = vi;
    }

    // pipes
    for (pugi::xml_node pnode : root.children("pipe")) {
        pugi::xml_node nameNode = pnode.child("name");
        pugi::xml_node typeNode = pnode.child("type");
        if (!nameNode || !typeNode) {
            return string("pipe missing <name> or <type> in design file: ") + filename;
        }
        VulPipe vp;
        vp.name = nameNode.text().as_string();
        vp.type = typeNode.text().as_string();
        pugi::xml_node commentNode = pnode.child("comment");
        vp.comment = commentNode ? commentNode.text().as_string() : string();
        pugi::xml_node inNode = pnode.child("inputsize"); vp.inputsize = inNode ? (unsigned int)inNode.text().as_llong() : 1u;
        pugi::xml_node outNode = pnode.child("outputsize"); vp.outputsize = outNode ? (unsigned int)outNode.text().as_llong() : 1u;
        pugi::xml_node bufNode = pnode.child("buffersize"); vp.buffersize = bufNode ? (unsigned int)bufNode.text().as_llong() : 0u;
        pugi::xml_node hskNode = pnode.child("handshake"); vp.handshake = hskNode ? true : false;
        pugi::xml_node visNode = pnode.child("visual"); _serializeParseVisualNode(visNode, vp.vis);
        pipes[vp.name] = vp;
    }

    // reqconn
    for (pugi::xml_node rc : root.children("reqconn")) {
        pugi::xml_node reqport = rc.child("reqport");
        if (!reqport) {
            return string("reqconn missing <reqport> in design file: ") + filename;
        }
        pugi::xml_node instn = reqport.child("instname");
        pugi::xml_node portn = reqport.child("portname");
        if (!instn || !portn) {
            return string("reqport missing <instname> or <portname> in design file: ") + filename;
        }
        VulReqConnection vrc;
        vrc.req_instance = instn.text().as_string();
        vrc.req_name = portn.text().as_string();

        pugi::xml_node servport = rc.child("servport");
        if (!servport) {
            return string("reqconn missing <servport> in design file: ") + filename;
        }
        pugi::xml_node sinst = servport.child("instname");
        pugi::xml_node sport = servport.child("portname");
        if (!sinst || !sport) {
            return string("reqconn missing <instname> or <portname> in design file: ") + filename;
        }
        vrc.serv_instance = sinst.text().as_string();
        vrc.serv_name = sport.text().as_string();

        pugi::xml_node visNode = rc.child("visual"); _serializeParseVisualNode(visNode, vrc.vis);
        req_connections.push_back(vrc);
    }

    // modpipe and pipemod
    for (pugi::xml_node mp : root.children("modpipe")) {
        pugi::xml_node instn = mp.child("instname");
        pugi::xml_node portn = mp.child("portname");
        pugi::xml_node pipen = mp.child("pipename");
        pugi::xml_node pindex = mp.child("portindex");
        if (!instn || !portn || !pipen || !pindex) {
            return string("modpipe missing fields in design file: ") + filename;
        }
        VulModulePipeConnection vmp;
        vmp.instance = instn.text().as_string();
        vmp.pipeoutport = portn.text().as_string();
        vmp.pipe = pipen.text().as_string();
        vmp.portindex = (unsigned int)pindex.text().as_llong();
        pugi::xml_node visNode = mp.child("visual"); _serializeParseVisualNode(visNode, vmp.vis);
        mod_pipe_connections.push_back(vmp);
    }

    for (pugi::xml_node pm : root.children("pipemod")) {
        pugi::xml_node instn = pm.child("instname");
        pugi::xml_node portn = pm.child("portname");
        pugi::xml_node pipen = pm.child("pipename");
        pugi::xml_node pindex = pm.child("portindex");
        if (!instn || !portn || !pipen || !pindex) {
            return string("pipemod missing fields in design file: ") + filename;
        }
        VulPipeModuleConnection vpm;
        vpm.instance = instn.text().as_string();
        vpm.pipeinport = portn.text().as_string();
        vpm.pipe = pipen.text().as_string();
        vpm.portindex = (unsigned int)pindex.text().as_llong();
        pugi::xml_node visNode = pm.child("visual"); _serializeParseVisualNode(visNode, vpm.vis);
        pipe_mod_connections.push_back(vpm);
    }

    // stalledconn
    for (pugi::xml_node sc : root.children("stalledconn")) {
        pugi::xml_node src = sc.child("src");
        pugi::xml_node dest = sc.child("dest");
        if (!src || !dest) {
            return string("stalledconn missing <src> or <dest> in design file: ") + filename;
        }
        pugi::xml_node sname = src.child("instname");
        pugi::xml_node dname = dest.child("instname");
        if (!sname || !dname) {
            return string("stalledconn src/dest missing <instname> in design file: ") + filename;
        }
        VulStalledConnection vsc;
        vsc.src_instance = sname.text().as_string();
        vsc.dest_instance = dname.text().as_string();
        pugi::xml_node visNode = sc.child("visual"); _serializeParseVisualNode(visNode, vsc.vis);
        stalled_connections.push_back(vsc);
    }

    // update constraints
    for (pugi::xml_node uc : root.children("updconstraint")) {
        pugi::xml_node former = uc.child("former");
        pugi::xml_node later = uc.child("later");
        if (!former || !later) {
            return string("updconstraint missing <former> or <later> in design file: ") + filename;
        }
        pugi::xml_node fname = former.child("instname");
        pugi::xml_node lname = later.child("instname");
        if (!fname || !lname) {
            return string("updconstraint former/later missing <instname> in design file: ") + filename;
        }
        VulUpdateSequence vus;
        vus.former_instance = fname.text().as_string();
        vus.latter_instance = lname.text().as_string();
        pugi::xml_node visNode = uc.child("visual"); _serializeParseVisualNode(visNode, vus.vis);
        update_constraints.push_back(vus);
    }

    // blocks and texts
    for (pugi::xml_node b : root.children("block")) {
        pugi::xml_node nameNode = b.child("name");
        if (!nameNode) {
            return string("block missing <name> in design file: ") + filename;
        }
        VulVisBlock vb;
        vb.name = nameNode.text().as_string();
        pugi::xml_node visNode = b.child("visual"); _serializeParseVisualNode(visNode, vb.vis);
        vis_blocks.push_back(vb);
    }

    for (pugi::xml_node t : root.children("text")) {
        pugi::xml_node textNode = t.child("text");
        if (!textNode) {
            return string("text missing <text> in design file: ") + filename;
        }
        VulVisText vt;
        vt.text = textNode.text().as_string();
        pugi::xml_node visNode = t.child("visual"); _serializeParseVisualNode(visNode, vt.vis);
        vis_texts.push_back(vt);
    }

    return string("");
}

/**
 * @brief Save VulDesign to an XML file.
 * @param filename The name of the XML file to save.
 * @param instances The unordered_map of VulInstance objects to serialize.
 * @param pipes The unordered_map of VulPipe objects to serialize.
 * @param req_connections The vector of VulReqConnection objects to serialize.
 * @param mod_pipe_connections The vector of VulModulePipeConnection objects to serialize.
 * @param pipe_mod_connections The vector of VulPipeModuleConnection objects to serialize.
 * @param stalled_connections The vector of VulStalledConnection objects to serialize.
 * @param update_constraints The vector of VulUpdateSequence objects to serialize.
 * @param vis_blocks The vector of VulVisBlock objects to serialize.
 * @param vis_texts The vector of VulVisText objects to serialize.
 * @return An error message string, empty on success.
 */
string serializeSaveDesignToFile(const string &filename,
    const unordered_map<string, VulInstance>  &instances,
    const unordered_map<string, VulPipe>      &pipes,
    const vector<VulReqConnection>            &req_connections,
    const vector<VulModulePipeConnection>     &mod_pipe_connections,
    const vector<VulPipeModuleConnection>     &pipe_mod_connections,
    const vector<VulStalledConnection>        &stalled_connections,
    const vector<VulUpdateSequence>           &update_constraints,
    const vector<VulVisBlock>                 &vis_blocks,
    const vector<VulVisText>                  &vis_texts
) {
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
        _serializeWriteVisualNode(in, vi.vis);
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
        if (vp.handshake) pn.append_child("handshake");
        _serializeWriteVisualNode(pn, vp.vis);
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
        _serializeWriteVisualNode(rcn, rc.vis);
    }

    // modpipe
    for (const VulModulePipeConnection &mp : mod_pipe_connections) {
        pugi::xml_node mn = root.append_child("modpipe");
        mn.append_child("instname").text().set(mp.instance.c_str());
        mn.append_child("portname").text().set(mp.pipeoutport.c_str());
        mn.append_child("pipename").text().set(mp.pipe.c_str());
        mn.append_child("portindex").text().set(std::to_string(mp.portindex).c_str());
        _serializeWriteVisualNode(mn, mp.vis);
    }

    // pipemod
    for (const VulPipeModuleConnection &pm : pipe_mod_connections) {
        pugi::xml_node pn = root.append_child("pipemod");
        pn.append_child("instname").text().set(pm.instance.c_str());
        pn.append_child("portname").text().set(pm.pipeinport.c_str());
        pn.append_child("pipename").text().set(pm.pipe.c_str());
        pn.append_child("portindex").text().set(std::to_string(pm.portindex).c_str());
        _serializeWriteVisualNode(pn, pm.vis);
    }

    // stalledconn
    for (const VulStalledConnection &sc : stalled_connections) {
        pugi::xml_node sn = root.append_child("stalledconn");
        pugi::xml_node src = sn.append_child("src");
        src.append_child("instname").text().set(sc.src_instance.c_str());
        pugi::xml_node dest = sn.append_child("dest");
        dest.append_child("instname").text().set(sc.dest_instance.c_str());
        _serializeWriteVisualNode(sn, sc.vis);
    }

    // update constraints (tag used in load: updconstraint)
    for (const VulUpdateSequence &us : update_constraints) {
        pugi::xml_node un = root.append_child("updconstraint");
        pugi::xml_node former = un.append_child("former");
        former.append_child("instname").text().set(us.former_instance.c_str());
        pugi::xml_node later = un.append_child("later");
        later.append_child("instname").text().set(us.latter_instance.c_str());
        _serializeWriteVisualNode(un, us.vis);
    }

    // blocks
    for (const VulVisBlock &vb : vis_blocks) {
        pugi::xml_node bn = root.append_child("block");
        bn.append_child("name").text().set(vb.name.c_str());
        _serializeWriteVisualNode(bn, vb.vis);
    }

    // texts
    for (const VulVisText &vt : vis_texts) {
        pugi::xml_node tn = root.append_child("text");
        tn.append_child("text").text().set(vt.text.c_str());
        _serializeWriteVisualNode(tn, vt.vis);
    }

    bool ok = doc.save_file(filename.c_str(), "  ");
    if (!ok) return string("failed to save design to file: ") + filename;
    return string("");
}







