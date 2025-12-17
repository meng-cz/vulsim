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

static unordered_map<OperationName, OperationFactory> operationFactories;

void VulProject::registerOperation(const OperationName &op_name, const OperationFactory &factory) {
    operationFactories[op_name] = factory;
}

ErrorMsg VulProject::doOperation(const OperationName &op_name, const vector<OperationArg> &op_args) {
    auto iter = operationFactories.find(op_name);
    if (iter == operationFactories.end()) {
        return EStr(EItemOPInvalid, string("Unsupported operation: ") + op_name);
    }
    auto &factory = iter->second;
    auto operation = factory();
    if (!operation) {
        return EStr(EItemOPInvalid, string("Failed to create operation instance for: ") + op_name);
    }
    return operation->execute(*this, op_args);
}





