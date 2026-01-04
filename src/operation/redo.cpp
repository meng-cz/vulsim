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

namespace operation_redo {

class RedoOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;
    
    virtual VulOperationResponse execute(VulProject &project) override;

    virtual vector<string> help() const override {
        return {
            "Redo Operation:",
            "Redo the last undone operation on the currently opened project.",
            "If no project is opened or there is no operation to redo, nothing happens.",
            "",
            "Arguments: None.",
            "Results: None.",
        };
    }
};

VulOperationResponse RedoOperation::execute(VulProject &project) {
    if (!project.is_opened || project.operation_redo_history.empty()) {
        return VulOperationResponse(); // Nothing to redo, but not an error
    }

    string redo_msg = project.redoLastOperation();
    if (!redo_msg.empty()) {
        return EStr(EOPRedoFailed, "Redo operation failed: " + redo_msg);
    }

    return VulOperationResponse(); // Success
}

auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<RedoOperation>(op);
};
struct RegisterRedoOperation {
    RegisterRedoOperation() {
        VulProject::registerOperation("redo", factory);
    }
} registerRedoOperationInstance;

} // namespace operation_redo
