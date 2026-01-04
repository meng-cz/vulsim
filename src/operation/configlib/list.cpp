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

namespace operation_configlib_list {

class ConfigLibListOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;
    
    virtual VulOperationResponse execute(VulProject &project) override;

    virtual vector<string> help() const override {
        return {
            "ConfigLib List Operation:",
            "List all configuration item names in the project's configuration library.",
            "",
            "Arguments:",
            "  - [0] name (optional): If provided, only list configuration items whose names contain this substring.",
            "  - [1] group (optional): If provided, only list configuration items that belong to this group.",
            "  - [2] exact (optional): If set to 'true', only list configuration items whose names exactly match the provided name.",
            "  - [3] reference (optional): If set to 'true', include references and reverse references in the output.",
            "Results:",
            "  - names: List of configuration item names.",
            "  - groups: List of groups each configuration item belongs to."
            "  - realvalues: List of evaluated real values for each configuration item."
            "  - comments: List of comments for each configuration item."
            "  - values: List of raw values for each configuration item."
            "  - references: List of references for each configuration item. Separated by spaces.",
            "  - reverse_references: List of configuration items that reference each configuration item. Separated by spaces.",
        };
    }
};

VulOperationResponse ConfigLibListOperation::execute(VulProject &project) {
    string name_filter = "";
    string group_filter = "";

    {
        auto name_arg = op.getArg("name", 0);
        if (name_arg) {
            name_filter = *name_arg;
        }
        auto group_arg = op.getArg("group", 1);
        if (group_arg) {
            group_filter = *group_arg;
        }
    }
    bool exact_match = op.getBoolArg("exact", 2);
    bool include_references = op.getBoolArg("reference", 3);

    vector<string> names;
    vector<string> groups;
    vector<string> realvalues;
    vector<string> comments;
    vector<string> values;
    vector<string> references;
    vector<string> reverse_references;

    for (const auto &pair : project.configlib->config_items) {
        const auto &confe = pair.second;
        const auto &item = pair.second.item;

        if (!name_filter.empty()) {
            if (exact_match) {
                if (item.name != name_filter) {
                    continue;
                }
            } else {
                if (item.name.find(name_filter) == string::npos) {
                    continue;
                }
            }
        }
        if (!group_filter.empty() && confe.group != group_filter) {
            continue;
        }
        names.push_back(item.name);
        groups.push_back(confe.group);
        realvalues.push_back(std::to_string(confe.real_value));
        comments.push_back(item.comment);
        values.push_back(item.value);
        if (include_references) {
            string refs;
            for (const auto &ref : confe.references) {
                if (!refs.empty()) refs += " ";
                refs += ref;
            }
            references.push_back(refs);
            string rev_refs;
            for (const auto &rev_ref : confe.reverse_references) {
                if (!rev_refs.empty()) rev_refs += " ";
                rev_refs += rev_ref;
            }
            reverse_references.push_back(rev_refs);
        }
    }

    VulOperationResponse resp;
    resp.list_results["names"] = std::move(names);
    resp.list_results["groups"] = std::move(groups);
    resp.list_results["realvalues"] = std::move(realvalues);
    resp.list_results["comments"] = std::move(comments);
    resp.list_results["values"] = std::move(values);
    if (include_references) {
        resp.list_results["references"] = std::move(references);
        resp.list_results["reverse_references"] = std::move(reverse_references);
    }
    return resp;
}


auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ConfigLibListOperation>(op);
};
struct RegisterConfigLibListOperation {
    RegisterConfigLibListOperation() {
        VulProject::registerOperation("configlib.list", factory);
    }
} registerConfigLibListOperationInstance;


} // namespace operation_configlib_list
