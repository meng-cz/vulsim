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


} // namespace serialize
