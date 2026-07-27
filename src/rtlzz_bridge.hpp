#pragma once

#include <string>
#include <vector>

struct RTLzzLogicRTLResult {
    bool ok = false;
    std::string rtl_text;
    std::vector<std::string> debug_codelines;
    std::string error;
    std::vector<std::string> error_debug_codelines;
    std::string error_signal_debug_text;
    std::vector<std::string> error_signal_names;
};

RTLzzLogicRTLResult generateLogicRTLWithRTLzz(
    const std::string &source_file,
    const std::string &top_function,
    const std::string &lib_include_dir,
    int unroll_limit = 1024
);
