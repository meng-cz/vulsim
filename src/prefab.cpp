
#include "prefab.h"
#include "pugixml.hpp"
#include <filesystem>
#include <system_error>

unique_ptr<VulPrefabMetaData> loadPrefabFromFile(const string &filename, string &err) {
    pugi::xml_document doc;
    pugi::xml_parse_result res = doc.load_file(filename.c_str());
    if (!res) {
        err = string("failed to parse XML: ") + res.description();
        return nullptr;
    }

    pugi::xml_node root = doc.child("prefab");
    if (!root) {
        // try document element
        root = doc.document_element();
        if (!root || string(root.name()) != "prefab") {
        err = "no <prefab> root element";
        return nullptr;
        }
    }

    auto meta = std::make_unique<VulPrefabMetaData>();

    // helpers
    auto child_text = [&](const pugi::xml_node &n, const char *name) {
        pugi::xml_node c = n.child(name);
        if (c) return string(c.child_value());
        return string();
    };

    // parse basic fields
    meta->name = child_text(root, "name");
    meta->comment = child_text(root, "comment");

    // parse config entries
    for (pugi::xml_node cfg : root.children("config")) {
        VulConfig c;
        c.name = child_text(cfg, "name");
        c.comment = child_text(cfg, "comment");
        // optional value
        pugi::xml_node value = cfg.child("value");
        if (value) c.value = string(value.child_value());
        meta->config.push_back(std::move(c));
    }

    // parse pipein
    for (pugi::xml_node p : root.children("pipein")) {
        VulPipePort port;
        port.name = child_text(p, "name");
        port.type = child_text(p, "type");
        port.comment = child_text(p, "comment");
        meta->pipein.push_back(std::move(port));
    }

    // parse pipeout
    for (pugi::xml_node p : root.children("pipeout")) {
        VulPipePort port;
        port.name = child_text(p, "name");
        port.type = child_text(p, "type");
        port.comment = child_text(p, "comment");
        meta->pipeout.push_back(std::move(port));
    }

    // helper to parse argument nodes
    auto parseArgNode = [&](const pugi::xml_node &argn) {
        VulArgument a;
        a.name = child_text(argn, "name");
        a.type = child_text(argn, "type");
        a.comment = child_text(argn, "comment");
        return a;
    };

    // parse requests
    for (pugi::xml_node r : root.children("request")) {
        VulRequest req;
        req.name = child_text(r, "name");
        req.comment = child_text(r, "comment");

        for (pugi::xml_node an : r.children("arg")) {
        req.arg.push_back(parseArgNode(an));
        }
        for (pugi::xml_node ret : r.children("return")) {
        req.ret.push_back(parseArgNode(ret));
        }

        meta->request.push_back(std::move(req));
    }

    // parse services
    for (pugi::xml_node s : root.children("service")) {
        VulService serv;
        serv.name = child_text(s, "name");
        serv.comment = child_text(s, "comment");

        for (pugi::xml_node an : s.children("arg")) {
        serv.arg.push_back(parseArgNode(an));
        }
        for (pugi::xml_node ret : s.children("return")) {
        serv.ret.push_back(parseArgNode(ret));
        }

        meta->service.push_back(std::move(serv));
    }

    return meta;
}

/**
 * @brief Load all prefab metadata from XML files recursively in a directory.
 * @param dir The directory to scan for prefab XML files.
 * @param err Error message in case of failure.
 * @return A unique_ptr to the loaded VulPrefabMetaDataMap, or nullptr on failure.
 */
unique_ptr<VulPrefabMetaDataMap> loadPrefabsFromDirectory(const string &dir, string &err) {
    namespace fs = std::filesystem;

    std::error_code ec;
    fs::path root(dir);

    if (!fs::exists(root, ec)) {
        err = "directory does not exist: " + dir;
        return nullptr;
    }

    if (!fs::is_directory(root, ec)) {
        err = "not a directory: " + dir;
        return nullptr;
    }

    auto map_ptr = std::make_unique<VulPrefabMetaDataMap>();

    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) {
            err = "error while iterating directory: " + ec.message();
            return nullptr;
        }

        const fs::directory_entry &entry = *it;

        if (!entry.is_regular_file()) continue;

        if (entry.path().extension() == ".xml") {
            string local_err;
            // call loader (the loader returns a reference we copy)
            try {
                unique_ptr<VulPrefabMetaData> meta = loadPrefabFromFile(entry.path().string(), local_err);

                if (!local_err.empty()) {
                    err = entry.path().string() + ": " + local_err;
                    return nullptr;
                }

                string key = meta->name;
                if (key.empty()) {
                    err = "prefab missing name: " + entry.path().string();
                    return nullptr;
                }

                // check duplicate
                if (map_ptr->find(key) != map_ptr->end()) {
                    err = "duplicate prefab name: " + key + " (file: " + entry.path().string() + ")";
                    return nullptr;
                }

                (*map_ptr)[key] = *meta;
            } catch (const std::exception &e) {
                err = string("exception while loading file ") + entry.path().string() + ": " + e.what();
                return nullptr;
            } catch (...) {
                err = string("unknown error while loading file ") + entry.path().string();
                return nullptr;
            }
        }
    }

    return map_ptr;
}




