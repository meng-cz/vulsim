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
#include "configuration.h"

#include "simulation/simman.h"

namespace operation_simulation_start {

class SimulationStartOperation : public VulProjectOperation {
public:
    using VulProjectOperation::VulProjectOperation;

    virtual VulOperationResponse execute(VulProject &project) override;

    virtual bool is_modify() const override { return true; }

    virtual vector<string> help() const override {
        return {
            "Simulation Start Operation:",
            "Start a simulation task for the currently opened project.",
            "Arguments:",
            "  - runid: ID for the simulation task.",
            "  - do_generation (optional): Whether to perform generation step (True or False).",
            "  - do_compilation (optional): Whether to perform compilation step (True or False).",
            "  - do_simulation (optional): Whether to perform simulation step (True or False).",
            "  - comp_release (optional): Whether to release compilation resources after compilation (True or False).",
        };
    }
};

VulOperationResponse SimulationStartOperation::execute(VulProject &project) {
    VulOperationResponse resp;

    auto runid_arg = op.getArg("runid", -1);
    if (!runid_arg) {
        return EStr(EOPBundCommentMissArg, "Missing required argument: runid.");
    }
    string runid = *runid_arg;

    bool do_generation = op.getBoolArg("do_generation", -1);
    bool do_compilation = op.getBoolArg("do_compilation", -1);
    bool do_simulation = op.getBoolArg("do_simulation", -1);
    bool comp_release = op.getBoolArg("comp_release", -1);

    std::optional<GenerationConfig> generation_config = std::nullopt;
    if (do_generation) {
        generation_config = GenerationConfig();
    }
    std::optional<CompilationConfig> compilation_config = std::nullopt;
    if (do_compilation) {
        CompilationConfig comp_config;
        comp_config.release_mode = comp_release;
        compilation_config = comp_config;
    }
    std::optional<SimulationConfig> simulation_config = std::nullopt;
    if (do_simulation) {
        simulation_config = SimulationConfig();
    }

    string vullib_path = getConfigValue(string(EnvVulLibraryPath), "vullib");
    auto simman = SimulationManager::getInstance();

    return simman->startTask(
        project.name,
        runid,
        vullib_path,
        generation_config,
        compilation_config,
        simulation_config
    );
}

auto factory = [](const VulOperationPackage &op) -> unique_ptr<VulProjectOperation> {
    return std::make_unique<SimulationStartOperation>(op);
};
struct RegisterSimulationStartOperation {
    RegisterSimulationStartOperation() {
        VulProject::registerOperation("simulation.start", factory);
    }
} registerSimulationStartOperationInstance;


} // namespace operation_simulation_start
