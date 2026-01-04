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

namespace operation_bundlelib_list {

class BundleLibListOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;
    
    virtual VulOperationResponse execute(VulProject &project) override;
    virtual vector<string> help() const override {
        return {
            "BundleLib List Operation:",
            "List all bundles in the project's bundle library.",
            "",
            "Arguments:",
            "  - [0] name (optional): If provided, only list bundles whose names contain this substring.",
            "  - [1] tag (optional): If provided, only list bundles that contain this tag.",
            "  - [2] exact (optional): If set to 'true', only list bundles whose names exactly match the provided name.",
            "  - [3] reference (optional): If set to 'true', include references and reverse references in the output.",
            "  - [4] definition (optional): If set to 'true', include JSON definitions in the output.",
            "Results:",
            "  - names: List of bundle names.",
            "  - comments: List of comments for each bundle.",
            "  - tags: List of tags for each bundle. Separated by spaces.",
            "  - definitions: List of JSON definitions for each bundle.",
            "  - references: List of references for each bundle. Separated by spaces.",
            "  - config_references: List of references for each bundle. Separated by spaces.",
            "  - reverse_references: List of bundles that reference each bundle. Separated by spaces.",
        };
    }
};

VulOperationResponse BundleLibListOperation::execute(VulProject &project) {
    string name_filter = "";
    string tag_filter = "";
    {
        auto name_arg = op.getArg("name", 0);
        if (name_arg) {
            name_filter = *name_arg;
        }
        auto tag_arg = op.getArg("tag", 1);
        if (tag_arg) {
            tag_filter = *tag_arg;
        }
    }
    bool exact_match = op.getBoolArg("exact", 2);
    bool include_references = op.getBoolArg("reference", 3);
    bool include_definition = op.getBoolArg("definition", 4);

    vector<string> names;
    vector<string> comments;
    vector<string> tags;
    vector<string> definitions;
    vector<string> references;
    vector<string> config_references;
    vector<string> reverse_references;

    for (const auto &pair : project.bundlelib->bundles) {
        const BundleName &bunde_name = pair.first;
        const VulBundleLib::BundleEntry &bunde_entry = pair.second;

        if (!name_filter.empty()) {
            if (exact_match) {
                if (bunde_name != name_filter) {
                    continue;
                }
            } else {
                if (bunde_name.find(name_filter) == string::npos) {
                    continue;
                }
            }
        }
        if (!tag_filter.empty()) {
            if (bunde_entry.tags.find(tag_filter) == bunde_entry.tags.end()) {
                continue;
            }
        }
        names.push_back(bunde_name);
        comments.push_back(bunde_entry.item.comment);
        string tag_str;
        for (const auto &tag : bunde_entry.tags) {
            if (!tag_str.empty()) tag_str += " ";
            tag_str += tag;
        }
        tags.push_back(tag_str);
        if (include_definition) {
            definitions.push_back(bunde_entry.item.toMemberJson());
        }
        if (include_references) {
            string refs;
            for (const auto &ref : bunde_entry.references) {
                if (!refs.empty()) refs += " ";
                refs += ref;
            }
            references.push_back(refs);
            string conf_refs;
            for (const auto &conf_ref : bunde_entry.confs) {
                if (!conf_refs.empty()) conf_refs += " ";
                conf_refs += conf_ref;
            }
            config_references.push_back(conf_refs);
            string rev_refs;
            for (const auto &rev_ref : bunde_entry.reverse_references) {
                if (!rev_refs.empty()) rev_refs += " ";
                rev_refs += rev_ref;
            }
            reverse_references.push_back(rev_refs);
        }
    }
    VulOperationResponse resp;
    resp.list_results["names"] = std::move(names);
    resp.list_results["comments"] = std::move(comments);
    resp.list_results["tags"] = std::move(tags);
    if (include_definition) {
        resp.list_results["definitions"] = std::move(definitions);
    }
    if (include_references) {
        resp.list_results["references"] = std::move(references);
        resp.list_results["config_references"] = std::move(config_references);
        resp.list_results["reverse_references"] = std::move(reverse_references);
    }
    return resp;
}

auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return make_unique<BundleLibListOperation>(op);
};
struct Register {
    Register() {
        VulProject::registerOperation("bundlelib.list", factory);
    }
} register_instance;


} // namespace operation_bundlelib_list
