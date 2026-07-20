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
    return inlineAPIs(module, bundlelib, logic_hls_codes, {}).lines;
}

InlineCode inlineAPIs(
    const VulStaticModuleInstance &module,
    const VulStaticBundleLib &bundlelib,
    const vector<string> &logic_hls_codes,
    const VulDebugLocs &logic_hls_debug
) {
    InlineCode codes = inlineRegisterAPIs(module, bundlelib, logic_hls_codes, logic_hls_debug);
    codes = inlineQueueAPIs(module, bundlelib, codes.lines, codes.debug);
    codes = inlineMemoryAPIs(module, bundlelib, codes.lines, codes.debug);
    codes = inlineRequestAPIs(module, bundlelib, codes.lines, codes.debug);
    codes = normalizeTemplateLambdaCalls(codes.lines, codes.debug);
    return codes;
}

} // namespace apiinline
