#include "rtlgen.h"

#include "debugmap.hpp"
#include "rtlzz_bridge.hpp"

namespace rtlgen {

namespace {

void appendTextAsLines(vector<string> &lines, const string &text) {
    size_t pos = 0;
    while (pos < text.size()) {
        size_t next = text.find('\n', pos);
        if (next == string::npos) {
            lines.push_back(text.substr(pos) + "\n");
            return;
        }
        lines.push_back(text.substr(pos, next - pos + 1));
        pos = next + 1;
    }
}

} // namespace

void appendRTLV2LogicRTL(
    RTLGenResult &result,
    const VulStaticModuleInstance &module,
    const string &logic_hls_filepath,
    const string &lib_include_dir,
    int unroll_limit
) {
    if (!result.has_logic_submodule || result.logic_hls_codes.empty()) {
        return;
    }

    VulErrorContextGuard rtlzz_err("running RTLzz for logic submodule: " + module.simClassName());
    const auto logic_module_name = LogicSubModuleName(module.simClassName());
    std::string logic_sv = generateLogicRTLWithRTLzz(
        logic_hls_filepath,
        logic_module_name,
        lib_include_dir,
        unroll_limit
    );
    result.rtl_skeleten_codes.push_back("\n");
    appendTextAsLines(result.rtl_skeleten_codes, logic_sv);
    vulDebugNormalize(result.rtl_skeleten_codes, result.rtl_skeleten_debug);
    result.rtl_skeleten_debug_lines = vulDebugBuildGeneratedMap(result.rtl_skeleten_debug);
}

} // namespace rtlgen
