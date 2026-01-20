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

namespace operation_simulation_state {

class SimulationStateOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;

    virtual vector<string> help() const override {
        return {
            "Simulation State Operation:",
            "Get the current simulation task state of the project.",
            "Arguments: None.",
            "Results:",
            "  - runid: Task ID that is currently running, empty if none.",
            "  - stage: Current simulation stage (generation, compilation, simulation, idle).",
            "  - errored: Whether an error has occurred, True or False (empty if false).",
            "  - gen_start_us: Generation start time in microseconds since epoch (only if generation is running).",
            "  - gen_finish_us: Generation finish time in microseconds since epoch (only if generation is finished).",
            "  - comp_start_us: Compilation start time in microseconds since epoch (only if compilation is running).",
            "  - comp_finish_us: Compilation finish time in microseconds since epoch (only if compilation is finished).",
            "  - sim_start_us: Simulation start time in microseconds since epoch (only if simulation is running).",
            "  - sim_finish_us: Simulation finish time in microseconds since epoch (only if simulation is finished).",
        };
    }
};

VulOperationResponse SimulationStateOperation::execute(VulProject &project) {
    VulOperationResponse resp;

    auto simman = SimulationManager::getInstance();
    auto state = simman->getState();

    resp.results["runid"] = (state.running ? state.run_id : "");
    if (state.running) {
        if (state.generation.started && !state.generation.finished) {
            resp.results["stage"] = (string("generation (") + std::to_string(state.generation.finished_modules) + "/" + std::to_string(state.generation.total_modules) + ")");
        } else if (state.compilation.started && !state.compilation.finished) {
            resp.results["stage"] = ("compilation (" + std::to_string(state.compilation.finished_files) + "/" + std::to_string(state.compilation.total_files) + ")");
        } else if (state.simulation.started && !state.simulation.finished) {
            resp.results["stage"] = "simulation";
        } else {
            resp.results["stage"] = "idle";
        }
    } else {
        resp.results["stage"] = "idle";
    }
    resp.results["errored"] = (state.errored ? "True" : "False");

    if (state.generation.started) {
        resp.results["gen_start_us"] = std::to_string(state.starttime_us);
    }
    if (state.generation.finished) {
        resp.results["gen_finish_us"] = std::to_string(state.generation.finished_us);
    }
    if (state.compilation.started) {
        resp.results["comp_start_us"] = std::to_string(state.starttime_us);
    }
    if (state.compilation.finished) {
        resp.results["comp_finish_us"] = std::to_string(state.compilation.finished_us);
    }
    if (state.simulation.started) {
        resp.results["sim_start_us"] = std::to_string(state.starttime_us);
    }
    if (state.simulation.finished) {
        resp.results["sim_finish_us"] = std::to_string(state.simulation.finished_us);
    }

    return resp;
}

auto factory_simulation_state = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<SimulationStateOperation>(op);
};
struct SimulationStateOperationRegister {
    SimulationStateOperationRegister() {
        VulProject::registerOperation("simulation.state", factory_simulation_state);
    }
} simulation_state_operation_register_instance;

} // namespace operation_simulation_state
