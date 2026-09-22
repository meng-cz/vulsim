#pragma once

#include "apiinline/utils.hpp"

#include <unordered_map>

namespace apiinline {

struct RegisterAPIUsage {
    bool hold = false;
    bool reset = false;
};

using RegisterAPIUsageMap = std::unordered_map<std::string, RegisterAPIUsage>;

inline RegisterAPIUsageMap checkRegisterAPIUsage(
    const VulStaticModuleInstance& module,
    const std::vector<std::string>& logic_lines) {
    RegisterAPIUsageMap result;
    for (const auto& reg : module.registers) result.emplace(reg.name, RegisterAPIUsage{});
    if (result.empty()) return result;
    const auto tokens = tokenizeWithLibclang(joinLines(logic_lines));
    for (std::size_t i = 0; i + 2 < tokens.size(); ++i) {
        auto found = result.find(tokens[i].spelling);
        if (found == result.end() || tokens[i].kind != CXToken_Identifier ||
            tokens[i + 1].spelling != ".") continue;
        const auto& method = tokens[i + 2].spelling;
        if (method == "holdnext") found->second.hold = true;
        else if (method == "resetnext") found->second.reset = true;
    }
    return result;
}

} // namespace apiinline
