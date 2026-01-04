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

#include "project.h"

namespace operation_configlib_listref {

class ConfigLibListRefOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;

    virtual vector<string> help() const override {
        return {
            "ConfigLib ListRef Operation:",
            "List all references recursively for a configuration item in the project's configuration library.",
            "Arguments:",
            "  - [0] name: The name of the configuration item to list references for.",
            "  - [1] reverse (optional): If set to 'true', list reverse references instead of forward references.",
            "Results:",
            "  - names: List of configuration item names.",
            "  - values: List of raw values for each configuration item.",
            "  - realvalues: List of evaluated real values for each configuration item.",
            "  - childs: List of references (or reverse references) for each configuration item. Separated by spaces.",
        };
    }
};

VulOperationResponse ConfigLibListRefOperation::execute(VulProject &project) {
    string name;
    bool reverse = false;
    auto &configlib = project.configlib;

    {
        auto name_arg = op.getArg("name", 0);
        if (!name_arg) {
            return EStr(EOPConfListRefMissArg, "Missing argument: [0] name");
        }
        name = *name_arg;

        reverse = op.getBoolArg("reverse", 1);
    }

    if (configlib->config_items.find(name) == configlib->config_items.end()) {
        return EStr(EOPConfListRefNameNotFound, "Configuration item '" + name + "' does not exist.");
    }

    vector<string> names;
    vector<string> childs;
    vector<string> values;
    vector<string> realvalues;

    std::deque<ConfigName> to_process;
    unordered_set<ConfigName> processed;
    to_process.push_back(name);
    processed.insert(name);

    while (!to_process.empty()) {
        ConfigName current_name = to_process.front();
        to_process.pop_front();

        auto iter = configlib->config_items.find(current_name);
        if (iter == configlib->config_items.end()) {
            // Should not happen
            continue;
        }
        const auto &confe = iter->second;
        string child_str;

        if (reverse) {
            for (const auto & rev_ref : confe.reverse_references) {
                if (!child_str.empty()) child_str += " ";
                child_str += rev_ref;
                if (processed.find(rev_ref) == processed.end()) {
                    to_process.push_back(rev_ref);
                    processed.insert(rev_ref);
                }
            }
        } else {
            for (const auto & ref : confe.references) {
                if (!child_str.empty()) child_str += " ";
                child_str += ref;
                if (processed.find(ref) == processed.end()) {
                    to_process.push_back(ref);
                    processed.insert(ref);
                }
            }
        }
        names.push_back(current_name);
        childs.push_back(child_str);
        values.push_back(confe.item.value);
        realvalues.push_back(std::to_string(confe.real_value));
    }

    VulOperationResponse resp;
    resp.list_results["names"] = std::move(names);
    resp.list_results["childs"] = std::move(childs);
    resp.list_results["values"] =  std::move(values);
    resp.list_results["realvalues"] = std::move(realvalues);
    return resp;
}


auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ConfigLibListRefOperation>(op);
};
struct RegisterConfigLibListRefOperation {
    RegisterConfigLibListRefOperation() {
        VulProject::registerOperation("configlib_listref", factory);
    }
} registerConfigLibListRefOperationInstance;

} // namespace operation_configlib_listref
