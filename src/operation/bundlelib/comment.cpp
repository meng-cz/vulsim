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

namespace operation_bundlelib_comment {

class BundleLibCommentOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const { return true; };

    virtual vector<string> help() const override {
        return {
            "BundleLib Comment Operation:",
            "Add or update the comment for an existing bundle in the project's bundle library.",
            "Arguments:",
            "  - [0] name: The name of the bundle to comment on.",
            "  - [1] comment (optional): The comment text to add or update. If omitted, the comment will be cleared.",
        };
    }

    string previous_comment;
    string new_comment;
    string bundle_name;
};

VulOperationResponse BundleLibCommentOperation::execute(VulProject &project) {
    auto &bundlelib = project.bundlelib;

    {
        auto name_arg = op.getArg("name", 0);
        if (!name_arg) {
            return EStr(EOPBundCommentMissArg, "Missing argument: [0] name");
        }
        bundle_name = *name_arg;
        auto comment_arg = op.getArg("comment", 1);
        if (comment_arg) {
            new_comment = *comment_arg;
        } else {
            new_comment = "";
        }
    }

    auto bundle_iter = bundlelib->bundles.find(bundle_name);
    if (bundle_iter == bundlelib->bundles.end()) {
        return EStr(EOPBundCommentNameNotFound, "Bundle does not exist: " + bundle_name);
    }

    previous_comment = bundle_iter->second.item.comment;
    bundle_iter->second.item.comment = new_comment;

    project.is_bundle_modified = true;

    return VulOperationResponse();
}

string BundleLibCommentOperation::undo(VulProject &project) {
    auto &bundlelib = project.bundlelib;

    auto bundle_iter = bundlelib->bundles.find(bundle_name);
    if (bundle_iter == bundlelib->bundles.end()) {
        return string("Undo failed: bundle '") + bundle_name + "' not found.";
    }

    bundle_iter->second.item.comment = previous_comment;

    project.is_bundle_modified = true;

    return "";
}


auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return make_unique<BundleLibCommentOperation>(op);
};
struct Register {
    Register() {
        VulProject::registerOperation("bundlelib.comment", factory);
    }
} register_instance;

} // namespace operation_bundlelib_comment
