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

using std::make_shared;

/**
 * @brief Get the singleton instance of the module library.
 * @return A shared_ptr to the module library instance.
 */
shared_ptr<unordered_map<ModuleName, shared_ptr<VulModuleBase>>> VulModuleBase::getModuleLibInstance() {
    static shared_ptr<unordered_map<ModuleName, shared_ptr<VulModuleBase>>> _singleton_instance;
    if (!_singleton_instance) {
        _singleton_instance = make_shared<unordered_map<ModuleName, shared_ptr<VulModuleBase>>>();
    }
    return _singleton_instance;
}

ErrorMsg VulModuleBase::updateDynamicReferences() {
    _dyn_referenced_configs.clear();
    _dyn_referenced_bundles.clear();
    _dyn_referenced_modules.clear();
    for (const auto &lc_entry : local_configs) {
        const VulLocalConfigItem &lc = lc_entry.second;
        string err;
        uint32_t errpos = 0;
        auto conf_refs = config_parser::parseReferencedIdentifier(lc.value, errpos, err);
        if (!err.empty()) {
            return EStr(EItemModConfInvalidValue, string("Invalid local config value expression for '") + lc.name + "': " + err);
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
        if (!rs.array_size.empty()) {
            string err;
            uint32_t errpos = 0;
            auto conf_refs = config_parser::parseReferencedIdentifier(rs.array_size, errpos, err);
            if (!err.empty()) {
                return EStr(EItemModConfInvalidValue, string("Invalid array size expression for '") + rs.name + "': " + err);
            }
            for (const auto &cn : *conf_refs) {
                _dyn_referenced_configs.insert(cn);
            }
        }
        return "";
    };
    for (const auto &req_entry : requests) {
        string err = parseReqServReferences(req_entry.second);
    }
    for (const auto &serv_entry : services) {
        string err = parseReqServReferences(serv_entry.second);
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
                return EStr(EItemModConfInvalidValue, string("Invalid uint length expression for storage '") + st.name + "': " + err);
            }
        } else if (!isBasicVulType(st.type)) {
            _dyn_referenced_bundles.insert(st.type);
        }
        for (const auto &dim_expr : st.dims) {
            err = parseConfigItem(dim_expr);
            if (!err.empty()) {
                return EStr(EItemModConfInvalidValue, string("Invalid dimension expression for storage '") + st.name + "': " + err);
            }
        }
        if (!st.value.empty()) {
            err = parseConfigItem(st.value);
            if (!err.empty()) {
                return EStr(EItemModConfInvalidValue, string("Invalid initial value expression for storage '") + st.name + "': " + err);
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
                return EStr(EItemModConfInvalidValue, string("Invalid local config override expression for instance '") + inst.name + "', config item '" + lc_override.first + "': " + err);
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


