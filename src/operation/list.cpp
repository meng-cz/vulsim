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
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace operation_list {

using namespace std::filesystem;
using namespace serialize;

class ListOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;
    
    virtual VulOperationResponse execute(VulProject &project) override {
        
        const ProjectPath &dirpath = project.dirpath;

        vector<string> project_names;
        vector<string> project_paths;
        vector<string> project_modtimes;

        path base(dirpath);

        for (const auto &entry : std::filesystem::recursive_directory_iterator(base, std::filesystem::directory_options::skip_permission_denied)) {
            try {                
                if (!entry.is_regular_file()) continue;
                path p = entry.path();
                if (p.extension() != ".vul") continue;

                path parent = p.parent_path();
                path config = parent / "configlib.xml";
                path bundle = parent / "bundlelib.xml";
                path modules = parent / "modules";

                if (!exists(config) || !is_regular_file(config)) continue;
                if (!exists(bundle) || !is_regular_file(bundle)) continue;
                if (!exists(modules) || !is_directory(modules)) continue;

                // compute latest modification time among .vul, configlib, bundlelib and all .xml in modules
                std::filesystem::file_time_type max_ft;
                try {
                    max_ft = std::filesystem::last_write_time(p);
                } catch (...) {
                    continue; // skip if cannot read time
                }

                auto try_update = [&](const path &f) {
                    try {
                        auto ft = std::filesystem::last_write_time(f);
                        if (ft > max_ft) max_ft = ft;
                    } catch (...) {}
                };

                try_update(config);
                try_update(bundle);

                for (const auto &mentry : std::filesystem::recursive_directory_iterator(modules, std::filesystem::directory_options::skip_permission_denied)) {
                    if (!mentry.is_regular_file()) continue;
                    if (mentry.path().extension() == ".xml") try_update(mentry.path());
                }

                // convert file_time_type to system_clock::time_point then to time_t
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    max_ft - decltype(max_ft)::clock::now() + std::chrono::system_clock::now()
                );
                std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
                std::tm* ptm = std::localtime(&tt);
                std::ostringstream oss;
                oss << std::put_time(ptm, "%Y-%m-%d %H:%M:%S");

                project_names.push_back(p.stem().string());
                path rel;
                try {
                    rel = p.lexically_relative(base);
                } catch (...) {
                    rel = p;
                }
                project_paths.push_back(rel.string());
                project_modtimes.push_back(oss.str());
            } catch (const std::exception &) {
                // ignore traversal errors
            }
        }

        VulOperationResponse ret;
        ret.results["rootpath"] = dirpath;
        ret.list_results["project_names"] = std::move(project_names);
        ret.list_results["project_paths"] = std::move(project_paths);
        ret.list_results["project_modtimes"] = std::move(project_modtimes);
        return ret;
    }

    virtual vector<string> help() const override {
        return {
            "List all projects in the local project library.",
            "Arguments: None",
        };
    }
};

OperationFactory factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<ListOperation>(op);
};

struct RegisterListOperation {
    RegisterListOperation() {
        VulProject::registerOperation("list", factory);
    }
} registerListOperationInstance;

} // namespace operation_list
