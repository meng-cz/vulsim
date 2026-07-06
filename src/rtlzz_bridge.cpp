#include "rtlzz_bridge.hpp"

#include "errormsg.hpp"

#include "rtlzz.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

std::string generateLogicRTLWithRTLzz(
    const std::string &source_file,
    const std::string &top_function,
    const std::string &lib_include_dir,
    int unroll_limit
) {
    std::ifstream input(source_file);
    if (!input) {
        throw VulException("RTLzz failed to open logic source '" + source_file + "'");
    }
    std::vector<std::string> source_codelines;
    std::string line;
    while (std::getline(input, line)) {
        source_codelines.push_back(line + "\n");
    }
    if (!input.eof()) {
        throw VulException("RTLzz failed to read logic source '" + source_file + "'");
    }

    rtlzz::CompileOptions options;
    options.source_codelines = std::move(source_codelines);
    options.vullib_dir = lib_include_dir;
    options.top_function = top_function;
    options.unroll_limit = unroll_limit;
    options.clang_args.push_back("-std=c++20");

    const auto source_parent = std::filesystem::path(source_file).parent_path();
    if (!source_parent.empty()) {
        options.include_dirs.push_back(source_parent.string());
    }

    auto result = rtlzz::compileToRtl(std::move(options));
    if (!result.ok()) {
        throw VulException("RTLzz compile failed for '" + top_function + "' in '" + source_file + "': " + result.error);
    }

    std::ostringstream output;
    for (const auto &output_line : result.output_codelines) {
        output << output_line;
        if (!output_line.empty() && output_line.back() != '\n') {
            output << '\n';
        }
    }
    return output.str();
}
