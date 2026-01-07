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

