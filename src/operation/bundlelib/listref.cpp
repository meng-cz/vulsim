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

namespace operation_bundlelib_listref {

class BundleLibListRefOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;

    virtual vector<string> help() const override {
        return {
            "BundleLib ListRef Operation:",
            "List all references recursively for a bundle in the project's bundle library.",
            "Arguments:",
            "  - [0] name: The name of the bundle to list references for.",
            "  - [1] reverse (optional): If set to 'true', list reverse references instead of forward references.",
            "Results:",
            "  - names: List of bundle names.",
            "  - childs: List of references (or reverse references) for each bundle. Separated by spaces.",
        };
    }
};

VulOperationResponse BundleLibListRefOperation::execute(VulProject &project) {
    string name;
    auto &bundlelib = project.bundlelib;

    {
        auto name_arg = op.getArg("name", 0);
        if (!name_arg) {
            return EStr(EOPBundListRefMissArg, "Missing argument: [0] name");
        }
        name = *name_arg;
    }
    bool reverse = op.getBoolArg("reverse", 1);
    if (!bundlelib->checkNameConflict(name)) {
        return EStr(EOPBundListRefNameNotFound, "Bundle name does not exist: " + name);
    }

    vector<string> names;
    vector<string> childs;

    std::deque<BundleName> to_process;
    unordered_set<BundleName> processed;
    to_process.push_back(name);
    processed.insert(name);

    while (!to_process.empty()) {
        BundleName current = to_process.front();
        to_process.pop_front();

        auto iter = bundlelib->bundles.find(current);
        if (iter == bundlelib->bundles.end()) {
            continue;
        }
        const auto &bunde = iter->second;
        string child_list_str;
        if (reverse) {
            for (const auto &ref_name : bunde.reverse_references) {
                if (processed.find(ref_name) == processed.end()) {
                    to_process.push_back(ref_name);
                    processed.insert(ref_name);
                }
                if (!child_list_str.empty()) {
                    child_list_str += " ";
                }
                child_list_str += ref_name;
            }
        } else {
            for (const auto &ref_name : bunde.references) {
                if (processed.find(ref_name) == processed.end()) {
                    to_process.push_back(ref_name);
                    processed.insert(ref_name);
                }
                if (!child_list_str.empty()) {
                    child_list_str += " ";
                }
                child_list_str += ref_name;
            }
        }
        names.push_back(current);
        childs.push_back(child_list_str);
    }
    VulOperationResponse resp;
    resp.list_results["names"] = names;
    resp.list_results["childs"] = childs;
    return resp;
}


auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<BundleLibListRefOperation>(op);
};
struct RegisterBundleLibListRefOperation {
    RegisterBundleLibListRefOperation() {
        VulProject::registerOperation("bundlelib.listref", factory);
    }
} registerBundleLibListRefOperationInstance;

} // namespace operation_bundlelib_listref
