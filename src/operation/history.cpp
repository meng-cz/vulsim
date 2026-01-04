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

namespace operation_history {

class HistoryOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;
    
    virtual VulOperationResponse execute(VulProject &project) override;

    virtual vector<string> help() const override {
        return {
            "History Operation:",
            "List the history of undoable operations performed on the currently opened project.",
            "Arguments: None.",
            "Results:",
            "  - undolist_names: List of operation names that can be undone.",
            "  - undolist_timestamps: List of timestamps corresponding to each undoable operation.",
            "  - undolist_args: List of serialized operation arguments for each undoable operation.",
            "  - redolist_names: List of operation names that can be redone.",
            "  - redolist_timestamps: List of timestamps corresponding to each redoable operation.",
            "  - redolist_args: List of serialized operation arguments for each redoable operation.",
        };
    }
};

VulOperationResponse HistoryOperation::execute(VulProject &project) {
    VulOperationResponse resp;

    vector<string> undolist_names;
    vector<string> undolist_timestamps;
    vector<string> undolist_args;

    for (const auto &op_ptr : project.operation_undo_history) {
        if (!op_ptr->is_undoable()) continue;
        undolist_names.push_back(op_ptr->op.name);
        undolist_timestamps.push_back(op_ptr->timestamp);
        string argstr = "";
        for (const auto &arg : op_ptr->op.arg_list) {
            if (!argstr.empty()) argstr += "; ";
            if (arg.index != (uint32_t)-1) {
                argstr += "[" + std::to_string(arg.index) + "]";
            }
            if (!arg.name.empty()) {
                argstr += arg.name;
            }
            argstr += ("=" + arg.value);
        }
        undolist_args.push_back(argstr);
    }

    vector<string> redolist_names;
    vector<string> redolist_timestamps;
    vector<string> redolist_args;

    for (const auto &op_ptr : project.operation_redo_history) {
        if (!op_ptr->is_undoable()) continue;
        redolist_names.push_back(op_ptr->op.name);
        redolist_timestamps.push_back(op_ptr->timestamp);
        string argstr = "";
        for (const auto &arg : op_ptr->op.arg_list) {
            if (!argstr.empty()) argstr += "; ";
            if (arg.index != (uint32_t)-1) {
                argstr += "[" + std::to_string(arg.index) + "]";
            }
            if (!arg.name.empty()) {
                argstr += arg.name;
            }
            argstr += ("=" + arg.value);
        }
        redolist_args.push_back(argstr);
    }

    resp.list_results["undolist_names"] = std::move(undolist_names);
    resp.list_results["undolist_timestamps"] = std::move(undolist_timestamps);
    resp.list_results["undolist_args"] = std::move(undolist_args);

    resp.list_results["redolist_names"] = std::move(redolist_names);
    resp.list_results["redolist_timestamps"] = std::move(redolist_timestamps);
    resp.list_results["redolist_args"] = std::move(redolist_args);

    return resp;
}



auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<HistoryOperation>(op);
};
struct RegisterHistoryOperation {
    RegisterHistoryOperation() {
        VulProject::registerOperation("history", factory);
    }
} registerHistoryOperationInstance;

} // namespace operation_history
