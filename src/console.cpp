#include "console.h"
#include "command.h"

#include <sstream>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include "codehelper.h"

/**
 * @brief Handle the 'show' command to display design information.
 * @param design The VulDesign object to query.
 * @param args The arguments passed to the 'show' command.
 * @return A unique_ptr to a vector<string> containing the output messages.
 */
unique_ptr<vector<string>> _consoleShow(VulDesign &design, vector<string> &args) {
    auto output = std::make_unique<vector<string>>();

    //      show                                    # 输出当前设计的全局摘要（bundles, combines, instances, pipes）
    //      show combine                            # 列出所有 combines
    //      show combine <combine-name>             # 显示指定 combine 的详细信息（ports, requests, services, storages, cpp funcs）
    //      show bundle                             # 列出所有 bundles
    //      show bundle <bundle-name>               # 显示 bundle 定义
    //      show instance                          # 列出所有实例及其所属 combine
    //      show pipe                               # 列出所有 pipes
    //      show config                             # 列出设计级 config 项

    auto toLower = [](const string &s) {
        string r = s;
        std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return std::tolower(c); });
        return r;
    };

    auto token = string();
    if (!args.empty()) token = toLower(args[0]);

    // normalize shorthand
    if (token == "c") token = "combine";
    if (token == "b") token = "bundle";
    if (token == "i") token = "instance";
    if (token == "p") token = "pipe";
    if (token == "cf") token = "config";

    // no args -> summary
    if (token.empty()) {
        output->push_back("Design summary:");
        output->push_back(string("  bundles: ") + std::to_string(design.bundles.size()));
        output->push_back(string("  combines: ") + std::to_string(design.combines.size()));
        output->push_back(string("  instances: ") + std::to_string(design.instances.size()));
        output->push_back(string("  pipes: ") + std::to_string(design.pipes.size()));
        output->push_back(string("  connections: ") + std::to_string(design.req_connections.size() + design.mod_pipe_connections.size() + design.pipe_mod_connections.size() + design.stalled_connections.size()));
        return output;
    }

    if (token == "combine" || token == "combines") {
        if (args.size() == 1) {
            output->push_back("Combines:");
            for (const auto &kv : design.combines) {
                output->push_back(string("  ") + kv.first);
            }
            return output;
        } else {
            string cname = args[1];
            auto it = design.combines.find(cname);
            if (it == design.combines.end()) {
                output->push_back(string("Error: combine '") + cname + "' not found");
                return output;
            }
            const VulCombine &vc = it->second;
            output->push_back(string("Combine: ") + vc.name);
            if (!vc.comment.empty()) { output->push_back(string("  comment: ") + vc.comment); }
            output->push_back(string("  stallable: ") + (vc.stallable ? "true" : "false"));

            output->push_back("  pipein:");
            if (vc.pipein.empty()) { output->push_back("    (none)"); }
            for (const VulPipePort &p : vc.pipein) {
                string s = string("    ") + p.name + " : " + p.type;
                if (!p.comment.empty()) s += string(" // ") + p.comment;
                output->push_back(s);
            }

            output->push_back("  pipeout:");
            if (vc.pipeout.empty()) { output->push_back("    (none)"); }
            for (const VulPipePort &p : vc.pipeout) {
                string s = string("    ") + p.name + " : " + p.type;
                if (!p.comment.empty()) s += string(" // ") + p.comment;
                output->push_back(s);
            }

            output->push_back("  requests:");
            if (vc.request.empty()) { output->push_back("    (none)"); }
            for (const VulRequest &r : vc.request) {
                string s = string("    ") + r.name + "(";
                for (size_t i = 0; i < r.arg.size(); ++i) { if (i) s += ", "; s += r.arg[i].type; }
                s += ") -> (";
                for (size_t i = 0; i < r.ret.size(); ++i) { if (i) s += ", "; s += r.ret[i].type; }
                s += ")";
                if (!r.comment.empty()) s += string(" // ") + r.comment;
                output->push_back(s);
            }

            output->push_back("  services:");
            if (vc.service.empty()) { output->push_back("    (none)"); }
            for (const VulService &s : vc.service) {
                string line_s = string("    ") + s.name + "(";
                for (size_t i = 0; i < s.arg.size(); ++i) { if (i) line_s += ", "; line_s += s.arg[i].type + string(" ") + s.arg[i].name; }
                line_s += ") -> (";
                for (size_t i = 0; i < s.ret.size(); ++i) { if (i) line_s += ", "; line_s += s.ret[i].type + string(" ") + s.ret[i].name; }
                line_s += ")";
                if (!s.comment.empty()) line_s += string(" // ") + s.comment;
                output->push_back(line_s);
                if (!s.cppfunc.name.empty()) {
                    string cs = string("      cpp: name='") + s.cppfunc.name + "' file='" + s.cppfunc.file + "'";
                    if (!s.cppfunc.code.empty()) cs += string(" (has inline code)");
                    output->push_back(cs);
                }
            }

            output->push_back("  storages:");
            if (vc.storage.empty()) { output->push_back("    (none)"); }
            for (const VulStorage &st : vc.storage) {
                string s = string("    ") + st.name + " : " + st.type;
                if (!st.comment.empty()) s += string(" // ") + st.comment;
                if (!st.value.empty()) s += string(" = ") + st.value;
                output->push_back(s);
            }

            output->push_back("  storagenext:");
            if (vc.storagenext.empty()) { output->push_back("    (none)"); }
            for (const VulStorage &st : vc.storagenext) {
                string s = string("    ") + st.name + " : " + st.type;
                if (!st.comment.empty()) s += string(" // ") + st.comment;
                output->push_back(s);
            }

            output->push_back("  storagetick:");
            if (vc.storagetick.empty()) { output->push_back("    (none)"); }
            for (const VulStorage &st : vc.storagetick) {
                string s = string("    ") + st.name + " : " + st.type;
                if (!st.comment.empty()) s += string(" // ") + st.comment;
                output->push_back(s);
            }

            output->push_back("  functions:");
            auto dumpFunc = [&](const VulCppFunc &f, const string &label) {
                string s = string("    ") + label + ": ";
                if (!f.name.empty()) { s += string("name='") + f.name + "' file='" + f.file + "'"; if (!f.code.empty()) s += string(" (inline code)"); }
                else { s += string("(none)"); }
                output->push_back(s);
            };
            dumpFunc(vc.init, "init");
            dumpFunc(vc.tick, "tick");
            dumpFunc(vc.applytick, "applytick");

            return output;
        }
    }

    if (token == "bundle") {
        if (args.size() == 1) {
            output->push_back("Bundles:");
            for (const auto &kv : design.bundles) { output->push_back(string("  ") + kv.first); }
            return output;
        } else {
            string bname = args[1];
            auto it = design.bundles.find(bname);
            if (it == design.bundles.end()) {
                output->push_back(string("Error: bundle '") + bname + "' not found");
                return output;
            }
            const VulBundle &vb = it->second;
            output->push_back(string("Bundle: ") + vb.name);
            if (!vb.comment.empty()) { output->push_back(string("  comment: ") + vb.comment); }
            output->push_back("  members:");
            if (vb.members.empty()) { output->push_back("    (none)"); }
            for (const VulArgument &m : vb.members) { output->push_back(string("    ") + m.name + " : " + m.type); }
            return output;
        }
    }

    if (token == "instances" || token == "instance") {
        output->push_back("Instances:");
        for (const auto &kv : design.instances) { output->push_back(string("  ") + kv.first + " -> " + kv.second.combine); }
        return output;
    }

    if (token == "pipe" || token == "pipes") {
        output->push_back("Pipes:");
        for (const auto &kv : design.pipes) { const VulPipe &p = kv.second; output->push_back(string("  ") + p.name + " : " + p.type + " (in=" + std::to_string(p.inputsize) + " out=" + std::to_string(p.outputsize) + " buf=" + std::to_string(p.buffersize) + ")"); }
        return output;
    }

    if (token == "config" || token == "configs") {
        output->push_back("Config library:");
        for (const auto &kv : design.config_lib) { const VulConfigItem &ci = kv.second; string s = string("  ") + ci.name + " = '" + ci.value + "'"; if (!ci.comment.empty()) s += string(" // ") + ci.comment; output->push_back(s); }
        return output;
    }

    // unknown token
    output->push_back(string("Unknown show target: '") + token + "'");
    return output;
}

/**
 * @brief Handle the 'config' command to manipulate config items in the configlib.
 * @param design The VulDesign object to modify.
 * @param args The arguments passed to the 'config' command.
 * @return A unique_ptr to a vector<string> containing the output messages.
 */
unique_ptr<vector<string>> _consoleConfig(VulDesign &design, vector<string> &args) {
    //  config add <name> <value> [--comment "..."]
    //  config update <name> <value> [--comment "..."]
    //  config rename <old> <new>
    //  config delete <name>

    auto output = std::make_unique<vector<string>>();

    auto toLower = [](const string &s) {
        string r = s;
        std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return std::tolower(c); });
        return r;
    };

    if (args.empty()) {
        output->push_back("Usage: config add|update <name> <value> [--comment ...] | rename <old> <new> | delete <name>");
        return output;
    }

    string sub = toLower(args[0]);

    if (sub == "add" || sub == "update") {
        if (args.size() < 3) {
            output->push_back("Error: missing arguments. Usage: config " + sub + " <name> <value> [--comment ...]");
            return output;
        }
        string name = args[1];
        string value = args[2];
        string comment;
        // parse optional --comment or -c
        for (size_t i = 3; i + 1 < args.size(); ++i) {
            if (args[i] == "--comment" || args[i] == "-c") {
                comment = args[i+1];
                break;
            }
        }

        string res = cmdUpdateConfigItem(design, name, value, comment);
        if (res.empty()) output->push_back(string("OK: config '") + name + " set");
        else output->push_back(res);
        return output;
    }

    if (sub == "rename") {
        if (args.size() < 3) {
            output->push_back("Error: missing arguments. Usage: config rename <old> <new>");
            return output;
        }
        string oldn = args[1];
        string newn = args[2];
        string res = cmdRenameConfigItem(design, oldn, newn);
        if (res.empty()) output->push_back(string("OK: config '") + oldn + " renamed to '" + newn + "'");
        else output->push_back(res);
        return output;
    }

    if (sub == "delete") {
        if (args.size() < 2) {
            output->push_back("Error: missing arguments. Usage: config delete <name>");
            return output;
        }
        string name = args[1];
        // cmdRenameConfigItem treats newname=="" as delete per header
        string res = cmdRenameConfigItem(design, name, "");
        if (res.empty()) output->push_back(string("OK: config '") + name + " deleted");
        else output->push_back(res);
        return output;
    }

    output->push_back(string("Unknown config subcommand: '") + args[0] + "'");
    return output;
}

/**
 * @brief Handle the 'combine' command to manipulate combines in the design.
 * @param design The VulDesign object to modify.
 * @param args The arguments passed to the 'combine' command.
 * @return A unique_ptr to a vector<string> containing the output messages.
 */
unique_ptr<vector<string>> _consoleCombine(VulDesign &design, vector<string> &args) {
    auto output = std::make_unique<vector<string>>();

    auto toLower = [](const string &s) {
        string r = s;
        std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return std::tolower(c); });
        return r;
    };

    if (args.empty()) {
        output->push_back("Usage: combine add|update <name> [--comment ...] [--stallable] | rename <old> <new> | duplicate <old> <new> | delete <name> | updatecpps <combine>");
        return output;
    }

    string sub = toLower(args[0]);

    if (sub == "add" || sub == "update") {
        if (args.size() < 2) { output->push_back("Error: missing combine name"); return output; }
        string name = args[1];
        string comment;
        bool stallable = false;
        for (size_t i = 2; i < args.size(); ++i) {
            if (args[i] == "--comment" || args[i] == "-c") { if (i+1 < args.size()) { comment = args[i+1]; i++; } }
            else if (args[i] == "--stallable") { stallable = true; }
            else if (args[i] == "--no-stallable") { stallable = false; }
        }
        string res = cmdUpdateCombine(design, name, comment, stallable);
        if (res.empty()) output->push_back(string("OK: combine '") + name + " updated"); else output->push_back(res);
        return output;
    }

    if (sub == "rename") {
        if (args.size() < 3) { output->push_back("Error: missing args. Usage: combine rename <old> <new>"); return output; }
        string oldn = args[1]; string newn = args[2];
        string res = cmdRenameCombine(design, oldn, newn);
        if (res.empty()) output->push_back(string("OK: combine '") + oldn + " renamed to '" + newn + "'"); else output->push_back(res);
        return output;
    }

    if (sub == "duplicate") {
        if (args.size() < 3) { output->push_back("Error: missing args. Usage: combine duplicate <old> <new>"); return output; }
        string oldn = args[1]; string newn = args[2];
        string res = cmdDuplicateCombine(design, oldn, newn);
        if (res.empty()) output->push_back(string("OK: combine '") + oldn + " duplicated to '" + newn + "'"); else output->push_back(res);
        return output;
    }

    if (sub == "delete") {
        if (args.size() < 2) { output->push_back("Error: missing args. Usage: combine delete <name>"); return output; }
        string name = args[1];
        string res = cmdRenameCombine(design, name, "");
        if (res.empty()) output->push_back(string("OK: combine '") + name + " deleted"); else output->push_back(res);
        return output;
    }

    if (sub == "updatecpps") {
        if (args.size() < 2) { output->push_back("Error: missing args. Usage: combine updatecpps <combine>"); return output; }
        string comb = args[1]; vector<string> broken; string res = cmdUpdateCombineCppFileHelpers(design, comb, broken);
        if (!res.empty()) { output->push_back(res); return output; }
        if (broken.empty()) output->push_back(string("OK: updatecpps for '") + comb + "' completed (no broken files)");
        else {
            output->push_back(string("OK: updatecpps for '") + comb + "' completed, broken files:");
            for (const auto &f : broken) output->push_back(string("  ") + f);
        }
        return output;
    }

    // sub-operations: config, pipein, pipeout, storage, storagenext, storagetick, request, service
    if (sub == "config") {
        if (args.size() < 2) { output->push_back("Error: missing action"); return output; }
        string action = toLower(args[1]);
        if (action == "add" || action == "update") {
            if (args.size() < 5) { output->push_back("Error: Usage: combine config " + action + " <combine> <name> <ref> [--comment <c>]"); return output; }
            string comb = args[2]; string name = args[3]; string ref = args[4]; string comment;
            for (size_t i = 5; i + 1 < args.size(); ++i) if (args[i] == "--comment" || args[i] == "-c") { comment = args[i+1]; break; }
            string res = cmdSetupCombineConfig(design, comb, name, ref, comment);
            if (res.empty()) output->push_back(string("OK: ") + sub + " " + action + " succeeded"); else output->push_back(res);
            return output;
        }
        if (action == "remove") {
            if (args.size() < 4) { output->push_back("Error: Usage: combine config remove <combine> <name>"); return output; }
            string comb = args[2]; string name = args[3]; string res = cmdSetupCombineConfig(design, comb, name, "", "");
            if (res.empty()) output->push_back(string("OK: ") + sub + " removed"); else output->push_back(res);
            return output;
        }
        output->push_back(string("Unknown action for ") + sub);
        return output;
    }

    if (sub == "pipein" || sub == "pipeout") {
        if (args.size() < 2) { output->push_back("Error: missing action"); return output; }
        string action = toLower(args[1]); bool isOut = (sub == "pipeout");
        if (action == "add" || action == "update") {
            if (args.size() < 4) { output->push_back("Error: Usage: combine " + sub + " " + action + " <combine> <port> [<type>] [--comment ...]"); return output; }
            string comb = args[2]; string port = args[3]; string type; string comment;
            if (args.size() >= 5 && args[4] != "--comment" && args[4] != "-c") type = args[4];
            for (size_t i = 4; i + 1 < args.size(); ++i) if (args[i] == "--comment" || args[i] == "-c") { comment = args[i+1]; break; }
            string res = isOut ? cmdSetupCombinePipeOut(design, comb, port, type, comment) : cmdSetupCombinePipeIn(design, comb, port, type, comment);
            if (res.empty()) output->push_back(string("OK: ") + sub + " " + action + " succeeded"); else output->push_back(res);
            return output;
        }
        if (action == "remove") {
            if (args.size() < 4) { output->push_back("Error: Usage: combine " + sub + " remove <combine> <port>"); return output; }
            string comb = args[2]; string port = args[3]; string res = isOut ? cmdRenameCombinePipeOut(design, comb, port, "") : cmdRenameCombinePipeIn(design, comb, port, "");
            if (res.empty()) output->push_back(string("OK: ") + sub + " removed"); else output->push_back(res);
            return output;
        }
        if (action == "rename") {
            if (args.size() < 5) { output->push_back("Error: Usage: combine " + sub + " rename <combine> <old> <new>"); return output; }
            string comb = args[2]; string oldn = args[3]; string newn = args[4]; string res = isOut ? cmdRenameCombinePipeOut(design, comb, oldn, newn) : cmdRenameCombinePipeIn(design, comb, oldn, newn);
            if (res.empty()) output->push_back(string("OK: ") + sub + " renamed"); else output->push_back(res);
            return output;
        }
        output->push_back(string("Unknown action for ") + sub);
        return output;
    }

    if (sub == "storage" || sub == "storagenext" || sub == "storagetick") {
        if (args.size() < 2) { output->push_back("Error: missing action"); return output; }
        string action = toLower(args[1]);
        if (action == "add" || action == "update") {
            if (args.size() < 5) { output->push_back("Error: Usage: combine " + sub + " " + action + " <combine> <name> <type> [--value <val>] [--comment <c>]"); return output; }
            string comb = args[2]; string name = args[3]; string type = args[4]; string value; string comment;
            for (size_t i = 5; i + 1 < args.size(); ++i) {
                if (args[i] == "--value") { value = args[i+1]; i++; }
                else if (args[i] == "--comment" || args[i] == "-c") { comment = args[i+1]; i++; }
            }
            string res;
            if (sub == "storage") res = cmdSetupCombineStorage(design, comb, name, type, value, comment);
            else if (sub == "storagenext") res = cmdSetupCombineStorageNext(design, comb, name, type, value, comment);
            else res = cmdSetupCombineStorageTick(design, comb, name, type, value, comment);
            if (res.empty()) output->push_back(string("OK: ") + sub + " " + action + " succeeded"); else output->push_back(res);
            return output;
        }
        if (action == "remove") {
            if (args.size() < 4) { output->push_back("Error: Usage: combine " + sub + " remove <combine> <name>"); return output; }
            string comb = args[2]; string name = args[3]; string res;
            if (sub == "storage") res = cmdSetupCombineStorage(design, comb, name, "", "", "");
            else if (sub == "storagenext") res = cmdSetupCombineStorageNext(design, comb, name, "", "", "");
            else res = cmdSetupCombineStorageTick(design, comb, name, "", "", "");
            if (res.empty()) output->push_back(string("OK: ") + sub + " removed"); else output->push_back(res);
            return output;
        }
        output->push_back(string("Unknown action for ") + sub);
        return output;
    }

    if (sub == "request" || sub == "service") {
        if (args.size() < 2) { output->push_back("Error: missing action"); return output; }
        string action = toLower(args[1]); bool isService = (sub == "service");
        if (action == "add" || action == "update") {
            if (args.size() < 4) { output->push_back("Error: Usage: combine " + sub + " " + action + " <combine> <name> [--comment ...]"); return output; }
            string comb = args[2]; string name = args[3]; string comment;
            for (size_t i = 4; i + 1 < args.size(); ++i) if (args[i] == "--comment" || args[i] == "-c") { comment = args[i+1]; break; }
            vector<string> anames, atypes, rnames, rtypes;
            string res = isService ? cmdUpdateCombineService(design, comb, name, anames, atypes, rnames, rtypes, comment) : cmdUpdateCombineRequest(design, comb, name, anames, atypes, rnames, rtypes, comment);
            if (res.empty()) output->push_back(string("OK: ") + sub + " " + action + " succeeded"); else output->push_back(res);
            return output;
        }
        if (action == "remove") {
            if (args.size() < 4) { output->push_back("Error: Usage: combine " + sub + " remove <combine> <name>"); return output; }
            string comb = args[2]; string name = args[3]; string res = isService ? cmdRenameCombineService(design, comb, name, "") : cmdRenameCombineRequest(design, comb, name, "");
            if (res.empty()) output->push_back(string("OK: ") + sub + " removed"); else output->push_back(res);
            return output;
        }
        if (action == "rename") {
            if (args.size() < 5) { output->push_back("Error: Usage: combine " + sub + " rename <combine> <old> <new>"); return output; }
            string comb = args[2]; string oldn = args[3]; string newn = args[4]; string res = isService ? cmdRenameCombineService(design, comb, oldn, newn) : cmdRenameCombineRequest(design, comb, oldn, newn);
            if (res.empty()) output->push_back(string("OK: ") + sub + " renamed"); else output->push_back(res);
            return output;
        }
        output->push_back(string("Unknown action for ") + sub);
        return output;
    }

    output->push_back(string("Unknown combine subcommand: '") + sub + "'");
    return output;
}


/**
 * @brief Handle the 'bundle' command to manipulate bundles in the design.
 * @param design The VulDesign object to modify.
 * @param args The arguments passed to the 'bundle' command.
 * @return A unique_ptr to a vector<string> containing the output messages.
 */
unique_ptr<vector<string>> _consoleBundle(VulDesign &design, vector<string> &args) {

//  Bundle 操作:
//      bundle add <name> [--comment "..."]
//      bundle update <name> [--comment "..."]
//      bundle member add <bundle> <member> <type> [--comment "..."]
//      bundle member update <bundle> <member> <type> [--comment "..."]
//      bundle member remove <bundle> <member>
//      bundle rename <old> <new>
//      bundle delete <name>

    auto output = std::make_unique<vector<string>>();

    auto toLower = [](const string &s) {
        string r = s;
        std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return std::tolower(c); });
        return r;
    };

    if (args.empty()) {
        output->push_back("Usage: bundle add|update <name> [--comment ...] | rename <old> <new> | delete <name>");
        output->push_back("       bundle member add|update|remove <bundle> <member> <type> [--comment ...] [--value ...]");
        return output;
    }

    string sub = toLower(args[0]);

    if (sub == "add" || sub == "update") {
        if (args.size() < 2) { output->push_back("Error: missing bundle name"); return output; }
        string name = args[1];
        string comment;
        for (size_t i = 2; i + 1 < args.size(); ++i) {
            if (args[i] == "--comment" || args[i] == "-c") { comment = args[i+1]; break; }
        }
        string res = cmdUpdateBundle(design, name, comment);
        if (res.empty()) output->push_back(string("OK: bundle '") + name + " updated"); else output->push_back(res);
        return output;
    }

    if (sub == "rename") {
        if (args.size() < 3) { output->push_back("Error: missing args. Usage: bundle rename <old> <new>"); return output; }
        string oldn = args[1]; string newn = args[2];
        string res = cmdRenameBundle(design, oldn, newn);
        if (res.empty()) output->push_back(string("OK: bundle '") + oldn + " renamed to '" + newn + "'"); else output->push_back(res);
        return output;
    }

    if (sub == "delete") {
        if (args.size() < 2) { output->push_back("Error: missing args. Usage: bundle delete <name>"); return output; }
        string name = args[1];
        string res = cmdRenameBundle(design, name, "");
        if (res.empty()) output->push_back(string("OK: bundle '") + name + " deleted"); else output->push_back(res);
        return output;
    }

    if (sub == "member") {
        if (args.size() < 2) { output->push_back("Error: missing action for member"); return output; }
        string action = toLower(args[1]);
        if (action == "add" || action == "update") {
            if (args.size() < 5) { output->push_back("Error: Usage: bundle member " + action + " <bundle> <member> <type> [--comment ...] [--value ...]"); return output; }
            string bundlename = args[2];
            string membername = args[3];
            string membertype = args[4];
            string comment;
            string value;
            for (size_t i = 5; i + 1 < args.size(); ++i) {
                if (args[i] == "--comment" || args[i] == "-c") { comment = args[i+1]; i++; }
                else if (args[i] == "--value") { value = args[i+1]; i++; }
            }
            string res = cmdSetupBundleMember(design, bundlename, membername, membertype, comment, value);
            if (res.empty()) output->push_back(string("OK: bundle member '") + membername + " updated in '" + bundlename + "'"); else output->push_back(res);
            return output;
        }
        if (action == "remove") {
            if (args.size() < 4) { output->push_back("Error: Usage: bundle member remove <bundle> <member>"); return output; }
            string bundlename = args[2]; string membername = args[3];
            // remove by passing empty membertype
            string res = cmdSetupBundleMember(design, bundlename, membername, "", "", "");
            if (res.empty()) output->push_back(string("OK: bundle member '") + membername + " removed from '" + bundlename + "'"); else output->push_back(res);
            return output;
        }
        output->push_back(string("Unknown member action: '") + action + "'");
        return output;
    }

    output->push_back(string("Unknown bundle subcommand: '") + sub + "'");
    return output;
}

/**
 * @brief Main console command handler that routes to instance command handlers.
 * @param design The VulDesign object to operate on.
 * @param args The arguments passed to the console command.
 * @return A unique_ptr to a vector<string> containing the output messages.
 */
unique_ptr<vector<string>> _consoleInstance(VulDesign &design, vector<string> &args) {
//  Instance 操作:
//      instance add <name> <combine> [--comment "..."]
//      instance update <name> [<combine>] [--comment "..."]   # 变更绑定或注释（变更绑定需确保无连接）
//      instance rename <old> <new>
//      instance delete <name>
//      instance config set <instance> <configname> <value>
//      instance config remove <instance> <configname>

    auto output = std::make_unique<vector<string>>();

    auto toLower = [](const string &s) {
        string r = s;
        std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return std::tolower(c); });
        return r;
    };

    if (args.empty()) {
        output->push_back("Usage: instance add <name> <combine> [--comment ...] | update <name> [<combine>] [--comment ...] | rename <old> <new> | delete <name>");
        output->push_back("       instance config set <instance> <configname> <value> | instance config remove <instance> <configname>");
        return output;
    }

    string sub = toLower(args[0]);

    if (sub == "add") {
        if (args.size() < 3) { output->push_back("Error: Usage: instance add <name> <combine> [--comment ...]"); return output; }
        string name = args[1]; string combine = args[2]; string comment;
        for (size_t i = 3; i + 1 < args.size(); ++i) if (args[i] == "--comment" || args[i] == "-c") { comment = args[i+1]; break; }
        string res = cmdUpdateInstance(design, name, combine, comment);
        if (res.empty()) output->push_back(string("OK: instance '") + name + " added"); else output->push_back(res);
        return output;
    }

    if (sub == "update") {
        if (args.size() < 2) { output->push_back("Error: Usage: instance update <name> [<combine>] [--comment ...]"); return output; }
        string name = args[1]; string combine; string comment;
        // optional combine (first non-flag token after name)
        size_t idx = 2;
        if (idx < args.size() && !args[idx].empty() && args[idx].rfind("--",0) != 0) { combine = args[idx]; idx++; }
        for (size_t i = idx; i + 1 < args.size(); ++i) if (args[i] == "--comment" || args[i] == "-c") { comment = args[i+1]; break; }
        string res = cmdUpdateInstance(design, name, combine, comment);
        if (res.empty()) output->push_back(string("OK: instance '") + name + " updated"); else output->push_back(res);
        return output;
    }

    if (sub == "rename") {
        if (args.size() < 3) { output->push_back("Error: Usage: instance rename <old> <new>"); return output; }
        string oldn = args[1]; string newn = args[2];
        string res = cmdRenameInstance(design, oldn, newn);
        if (res.empty()) output->push_back(string("OK: instance '") + oldn + " renamed to '" + newn + "'"); else output->push_back(res);
        return output;
    }

    if (sub == "delete") {
        if (args.size() < 2) { output->push_back("Error: Usage: instance delete <name>"); return output; }
        string name = args[1];
        string res = cmdRenameInstance(design, name, "");
        if (res.empty()) output->push_back(string("OK: instance '") + name + " deleted"); else output->push_back(res);
        return output;
    }

    if (sub == "config") {
        if (args.size() < 2) { output->push_back("Error: missing action. Usage: instance config set|remove ..."); return output; }
        string action = toLower(args[1]);
        if (action == "set") {
            if (args.size() < 5) { output->push_back("Error: Usage: instance config set <instance> <configname> <value>"); return output; }
            string inst = args[2]; string conf = args[3]; string val = args[4];
            string res = cmdSetupInstanceConfigOverride(design, inst, conf, val);
            if (res.empty()) output->push_back(string("OK: instance config '") + conf + " set for '" + inst + "'"); else output->push_back(res);
            return output;
        }
        if (action == "remove") {
            if (args.size() < 4) { output->push_back("Error: Usage: instance config remove <instance> <configname>"); return output; }
            string inst = args[2]; string conf = args[3];
            string res = cmdSetupInstanceConfigOverride(design, inst, conf, string());
            if (res.empty()) output->push_back(string("OK: instance config '") + conf + " removed from '" + inst + "'"); else output->push_back(res);
            return output;
        }
        output->push_back(string("Unknown action for config: '") + args[1] + "'");
        return output;
    }

    output->push_back(string("Unknown instance subcommand: '") + sub + "'");
    return output;
}

/**
 * @brief Handle the 'pipe' command to manipulate pipes in the design.
 * @param design The VulDesign object to modify.
 * @param args The arguments passed to the 'pipe' command.
 * @return A unique_ptr to a vector<string> containing the output messages.
 */
unique_ptr<vector<string>> _consolePipe(VulDesign &design, vector<string> &args) {
//  Pipe 操作:
//      pipe add <name> <type> [--inputsize N] [--outputsize N] [--buffersize N] [--comment "..."]
//      pipe update <name> [<type>] [--inputsize N] [--outputsize N] [--buffersize N] [--comment "..."]
//      pipe rename <old> <new>
//      pipe delete <name>

    auto output = std::make_unique<vector<string>>();

    auto toLower = [](const string &s) {
        string r = s;
        std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return std::tolower(c); });
        return r;
    };

    if (args.empty()) {
        output->push_back("Usage: pipe add <name> <type> [--inputsize N] [--outputsize N] [--buffersize N] [--comment ...] | update <name> [<type>] [--inputsize N] [--outputsize N] [--buffersize N] [--comment ...] | rename <old> <new> | delete <name>");
        return output;
    }

    string sub = toLower(args[0]);

    if (sub == "add" || sub == "update") {
        if (args.size() < 2) { output->push_back("Error: missing pipe name"); return output; }
        string name = args[1];
        string type = args[2];
        unsigned int inputsize = 1, outputsize = 1, buffersize = 0;
        string comment;
        size_t idx = 3;
        for (size_t i = idx; i < args.size(); ++i) {
            if (args[i] == "--inputsize" && i+1 < args.size()) { inputsize = static_cast<unsigned int>(std::stoul(args[i+1])); i++; }
            else if (args[i] == "--outputsize" && i+1 < args.size()) { outputsize = static_cast<unsigned int>(std::stoul(args[i+1])); i++; }
            else if (args[i] == "--buffersize" && i+1 < args.size()) { buffersize = static_cast<unsigned int>(std::stoul(args[i+1])); i++; }
            else if ((args[i] == "--comment" || args[i] == "-c") && i+1 < args.size()) { comment = args[i+1]; i++; }
        }
        string res = cmdUpdatePipe(design, name, type, comment, inputsize, outputsize, buffersize);
        if (res.empty()) output->push_back(string("OK: pipe '") + name + " updated"); else output->push_back(res);
        return output;
    }

    if (sub == "rename") {
        if (args.size() < 3) { output->push_back("Error: Usage: pipe rename <old> <new>"); return output; }
        string oldn = args[1]; string newn = args[2];
        string res = cmdRenamePipe(design, oldn, newn);
        if (res.empty()) output->push_back(string("OK: pipe '") + oldn + " renamed to '" + newn + "'"); else output->push_back(res);
        return output;
    }

    if (sub == "delete") {
        if (args.size() < 2) { output->push_back("Error: Usage: pipe delete <name>"); return output; }
        string name = args[1];
        string res = cmdRenamePipe(design, name, "");
        if (res.empty()) output->push_back(string("OK: pipe '") + name + " deleted"); else output->push_back(res);
        return output;
    }

    output->push_back(string("Unknown pipe subcommand: '") + sub + "'");
    return output;
}

/**
 * @brief Handle the 'connect' command to create connections between instances, pipes, requests, and services.
 * @param design The VulDesign object to modify.
 * @param args The arguments passed to the 'connect' command.
 * @return A unique_ptr to a vector<string> containing the output messages.
 */
unique_ptr<vector<string>> _consoleConnect(VulDesign &design, vector<string> &args) {
    //  支持的 connect 子命令见 console.h 注释
    auto output = std::make_unique<vector<string>>();
    if (args.empty()) { output->push_back("Error: missing args. Usage: connect <type> ..."); return output; }

    auto toLower = [](const string &s) { string r = s; std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return std::tolower(c); }); return r; };
    string sub = toLower(args[0]);

    if (sub == "mod->pipe") {
        if (args.size() < 5) { output->push_back("Error: Usage: connect mod->pipe <instance> <pipeoutport> <pipename> <portindex>"); return output; }
        string inst = args[1]; string port = args[2]; string pipename = args[3]; unsigned int idx;
        try { idx = static_cast<unsigned int>(std::stoul(args[4])); } catch(...) { output->push_back("Error: portindex must be a number"); return output; }
        string res = cmdConnectModuleToPipe(design, inst, port, pipename, idx);
        if (res.empty()) output->push_back("OK: mod->pipe connected"); else output->push_back(res);
        return output;
    }

    if (sub == "pipe->mod") {
        if (args.size() < 5) { output->push_back("Error: Usage: connect pipe->mod <instance> <pipeinport> <pipename> <portindex>"); return output; }
        string inst = args[1]; string port = args[2]; string pipename = args[3]; unsigned int idx;
        try { idx = static_cast<unsigned int>(std::stoul(args[4])); } catch(...) { output->push_back("Error: portindex must be a number"); return output; }
        string res = cmdConnectPipeToModule(design, inst, port, pipename, idx);
        if (res.empty()) output->push_back("OK: pipe->mod connected"); else output->push_back(res);
        return output;
    }

    if (sub == "req->serv") {
        if (args.size() < 5) { output->push_back("Error: Usage: connect req->serv <req-instance> <req-port> <serv-instance> <serv-port>"); return output; }
        string reqinst = args[1]; string reqport = args[2]; string servinst = args[3]; string servport = args[4];
        string res = cmdConnectReqToServ(design, reqinst, reqport, servinst, servport);
        if (res.empty()) output->push_back("OK: req->serv connected"); else output->push_back(res);
        return output;
    }

    if (sub == "stalled") {
        if (args.size() < 3) { output->push_back("Error: Usage: connect stalled <src-instance> <dest-instance>"); return output; }
        string src = args[1]; string dest = args[2];
        string res = cmdConnectStalled(design, src, dest);
        if (res.empty()) output->push_back("OK: stalled connection added"); else output->push_back(res);
        return output;
    }

    if (sub == "sequence") {
        if (args.size() < 3) { output->push_back("Error: Usage: connect sequence <former-instance> <latter-instance>"); return output; }
        string former = args[1]; string latter = args[2];
        string res = cmdConnectUpdateSequence(design, former, latter);
        if (res.empty()) output->push_back("OK: update sequence added"); else output->push_back(res);
        return output;
    }

    output->push_back(string("Unknown connect subcommand: '") + args[0] + "'");
    return output;
}

/**
 * @brief Handle the 'disconnect' command to remove connections between instances, pipes, requests, and services.
 * @param design The VulDesign object to modify.
 * @param args The arguments passed to the 'disconnect' command.
 * @return A unique_ptr to a vector<string> containing the output messages.
 */
unique_ptr<vector<string>> _consoleDisconnect(VulDesign &design, vector<string> &args) {
    //  支持的 disconnect 子命令见 console.h 注释
    auto output = std::make_unique<vector<string>>();
    if (args.empty()) { output->push_back("Error: missing args. Usage: disconnect <type> ..."); return output; }

    auto toLower = [](const string &s) { string r = s; std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return std::tolower(c); }); return r; };
    string sub = toLower(args[0]);

    if (sub == "mod->pipe") {
        if (args.size() < 5) { output->push_back("Error: Usage: disconnect mod->pipe <instance> <pipeoutport> <pipename> <portindex>"); return output; }
        string inst = args[1]; string port = args[2]; string pipename = args[3]; unsigned int idx;
        try { idx = static_cast<unsigned int>(std::stoul(args[4])); } catch(...) { output->push_back("Error: portindex must be a number"); return output; }
        string res = cmdDisconnectModuleFromPipe(design, inst, port, pipename, idx);
        if (res.empty()) output->push_back("OK: mod->pipe disconnected"); else output->push_back(res);
        return output;
    }

    if (sub == "pipe->mod") {
        if (args.size() < 5) { output->push_back("Error: Usage: disconnect pipe->mod <instance> <pipeinport> <pipename> <portindex>"); return output; }
        string inst = args[1]; string port = args[2]; string pipename = args[3]; unsigned int idx;
        try { idx = static_cast<unsigned int>(std::stoul(args[4])); } catch(...) { output->push_back("Error: portindex must be a number"); return output; }
        string res = cmdDisconnectPipeFromModule(design, inst, port, pipename, idx);
        if (res.empty()) output->push_back("OK: pipe->mod disconnected"); else output->push_back(res);
        return output;
    }

    if (sub == "req->serv") {
        if (args.size() < 5) { output->push_back("Error: Usage: disconnect req->serv <req-instance> <req-port> <serv-instance> <serv-port>"); return output; }
        string reqinst = args[1]; string reqport = args[2]; string servinst = args[3]; string servport = args[4];
        string res = cmdDisconnectReqFromServ(design, reqinst, reqport, servinst, servport);
        if (res.empty()) output->push_back("OK: req->serv disconnected"); else output->push_back(res);
        return output;
    }

    if (sub == "stalled") {
        if (args.size() < 3) { output->push_back("Error: Usage: disconnect stalled <src-instance> <dest-instance>"); return output; }
        string src = args[1]; string dest = args[2];
        string res = cmdDisconnectStalled(design, src, dest);
        if (res.empty()) output->push_back("OK: stalled connection removed"); else output->push_back(res);
        return output;
    }

    if (sub == "sequence") {
        if (args.size() < 3) { output->push_back("Error: Usage: disconnect sequence <former-instance> <latter-instance>"); return output; }
        string former = args[1]; string latter = args[2];
        string res = cmdDisconnectUpdateSequence(design, former, latter);
        if (res.empty()) output->push_back("OK: update sequence removed"); else output->push_back(res);
        return output;
    }

    output->push_back(string("Unknown disconnect subcommand: '") + args[0] + "'");
    return output;
}


/**
 * @brief Handle the 'load' command to load a design project from a file or directory.
 * @param args The arguments passed to the 'load' command.
 * @return A unique_ptr to a vector<string> containing the output messages.
 */
unique_ptr<vector<string>> VulConsole::_consoleLoad(const vector<string> &args) {
    auto output = std::make_unique<vector<string>>();

    if (!args.size()) { output->push_back("Error: missing path. Usage: load <project-file-or-dir>"); return output; }

    if (design) { output->push_back("Error: a design is already loaded. Close it before loading another."); return output; }

    std::filesystem::path p(args[0]);
    std::filesystem::path vulfile;
    if (!std::filesystem::exists(p)) {
        output->push_back(string("Error: path does not exist: ") + args[0]);
        return output;
    }

    if (std::filesystem::is_directory(p)) {
        // find first .vul file in directory
        bool found = false;
        for (auto &entry : std::filesystem::directory_iterator(p)) {
            if (!entry.is_regular_file()) continue;
            auto ep = entry.path();
            if (ep.extension() == ".vul") { vulfile = ep; found = true; break; }
        }
        if (!found) { output->push_back(string("Error: no .vul project file found in directory: ") + args[0]); return output; }
    } else {
        // is file
        if (p.extension() != ".vul") { output->push_back(string("Error: file is not a .vul project: ") + args[0]); return output; }
        vulfile = p;
    }

    // project name = filename without extension
    string projname = vulfile.stem().string();

    // currently prefab list is empty
    VulPrefabMetaDataMap prefabs;
    string err;
    auto d = VulDesign::loadFromFile(vulfile.string(), prefabs, err);
    if (!d) {
        if (err.empty()) err = string("#10000: failed to load project '") + vulfile.string() + "'";
        output->push_back(err);
        return output;
    }

    design = std::move(d);
    output->push_back(string("OK: project '") + design->project_name + "' loaded from '" + vulfile.string() + "'");
    return output;
}

/**
 * @brief Handle the 'save' command to save the current design project to a file or directory.
 * @param args The arguments passed to the 'save' command.
 * @return A unique_ptr to a vector<string> containing the output messages.
 */
unique_ptr<vector<string>> VulConsole::_consoleSave(const vector<string> &args) {
    auto output = std::make_unique<vector<string>>();

    if (!design) { output->push_back("Error: no design loaded"); return output; }

    // no args -> save to current project location
    if (args.empty()) {
        string res = design->saveProject();
        if (res.empty()) output->push_back(string("OK: project saved to '") + design->project_dir + "'"); else output->push_back(res);
        return output;
    }

    // save-as behavior: args[0] is target path (dir or .vul file)
    std::filesystem::path p(args[0]);

    // remember old values so we can restore on failure
    string old_dir = design->project_dir;
    string old_name = design->project_name;

    try {
        if (std::filesystem::exists(p) && std::filesystem::is_directory(p)) {
            // target is an existing directory
            design->project_dir = p.string();
            string res = design->saveProject();
            if (res.empty()) output->push_back(string("OK: project saved to '") + design->project_dir + "'"); else { design->project_dir = old_dir; output->push_back(res); }
            return output;
        }

        // if path has .vul extension, treat as project file
        if (p.extension() == ".vul") {
            auto parent = p.parent_path();
            if (parent.empty()) parent = std::filesystem::current_path();
            if (!std::filesystem::exists(parent)) std::filesystem::create_directories(parent);

            design->project_dir = parent.string();
            design->project_name = p.stem().string();
            string res = design->saveProject();
            if (res.empty()) output->push_back(string("OK: project saved to '") + p.string() + "'"); else { design->project_dir = old_dir; design->project_name = old_name; output->push_back(res); }
            return output;
        }

        // if path does not have an extension, treat as directory path (create if needed)
        if (p.has_extension() == false) {
            if (!std::filesystem::exists(p)) std::filesystem::create_directories(p);
            design->project_dir = p.string();
            string res = design->saveProject();
            if (res.empty()) output->push_back(string("OK: project saved to '") + design->project_dir + "'"); else { design->project_dir = old_dir; output->push_back(res); }
            return output;
        }

        // otherwise unknown target type (a file with non-.vul extension)
        output->push_back(string("Error: target must be a directory or a .vul project file: ") + args[0]);
        return output;
    } catch (const std::exception &e) {
        // restore
        design->project_dir = old_dir;
        design->project_name = old_name;
        output->push_back(string("Error: exception during save: ") + e.what());
        return output;
    } catch (...) {
        design->project_dir = old_dir;
        design->project_name = old_name;
        output->push_back(string("Error: unknown exception during save"));
        return output;
    }
}


vector<string> _splitCommandLine(const string &cmdline) {
    vector<string> out;
    std::istringstream iss(cmdline);
    string token;
    bool inQuotes = false;
    string currentToken;

    while (iss) {
        char c = static_cast<char>(iss.get());
        if (!iss) break;

        if (c == '"') {
            inQuotes = !inQuotes;
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(c)) && !inQuotes) {
            if (!currentToken.empty()) {
                out.push_back(currentToken);
                currentToken.clear();
            }
        } else {
            currentToken += c;
        }
    }

    if (!currentToken.empty()) {
        out.push_back(currentToken);
    }

    return out;
}

/**
 * @brief Handle the 'help' command to provide assistance on available commands.
 * @param args The arguments passed to the 'help' command.
 * @return A unique_ptr to a vector<string> containing the help messages.
 */
unique_ptr<vector<string>> _consoleHelp(vector<string> &args) {

    string cmd = ((args.size() > 0) ? args[0] : "");
    auto toLower = [](const string &s) { string r = s; std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return std::tolower(c); }); return r; };
    cmd = toLower(cmd);

    if (cmd == "bundle") {
        auto output = std::make_unique<vector<string>>();
        // bundle add <name> [--comment "..."]
        // bundle update <name> [--comment "..."]
        // bundle member add <bundle> <member> <type> [--comment "..."]
        // bundle member update <bundle> <member> <type> [--comment "..."]
        // bundle member remove <bundle> <member>
        // bundle rename <old> <new>
        // bundle delete <name>
        output->push_back("Bundle Command Help:");
        output->push_back("  bundle add <name> [--comment ...]                   Add a new bundle");
        output->push_back("  bundle update <name> [--comment ...]                  Update an existing bundle's comment");
        output->push_back("  bundle rename <old> <new>                         Rename an existing bundle");
        output->push_back("  bundle delete <name>                              Delete an existing bundle");
        output->push_back("  bundle member add <bundle> <member> <type> [--comment ...]    Add a member to a bundle");
        output->push_back("  bundle member update <bundle> <member> <type> [--comment ...] Update a member in a bundle");
        output->push_back("  bundle member remove <bundle> <member>             Remove a member from a bundle");
        return output;
    }

    if (cmd == "combine") {
        auto output = std::make_unique<vector<string>>();
        //  Combine 操作:
        //     combine add <name> [--comment "..."] [--stallable]
        //     combine update <name> [--comment "..."] [--stallable|--no-stallable]
        //     combine rename <old> <new>
        //     combine duplicate <old> <new>
        //     combine delete <name>
        //     combine updatecpps <combine>   # 生成或更新 init/tick/applytick/service 的 cpp 帮助文件并汇报损坏文件

        // Combine 内部成员:
        //     combine pipein add|update|remove <combine> <port> [<type>] [--comment "..."]
        //     combine pipeout add|update|remove <combine> <port> [<type>] [--comment "..."]
        //     combine request add|update|remove|rename <combine> <req> [<newname>] [--arg <type> <name> [<comment>]] [--return <type> <name> [<comment>]]  [--comment "..."]
        //     combine service add|update|remove|rename <combine> <serv> [<newname>] [--arg <type> <name> [<comment>]] [--return <type> <name> [<comment>]]  [--comment "..."]
        //     combine storage add|update|remove <combine> <storage> <type> [--value "..."] [--comment "..."]
        //     combine storagenext / storagetick 同上

        output->push_back("Combine Command Help:");
        output->push_back("  combine add <name> [--comment ...] [--stallable]          Add a new combine");
        output->push_back("  combine update <name> [--comment ...] [--stallable|--no-stallable] Update an existing combine");
        output->push_back("  combine rename <old> <new>                         Rename an existing combine");
        output->push_back("  combine duplicate <old> <new>                      Duplicate an existing combine");
        output->push_back("  combine delete <name>                              Delete an existing combine");
        output->push_back("  combine updatecpps <combine>                       Generate or update C++ helper files for a combine and report any issues");
        output->push_back("  combine config add|update|remove <combine> <configname> [<ref>] [--comment ...]  Manage configuration references of a combine");
        output->push_back("  combine pipein|pipeout add|update|remove <combine> <port> [<type>] [--comment ...]                 Manage pipes of a combine");
        output->push_back("  combine pipein|pipeout rename <combine> <oldport> <newport>                                        Rename a pipe port of a combine");
        output->push_back("  combine request|service add|update|remove <combine> <name> [--arg <type> <name> [<comment>]] [--return <type> <name> [<comment>]] [--comment ...]   Manage requests and services of a combine");
        output->push_back("  combine request|service rename <combine> <oldname> <newname>                                       Rename a request or service of a combine");
        output->push_back("  combine storage add|update|remove <combine> <storage> <type> [--value ...] [--comment ...]         Manage storage variables of a combine");
        output->push_back("  combine storagenext add|update|remove <combine> <storage> <type> [--value ...] [--comment ...]     Manage storagenext variables of a combine");
        output->push_back("  combine storagetick add|update|remove <combine> <storage> <type> [--value ...] [--comment ...]     Manage storagetick variables of a combine");

        return output;
    }

    if (cmd == "instance") {
        // instance add <name> <combine> [--comment "..."]
        // instance update <name> [<combine>] [--comment "..."]   # 变更绑定或注释（变更绑定需确保无连接）
        // instance rename <old> <new>
        // instance delete <name>
        // instance config set <instance> <configname> <value>
        // instance config remove <instance> <configname>

        auto output = std::make_unique<vector<string>>();
        output->push_back("Instance Command Help:");
        output->push_back("  instance add <name> <combine> [--comment ...]          Add a new instance");
        output->push_back("  instance update <name> [<combine>] [--comment ...]     Update an existing instance's combine or comment");
        output->push_back("  instance rename <old> <new>                         Rename an existing instance");
        output->push_back("  instance delete <name>                              Delete an existing instance");
        output->push_back("  instance config set <instance> <configname> <value>    Set a configuration override for an instance");
        output->push_back("  instance config remove <instance> <configname>       Remove a configuration override from an instance");
        return output;
    }

    if (cmd == "pipe") {
        auto output = std::make_unique<vector<string>>();
        // pipe add <name> <type> [--inputsize N] [--outputsize N] [--buffersize N] [--comment "..."]
        // pipe update <name> [<type>] [--inputsize N] [--outputsize N] [--buffersize N] [--comment "..."]
        // pipe rename <old> <new>
        // pipe delete <name>
        output->push_back("Pipe Command Help:");
        output->push_back("  pipe add <name> <type> [--inputsize N] [--outputsize N] [--buffersize N] [--comment ...]    Add a new pipe");
        output->push_back("  pipe update <name> [<type>] [--inputsize N] [--outputsize N] [--buffersize N] [--comment ...] Update an existing pipe");
        output->push_back("  pipe rename <old> <new>                         Rename an existing pipe");
        output->push_back("  pipe delete <name>                              Delete an existing pipe");
        return output;
    }

    if (cmd == "connect") {
        auto output = std::make_unique<vector<string>>();
        output->push_back("Connect Command Help:");
        output->push_back("  connect mod->pipe <instance> <pipeoutport> <pipename> <portindex>   Connect module output port to pipe");
        output->push_back("  connect pipe->mod <instance> <pipeinport> <pipename> <portindex>    Connect pipe to module input port");
        output->push_back("  connect req->serv <req-instance> <req-port> <serv-instance> <serv-port>   Connect request to service");
        output->push_back("  connect stalled <src-instance> <dest-instance>      Add a stalled connection between instances");
        output->push_back("  connect sequence <former-instance> <latter-instance> Add an update sequence between instances");
        return output;
    }

    if (cmd == "disconnect") {
        auto output = std::make_unique<vector<string>>();
        output->push_back("Disconnect Command Help:");
        output->push_back("  disconnect mod->pipe <instance> <pipeoutport> <pipename> <portindex>   Disconnect module output port from pipe");
        output->push_back("  disconnect pipe->mod <instance> <pipeinport> <pipename> <portindex>    Disconnect pipe from module input port");
        output->push_back("  disconnect req->serv <req-instance> <req-port> <serv-instance> <serv-port>   Disconnect request from service");
        output->push_back("  disconnect stalled <src-instance> <dest-instance>      Remove a stalled connection between instances");
        output->push_back("  disconnect sequence <former-instance> <latter-instance> Remove an update sequence between instances");
        return output;
    }

    {
        auto output = std::make_unique<vector<string>>();
        output->push_back("Vulcan Console Help:");
        output->push_back("  help [command]               Show this help or help for a specific command");
        output->push_back("  new <name> <project-dir>     Create a new design project in the specified directory");
        output->push_back("  load <project-file-or-dir>   Load a design project from a .vul file or directory");
        output->push_back("  save [project-file-or-dir]   Save the current design project, optionally to a new location");
        output->push_back("  close                        Close the current design project (prompts to save if needed)");
        output->push_back("  cancel                       Unload the current design project without saving");
        output->push_back("  info                         Show information about the current design project");
        output->push_back("  genbundleh [output-file]     Generate C++ bundle header file for the current design");
        output->push_back("  show [bundles|combines|instances|pipes|connections]");
        output->push_back("                               Show details of the current design");
        output->push_back("  config [show|set|remove] ... Manage global configuration settings");
        output->push_back("  combine ...                  Manage combines in the design");
        output->push_back("  bundle ...                   Manage bundles in the design");
        output->push_back("  instance ...                 Manage instances in the design");
        output->push_back("  pipe ...                     Manage pipes in the design");
        output->push_back("  connect ...                  Create connections between instances, pipes, requests, and services");
        output->push_back("  disconnect ...               Remove connections between instances, pipes, requests, and services");
        return output;
    }
}

/**
 * @brief Perform a command line input.
 * @param cmdline The command line input string.
 * @return A unique_ptr to vector<string> containing the output messages, or nullptr if no output.
 *         The output message can be an error message or a success message.
 */
unique_ptr<vector<string>> VulConsole::performCommand(const string &cmdline) {
    // split into tokens (supports quoted strings)
    vector<string> toks = _splitCommandLine(cmdline);
    if (toks.empty()) return nullptr;

    // normalize tokens: lowercase for command names and known shorthands
    auto toLower = [](const string &s) { string r = s; std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return std::tolower(c); }); return r; };

    string cmd = toLower(toks[0]);
    string subcmd;
    if (toks.size() >= 2 && cmd != "load" && cmd != "save") subcmd = toLower(toks[1]);

    // expand common shorthand for top-level commands
    if (cmd == "s") cmd = "show";
    if (cmd == "cf") cmd = "config";
    if (cmd == "c") cmd = "combine";
    if (cmd == "b") cmd = "bundle";
    if (cmd == "i") cmd = "instance";
    if (cmd == "p") cmd = "pipe";
    if (cmd == "co") cmd = "connect";
    if (cmd == "dco") cmd = "disconnect";

    // normalize second token shorthand for subcommands like 'req'/'serv' etc.
    if (!subcmd.empty()) {
        if (subcmd == "req") subcmd = "request";
        if (subcmd == "serv") subcmd = "service";
        if (subcmd == "st") subcmd = "storage";
        if (subcmd == "stn") subcmd = "storagenext";
        if (subcmd == "stt") subcmd = "storagetick";
        if (subcmd == "pi") subcmd = "pipein";
        if (subcmd == "po") subcmd = "pipeout";
    }

    // replace in toks the normalized tokens
    toks[0] = cmd;
    if (!subcmd.empty()) toks[1] = subcmd;

    // normalize comment flag -c -> --comment
    for (size_t i = 0; i < toks.size(); ++i) if (toks[i] == "-c") toks[i] = "--comment";

    // helper to build args vector (tokens after command or after two tokens)
    auto args_from = [&](size_t start)->vector<string> {
        vector<string> a;
        for (size_t i = start; i < toks.size(); ++i) a.push_back(toks[i]);
        return a;
    };

    // dispatch
    if (cmd == "help") {
        vector<string> args = args_from(1);
        return _consoleHelp(args);
    }

    if (cmd == "load") {
        vector<string> args = args_from(1);
        return _consoleLoad(args);
    }

    if (cmd == "save") {
        vector<string> args = args_from(1);
        return _consoleSave(args);
    }

    if (cmd == "new") {
        auto output = std::make_unique<vector<string>>();
        if (toks.size() < 3) { output->push_back("Error: missing args. Usage: new <project-name> <project-dir>"); return output; }
        if (design) { output->push_back("Error: a design is already loaded. Close it before creating a new one."); return output; }
        string projname = toks[1];
        string projdir = toks[2];

        // currently prefab list is empty
        VulPrefabMetaDataMap prefabs;
        string err;
        auto d = VulDesign::initNewDesign(projname, projdir, err);
        if (!d) {
            output->push_back(err);
            return output;
        }

        design = std::move(d);
        output->push_back(string("OK: new project '") + design->project_name + "' created in '" + design->project_dir + "'");
        return output;
    }

    if (cmd == "close") {
        auto output = std::make_unique<vector<string>>();
        if (design) {
            string res = design->saveProject();
            if (!res.empty()) { output->push_back(res); return output; }
            design.reset();
            output->push_back("OK: project closed");
            return output;
        }
        output->push_back("Error: no design loaded");
        return output;
    }

    if (cmd == "cancel") {
        auto output = std::make_unique<vector<string>>();
        if (design) { design.reset(); output->push_back("OK: project unloaded (unsaved)"); return output; }
        output->push_back("Error: no design loaded");
        return output;
    }

    if (cmd == "info") {
        auto output = std::make_unique<vector<string>>();
        if (!design) { output->push_back("Error: no design loaded"); return output; }
        output->push_back(string("project_dir: ") + design->project_dir);
        output->push_back(string("project_name: ") + design->project_name);
        return output;
    }

    if (cmd == "genbundleh") {
        auto output = std::make_unique<vector<string>>();
        if (!design) { output->push_back("Error: no design loaded"); return output; }
        // generate bundle header lines
        auto lines = codeGenerateBundleHeaderFile(design->bundles);
        if (!lines) { output->push_back("Error: no bundles to generate"); return output; }

        // determine output path
        string outpath;
        if (toks.size() >= 2) outpath = toks[1];
        else {
            // default to project_dir/cpp/bundle.h
            std::filesystem::path p = design->project_dir;
            p /= "cpp";
            if (!std::filesystem::exists(p)) std::filesystem::create_directories(p);
            p /= "bundle.h";
            outpath = p.string();
        }

        try {
            auto parent = std::filesystem::path(outpath).parent_path();
            if (!parent.empty() && !std::filesystem::exists(parent)) std::filesystem::create_directories(parent);
            std::ofstream ofs(outpath);
            if (!ofs) { output->push_back(string("Error: cannot open output file: ") + outpath); return output; }
            for (const auto &ln : *lines) ofs << ln << "\n";
            ofs.close();
            output->push_back(string("OK: bundle header written to '") + outpath + "'");
            return output;
        } catch (const std::exception &e) {
            output->push_back(string("Error: exception writing file: ") + e.what());
            return output;
        }
    }

    if (cmd == "validate") {
        auto output = std::make_unique<vector<string>>();
        if (!design) { output->push_back("Error: no design loaded"); return output; }
        string err = design->checkAllError();
        if (err.empty()) output->push_back("OK: design validation passed"); else output->push_back(err);
        return output;
    }

    // Commands that require a loaded design and dispatch to specific handlers
    if (!design) {
        auto output = std::make_unique<vector<string>>();
        output->push_back("Error: no design loaded");
        return output;
    }

    // map top-level commands to handler functions
    if (cmd == "show") {
        vector<string> args = args_from(1);
        return _consoleShow(*design, args);
    }

    if (cmd == "config") {
        vector<string> args = args_from(1);
        return _consoleConfig(*design, args);
    }

    if (cmd == "bundle") {
        vector<string> args = args_from(1);
        return _consoleBundle(*design, args);
    }

    if (cmd == "combine") {
        vector<string> args = args_from(1);
        return _consoleCombine(*design, args);
    }

    if (cmd == "instance") {
        vector<string> args = args_from(1);
        return _consoleInstance(*design, args);
    }

    if (cmd == "pipe") {
        vector<string> args = args_from(1);
        return _consolePipe(*design, args);
    }

    if (cmd == "connect") {
        vector<string> args = args_from(1);
        return _consoleConnect(*design, args);
    }

    if (cmd == "disconnect") {
        vector<string> args = args_from(1);
        return _consoleDisconnect(*design, args);
    }

    // unknown command
    auto output = std::make_unique<vector<string>>();
    output->push_back(string("Unknown command: ") + cmd);
    return output;
}




