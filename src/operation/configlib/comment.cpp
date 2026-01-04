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

namespace operation_configlib_comment {

class ConfigLibCommentOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;
    virtual string undo(VulProject &project) override;

    virtual bool is_modify() const override { return true; };
    virtual bool is_undoable() const { return true; };

    virtual vector<string> help() const override {
        return {
            "ConfigLib Comment Operation:",
            "Add or update the comment for an existing configuration item in the project's configuration library.",
            "Arguments:",
            "  - [0] name: The name of the configuration item to comment on.",
            "  - [1] comment (optional): The comment text to add or update. If omitted, the comment will be cleared.",
        };
    }

protected:
    string previous_comment;
};

VulOperationResponse ConfigLibCommentOperation::execute(VulProject &project) {
    string name;
    string comment = "";
    auto &configlib = project.configlib;

    {
        auto name_arg = op.getArg("name", 0);
        if (!name_arg) {
            return EStr(EOPConfCommentMissArg, "Missing argument: [0] name");
        }
        name = *name_arg;
        auto comment_arg = op.getArg("comment", 1);
        if (comment_arg) {
            comment = *comment_arg;
        }
    }

    auto iter = configlib->config_items.find(name);
    if (iter == configlib->config_items.end()) {
        return EStr(EOPConfCommentNameNotFound, "Configuration item '" + name + "' does not exist.");
    }
    if (iter->second.group != configlib->DefaultGroupName) {
        return EStr(EOPConfCommentImport, "Configuration item '" + name + "' is imported and cannot be commented.");
    }

    // store previous comment for undo
    previous_comment = iter->second.item.comment;

    // update comment
    iter->second.item.comment = comment;

    project.is_config_modified = true;

    return VulOperationResponse(); // Success
}

string ConfigLibCommentOperation::undo(VulProject &project) {
    auto &configlib = project.configlib;
    auto namearg = op.getArg("name", 0);
    if (!namearg) {
        return "Name '" + string("name") + "' not found in undo operation.";
    }
    const string &name = *namearg;
    auto iter = configlib->config_items.find(name);
    if (iter == configlib->config_items.end()) {
        return "Config item '" + name + "' not found in undo operation.";
    }
    // restore previous comment
    iter->second.item.comment = previous_comment;
    project.is_config_modified = true;
    return "";
}


auto factory = [](const VulOperationPackage &op) -> std::unique_ptr<VulProjectOperation> {
    return std::make_unique<ConfigLibCommentOperation>(op);
};
struct RegisterConfigLibCommentOperation {
    RegisterConfigLibCommentOperation() {
        VulProject::registerOperation("configlib.comment", factory);
    }
} registerConfigLibCommentOperationInstance;

} // namespace operation_configlib_comment
