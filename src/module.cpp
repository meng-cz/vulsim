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

#include "module.h"
#include "configexpr.hpp"
#include "toposort.hpp"
#include "cppparse.hpp"
#include <cassert>
#include <functional>
#include <iostream>

using std::make_shared;

ErrorMsg VulModuleBase::updateDynamicReferences() {
    _dyn_referenced_configs.clear();
    _dyn_referenced_bundles.clear();
    _dyn_referenced_modules.clear();
    vector<std::pair<ConfigValue, ErrorMsg>> to_process;
    for (const auto &lc_entry : local_configs) {
        const VulLocalConfigItem &lc = lc_entry.second;
        to_process.push_back({lc.value, string("Invalid local config value expression for '") + lc.name + "': "});
    }
    for (const auto &lb_entry : local_bundles) {
        const VulBundleItem &lb = lb_entry.second;
        for (const auto &member : lb.members) {
            for (const auto &dim_expr : member.dims) {
                to_process.push_back({dim_expr, string("Invalid dimension expression for local bundle '") + lb.name + "', member '" + member.name + "': "});
            }
            if (!member.uint_length.empty()) {
                to_process.push_back({member.uint_length, string("Invalid uint length expression for local bundle '") + lb.name + "', member '" + member.name + "': "});
            }
            if (!member.value.empty()) {
                to_process.push_back({member.value, string("Invalid default value expression for local bundle '") + lb.name + "', member '" + member.name + "': "});
            }
        }
        for (const auto &enum_member : lb.enum_members) {
            if (!enum_member.value.empty()) {
                to_process.push_back({enum_member.value, string("Invalid enum value expression for local bundle '") + lb.name + "', enum member '" + enum_member.name + "': "});
            }
        }
    }
    for (const auto &entry : to_process) {
        const ConfigValue &expr = entry.first;
        const ErrorMsg &prefix_err = entry.second;
        string err;
        uint32_t errpos = 0;
        auto conf_refs = config_parser::parseReferencedIdentifier(expr, errpos, err);
        if (!err.empty()) {
            return prefix_err + err;
        }
        for (const auto &cn : *conf_refs) {
            _dyn_referenced_configs.insert(cn);
        }
    }
    
    auto parseReqServReferences = [&](const VulReqServ &rs) -> ErrorMsg {
        for (const auto &arg : rs.args) {
            if (!isBasicVulType(arg.type)) {
                _dyn_referenced_bundles.insert(arg.type);
            }
        }
        for (const auto &ret : rs.rets) {
            if (!isBasicVulType(ret.type)) {
                _dyn_referenced_bundles.insert(ret.type);
            }
        }
        // if (!rs.array_size.empty()) {
        //     string err;
        //     uint32_t errpos = 0;
        //     auto conf_refs = config_parser::parseReferencedIdentifier(rs.array_size, errpos, err);
        //     if (!err.empty()) {
        //         return EStr(EItemModConfInvalidValue, string("Invalid array size expression for '") + rs.name + "': " + err);
        //     }
        //     for (const auto &cn : *conf_refs) {
        //         _dyn_referenced_configs.insert(cn);
        //     }
        // }
        return "";
    };
    for (const auto &req_entry : requests) {
        ErrorMsg err = parseReqServReferences(req_entry.second);
    }
    for (const auto &serv_entry : services) {
        ErrorMsg err = parseReqServReferences(serv_entry.second);
    }
    auto parsePipePortReferences = [&](const VulPipePort &pp) {
        if (!isBasicVulType(pp.type)) {
            _dyn_referenced_bundles.insert(pp.type);
        }
    };
    for (const auto &in_entry : pipe_inputs) {
        parsePipePortReferences(in_entry.second);
    }
    for (const auto &out_entry : pipe_outputs) {
        parsePipePortReferences(out_entry.second);
    }
    return "";
}

/**
 * @brief Check if the given name conflicts with existing names in local scope.
 * @param name The name to check.
 */
bool VulModule::localCheckNameConflict(const string &name) {
    if (instances.find(name) != instances.end()) {
        return true;
    }
    if (pipe_instances.find(name) != pipe_instances.end()) {
        return true;
    }
    if (storages.find(name) != storages.end()) {
        return true;
    }
    if (storagenexts.find(name) != storagenexts.end()) {
        return true;
    }
    if (storagetmp.find(name) != storagetmp.end()) {
        return true;
    }
    if (requests.find(name) != requests.end()) {
        return true;
    }
    if (services.find(name) != services.end()) {
        return true;
    }
    if (pipe_inputs.find(name) != pipe_inputs.end()) {
        return true;
    }
    if (pipe_outputs.find(name) != pipe_outputs.end()) {
        return true;
    }
    if (local_configs.find(name) != local_configs.end()) {
        return true;
    }
    if (local_bundles.find(name) != local_bundles.end()) {
        return true;
    }
    if (user_tick_codeblocks.find(name) != user_tick_codeblocks.end()) {
        return true;
    }
    return false;
}

/**
 * @brief Update the dynamic references (_dyn_referenced_configs, _dyn_referenced_bundles, _dyn_referenced_modules) based on current module definition.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg VulModule::updateDynamicReferences() {
    ErrorMsg err = VulModuleBase::updateDynamicReferences();
    if (!err.empty()) {
        return err;
    }

    auto parseConfigItem = [&](const string &expr) -> ErrorMsg {
        string err;
        uint32_t errpos = 0;
        auto conf_refs = config_parser::parseReferencedIdentifier(expr, errpos, err);
        if (!err.empty()) {
            return string("Invalid config expression at position ") + std::to_string(errpos) + ": " + err + string(": ") + expr;
        }
        for (const auto &cn : *conf_refs) {
            _dyn_referenced_configs.insert(cn);
        }
        return "";
    };

    auto parseStorageItemWithE = [&](const VulStorage &st) -> ErrorMsg {
        if (!st.uint_length.empty()) {
            err = parseConfigItem(st.uint_length);
            if (!err.empty()) {
                return EStr(EItemModConfInvalidValue, string("Invalid uint length expression for storage '") + st.name + "': " + err.msg);
            }
        } else if (!isBasicVulType(st.type)) {
            _dyn_referenced_bundles.insert(st.type);
        }
        for (const auto &dim_expr : st.dims) {
            err = parseConfigItem(dim_expr);
            if (!err.empty()) {
                return EStr(EItemModConfInvalidValue, string("Invalid dimension expression for storage '") + st.name + "': " + err.msg);
            }
        }
        if (!st.value.empty()) {
            err = parseConfigItem(st.value);
            if (!err.empty()) {
                return EStr(EItemModConfInvalidValue, string("Invalid initial value expression for storage '") + st.name + "': " + err.msg);
            }
        }
        return "";
    };
    for (const auto &st_entry : storages) {
        err = parseStorageItemWithE(st_entry.second);
        if (!err.empty()) {
            return err;
        }
    }
    for (const auto &stn_entry : storagenexts) {
        err = parseStorageItemWithE(stn_entry.second);
        if (!err.empty()) {
            return err;
        }
    }
    for (const auto &stt_entry : storagetmp) {
        err = parseStorageItemWithE(stt_entry.second);
        if (!err.empty()) {
            return err;
        }
    }

    for (const auto &inst_entry : instances) {
        const VulInstance &inst = inst_entry.second;
        _dyn_referenced_modules.insert(inst.module_name);
        for (const auto &lc_override : inst.local_config_overrides) {
            err = parseConfigItem(lc_override.second);
            if (!err.empty()) {
                return EStr(EItemModConfInvalidValue, string("Invalid local config override expression for instance '") + inst.name + "', config item '" + lc_override.first + "': " + err.msg);
            }
        }
    }
    for (const auto &pipe_inst_entry : pipe_instances) {
        const VulPipe &pipe_inst = pipe_inst_entry.second;
        if (!isBasicVulType(pipe_inst.type)) {
            _dyn_referenced_bundles.insert(pipe_inst.type);
        }
    }
    return "";
}

ErrorMsg VulModule::_0_validateBrokenIndexes(shared_ptr<VulConfigLib> config_lib, shared_ptr<VulBundleLib> bundle_lib, shared_ptr<VulModuleLib> module_lib) const {
    const string prefix = "Module '" + name + "': ";

    for (const auto &lc_entry : local_configs) {
        const VulLocalConfigItem &lc = lc_entry.second;
        if (lc.name != lc_entry.first) [[unlikely]] {
            return EStr(ErrModuleBrokenIndex, prefix + "Local config index broken for item '" + lc_entry.first + "', should be '" + lc.name + "'.");
        }
    }
    for (const auto &lb_entry : local_bundles) {
        const VulBundleItem &lb = lb_entry.second;
        if (lb.name != lb_entry.first) [[unlikely]] {
            return EStr(ErrModuleBrokenIndex, prefix + "Local bundle index broken for item '" + lb_entry.first + "', should be '" + lb.name + "'.");
        }
    }
    for (const auto &entry : requests) {
        const auto &item = entry.second;
        if (item.name != entry.first) [[unlikely]] {
            return EStr(ErrModuleBrokenIndex, prefix + "Request index broken for item '" + entry.first + "', should be '" + item.name + "'.");
        }
    }
    for (const auto &entry : services) {
        const auto &item = entry.second;
        if (item.name != entry.first) [[unlikely]] {
            return EStr(ErrModuleBrokenIndex, prefix + "Service index broken for item '" + entry.first + "', should be '" + item.name + "'.");
        }
    }
    for (const auto &entry : pipe_inputs) {
        const auto &item = entry.second;
        if (item.name != entry.first) [[unlikely]] {
            return EStr(ErrModuleBrokenIndex, prefix + "Pipe input port index broken for item '" + entry.first + "', should be '" + item.name + "'.");
        }
    }
    for (const auto &entry : pipe_outputs) {
        const auto &item = entry.second;
        if (item.name != entry.first) [[unlikely]] {
            return EStr(ErrModuleBrokenIndex, prefix + "Pipe output port index broken for item '" + entry.first + "', should be '" + item.name + "'.");
        }
    }
    for (const auto &entry : user_tick_codeblocks) {
        const auto &item = entry.second;
        if (item.name != entry.first) [[unlikely]] {
            return EStr(ErrModuleBrokenIndex, prefix + "User tick codeblock index broken for item '" + entry.first + "', should be '" + item.name + "'.");
        }
    }
    for (const auto &entry : storages) {
        const auto &item = entry.second;
        if (item.name != entry.first) [[unlikely]] {
            return EStr(ErrModuleBrokenIndex, prefix + "Storage index broken for item '" + entry.first + "', should be '" + item.name + "'.");
        }
    }
    for (const auto &entry : storagenexts) {
        const auto &item = entry.second;
        if (item.name != entry.first) [[unlikely]] {
            return EStr(ErrModuleBrokenIndex, prefix + "Storagenext index broken for item '" + entry.first + "', should be '" + item.name + "'.");
        }
    }
    for (const auto &entry : storagetmp) {
        const auto &item = entry.second;
        if (item.name != entry.first) [[unlikely]] {
            return EStr(ErrModuleBrokenIndex, prefix + "Storagetmp index broken for item '" + entry.first + "', should be '" + item.name + "'.");
        }
    }
    for (const auto &entry : instances) {
        const auto &item = entry.second;
        if (item.name != entry.first) [[unlikely]] {
            return EStr(ErrModuleBrokenIndex, prefix + "Instance index broken for item '" + entry.first + "', should be '" + item.name + "'.");
        }
    }
    for (const auto &entry : pipe_instances) {
        const auto &item = entry.second;
        if (item.name != entry.first) [[unlikely]] {
            return EStr(ErrModuleBrokenIndex, prefix + "Pipe instance index broken for item '" + entry.first + "', should be '" + item.name + "'.");
        }
    }
    for (const auto &entry : req_connections) {
        const InstanceName &inst_name = entry.first;
        for (const auto &conn : entry.second) {
            if (conn.req_instance != inst_name) [[unlikely]] {
                return EStr(ErrModuleBrokenIndex, prefix + "Request-service connection index broken for connection '" + entry.first + "': " + conn.toString() + ".");
            }
        }
    }
    for (const auto &entry : mod_pipe_connections) {
        const InstanceName &inst_name = entry.first;
        for (const auto &conn : entry.second) {
            if (conn.instance != inst_name) [[unlikely]] {
                return EStr(ErrModuleBrokenIndex, prefix + "Module-pipe connection index broken for connection '" + entry.first + "': " + conn.toString() + ".");
            }
        }
    }

    return "";
}

ErrorMsg VulModule::_1_validateNameConflicts(shared_ptr<VulConfigLib> configlib, shared_ptr<VulBundleLib> bundlelib, shared_ptr<VulModuleLib> modulelib) const {

    const string prefix = "Module '" + name + "': ";

    vector<pair<string, string>> local_names; // name -> from which element

    auto isGloballyConflict = [&](const string &name) -> string {
        if (configlib->checkNameConflict(name)) {
            return "Global config";
        }
        if (bundlelib->checkNameConflict(name)) {
            return "Global bundle";
        }
        if (modulelib->modules.find(name) != modulelib->modules.end()) {
            return "Global module";
        }
        return "";
    };

    // insert local names from: local configs/bundles, req/serv, pipein/out, inst/pipeinst/tickcodeblock, storages
    for (const auto &lc_entry : local_configs) {
        local_names.push_back({lc_entry.first, "Local config"});
    }
    for (const auto &lb_entry : local_bundles) {
        local_names.push_back({lb_entry.first, "Local bundle"});
    }
    for (const auto &req_entry : requests) {
        local_names.push_back({req_entry.first, "Request"});
    }
    for (const auto &serv_entry : services) {
        local_names.push_back({serv_entry.first, "Service"});
    }
    for (const auto &in_entry : pipe_inputs) {
        local_names.push_back({in_entry.first, "Pipe input port"});
    }
    for (const auto &out_entry : pipe_outputs) {
        local_names.push_back({out_entry.first, "Pipe output port"});
    }
    for (const auto &inst_entry : instances) {
        local_names.push_back({inst_entry.first, "Instance"});
    }
    for (const auto &pipe_inst_entry : pipe_instances) {
        local_names.push_back({pipe_inst_entry.first, "Pipe instance"});
    }
    for (const auto &tick_entry : user_tick_codeblocks) {
        local_names.push_back({tick_entry.first, "Tick codeblock"});
    }
    for (const auto &st_entry : storages) {
        local_names.push_back({st_entry.first, "Storage"});
    }
    for (const auto &stn_entry : storagenexts) {
        local_names.push_back({stn_entry.first, "Storagenext"});
    }
    for (const auto &stt_entry : storagetmp) {
        local_names.push_back({stt_entry.first, "Storagetmp"});
    }

    // check conflicts
    unordered_map<string, string> seen_local_names;
    for (const auto &name_pair : local_names) {
        const string &name = name_pair.first;
        const string &from_what = name_pair.second;
        // check valid name
        if (!isValidIdentifier(name)) [[unlikely]] {
            return EStr(EItemModLocalNameInvalid, prefix + from_what + " name '" + name + "' is not a valid identifier.");
        }
        // check global conflict
        string glob_conflict = isGloballyConflict(name);
        if (!glob_conflict.empty()) [[unlikely]] {
            return EStr(EItemModLocalNameDup, prefix + from_what + " name '" + name + "' conflicts with existing " + glob_conflict + " name.");
        }
        // check local conflict
        auto iter = seen_local_names.find(name);
        if (iter != seen_local_names.end()) [[unlikely]] {
            return EStr(EItemModLocalNameDup, prefix + from_what + " name '" + name + "' conflicts with existing local " + iter->second + " name.");
        }
        seen_local_names[name] = from_what;
    }

    return "";
}

ErrorMsg VulModule::_2_validateLocalConfigBundleDefinitions(shared_ptr<VulConfigLib> configlib, shared_ptr<VulBundleLib> bundlelib, shared_ptr<VulModuleLib> modulelib) const {

    ErrorMsg err;
    const string prefix = "Module '" + name + "': ";

    unordered_map<ConfigName, ConfigRealValue> local_config_values;
    for (const auto &lc_entry : local_configs) {
        const VulLocalConfigItem &lc = lc_entry.second;
        ConfigRealValue val;
        unordered_set<ConfigName> conf_refs;
        err = configlib->calculateConfigExpression(lc.value, val, conf_refs);
        if (!err.empty()) [[unlikely]] {
            return EStr(EItemModConfInvalidValue, prefix + string("Local config '") + lc.name + "' has invalid default value expression: " + err.msg);
        }
        local_config_values[lc.name] = val;
    }

    unordered_set<BundleName> local_bundle_names;
    unordered_map<BundleName, unordered_set<BundleName>> local_bundle_rev_refs;
    for (const auto &lb_entry : local_bundles) {
        local_bundle_names.insert(lb_entry.first);
        local_bundle_rev_refs[lb_entry.first] = unordered_set<BundleName>();
    }

    vector<std::pair<ConfigValue, string>> to_process; // expr -> from_what
    auto parseBundleMember = [&](const VulBundleMember &member, const string &bundname) -> ErrorMsg {
        if (!isValidIdentifier(member.name)) [[unlikely]] {
            return EStr(EItemModBundMemNameInvalid, prefix + bundname + "' has invalid member name '" + member.name + "'.");
        }
        if (!member.uint_length.empty() || member.type == "uint") {
            if (!member.type.empty() && member.type != "uint") [[unlikely]] {
                return EStr(EItemModBundInvalidType, prefix + bundname + "', member '" + member.name + "' has uint_length but type is not 'uint'.");
            }
            to_process.push_back({member.uint_length, prefix + bundname + "', member '" + member.name + "', uint length expression"});
        } else if (member.type.empty()) [[unlikely]] {
            return EStr(EItemModBundInvalidType, prefix + bundname + "', member '" + member.name + "' has empty type.");
        } else if (!isBasicVulType(member.type)) {
            if (local_bundles.find(member.type) == local_bundles.end() && !bundlelib->checkNameConflict(member.type)) {
                return EStr(EItemModBundMemRefNotFound, prefix + bundname + "', member '" + member.name + "' has undefined bundle type '" + member.type + "'.");
            }
            if (local_bundles.find(member.type) != local_bundles.end()) {
                local_bundle_rev_refs[member.type].insert(bundname);
            }
            if (!member.value.empty()) [[unlikely]] {
                return EStr(EItemModBundMemUnexpectedValue, prefix + bundname + "', member '" + member.name + "' of non-basic type cannot have default value.");
            }
        }
        for (const auto &dim_expr : member.dims) {
            to_process.push_back({dim_expr, prefix + bundname + "', member '" + member.name + "', dimension expression"});
        }
        if (!member.value.empty()) {
            to_process.push_back({member.value, bundname + "', member '" + member.name + "', default value expression"});
        }
        return "";
    };

    for (const auto &lb_entry : local_bundles) {
        const VulBundleItem &lb = lb_entry.second;
        if (lb.is_alias) {
            if (lb.members.size() != 1 || !lb.enum_members.empty()) [[unlikely]] {
                return EStr(EItemModBundInvalidType, prefix + string("Local bundle alias '") + lb.name + "' must have exactly one member and no enum members.");
            }
        }
        if (!lb.enum_members.empty()) {
            if (!lb.members.empty()) [[unlikely]] {
                return EStr(EItemModBundInvalidType, prefix + string("Local bundle enum '") + lb.name + "' cannot have regular members.");
            }
            unordered_set<string> enum_member_names;
            unordered_set<ConfigRealValue> enum_member_values;
            for (const auto &enum_member : lb.enum_members) {
                if (!isValidIdentifier(enum_member.name)) [[unlikely]] {
                    return EStr(EItemModBundInvalidEnum, prefix + string("Local bundle enum '") + lb.name + "' has invalid enum member name '" + enum_member.name + "'.");
                }
                if (enum_member_names.find(enum_member.name) != enum_member_names.end()) [[unlikely]] {
                    return EStr(EItemModBundInvalidEnum, prefix + string("Local bundle enum '") + lb.name + "' has duplicate enum member name '" + enum_member.name + "'.");
                }
                if (!enum_member.value.empty()) {
                    ConfigRealValue val;
                    unordered_set<ConfigName> conf_refs;
                    err = configlib->calculateConfigExpression(enum_member.value, local_config_values, val, conf_refs);
                    if (!err.empty()) [[unlikely]] {
                        return EStr(EItemModConfInvalidValue, prefix + string("Local bundle enum '") + lb.name + "', enum member '" + enum_member.name + "' has invalid value expression: " + err.msg);
                    }
                    if (enum_member_values.find(val) != enum_member_values.end()) [[unlikely]] {
                        return EStr(EItemModBundInvalidEnum, prefix + string("Local bundle enum '") + lb.name + "', enum member '" + enum_member.name + "' has duplicate enum member value.");
                    }
                    enum_member_values.insert(val);
                }
            }
        } else {
            unordered_set<string> member_names;
            for (const auto &member : lb.members) {
                if (member_names.find(member.name) != member_names.end()) [[unlikely]] {
                    return EStr(EItemModBundMemNameDup, prefix + string("Local bundle '") + lb.name + "' has duplicate member name '" + member.name + "'.");
                }
                member_names.insert(member.name);
                err = parseBundleMember(member, string("Local bundle '") + lb.name);
                if (!err.empty()) [[unlikely]] {
                    return err;
                }
            }
        }
    }

    vector<BundleName> looped_bundles;
    auto sorted_local_bundles = topologicalSort(local_bundle_names, local_bundle_rev_refs, looped_bundles);
    if (sorted_local_bundles == nullptr) [[unlikely]] {
        string looped_str;
        for (const auto &bn : looped_bundles) {
            if (!looped_str.empty()) looped_str += ", ";
            looped_str += "'" + bn + "'";
        }
        return EStr(EItemModBundRefLooped, prefix + string("Local bundles have cyclic references: ") + looped_str);
    }

    for (const auto &st_entry : storages) {
        const VulStorage &st = st_entry.second;
        err = parseBundleMember(st, string("Storage '") + st.name);
        if (!err.empty()) [[unlikely]] {
            return err;
        }
    }
    for (const auto &stn_entry : storagenexts) {
        const VulStorage &stn = stn_entry.second;
        err = parseBundleMember(stn, string("Storagenext '") + stn.name);
        if (!err.empty()) [[unlikely]] {
            return err;
        }
    }
    for (const auto &stt_entry : storagetmp) {
        const VulStorage &stt = stt_entry.second;
        err = parseBundleMember(stt, string("Storagetmp '") + stt.name);
        if (!err.empty()) [[unlikely]] {
            return err;
        }
    }

    for (const auto &inst_entry : instances) {
        const VulInstance &inst = inst_entry.second;
        for (const auto &lc_override : inst.local_config_overrides) {
            to_process.push_back({lc_override.second, string("Instance '") + inst.name + "', local config override for '" + lc_override.first + "'"});
        }
    }

    for (const auto &expr_pair : to_process) {
        const ConfigValue &expr = expr_pair.first;
        const string &from_what = expr_pair.second;
        ConfigRealValue val;
        unordered_set<ConfigName> conf_refs;
        err = configlib->calculateConfigExpression(expr, local_config_values, val, conf_refs);
        if (!err.empty()) [[unlikely]] {
            return EStr(EItemModConfInvalidValue, prefix + from_what + " has invalid expression: " + err.msg + string(": ") + expr);
        }
    }

    return "";
}


ErrorMsg VulModule::_3_validateInstanceDefinitions(shared_ptr<VulConfigLib> configlib, shared_ptr<VulBundleLib> bundlelib, shared_ptr<VulModuleLib> modulelib) const {
    ErrorMsg err;
    const string prefix = "Module '" + name + "': ";

    for (const auto &inst_entry : instances) {
        const VulInstance &inst = inst_entry.second;
        // check module existence
        auto mod_iter = modulelib->modules.find(inst.module_name);
        if (mod_iter == modulelib->modules.end()) [[unlikely]] {
            return EStr(EItemModInstRefNotFound, prefix + string("Instance '") + inst.name + "' references undefined module '" + inst.module_name + "'.");
        }
        // check local config overrides
        const VulModuleBase &inst_mod = *(mod_iter->second);
        for (const auto &lc_override : inst.local_config_overrides) {
            const ConfigName &cn = lc_override.first;
            auto lc_iter = inst_mod.local_configs.find(cn);
            if (lc_iter == inst_mod.local_configs.end()) [[unlikely]] {
                return EStr(EItemModInstConfOverrideNotFound, prefix + string("Instance '") + inst.name + "' has local config override for non-existing local config '" + cn + "' in module '" + inst.module_name + "'.");
            }
        }
    }

    return "";
}

ErrorMsg VulModule::_4_validateReqServConnections(shared_ptr<VulConfigLib> configlib, shared_ptr<VulBundleLib> bundlelib, shared_ptr<VulModuleLib> modulelib) const {
    
    const string prefix = "Module '" + name + "': ";
    ErrorMsg err;

    VulReqServ pipe_clear_srv = VulReqServ{
        .name = "clear",
        .comment = "Clear the pipe",
        .args = {},
        .rets = {},
        // .array_size = "",
        .has_handshake = false
    };


    auto findChildService = [&](const InstanceName &inst_name, const ReqServName &serv_name) -> const VulReqServ* {
        auto inst_iter = instances.find(inst_name);
        if (inst_iter == instances.end()) {
            auto pipe_iter = pipe_instances.find(inst_name);
            if (pipe_iter != pipe_instances.end() && serv_name == "clear") [[likely]] {
                return &pipe_clear_srv;
            }
            return nullptr;
        }
        const VulInstance &inst = inst_iter->second;
        auto mod_iter = modulelib->modules.find(inst.module_name);
        if (mod_iter == modulelib->modules.end()) [[unlikely]] {
            return nullptr;
        }
        const VulModuleBase &inst_mod = *(mod_iter->second);
        auto serv_iter = inst_mod.services.find(serv_name);
        if (serv_iter == inst_mod.services.end()) [[unlikely]] {
            return nullptr;
        }
        return &(serv_iter->second);
    };

    auto findChildRequest = [&](const InstanceName &inst_name, const ReqServName &req_name) -> const VulReqServ* {
        auto inst_iter = instances.find(inst_name);
        if (inst_iter == instances.end()) [[unlikely]] {
            return nullptr;
        }
        const VulInstance &inst = inst_iter->second;
        auto mod_iter = modulelib->modules.find(inst.module_name);
        if (mod_iter == modulelib->modules.end()) [[unlikely]] {
            return nullptr;
        }
        const VulModuleBase &inst_mod = *(mod_iter->second);
        auto req_iter = inst_mod.requests.find(req_name);
        if (req_iter == inst_mod.requests.end()) [[unlikely]] {
            return nullptr;
        }
        return &(req_iter->second);
    };

    // confirm all instances and req/serv ports exist and match
    for (const auto &rconn_entry : req_connections) {
        const auto &rconns = rconn_entry.second;
        for (const auto &conn : rconns) {
            if (conn.req_instance == TopInterface) {
                auto serv_iter = services.find(conn.serv_name);
                if (serv_iter == services.end()) [[unlikely]] {
                    return EStr(EItemModRConnInvalidPort, prefix + string("Req-Serv connection references non-existing service '") + conn.req_name + "': " + conn.toString());
                }
                auto child_serv = findChildService(conn.serv_instance, conn.serv_name);
                if (child_serv == nullptr) [[unlikely]] {
                    return EStr(EItemModRConnInvalidPort, prefix + string("Req-Serv connection references non-existing service '") + conn.serv_name + "' in instance '" + conn.serv_instance + "': " + conn.toString());
                }
                if (!serv_iter->second.match(*child_serv)) [[unlikely]] {
                    return EStr(EItemModRConnMismatch, prefix + string("Req-Serv connection has mismatched service signature for service : " + conn.toString()));
                }
            } else if (conn.serv_instance == TopInterface) {
                auto child_req = findChildRequest(conn.req_instance, conn.req_name);
                if (child_req == nullptr) [[unlikely]] {
                    return EStr(EItemModRConnInvalidPort, prefix + string("Req-Serv connection references non-existing request '") + conn.req_name + "' in instance '" + conn.req_instance + "': " + conn.toString());
                }
                auto req_iter = requests.find(conn.serv_name);
                if (req_iter == requests.end()) [[unlikely]] {
                    return EStr(EItemModRConnInvalidPort, prefix + string("Req-Serv connection references non-existing request '") + conn.serv_name + "': " + conn.toString());
                }
                if (!req_iter->second.match(*child_req)) [[unlikely]] {
                    return EStr(EItemModRConnMismatch, prefix + string("Req-Serv connection has mismatched request signature for request : " + conn.toString()));
                }
            } else {
                auto child_req = findChildRequest(conn.req_instance, conn.req_name);
                if (child_req == nullptr) [[unlikely]] {
                    return EStr(EItemModRConnInvalidInst, prefix + string("Req-Serv connection references non-existing request '") + conn.req_name + "' in instance '" + conn.req_instance + "': " + conn.toString());
                }
                auto child_serv = findChildService(conn.serv_instance, conn.serv_name);
                if (child_serv == nullptr) [[unlikely]] {
                    return EStr(EItemModRConnInvalidInst, prefix + string("Req-Serv connection references non-existing service '") + conn.serv_name + "' in instance '" + conn.serv_instance + "': " + conn.toString());
                }
                if (!child_req->match(*child_serv)) [[unlikely]] {
                    return EStr(EItemModRConnMismatch, prefix + string("Req-Serv connection has mismatched request-service signature: " + conn.toString()));
                }
            }
        }
    }

    for (const auto &serv_entry : services) {
        const VulReqServ &serv = serv_entry.second;
        bool codeline_impl = (serv_codelines.find(serv.name) != serv_codelines.end());
        unordered_set<VulReqServConnection, ReqServConnHash> connected_set;
        auto conn_iter = req_connections.find(TopInterface);
        if (conn_iter != req_connections.end()) {
            for (const auto &conn : conn_iter->second) {
                if (conn.req_name == serv.name) {
                    connected_set.insert(conn);
                }
            }
        }
        if (!serv.allowMultiConnect()) [[unlikely]] {
            if (connected_set.size() > 1 || (connected_set.size() == 1 && codeline_impl)) {
                return EStr(EItemModRConnServMultiConn, prefix + string("Service '") + serv.name + "' has multiple request connections but does not allow multi-connect.");
            }
        }
        if (connected_set.empty() && !codeline_impl) [[unlikely]] {
            return EStr(EItemModRConnServNotConnected, prefix + string("Service '") + serv.name + "' is not connected and has no codeline implementation, but is not optional.");
        }
    }

    for (const auto &inst_entry : instances) {
        const auto &inst = inst_entry.second;
        auto mod_iter = modulelib->modules.find(inst.module_name);
        if (mod_iter == modulelib->modules.end()) [[unlikely]] {
            continue;
        }
        const auto &childmod_ptr = mod_iter->second;
        for (const auto &creq_entry : childmod_ptr->requests) {
            const auto &creq = creq_entry.second;
            bool codeline_impl = false;
            {
                auto reqcode_iter = req_codelines.find(inst.name);
                if (reqcode_iter != req_codelines.end()) {
                    if (reqcode_iter->second.find(creq.name) != reqcode_iter->second.end()) {
                        codeline_impl = true;
                    }
                }
            }
            unordered_set<VulReqServConnection, ReqServConnHash> connected_set;
            auto conn_iter = req_connections.find(inst.name);
            if (conn_iter != req_connections.end()) {
                for (const auto &conn : conn_iter->second) {
                    if (conn.req_name == creq.name) {
                        connected_set.insert(conn);
                    }
                }
            }
            if (!creq.allowMultiConnect()) [[unlikely]] {
                if (connected_set.size() > 1 || (connected_set.size() == 1 && codeline_impl)) {
                    return EStr(EItemModRConnChildReqMultiConn, prefix + string("Request '") + creq.name + "' in instance '" + inst.name + "' has multiple service connections but does not allow multi-connect.");
                }
            }
            if (connected_set.empty() && !codeline_impl) [[unlikely]] {
                return EStr(EItemModRConnChildReqNotConnected, prefix + string("Request '") + creq.name + "' in instance '" + inst.name + "' is not connected and has no codeline implementation, but is not optional.");
            }
        }
    }

    return "";
}

ErrorMsg VulModule::_5_validatePipeConnections(shared_ptr<VulConfigLib> configlib, shared_ptr<VulBundleLib> bundlelib, shared_ptr<VulModuleLib> modulelib) const {
    
    const string prefix = "Module '" + name + "': ";
    ErrorMsg err;

    // confirm all instances and pipe ports exist and match
    for (const auto &pconn_entry : mod_pipe_connections) {
        const auto &pconns = pconn_entry.second;
        for (const auto &conn : pconns) {
            auto inst_iter = instances.find(conn.instance);
            if (inst_iter == instances.end()) [[unlikely]] {
                return EStr(EItemModPConnInvalidInst, prefix + string("Module-Pipe connection references non-existing instance '") + conn.instance + "': " + conn.toString());
            }
            const VulInstance &inst = inst_iter->second;
            auto mod_iter = modulelib->modules.find(inst.module_name);
            if (mod_iter == modulelib->modules.end()) [[unlikely]] {
                return EStr(EItemModPConnInvalidInst, prefix + string("Module-Pipe connection references instance '") + conn.instance + "' which references undefined module '" + inst.module_name + "': " + conn.toString());
            }
            const VulModuleBase &inst_mod = *(mod_iter->second);
            auto pipe_in_iter = inst_mod.pipe_inputs.find(conn.instance_pipe_port);
            auto pipe_out_iter = inst_mod.pipe_outputs.find(conn.instance_pipe_port);
            bool hit_input = (pipe_in_iter != inst_mod.pipe_inputs.end());
            bool hit_output = (pipe_out_iter != inst_mod.pipe_outputs.end());
            if (!hit_input && !hit_output) [[unlikely]] {
                return EStr(EItemModPConnInvalidPort, prefix + string("Module-Pipe connection references non-existing pipe port '") + conn.instance_pipe_port + "' in instance '" + conn.instance + "': " + conn.toString());
            } else if (hit_input && hit_output) [[unlikely]] {
                return EStr(EItemModPConnAmbigPort, prefix + string("Module-Pipe connection references ambiguous pipe port '") + conn.instance_pipe_port + "' in instance '" + conn.instance + "': " + conn.toString());
            } else if (hit_input) {
                // it's an input port
                if (conn.pipe_instance == TopInterface || !conn.top_pipe_port.empty()) {
                    // connected to top-level pipe input
                    auto top_pipe_in_iter = pipe_inputs.find(conn.top_pipe_port);
                    if (top_pipe_in_iter == pipe_inputs.end()) [[unlikely]] {
                        return EStr(EItemModPConnInvalidPort, prefix + string("Module-Pipe connection references non-existing top-level pipe input port '") + conn.top_pipe_port + "': " + conn.toString());
                    }
                    if (top_pipe_in_iter->second.type != pipe_in_iter->second.type) [[unlikely]] {
                        return EStr(EItemModPConnMismatch, prefix + string("Module-Pipe connection has type mismatch between top-level pipe input port '") + conn.top_pipe_port + "' and instance pipe input port '" + conn.instance_pipe_port + "': " + conn.toString());
                    }
                } else {
                    // connected to pipe instance
                    auto pipe_inst_iter = pipe_instances.find(conn.pipe_instance);
                    if (pipe_inst_iter == pipe_instances.end()) [[unlikely]] {
                        return EStr(EItemModPConnInvalidInst, prefix + string("Module-Pipe connection references non-existing pipe instance '") + conn.pipe_instance + "': " + conn.toString());
                    }
                    if (pipe_inst_iter->second.type != pipe_in_iter->second.type) [[unlikely]] {
                        return EStr(EItemModPConnMismatch, prefix + string("Module-Pipe connection has type mismatch between pipe instance '") + conn.pipe_instance + "' and instance pipe input port '" + conn.instance_pipe_port + "': " + conn.toString());
                    }
                }
            } else {
                // it's an output port
                if (conn.pipe_instance == TopInterface || !conn.top_pipe_port.empty()) {
                    // connected to top-level pipe output
                    auto top_pipe_out_iter = pipe_outputs.find(conn.top_pipe_port);
                    if (top_pipe_out_iter == pipe_outputs.end()) [[unlikely]] {
                        return EStr(EItemModPConnInvalidPort, prefix + string("Module-Pipe connection references non-existing top-level pipe output port '") + conn.top_pipe_port + "': " + conn.toString());
                    }
                    if (top_pipe_out_iter->second.type != pipe_out_iter->second.type) [[unlikely]] {
                        return EStr(EItemModPConnMismatch, prefix + string("Module-Pipe connection has type mismatch between top-level pipe output port '") + conn.top_pipe_port + "' and instance pipe output port '" + conn.instance_pipe_port + "': " + conn.toString());
                    }
                } else {
                    // connected to pipe instance
                    auto pipe_inst_iter = pipe_instances.find(conn.pipe_instance);
                    if (pipe_inst_iter == pipe_instances.end()) [[unlikely]] {
                        return EStr(EItemModPConnInvalidInst, prefix + string("Module-Pipe connection references non-existing pipe instance '") + conn.pipe_instance + "': " + conn.toString());
                    }
                    if (pipe_inst_iter->second.type != pipe_out_iter->second.type) [[unlikely]] {
                        return EStr(EItemModPConnMismatch, prefix + string("Module-Pipe connection has type mismatch between pipe instance '") + conn.pipe_instance + "' and instance pipe output port '" + conn.instance_pipe_port + "': " + conn.toString());
                    }
                }
            }
        }
    }

    for (const auto &inst_entry : instances) {
        const auto &inst = inst_entry.second;
        auto mod_iter = modulelib->modules.find(inst.module_name);
        if (mod_iter == modulelib->modules.end()) [[unlikely]] {
            continue;
        }
        const auto &childmod_ptr = mod_iter->second;
        auto conns_iter = mod_pipe_connections.find(inst.name);
        if (conns_iter == mod_pipe_connections.end()) {
            continue;
        }
        const auto &conns = conns_iter->second;
        
        auto countPipePortConnected = [&](const VulPipePort &port) -> uint32_t {
            uint32_t conn_count = 0;
            for (const auto &conn : conns) {
                if (conn.instance_pipe_port == port.name) {
                    conn_count++;
                }
            }
            return conn_count;
        };

        for (const auto &pipe_in_entry : childmod_ptr->pipe_inputs) {
            const auto &pipe_in = pipe_in_entry.second;
            uint32_t conn_count = countPipePortConnected(pipe_in);
            if (conn_count > 1) [[unlikely]] {
                return EStr(EItemModPConnChildPortMultiConn, prefix + string("Pipe input port '") + pipe_in.name + "' in instance '" + inst.name + "' has multiple connections but does not allow multi-connect.");
            }
            if (conn_count == 0) [[unlikely]] {
                return EStr(EItemModPConnChildPortNotConnected, prefix + string("Pipe input port '") + pipe_in.name + "' in instance '" + inst.name + "' is not connected but is not optional.");
            }
        }
        for (const auto &pipe_out_entry : childmod_ptr->pipe_outputs) {
            const auto &pipe_out = pipe_out_entry.second;
            uint32_t conn_count = countPipePortConnected(pipe_out);
            if (conn_count > 1) [[unlikely]] {
                return EStr(EItemModPConnChildPortMultiConn, prefix + string("Pipe output port '") + pipe_out.name + "' in instance '" + inst.name + "' has multiple connections but does not allow multi-connect.");
            }
            if (conn_count == 0) [[unlikely]] {
                return EStr(EItemModPConnChildPortNotConnected, prefix + string("Pipe output port '") + pipe_out.name + "' in instance '" + inst.name + "' is not connected but is not optional.");
            }
        }
    }

    return "";
}

ErrorMsg VulModule::_6_validateStallSequenceConnections(shared_ptr<VulConfigLib> configlib, shared_ptr<VulBundleLib> bundlelib, shared_ptr<VulModuleLib> modulelib) const {
    
    const string prefix = "Module '" + name + "': ";
    ErrorMsg err;

    unordered_set<InstanceName> update_seq_instances;
    unordered_map<InstanceName, unordered_set<InstanceName>> update_seq_graph;

    unordered_set<InstanceName> all_user_tick_instances;

    unordered_set<InstanceName> stall_instances;
    unordered_map<InstanceName, unordered_set<InstanceName>> stall_graph;

    stall_instances.insert(TopInterface);
    stall_graph[TopInterface] = unordered_set<InstanceName>();

    for (const auto &inst_entry : instances) {
        const auto &inst = inst_entry.second;
        update_seq_instances.insert(inst.name);
        update_seq_graph[inst.name] = unordered_set<InstanceName>();
        stall_instances.insert(inst.name);
        stall_graph[inst.name] = unordered_set<InstanceName>();
    }
    for (const auto &utick_entry : user_tick_codeblocks) {
        const auto &utick = utick_entry.second;
        update_seq_instances.insert(utick.name);
        update_seq_graph[utick.name] = unordered_set<InstanceName>();
        all_user_tick_instances.insert(utick.name);
    }

    for (const auto &conn : stalled_connections) {
        // check instances exist
        if (stall_instances.find(conn.former_instance) == stall_instances.end()) [[unlikely]] {
            return EStr(EItemModSConnInvalidInst, prefix + string("Stall connection references non-existing from_instance '") + conn.former_instance + "': " + conn.toString());
        }
        if (stall_instances.find(conn.latter_instance) == stall_instances.end()) [[unlikely]] {
            return EStr(EItemModSConnInvalidInst, prefix + string("Stall connection references non-existing to_instance '") + conn.latter_instance + "': " + conn.toString());
        }

        if (conn.former_instance == conn.latter_instance) [[unlikely]] {
            return EStr(EItemModSConnSelfLoop, prefix + string("Stall connection cannot connect an instance to itself: ") + conn.toString());
        }

        // add to stall graph
        stall_graph[conn.former_instance].insert(conn.latter_instance);

        // add to update sequence graph
        if (conn.former_instance != TopInterface) {
            if (conn.latter_instance != TopInterface) {
                update_seq_graph[conn.former_instance].insert(conn.latter_instance);
            } else {
                // add all user_tick instances to latter side
                update_seq_graph[conn.former_instance].insert(all_user_tick_instances.begin(), all_user_tick_instances.end());
            }
        }
    }

    vector<InstanceName> looped_instances;
    auto sorted_stall_instances = topologicalSort(stall_instances, stall_graph, looped_instances);
    if (sorted_stall_instances == nullptr) [[unlikely]] {
        string looped_str;
        for (const auto &inm : looped_instances) {
            if (!looped_str.empty()) looped_str += ", ";
            looped_str += "'" + inm + "'";
        }
        return EStr(EItemModSConnLoop, prefix + string("Stall connections have cyclic references: ") + looped_str);
    }

    for (const auto &conn : update_constraints) {
        // check instances exist
        if (update_seq_instances.find(conn.former_instance) == update_seq_instances.end()) [[unlikely]] {
            return EStr(EItemModUConnInvalidInst, prefix + string("Update sequence constraint references non-existing from_instance '") + conn.former_instance + "': " + conn.toString());
        }
        if (update_seq_instances.find(conn.latter_instance) == update_seq_instances.end()) [[unlikely]] {
            return EStr(EItemModUConnInvalidInst, prefix + string("Update sequence constraint references non-existing to_instance '") + conn.latter_instance + "': " + conn.toString());
        }

        if (conn.former_instance == conn.latter_instance) [[unlikely]] {
            return EStr(EItemModUConnSelfLoop, prefix + string("Update sequence constraint cannot connect an instance to itself: ") + conn.toString());
        }

        // add to update sequence graph
        update_seq_graph[conn.former_instance].insert(conn.latter_instance);
    }

    looped_instances.clear();
    auto sorted_update_seq_instances = topologicalSort(update_seq_instances, update_seq_graph, looped_instances);
    if (sorted_update_seq_instances == nullptr) [[unlikely]] {
        string looped_str;
        for (const auto &inm : looped_instances) {
            if (!looped_str.empty()) looped_str += ", ";
            looped_str += "'" + inm + "'";
        }
        return EStr(EItemModUConnLoop, prefix + string("Update sequence constraints have cyclic references: ") + looped_str);
    }

    return "";
}

unique_ptr<vector<InstanceName>> VulModule::getInstanceUpdateOrder(const vector<VulSequenceConnection> & additional_conn, vector<InstanceName> & looped_inst) const {
    
    unordered_set<InstanceName> update_seq_instances;
    unordered_map<InstanceName, unordered_set<InstanceName>> update_seq_graph;

    unordered_set<InstanceName> all_user_tick_instances;

    for (const auto &inst_entry : instances) {
        update_seq_instances.insert(inst_entry.first);
        update_seq_graph[inst_entry.first] = unordered_set<InstanceName>();
    }
    for (const auto &cb : user_tick_codeblocks) {
        all_user_tick_instances.insert(cb.first);
        update_seq_instances.insert(cb.first);
        update_seq_graph[cb.first] = unordered_set<InstanceName>();
    }

    auto addConn = [&](const VulSequenceConnection & conn) {
        if (conn.former_instance != TopInterface && update_seq_instances.find(conn.former_instance) == update_seq_instances.end()) {
            return;
        }
        if (conn.latter_instance != TopInterface && update_seq_instances.find(conn.latter_instance) == update_seq_instances.end()) {
            return;
        }
        if (conn.former_instance == conn.latter_instance) {
            return;
        }
        if (conn.former_instance == TopInterface) {
            // add all user_tick instances to former side
            for (const auto &cb : user_tick_codeblocks) {
                update_seq_graph[cb.first].insert(conn.latter_instance);
            }
        } else if (conn.latter_instance == TopInterface) {
            // add all user_tick instances to latter side
            update_seq_graph[conn.former_instance].insert(all_user_tick_instances.begin(), all_user_tick_instances.end());
        } else {
            update_seq_graph[conn.former_instance].insert(conn.latter_instance);
        }
    };

    for (const auto &conn : stalled_connections) {
        addConn(conn);
    }
    for (const auto &conn : update_constraints) {
        addConn(conn);
    }
    for (const auto &conn : additional_conn) {
        addConn(conn);
    }
    return topologicalSort(update_seq_instances, update_seq_graph, looped_inst);
}

vector<string> VulModule::debugPrintModule() const {
    vector<string> lines;
    lines.push_back("Module: " + name);
    // local_configs
    lines.push_back("  Local Configs:");
    for (const auto &lc_entry : local_configs) {
        const VulLocalConfigItem &lc = lc_entry.second;
        lines.push_back("    " + lc.name + " = " + lc.value);
    }
    // local_bundles
    lines.push_back("  Local Bundles:");
    for (const auto &lb_entry : local_bundles) {
        const VulBundleItem &lb = lb_entry.second;
        lines.push_back("    " + lb.name + (lb.is_alias ? " (alias)" : "") + (lb.enum_members.empty() ? "" : " (enum)"));
        for (const auto &member : lb.members) {
            string member_line = "      " + member.typeString() + " " + member.name;
            lines.push_back(member_line);
        }
        for (const auto &enum_member : lb.enum_members) {
            string enum_member_line = "      " + enum_member.name;
            if (!enum_member.value.empty()) {
                enum_member_line += " = " + enum_member.value;
            }
            lines.push_back(enum_member_line);
        }
    }
    // requests
    lines.push_back("  Requests:");
    for (const auto &req_entry : requests) {
        const VulReqServ &req = req_entry.second;
        lines.push_back("    " + req.signatureFull());
    }
    // services
    lines.push_back("  Services:");
    for (const auto &serv_entry : services) {
        const VulReqServ &serv = serv_entry.second;
        lines.push_back("    " + serv.signatureFull());
    }
    // pipe_inputs
    lines.push_back("  Pipe Inputs:");
    for (const auto &pi_entry : pipe_inputs) {
        const VulPipePort &pi = pi_entry.second;
        lines.push_back("    " + pi.type + " " + pi.name);
    }
    // pipe_outputs
    lines.push_back("  Pipe Outputs:");
    for (const auto &po_entry : pipe_outputs) {
        const VulPipePort &po = po_entry.second;
        lines.push_back("    " + po.type + " " + po.name);
    }
    // user_tick_codeblocks
    lines.push_back("  User Tick Codeblocks:");
    for (const auto &utick_entry : user_tick_codeblocks) {
        const auto &utick = utick_entry.second;
        lines.push_back("    " + utick.name);
    }
    // serv_codelines
    lines.push_back("  Service Codelines:");
    for (const auto &sc_entry : serv_codelines) {
        lines.push_back("    " + sc_entry.first);
    }
    // serv_cond_codelines
    lines.push_back("  Service Conditional Codelines:");
    for (const auto &scc_entry : serv_cond_codelines) {
        lines.push_back("    " + scc_entry.first);
    }
    // req_codelines
    lines.push_back("  Request Codelines:");
    for (const auto &rc_entry : req_codelines) {
        for (const auto &reqcode : rc_entry.second) {
            lines.push_back("    " + rc_entry.first + " - " + reqcode.first);
        }
    }
    // storages
    lines.push_back("  Storages:");
    for (const auto &st_entry : storages) {
        const VulStorage &st = st_entry.second;
        lines.push_back("    " + st.typeString() + " " + st.name + (st.value.empty() ? "" : (" = " + st.value)));
    }
    // storagenexts
    lines.push_back("  Storagenexts:");
    for (const auto &stn_entry : storagenexts) {
        const VulStorage &stn = stn_entry.second;
        lines.push_back("    " + stn.typeString() + " " + stn.name + (stn.value.empty() ? "" : (" = " + stn.value)));
    }
    // storagetmps
    lines.push_back("  Storagetmps:");
    for (const auto &stt_entry : storagetmp) {
        const VulStorage &stt = stt_entry.second;
        lines.push_back("    " + stt.typeString() + " " + stt.name + (stt.value.empty() ? "" : (" = " + stt.value)));
    }
    // instances
    lines.push_back("  Instances:");
    for (const auto &inst_entry : instances) {
        const VulInstance &inst = inst_entry.second;
        lines.push_back("    " + inst.name + " : " + inst.module_name);
        for (const auto &lc_override : inst.local_config_overrides) {
            lines.push_back("      Local Config Override: " + lc_override.first + " = " + lc_override.second);
        }
    }
    // pipe_instances
    lines.push_back("  Pipe Instances:");
    for (const auto &pipe_inst_entry : pipe_instances) {
        const auto &pipe_inst = pipe_inst_entry.second;
        lines.push_back("    " + pipe_inst.name + " : " + pipe_inst.type);
    }
    // req_connections
    lines.push_back("  Req-Serv Connections:");
    for (const auto &rconn_entry : req_connections) {
        const auto &rconns = rconn_entry.second;
        for (const auto &conn : rconns) {
            lines.push_back("    " + conn.toString());
        }
    }
    // mod_pipe_connections
    lines.push_back("  Module-Pipe Connections:");
    for (const auto &pconn_entry : mod_pipe_connections) {
        const auto &pconns = pconn_entry.second;
        for (const auto &conn : pconns) {
            lines.push_back("    " + conn.toString());
        }
    }
    // stalled_connections
    lines.push_back("  Stall Connections:");
    for (const auto &conn : stalled_connections) {
        lines.push_back("    " + conn.toString());
    }
    // update_constraints
    lines.push_back("  Update Sequence Constraints:");
    for (const auto &conn : update_constraints) {
        lines.push_back("    " + conn.toString());
    }
    return lines;
}

void detectRequestCallInLogicBlocks(VulStaticModuleInstance &module_instance) {
    unordered_map<string, LogicBlockCall> valid_function_names;
    for (const auto &req_entry : module_instance.requests) {
        LogicBlockCall call;
        call.instance = "";
        call.port = req_entry.first;
        valid_function_names[req_entry.first] = call;
    }
    for (const auto &inst_entry : module_instance.instances) {
        const auto &inst = inst_entry.second;
        for (const auto &serv : inst.referenced_services) {
            LogicBlockCall call;
            call.instance = inst_entry.first;
            call.port = serv;
            valid_function_names[inst_entry.first + "_" + serv] = call;
        }
    }
    for (auto &serv_entry: module_instance.serv_logic_blocks) {
        auto &serv_lb = serv_entry.second;
        for (auto &func_entry : valid_function_names) {
            if (cppparse::codeblockContainsFunctionCall(serv_lb.codelines, func_entry.first)) {
                serv_lb.call_requests.push_back(func_entry.second);
            }
        }
    }
    for (auto &tb: module_instance.tick_blocks) {
        for (auto &func_entry : valid_function_names) {
            if (cppparse::codeblockContainsFunctionCall(tb.codelines, func_entry.first)) {
                tb.call_requests.push_back(func_entry.second);
            }
        }
    }
}


uint64_t findConnectedLogicBlockID(shared_ptr<VulStaticModuleInstance> instance, const LogicBlockCall &call) {
    // 顺着指针关系和req_connections成员找到这个call最终连接到的serv_logic_block的ID，返回这个ID（高32位为instance_id，低32位为block_id）。由于连接的单一性，一个call最多只能连接到一个serv_logic_block，如果找不到连接的serv_logic_block，抛出异常并退出

    auto find_child_ptr_by_name = [](shared_ptr<VulStaticModuleInstance> &mod, const InstanceName &child_name) -> shared_ptr<VulStaticModuleInstance> {
        for (auto &child : mod->children) {
            if (!child->instance_path.empty() && child->instance_path.back() == child_name) {
                return child;
            }
        }
        return nullptr;
    };
    auto pack_lb_id = [](uint32_t instance_id, uint32_t block_id) -> uint64_t {
        return ((uint64_t)instance_id << 32) | block_id;
    };

    shared_ptr<VulStaticModuleInstance> cur = instance;
    ReqServName port;
    bool is_serv_call = false;

    if (call.instance.empty()) {
        // it's a request call
        is_serv_call = false;
        port = call.port;
    } else {
        // it's a service call
        is_serv_call = true;
        port = call.port;
        cur = find_child_ptr_by_name(cur, call.instance);
        if (cur == nullptr) {
            throw VulException("Invalid LogicBlockCall: instance '" + call.instance + "' not found in instance '" + instance->instance_path.back() + "'");
        }
    }

    for (uint32_t hop = 0; hop < 4096; hop ++) {
        VulErrorContextGuard hop_guard("Entering instance '" + cur->simClassName() + "' with port '" + port + "'");
        if (is_serv_call) {
            // impl as code block here, or connected to a child service
            auto lb_iter = cur->serv_logic_blocks.find(port);
            if (lb_iter != cur->serv_logic_blocks.end()) {
                return pack_lb_id(cur->instance_id, lb_iter->second.block_id);
            } else {
                // find connected child service
                bool found_conn = false;
                for (const auto &conn : cur->req_connections) {
                    if (conn.req_instance == VulModule::TopInterface && conn.req_name == port) {
                        cur = find_child_ptr_by_name(cur, conn.serv_instance);
                        if (cur == nullptr) {
                            throw VulException("Invalid Req-Serv connection: instance '" + conn.serv_instance + "' not found in instance '" + instance->instance_path.back() + "'");
                        }
                        port = conn.serv_name;
                        found_conn = true;
                        break;
                    }
                }
                if (!found_conn) {
                    throw VulException("No connected service found for LogicBlockCall to service '" + port + "' in instance '" + instance->instance_path.back() + "'");
                }
            }
            continue;
        }
        // request call, should be connected at parent level
        auto parent = cur->parent;
        // CR-R: connected to parent's request
        // CR-S: connected to parent's service
        // CR-CS: connected to another child's service
        bool found_conn = false;
        for (const auto &conn : parent->req_connections) {
            if (conn.req_instance == cur->instance_path.back() && conn.req_name == port) {
                if (conn.serv_instance == VulModule::TopInterface) {
                    // connected to parent's service/request
                    cur = parent;
                    port = conn.serv_name;
                    is_serv_call = (parent->requests.find(conn.serv_name) == parent->requests.end());
                } else {
                    // connected to another child's service
                    cur = find_child_ptr_by_name(parent, conn.serv_instance);
                    if (cur == nullptr) {
                        throw VulException("Invalid Req-Serv connection: instance '" + conn.serv_instance + "' not found in instance '" + parent->instance_path.back() + "'");
                    }
                    port = conn.serv_name;
                    is_serv_call = true;
                }
                found_conn = true;
                break;
            }
        }
        if (!found_conn) {
            throw VulException("No connected service found for LogicBlockCall to request '" + port + "' in instance '" + instance->instance_path.back() + "'");
        }
    }
    throw VulException("Exceeded maximum hop count while finding connected logic block for LogicBlockCall to '" + port + "' in instance '" + instance->instance_path.back() + "'. Possible cyclic connections.");
}

void setupUpdateSequence(shared_ptr<VulStaticModuleInstance> &top) {

    VulErrorContextGuard top_guard("Setting up update sequence for instance '" + top->simClassName() + "'");

    unordered_set<uint64_t> all_logic_block_ids;
    unordered_map<uint64_t, unordered_set<uint64_t>> logic_block_call_graph;
    unordered_map<VulInstanceID, shared_ptr<VulStaticModuleInstance>> instance_id_map;

    std::deque<shared_ptr<VulStaticModuleInstance>> bfs_queue;
    bfs_queue.push_back(top);
    while (!bfs_queue.empty()) {
        auto cur_inst = bfs_queue.front();
        bfs_queue.pop_front();

        instance_id_map[cur_inst->instance_id] = cur_inst;

        VulErrorContextGuard inst_guard("Parsing connection from instance '" + cur_inst->simClassName() + "' (IID: " + std::to_string(cur_inst->instance_id) + ")");

        for (const auto &serv_lb_entry : cur_inst->serv_logic_blocks) {
            const auto &serv_lb = serv_lb_entry.second;
            VulErrorContextGuard lb_guard("Parsing service logic block '" + serv_lb_entry.first + "' (BID: " + std::to_string(serv_lb.block_id) + ")");
            uint64_t lb_id = ((uint64_t)cur_inst->instance_id << 32) | serv_lb.block_id;
            all_logic_block_ids.insert(lb_id);
            for (const auto &req_call : serv_lb.call_requests) {
                uint64_t called_lb_id = findConnectedLogicBlockID(cur_inst, req_call);
                logic_block_call_graph[lb_id].insert(called_lb_id);
            }
        }
        for (const auto &tick_lb : cur_inst->tick_blocks) {
            VulErrorContextGuard lb_guard("Parsing tick logic block (BID: 0)");
            uint64_t lb_id = ((uint64_t)cur_inst->instance_id << 32);
            all_logic_block_ids.insert(lb_id);
            for (const auto &req_call : tick_lb.call_requests) {
                uint64_t called_lb_id = findConnectedLogicBlockID(cur_inst, req_call);
                logic_block_call_graph[lb_id].insert(called_lb_id);
            }
        }
    }

    auto debug_lb_name_by_id = [&](uint64_t lb_id) -> string {
        VulInstanceID inst_id = lb_id >> 32;
        uint32_t block_id = lb_id & 0xFFFFFFFF;
        auto inst_iter = instance_id_map.find(inst_id);
        if (inst_iter == instance_id_map.end()) {
            return "<unknown instance " + std::to_string(inst_id) + ">";
        }
        auto &inst_ptr = inst_iter->second;
        string instance_path_str;
        for (const auto &path_elem : inst_ptr->instance_path) {
            if (!instance_path_str.empty()) instance_path_str += "::";
            instance_path_str += path_elem;
        }
        if (block_id == 0) {
            return instance_path_str + ".<tick>";
        }
        for (auto &serv_lb_entry : inst_ptr->serv_logic_blocks) {
            auto &serv_lb = serv_lb_entry.second;
            if (serv_lb.block_id == block_id) {
                return instance_path_str + "." + serv_lb_entry.first;
            }
        }
        return instance_path_str + ".<unknown>";
    };

    VulErrorContextGuard graph_guard("Checking logic block call graph");
    unordered_map<VulInstanceID, vector<uint64_t>> instance_tick_to_lb_call_graph;
    // logic_block_call_graph 是 {tick_code_blocks, serv_logic_blocks} 之间的调用关系图，instance_tick_to_lb_call_graph 是从实例的 tick_code_block 到被所有可能被递归调用的 logic_block 的调用关系图
    for (const auto &inst_entry : instance_id_map) {
        const auto &inst_id = inst_entry.first;
        const auto &inst_ptr = inst_entry.second;
        uint64_t tick_lb_id = ((uint64_t)inst_id << 32);
        unordered_set<uint64_t> visited;
        struct CallPathNode {
            uint64_t lb_id;
            vector<uint64_t> call_path; // for debugging
        };
        std::deque<CallPathNode> bfs_queue;
        bfs_queue.push_back({tick_lb_id, {tick_lb_id}});
        while (!bfs_queue.empty()) {
            auto [cur_lb_id, call_path] = bfs_queue.front();
            bfs_queue.pop_front();
            if (visited.find(cur_lb_id) != visited.end()) {
                string loop_str;
                for (const auto &lb_id : call_path) {
                    if (!loop_str.empty()) loop_str += " -> ";
                    loop_str += debug_lb_name_by_id(lb_id);
                }
                throw VulException("Cyclic call or repeated call detected in logic block call graph: " + loop_str);
            }
            visited.insert(cur_lb_id);
            const auto &called_lbs_set = logic_block_call_graph.find(cur_lb_id);
            if (called_lbs_set != logic_block_call_graph.end()) {
                for (const auto &called_lb_id : called_lbs_set->second) {
                    vector<uint64_t> new_call_path = call_path;
                    new_call_path.push_back(called_lb_id);
                    bfs_queue.push_back({called_lb_id, new_call_path});
                }
            }
        }
    }

    unordered_map<uint64_t, VulInstanceID> logic_block_id_to_instance_id;
    // 反向映射一下，同时保证每一个serv codeblock仅会被最多一个tick codeblock调用，否则这个重复调用行为在硬件上是无法实现的
    for (const auto &entry : instance_tick_to_lb_call_graph) {
        const auto &inst_id = entry.first;
        const auto &called_lb_ids = entry.second;
        for (const auto &lb_id : called_lb_ids) {
            auto iter = logic_block_id_to_instance_id.find(lb_id);
            if (iter != logic_block_id_to_instance_id.end() && iter->second != inst_id) {
                string called_inst1 = debug_lb_name_by_id(static_cast<uint64_t>(iter->second) << 32);
                string called_inst2 = debug_lb_name_by_id(static_cast<uint64_t>(inst_id) << 32);
                string lb_name = debug_lb_name_by_id(lb_id);
                throw VulException("Logic block '" + lb_name + "' is called by multiple instances: '" + called_inst1 + "' and '" + called_inst2 + "'. This is not allowed because it cannot be implemented in hardware.");
            }
            logic_block_id_to_instance_id[lb_id] = inst_id;
        }
    }

    VulErrorContextGuard order_guard("Determining instance update order");

    unordered_map<VulInstanceID, unordered_set<VulInstanceID>> instance_order_graph;
    bfs_queue.clear();
    bfs_queue.push_back(top);
    while (!bfs_queue.empty()) {
        auto cur_inst = bfs_queue.front();
        bfs_queue.pop_front();
        VulInstanceID cur_inst_id = cur_inst->instance_id;
        map<int32_t, vector<VulInstanceID>> priority_to_callee_instances;
        priority_to_callee_instances[0].push_back(cur_inst_id); // tick block has default priority 0

        VulErrorContextGuard inst_guard("Processing instance '" + cur_inst->simClassName() + "' (IID: " + std::to_string(cur_inst_id) + ") for update order");

        for (const auto &serv_lb_entry : cur_inst->serv_logic_blocks) {
            const auto &serv_lb = serv_lb_entry.second;
            if (!serv_lb.with_priority) {
                continue;
            }
            uint64_t lb_id = ((uint64_t)cur_inst->instance_id << 32) | serv_lb.block_id;
            auto callee_inst_iter = logic_block_id_to_instance_id.find(lb_id);
            if (callee_inst_iter == logic_block_id_to_instance_id.end()) {
                // not called by any tick block, skip
                continue;
            }
            VulInstanceID callee_inst_id = callee_inst_iter->second;
            if (callee_inst_id == cur_inst_id) {
                string lb_name = debug_lb_name_by_id(lb_id);
                throw VulException("Logic block '" + lb_name + "' is called by its own tick block.");
            }
            priority_to_callee_instances[serv_lb.priority].push_back(callee_inst_id);
        }
        
        unordered_set<VulInstanceID> lower_priority_instance_set;
        vector<VulInstanceID> lower_priority_instances;
        for (auto prio_it = priority_to_callee_instances.rbegin(); prio_it != priority_to_callee_instances.rend(); ++prio_it) {
            auto &same_priority_instances = prio_it->second;
            if (same_priority_instances.empty()) {
                continue;
            }
            std::sort(same_priority_instances.begin(), same_priority_instances.end());
            same_priority_instances.erase(std::unique(same_priority_instances.begin(), same_priority_instances.end()), same_priority_instances.end());

            if (!lower_priority_instances.empty()) {
                for (VulInstanceID from_inst_id : same_priority_instances) {
                    auto &out_edges = instance_order_graph[from_inst_id];
                    for (VulInstanceID to_inst_id : lower_priority_instances) {
                        if (from_inst_id != to_inst_id) {
                            out_edges.insert(to_inst_id);
                        }
                    }
                }
            }

            for (VulInstanceID inst_id : same_priority_instances) {
                if (lower_priority_instance_set.insert(inst_id).second) {
                    lower_priority_instances.push_back(inst_id);
                }
            }
        }
    }

    unordered_map<VulInstanceID, unordered_map<VulInstanceID, unordered_set<VulInstanceID>>> child_instance_order_graph; // instance_id -> child_instance_id -> set of child_instance_id that should be updated after this child_instance_id
    for (const auto &entry : instance_order_graph) {
        const auto &former_inst_id = entry.first;
        const auto &latter_inst_ids = entry.second;
        auto former_inst_iter = instance_id_map.find(former_inst_id);
        if (former_inst_iter == instance_id_map.end()) {
            throw VulException("Instance ID " + std::to_string(former_inst_id) + " not found in instance_id_map");
        }
        shared_ptr<VulStaticModuleInstance> former_inst_ptr = former_inst_iter->second;
        for (const auto &latter_inst_id : latter_inst_ids) {
            auto latter_inst_iter = instance_id_map.find(latter_inst_id);
            if (latter_inst_iter == instance_id_map.end()) {
                throw VulException("Instance ID " + std::to_string(latter_inst_id) + " not found in instance_id_map");
            }
            shared_ptr<VulStaticModuleInstance> latter_inst_ptr = latter_inst_iter->second;
            
            size_t former_depth = former_inst_ptr->instance_path.size();
            size_t latter_depth = latter_inst_ptr->instance_path.size();
            shared_ptr<VulStaticModuleInstance> former_ancestor = former_inst_ptr;
            shared_ptr<VulStaticModuleInstance> latter_ancestor = latter_inst_ptr;
            VulInstanceID former_last_id = former_inst_id;
            VulInstanceID latter_last_id = latter_inst_id;

            while (former_depth > latter_depth) {
                former_last_id = former_ancestor->instance_id;
                former_ancestor = former_ancestor->parent;
                --former_depth;
            }
            while (latter_depth > former_depth) {
                latter_last_id = latter_ancestor->instance_id;
                latter_ancestor = latter_ancestor->parent;
                --latter_depth;
            }
            while (former_ancestor.get() != latter_ancestor.get()) {
                if (former_ancestor == nullptr || latter_ancestor == nullptr) {
                    throw VulException("Cannot find common ancestor for instance IDs " + std::to_string(former_inst_id) + " and " + std::to_string(latter_inst_id));
                }
                former_last_id = former_ancestor->instance_id;
                latter_last_id = latter_ancestor->instance_id;
                former_ancestor = former_ancestor->parent;
                latter_ancestor = latter_ancestor->parent;
            }
            if (former_ancestor == nullptr) {
                throw VulException("Cannot find common ancestor for instance IDs " + std::to_string(former_inst_id) + " and " + std::to_string(latter_inst_id));
            }

            shared_ptr<VulStaticModuleInstance> common_ancestor = former_ancestor;
            if (former_last_id != latter_last_id) {
                child_instance_order_graph[common_ancestor->instance_id][former_last_id].insert(latter_last_id);
            }
        }
    }

    bfs_queue.clear();
    bfs_queue.push_back(top);
    while (!bfs_queue.empty()) {
        auto cur_inst = bfs_queue.front();
        bfs_queue.pop_front();

        const VulInstanceID cur_inst_id = cur_inst->instance_id;
        vector<VulInstanceID> update_nodes;
        update_nodes.reserve(cur_inst->children.size() + 1);
        update_nodes.push_back(cur_inst_id); // self ID represents local tick
        for (const auto &child : cur_inst->children) {
            update_nodes.push_back(child->instance_id);
            bfs_queue.push_back(child);
        }

        unordered_map<VulInstanceID, int32_t> indegree;
        indegree.reserve(update_nodes.size() * 2);
        for (VulInstanceID node_id : update_nodes) {
            indegree.emplace(node_id, 0);
        }

        unordered_map<VulInstanceID, vector<VulInstanceID>> adj;
        auto order_it = child_instance_order_graph.find(cur_inst_id);
        if (order_it != child_instance_order_graph.end()) {
            adj.reserve(order_it->second.size());
            for (const auto &from_entry : order_it->second) {
                VulInstanceID from_id = from_entry.first;
                if (indegree.find(from_id) == indegree.end()) {
                    continue;
                }
                auto &out_edges = adj[from_id];
                out_edges.reserve(from_entry.second.size());
                for (VulInstanceID to_id : from_entry.second) {
                    auto indeg_it = indegree.find(to_id);
                    if (indeg_it == indegree.end() || to_id == from_id) {
                        continue;
                    }
                    out_edges.push_back(to_id);
                    indeg_it->second += 1;
                }
            }
        }

        std::priority_queue<VulInstanceID, vector<VulInstanceID>, std::greater<VulInstanceID>> ready;
        for (const auto &entry : indegree) {
            if (entry.second == 0) {
                ready.push(entry.first);
            }
        }

        cur_inst->update_seq.clear();
        cur_inst->update_seq.reserve(update_nodes.size());

        while (!ready.empty()) {
            VulInstanceID from_id = ready.top();
            ready.pop();
            cur_inst->update_seq.push_back(from_id);

            auto adj_it = adj.find(from_id);
            if (adj_it == adj.end()) {
                continue;
            }
            for (VulInstanceID to_id : adj_it->second) {
                auto indeg_it = indegree.find(to_id);
                if (indeg_it == indegree.end()) {
                    continue;
                }
                indeg_it->second -= 1;
                if (indeg_it->second == 0) {
                    ready.push(to_id);
                }
            }
        }

        if (cur_inst->update_seq.size() != update_nodes.size()) {
            throw VulException("Cyclic update constraints found in instance update order graph for instance ID " + std::to_string(cur_inst_id));
        }
    }

}

