#include "rtlzz_bridge.hpp"

#include "errormsg.hpp"

#include "ast/ASTBuilder.h"
#include "ir/CFG.h"
#include "ir/SSA.h"
#include "transform/LoopUnroll.h"
#include "transform/Normalize.h"
#include "transform/Predicate.h"
#include "predicate/OutputExpressionMap.h"
#include "predicate/PredicateVerifier.h"
#include "backend/rtlgen.hpp"

#include <filesystem>
#include <vector>

std::string generateLogicRTLWithRTLzz(
    const std::string &source_file,
    const std::string &top_function,
    const std::string &lib_include_dir,
    int unroll_limit
) {
    std::vector<std::string> clang_args;
    clang_args.push_back("-std=c++20");
    if (!lib_include_dir.empty()) {
        clang_args.push_back("-I" + lib_include_dir);
    }
    const auto source_parent = std::filesystem::path(source_file).parent_path();
    if (!source_parent.empty()) {
        clang_args.push_back("-I" + source_parent.string());
    }

    auto build_result = pred::buildASTFromSource(source_file, top_function, clang_args);
    if (!build_result.error.empty()) {
        throw VulException("RTLzz AST build failed for '" + source_file + "': " + build_result.error);
    }
    if (!build_result.function.has_value()) {
        throw VulException("RTLzz failed to extract top function '" + top_function + "' from '" + source_file + "'");
    }

    auto &func = build_result.function.value();

    pred::UnrollConfig unroll_cfg;
    unroll_cfg.max_iterations = unroll_limit;
    auto unroll_result = pred::unrollLoops(func.body, unroll_cfg);
    if (!unroll_result.error.empty()) {
        throw VulException("RTLzz loop unrolling failed for '" + top_function + "': " + unroll_result.error);
    }

    auto norm_result = pred::normalizeFunction(func, unroll_result.body);
    if (!norm_result.error.empty()) {
        throw VulException("RTLzz normalization failed for '" + top_function + "': " + norm_result.error);
    }

    auto cfg = pred::buildCFG(norm_result.body);

    auto ssa = pred::buildSSA(cfg, norm_result.ssa_seed_symbols);
    if (!ssa.error.empty()) {
        throw VulException("RTLzz SSA construction failed for '" + top_function + "': " + ssa.error);
    }

    auto pred_prog = pred::predicate(ssa);
    pred_prog.function_name = func.name;

    for (auto &entry : norm_result.symbols) {
        pred_prog.symbols[entry.first] = entry.second;
    }
    pred_prog.param_directions = norm_result.param_directions;
    for (const auto &param : func.params) {
        if (param.debug_loc.valid()) {
            pred_prog.param_debug_locs[param.name] = param.debug_loc;
        }
    }
    pred_prog.output_default_reasons = norm_result.output_default_reasons;
    pred_prog.output_paired_controls = norm_result.output_paired_controls;
    pred_prog.lookup_tables = norm_result.lookup_tables;
    pred_prog.outputs = norm_result.output_params;
    pred::buildOutputExpressionMap(pred_prog);

    auto verify_result = pred::verifyPredicateProgram(pred_prog);
    if (!verify_result.ok) {
        throw VulException("RTLzz Predicate IR verification failed for '" + top_function + "': " + verify_result.error);
    }

    return pred::rtlgen::emitSystemVerilog(pred_prog);
}
