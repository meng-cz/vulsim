// MIT License

// Copyright (c) 2025 Meng Chengzhen, in Shandong University

#include "apiinline/apiinline.hpp"

#include "apiinline/register.hpp"

namespace apiinline {

vector<string> inlineAPIs(
    const VulStaticModuleInstance &module,
    const VulStaticBundleLib &bundlelib,
    const vector<string> &logic_hls_codes
) {
    vector<string> codes = inlineRegisterAPIs(module, bundlelib, logic_hls_codes);
    return codes;
}

} // namespace apiinline
