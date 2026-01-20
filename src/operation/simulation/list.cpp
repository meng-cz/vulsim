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

#include "projman.h"
#include "project.h"

#include "simulation/simman.h"

#include <filesystem>

namespace operation_simulation_list {

class SimulationListOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;

    virtual vector<string> help() const override {
        return {
            "Simulation List Operation:",
            "List all simulation tasks for the currently opened project.",
            "Arguments: None.",
            "Results:",
            "  - runids: List of all simulation task IDs for the project.",
            "  - gen_finished_us: List of timestamps indicating when generation finished for each task, '0' or empty if not finished.",
            "  - comp_finished_us: List of timestamps indicating when compilation finished for each task, '0' or empty if not finished."
        };
    }
};

VulOperationResponse SimulationListOperation::execute(VulProject &project) {
    VulOperationResponse resp;

    auto all_runs = VulProjectPathManager::getInstance()->getProjectRunIDs(project.name);
    if (!all_runs.has_value()) {
        return resp;
    }

    vector<string> runids;
    vector<string> gen_finished_us;
    vector<string> comp_finished_us;

    using namespace std::filesystem;

    auto get_file_modified_time_us = [](const path &p) -> string {
        if (!exists(p)) {
            return "0";
        }
        auto ftime = last_write_time(p);
        auto sctp = std::chrono::time_point_cast<std::chrono::microseconds>(ftime);
        auto epoch = sctp.time_since_epoch();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(epoch).count();
        return std::to_string(us);
    };

    for (const auto &run_id : all_runs.value()) {
        auto _run_path = VulProjectPathManager::getInstance()->getProjectRunPath(project.name, run_id);
        if (!_run_path.has_value()) {
            continue;
        }
        runids.push_back(run_id);
        path run_path(_run_path.value());

        path gen_finished_file_path = run_path / "generation" / ".finished";
        gen_finished_us.push_back(get_file_modified_time_us(gen_finished_file_path));

        path comp_finished_file_path = run_path / "build" / ".finished";
        comp_finished_us.push_back(get_file_modified_time_us(comp_finished_file_path));
    }

    resp.list_results["runids"] = std::move(runids);
    resp.list_results["gen_finished_us"] = std::move(gen_finished_us);
    resp.list_results["comp_finished_us"] = std::move(comp_finished_us);

    return resp;
}

auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<SimulationListOperation>(op);
};
struct RegisterSimulationListOperation {
    RegisterSimulationListOperation() {
        VulProject::registerOperation("simulation.list", factory);
    }
} registerSimulationListOperationInstance;


} // namespace operation_simulation_list
