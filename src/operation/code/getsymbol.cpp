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
#include "simgen.h"
#include "serializejson.h"

namespace operation_code_getsymbol {

class CodeGetSymbolOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;

    virtual vector<string> help() const override {
        return {
            "Code Get Symbol Operation:",
            "Retrieve the symbol table for a specified module in the project.",
            "",
            "Arguments:",
            "  - [0] module_name : The name of the module to retrieve symbols for.",
            "  - [1] full_helper (optional): If set to 'true', generate full helper code including headers. Otherwise, only provide symbol table.",
            "Results:",
            "  - symbols: JSON string of the valid symbols in the module.",
            "  - helper_code_lines: List of lines of full helper C++ code, if requested.",
        };
    }
};

VulOperationResponse CodeGetSymbolOperation::execute(VulProject &project) {
    auto module_name_arg = op.getArg("module_name", 0);
    if (!module_name_arg) {
        return EStr(EOPCodeGetInvalidArg, "Argument 'module_name' is required.");
    }
    ModuleName module_name = *module_name_arg;

    auto mod_iter = project.modulelib->modules.find(module_name);
    if (mod_iter == project.modulelib->modules.end()) {
        return EStr(EOPModCommonNotFound, "Module '" + module_name + "' not found in project.");
    }
    shared_ptr<VulModule> mod_ptr = std::dynamic_pointer_cast<VulModule>(mod_iter->second);
    if (!mod_ptr) {
        return EStr(EOPModCommonImport, "Module '" + module_name + "' is imported and cannot be modified.");
    }

    bool full_helper = op.getBoolArg("full_helper", 1);

    VulOperationResponse response;
    auto symbols = simgen::getValidSymbols(project, module_name);
    response.results["symbols"] = serialize::serializeSymbolsToJSON(symbols);

    if (full_helper) {
        response.list_results["helper_code_lines"] = simgen::getHelperCodeLines(project, module_name);
    }
    return response;
}

auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return make_unique<CodeGetSymbolOperation>(op);
};
struct Register {
    Register() {
        VulProject::registerOperation("code.getsymbol", factory);
    }
} register_instance;

} // namespace operation_code_getsymbol
