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
#include "serialize.h"

#include <filesystem>

/**
 * create Operation
 * args:
 *   name: ProjectName
 */

namespace operation_create {

using namespace std::filesystem;
using namespace serialize;

class CreateOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;
    
    virtual VulOperationResponse execute(VulProject &project) override {
        if (project.is_opened) {
            return EStr(EOPCreateAlreadyOpened, "A project is already opened, cannot create a new project without closing first.");
        }
        project.closeAndFinalize();

        ProjectName proj_name;
        {
            auto it = op.args.find("name");
            if (it == op.args.end()) {
                return EStr(EOPCreateMissArg, "Missing argument: name");
            }
            proj_name = it->second;

            string findpath = project.findProjectPathInLocalLibrary(proj_name);
            if (!findpath.empty()) {
                return EStr(EOPCreateNameExists, "Project '" + proj_name + "' already exists at: " + findpath);
            }
        }

        path proj_dir = std::filesystem::absolute(project.project_local_path) / proj_name;
        path proj_modules_dir = proj_dir / "modules";
        try {
            create_directory(proj_dir);
            create_directory(proj_modules_dir);
        } catch (const std::exception &e) {
            return EStr(EOPCreateFileFailed, string("Failed to create project directory: ") + e.what());
        }

        VulProjectRaw project_raw;
        project_raw.top_module = "";
        project_raw.imports = {};
        ErrorMsg err = writeProjectToXMLFile((proj_dir / (proj_name + ".vul")).string(), project_raw);
        if (err) {
            std::filesystem::remove_all(proj_dir);
            return EStr(EOPCreateFileFailed, string("Failed to write project file: ") + err.msg);
        }

        vector<VulConfigItem> empty_configs;
        err = writeConfigLibToXMLFile((proj_dir / "configlib.xml").string(), empty_configs);
        if (err) {
            std::filesystem::remove_all(proj_dir);
            return EStr(EOPCreateFileFailed, string("Failed to write config library file: ") + err.msg);
        }
        vector<VulBundleItem> empty_bundles;
        err = writeBundleLibToXMLFile((proj_dir / "bundlelib.xml").string(), empty_bundles);
        if (err) {
            std::filesystem::remove_all(proj_dir);
            return EStr(EOPCreateFileFailed, string("Failed to write bundle library file: ") + err.msg);
        }

        project.name = proj_name;
        project.dirpath = proj_dir.string();
        project.is_opened = true;

        return VulOperationResponse(); // Success
    }
    
    virtual bool is_modify() const override { return true; }; // modifies project
};

OperationFactory createOperationFactory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<CreateOperation>(op);
};

struct RegisterCreateOperation {
    RegisterCreateOperation() {
        VulProject::registerOperation("create", createOperationFactory);
    }
} registerCreateOperationInstance;

} // namespace operation_create
