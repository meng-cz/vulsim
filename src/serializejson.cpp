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

#include "serializejson.h"

#include "json.hpp"

namespace serialize {

using nlohmann::json;


VulOperationPackage serializeOperationPackageFromJSON(const string &json_str) {
    VulOperationPackage op;
    json j = json::parse(json_str);
    op.name = j.at("name").get<OperationName>();
    if (j.contains("args")) {
        /*
        {
        "args": [
            { "index": 0, "name": "width", "value": 64 },
            { "index": 1, "name": "height", "value": 32 },
            { "index": 2, "name": "pipeline_depth", "value": 5 }
        ]
        }
        */
        for (const auto &arg_json : j.at("args")) {
            VulOperationArg arg;
            if (arg_json.contains("index")) {
                arg.index = arg_json.at("index").get<uint32_t>();
            } else {
                arg.index = -1;
            }
            if (arg_json.contains("name")) {
                arg.name = arg_json.at("name").get<OperationArgName>();
            } else {
                arg.name = "";
            }
            if (arg_json.contains("value")) {
                arg.value = arg_json.at("value").get<OperationArg>();
            } else {
                arg.value = OperationArg();
            }
            op.arg_list.push_back(std::move(arg));
        }
    }
    return op;
}

string serializeOperationResponseToJSON(const VulOperationResponse &response) {
    json j;
    j["code"] = response.code;
    j["msg"] = response.msg;
    json args_json;
    for (const auto& [key, value] : response.results) {
        args_json[key] = value;
    }
    j["results"] = args_json;
    json list_args_json;
    for (const auto& [key, value] : response.list_results) {
        list_args_json[key] = value;
    }
    j["list_results"] = list_args_json;
    return j.dump(1, ' ');
}

string serializeBundleItemToJSON(const VulBundleItem &bundle) {
    json j;
    j["is_alias"] = bundle.is_alias;
    j["members"] = json::array();
    if (!bundle.comment.empty()) {
        j["comment"] = bundle.comment;
    }
    for (const auto &mem : bundle.members) {
        json jm;
        jm["name"] = mem.name;
        jm["comment"] = mem.comment;
        jm["type"] = mem.type;
        jm["value"] = mem.value;
        jm["uint_length"] = mem.uint_length;
        jm["dims"] = json::array();
        for (const auto &d : mem.dims) {
            jm["dims"].push_back(d);
        }
        j["members"].push_back(jm);
    }
    j["enum_members"] = json::array();
    for (const auto &emen : bundle.enum_members) {
        json je;
        je["name"] = emen.name;
        je["comment"] = emen.comment;
        je["value"] = emen.value;
        j["enum_members"].push_back(je);
    }
    return j.dump();
}

/**
 * json format:
 * {
 *   "members": [
 *    {
 *    "name": "...",
 *    "comment": "...",
 *    "type": "...",
 *    "value": "...",
 *    "uint_length": "...",
 *    "dims": ["...", "...", ...]
 *   },
 *   "enum_members": [
 *   {
 *   "name": "...",
 *   "comment": "...",
 *   "value": "..."
 *   },
 *   "is_alias": true/false
 * }
 */

void parseBundleItemFromJSON(const string &json_str, VulBundleItem &out_bundle) {
    out_bundle.members.clear();
    out_bundle.enum_members.clear();
    json j = json::parse(json_str);
    if (j.contains("is_alias")) {
        out_bundle.is_alias = j.at("is_alias").get<bool>();
    } else {
        out_bundle.is_alias = false;
    }
    if (j.contains("comment")) {
        out_bundle.comment = j.at("comment").get<Comment>();
    } else {
        out_bundle.comment = "";
    }
    if (j.contains("members")) {
        for (const auto &jm : j.at("members")) {
            VulBundleMember mem;
            mem.name = jm.contains("name") ? jm.at("name").get<BMemberName>() : "";
            mem.comment =  jm.contains("comment") ? jm.at("comment").get<Comment>() : "";
            mem.type = jm.contains("type") ? jm.at("type").get<BMemberType>() : "";
            mem.value = jm.contains("value") ? jm.at("value").get<ConfigValue>() : ConfigValue();
            mem.uint_length = jm.contains("uint_length") ? jm.at("uint_length").get<ConfigValue>() : ConfigValue();
            if (jm.contains("dims"))
            {
                for (const auto &d : jm.at("dims")) {
                    mem.dims.push_back(d.get<ConfigValue>());
                }
            }
            out_bundle.members.push_back(mem);
        }
    }
    if (j.contains("enum_members")) {
        for (const auto &je : j.at("enum_members")) {
            VulBundleEnumMember emen;
            emen.name = je.contains("name") ? je.at("name").get<BMemberName>() : "";
            emen.comment = je.contains("comment") ? je.at("comment").get<Comment>() : "";
            emen.value = je.contains("value") ? je.at("value").get<ConfigValue>() : ConfigValue();
            out_bundle.enum_members.push_back(emen);
        }
    }
}

/**
 * json format for reqserv:
 * {
 *   "comment": "...",
 *   "has_handshake": true/false,
 *   "args": [
 *    {
 *      "name": "...",
 *      "type": "..."
 *      "comment": "..."
 *   }, ...
 *   ],
 *   "rets": [
 *    {
 *      "name": "...",
 *      "type": "..."
 *      "comment": "..."
 *   }, ...
 *   ]
 * }
 */

string serializeReqServToJSON(const VulReqServ &reqserv) {
    json j;
    if (!reqserv.comment.empty()) j["comment"] = reqserv.comment;
    j["has_handshake"] = reqserv.has_handshake;
    j["args"] = json::array();
    for (const auto &arg : reqserv.args) {
        json ja;
        ja["name"] = arg.name;
        ja["type"] = arg.type;
        if (!arg.comment.empty()) ja["comment"] = arg.comment;
        j["args"].push_back(ja);
    }
    j["rets"] = json::array();
    for (const auto &ret : reqserv.rets) {
        json jr;
        jr["name"] = ret.name;
        jr["type"] = ret.type;
        if (!ret.comment.empty()) jr["comment"] = ret.comment;
        j["rets"].push_back(jr);
    }
    return j.dump();
}

void parseReqServFromJSON(const string &json_str, VulReqServ &out_reqserv) {
    out_reqserv.args.clear();
    out_reqserv.rets.clear();
    json j = json::parse(json_str);
    if (j.contains("comment")) {
        out_reqserv.comment = j.at("comment").get<Comment>();
    } else {
        out_reqserv.comment = "";
    }
    if (j.contains("has_handshake")) {
        out_reqserv.has_handshake = j.at("has_handshake").get<bool>();
    } else {
        out_reqserv.has_handshake = false;
    }
    if (j.contains("args")) {
        for (const auto &ja : j.at("args")) {
            VulArg arg;
            arg.name = ja.contains("name") ? ja.at("name").get<ArgName>() : "";
            arg.type = ja.contains("type") ? ja.at("type").get<DataType>() : "";
            arg.comment = ja.contains("comment") ? ja.at("comment").get<Comment>() : "";
            if (arg.name.empty() || arg.type.empty()) {
                continue;
            }
            out_reqserv.args.push_back(arg);
        }
    }
    if (j.contains("rets")) {
        for (const auto &jr : j.at("rets")) {
            VulArg ret;
            ret.name = jr.contains("name") ? jr.at("name").get<ArgName>() : "";
            ret.type = jr.contains("type") ? jr.at("type").get<DataType>() : "";
            ret.comment = jr.contains("comment") ? jr.at("comment").get<Comment>() : "";
            if (ret.name.empty() || ret.type.empty()) {
                continue;
            }
            out_reqserv.rets.push_back(ret);
        }
    }
}

/**
 * json format for instance:
 * {
 *   "module_name": "...",
 *   "comment": "...",
 *   "local_config_overrides": {
 *      "config1": "value1",
 *      "config2": "value2"
 *   }
 * }
 */

string serializeInstanceToJSON(const VulInstance &inst) {
    json j;
    j["module_name"] = inst.module_name;
    if (!inst.comment.empty()) j["comment"] = inst.comment;
    j["local_config_overrides"] = json::object();
    for (const auto & [key, value] : inst.local_config_overrides) {
        j["local_config_overrides"][key] = value;
    }
    return j.dump();
}

void parseInstanceFromJSON(const string &json_str, VulInstance &out_inst) {
    out_inst.local_config_overrides.clear();
    json j = json::parse(json_str);
    if (j.contains("module_name")) {
        out_inst.module_name = j.at("module_name").get<ModuleName>();
    } else {
        out_inst.module_name = "";
    }
    if (j.contains("comment")) {
        out_inst.comment = j.at("comment").get<Comment>();
    } else {
        out_inst.comment = "";
    }
    if (j.contains("local_config_overrides")) {
        for (const auto& [key, value] : j.at("local_config_overrides").items()) {
            out_inst.local_config_overrides[key] = value.get<LocalConfigValue>();
        }
    }
}

/**
 * json format for pipe:
 * {
 *  "comment": "...",
 *  "type": "...",
 *  "input_size": "...",
 *  "output_size": "...",
 *  "buffer_size": "...",
 *  "latency": "...",
 *  "has_handshake": true/false,
 *  "has_valid": true/false
 * }
 */

string serializePipeToJSON(const VulPipe &pipe) {
    json j;
    if (!pipe.comment.empty()) j["comment"] = pipe.comment;
    j["type"] = pipe.type;
    j["input_size"] = pipe.input_size;
    j["output_size"] = pipe.output_size;
    j["buffer_size"] = pipe.buffer_size;
    j["latency"] = pipe.latency;
    j["has_handshake"] = pipe.has_handshake;
    j["has_valid"] = pipe.has_valid;
    return j.dump();
}

void parsePipeFromJSON(const string &json_str, VulPipe &out_pipe) {
    json j = json::parse(json_str);
    if (j.contains("comment")) {
        out_pipe.comment = j.at("comment").get<Comment>();
    } else {
        out_pipe.comment = "";
    }
    if (j.contains("type")) {
        out_pipe.type = j.at("type").get<DataType>();
    } else {
        out_pipe.type = "";
    }
    if (j.contains("input_size")) {
        out_pipe.input_size = j.at("input_size").get<PipeSize>();
    } else {
        out_pipe.input_size = "1";
    }
    if (j.contains("output_size")) {
        out_pipe.output_size = j.at("output_size").get<PipeSize>();
    } else {
        out_pipe.output_size = "1";
    }
    if (j.contains("buffer_size")) {
        out_pipe.buffer_size = j.at("buffer_size").get<PipeSize>();
    } else {
        out_pipe.buffer_size = "0";
    }
    if (j.contains("latency")) {
        out_pipe.latency = j.at("latency").get<PipeSize>();
    } else {
        out_pipe.latency = "1";
    }
    if (j.contains("has_handshake")) {
        out_pipe.has_handshake = j.at("has_handshake").get<bool>();
    } else {
        out_pipe.has_handshake = false;
    }
    if (j.contains("has_valid")) {
        out_pipe.has_valid = j.at("has_valid").get<bool>();
    } else {
        out_pipe.has_valid = false;
    }
}

/**
 * json format for module:
 * {
 *  "comment": "...",
 *  "local_configs": [
 *   { "name": "...", "value": "...", "comment": "..." }, ...
 *  ],
 *  "local_bundles": [
 *   { "name": "...", "comment": "..."}, ...
 *  ],
 *  "requests": [
 *   { "name": "...", "comment": "...", "sig": "..." }, ...
 *  ],
 *  "services": [
 *   { "name": "...", "comment": "...", "sig": "..." }, ...
 *  ],
 *  "pipein": [
 *   { "name": "...", "comment": "...", "type": "..." }, ...
 *  ],
 *  "pipeout": [
 *   { "name": "...", "comment": "...", "type": "..." }, ...
 *  ],
 *  "instances": [
 *   { "name": "...", "comment": "...", "module": "...", "order": "...", "configs": [{ "name": "...", "value": "..." }, ...], "module" : {...} }, ...
 *  ],
 *  "pipes": [
 *   { "name": "...", "comment": "...", "type": "...", "has_handshake": true/false, "has_valid": true/false }, ...
 *  ],
 *  "user_tick_codeblocks": [
 *   { "name": "...", "comment": "...", "order": "..." }, ...
 *  ],
 *  "serv_codelines": [ "...", ... ],
 *  "req_codelines": [
 *   { "instance": "...", "req_name": "..."}, ...
 *  ],
 *  "storages": [
 *   { "name": "...", "category": "...", "type": "...", "comment": "..." },
 *  ],
 *  "connections" : [
 *   { "src_instance": "...", "src_port": "...", "dst_instance": "...", "dst_port": "..." }, ...
 *  ],
 *  "pipe_connections" : [
 *   { "instance": "...", "port": "...", "pipe": "...", "pipe_port": "..." }, ...
 *  ],
 *  "stalled_connections" : [
 *   { "former_instance": "...", "latter_instance": "..." }, ...
 *  ],
 *  "update_constraints" : [
 *   { "former_instance": "...", "latter_instance": "..." }, ...
 *  ]
 * }
 */

json _constructModuleBase(const VulModuleBase &mod_info) {
    json j;
    j["comment"] = mod_info.comment;
    // local_configs
    j["local_configs"] = json::array();
    for (const auto& [cfg_name, cfg_value] : mod_info.local_configs) {
        json jcfg;
        jcfg["name"] = cfg_name;
        jcfg["value"] = cfg_value.value;
        jcfg["comment"] = cfg_value.comment;
        j["local_configs"].push_back(jcfg);
    }
    // local_bundles
    j["local_bundles"] = json::array();
    for (const auto& [bundle_name, bundle_item] : mod_info.local_bundles) {
        json jbun;
        jbun["name"] = bundle_name;
        jbun["comment"] = bundle_item.comment;
        j["local_bundles"].push_back(jbun);
    }
    // requests
    j["requests"] = json::array();
    for (const auto& [req_name, reqserv] : mod_info.requests) {
        json jreq;
        jreq["name"] = req_name;
        jreq["comment"] = reqserv.comment;
        jreq["sig"] = reqserv.signatureFull();
        j["requests"].push_back(jreq);
    }
    // services
    j["services"] = json::array();
    for (const auto& [serv_name, reqserv] : mod_info.services) {
        json jserv;
        jserv["name"] = serv_name;
        jserv["comment"] = reqserv.comment;
        jserv["sig"] = reqserv.signatureFull();
        j["services"].push_back(jserv);
    }
    // pipein
    j["pipein"] = json::array();
    for (const auto& [pipe_name, pipe] : mod_info.pipe_inputs) {
        json jpipe;
        jpipe["name"] = pipe_name;
        jpipe["comment"] = pipe.comment;
        jpipe["type"] = pipe.type;
        j["pipein"].push_back(jpipe);
    }
    // pipeout
    j["pipeout"] = json::array();
    for (const auto& [pipe_name, pipe] : mod_info.pipe_outputs) {
        json jpipe;
        jpipe["name"] = pipe_name;
        jpipe["comment"] = pipe.comment;
        jpipe["type"] = pipe.type;
        j["pipeout"].push_back(jpipe);
    }
    return j;
}

string serializeModuleInfoToJSON(const VulModule &mod_info, shared_ptr<VulModuleLib> modulelib) {
    json j = _constructModuleBase(mod_info);
    // pipes
    j["pipes"] = json::array();
    for (const auto& [pipe_name, pipe] : mod_info.pipe_instances) {
        json jpipe;
        jpipe["name"] = pipe_name;
        jpipe["comment"] = pipe.comment;
        jpipe["type"] = pipe.type;
        jpipe["input_size"] = pipe.input_size;
        jpipe["output_size"] = pipe.output_size;
        jpipe["buffer_size"] = pipe.buffer_size;
        jpipe["latency"] = pipe.latency;
        jpipe["has_handshake"] = pipe.has_handshake;
        jpipe["has_valid"] = pipe.has_valid;
        j["pipes"].push_back(jpipe);
    }
    vector<InstanceName> _dummy;
    unique_ptr<vector<InstanceName>> inst_update_order = mod_info.getInstanceUpdateOrder(_dummy);
    j["instances"] = json::array();
    j["user_tick_codeblocks"] = json::array();

    auto constructInstance = [&](const VulInstance &inst, uint64_t order) -> json {
        json jinst;
        jinst["name"] = inst.name;
        jinst["comment"] = inst.comment;
        jinst["module"] = inst.module_name;
        jinst["order"] = order;
        jinst["configs"] = json::array();
        for (const auto& [cfg_name, cfg_value] : inst.local_config_overrides) {
            json jcfg;
            jcfg["name"] = cfg_name;
            jcfg["value"] = cfg_value;
            jinst["configs"].push_back(jcfg);
        }
        auto childmod_iter = modulelib->modules.find(inst.module_name);
        if (childmod_iter != modulelib->modules.end()) {
            jinst["module"] = _constructModuleBase(*(childmod_iter->second));
        }
        return jinst;
    };

    if (inst_update_order) {
        for (uint64_t i = 0; i < inst_update_order->size(); i++) {
            const InstanceName &inst_name = (*inst_update_order)[i];
            auto iter = mod_info.instances.find(inst_name);
            auto cbiter = mod_info.user_tick_codeblocks.find(inst_name);
            if (iter != mod_info.instances.end()) {
                const VulInstance &inst = iter->second;
                j["instances"].push_back(constructInstance(inst, i + 1));
            } else if (cbiter != mod_info.user_tick_codeblocks.end()) {
                const auto &cb = cbiter->second;
                json jcb;
                jcb["name"] = inst_name;
                jcb["comment"] = cb.comment;
                jcb["order"] = i + 1;
                j["user_tick_codeblocks"].push_back(jcb);
            }
        }
    } else {
        for (const auto& [inst_name, inst] : mod_info.instances) {
            j["instances"].push_back(constructInstance(inst, 0));
        }
        for (const auto& [cb_name, cb] : mod_info.user_tick_codeblocks) {
            json jcb;
            jcb["name"] = cb_name;
            jcb["comment"] = cb.comment;
            jcb["order"] = 0;
            j["user_tick_codeblocks"].push_back(jcb);
        }
    }
    // serv_codelines
    j["serv_codelines"] = json::array();
    for (const auto & line : mod_info.serv_codelines) {
        if (isCodeLineEmpty(line.second)) continue;
        j["serv_codelines"].push_back(line.first);
    }
    // req_codelines
    j["req_codelines"] = json::array();
    for (const auto & [inst, reqmap] : mod_info.req_codelines) {
        for (const auto & [req_name, codelines] : reqmap) {
            if (isCodeLineEmpty(codelines)) continue;
            json jreq;
            jreq["instance"] = inst;
            jreq["req_name"] = req_name;
            j["req_codelines"].push_back(jreq);
        }
    }
    // storages
    j["storages"] = json::array();
    auto addStorageJson = [&](const VulStorage &storage, const string & category) {
        json jstor;
        jstor["name"] = storage.name;
        jstor["category"] = category;
        jstor["type"] = storage.typeString();
        jstor["comment"] = storage.comment;
        j["storages"].push_back(jstor);
    };
    for (const auto & storage : mod_info.storages) {
        addStorageJson(storage.second, "regular");
    }
    for (const auto & storage : mod_info.storagenexts) {
        addStorageJson(storage.second, "register");
    }
    for (const auto & storage : mod_info.storagetmp) {
        addStorageJson(storage.second, "temporary");
    }
    // connections
    j["connections"] = json::array();
    for (const auto &connset : mod_info.req_connections) {
        for (const auto & c : connset.second) {
            json jconn;
            jconn["src_instance"] = c.req_instance;
            jconn["src_port"] = c.req_name;
            jconn["dst_instance"] = c.serv_instance;
            jconn["dst_port"] = c.serv_name;
            j["connections"].push_back(jconn);
        }
    }
    // pipe_connections
    j["pipe_connections"] = json::array();
    for (const auto &connset : mod_info.mod_pipe_connections) {
        for (const auto & c : connset.second) {
            json jconn;
            jconn["instance"] = c.instance;
            jconn["port"] = c.instance_pipe_port;
            jconn["pipe"] = c.pipe_instance;
            jconn["pipe_port"] = c.top_pipe_port;
            j["pipe_connections"].push_back(jconn);
        }
    }
    // stalled_connections
    j["stalled_connections"] = json::array();
    for (const auto & c : mod_info.stalled_connections) {
        json jconn;
        jconn["former_instance"] = c.former_instance;
        jconn["latter_instance"] = c.latter_instance;
        j["stalled_connections"].push_back(jconn);
    }
    // update_constraints
    j["update_constraints"] = json::array();
    for (const auto & c : mod_info.update_constraints) {
        json jconn;
        jconn["former_instance"] = c.former_instance;
        jconn["latter_instance"] = c.latter_instance;
        j["update_constraints"].push_back(jconn);
    }   
    return j.dump();
}

string serializeModuleBaseInfoToJSON(const VulModuleBase &mod_info) {
    json j= _constructModuleBase(mod_info);
    return j.dump();
}

} // namespace serialize
