// MIT License

// Copyright (c) 2025 Meng Chengzhen, in Shandong University

#include "apiinline/apiinline.hpp"

#include "apiinline/bram.hpp"
#include "apiinline/queue.hpp"
#include "apiinline/register.hpp"
#include "apiinline/request.hpp"

namespace apiinline {

vector<string> inlineAPIs(
    const VulStaticModuleInstance &module,
    const VulStaticBundleLib &bundlelib,
    const vector<string> &logic_hls_codes
) {
    vector<string> codes = inlineRegisterAPIs(module, bundlelib, logic_hls_codes);
    codes = inlineQueueAPIs(module, bundlelib, codes);
    codes = inlineMemoryAPIs(module, bundlelib, codes);
    codes = inlineRequestAPIs(module, bundlelib, codes);
    return codes;
}

} // namespace apiinline
