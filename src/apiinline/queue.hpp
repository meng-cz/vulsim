// MIT License

// Copyright (c) 2025 Meng Chengzhen, in Shandong University

#pragma once

#include "bundlelib.h"
#include "module.h"
#include "apiinline/utils.hpp"

namespace apiinline {

vector<string> inlineQueueAPIs(
    const VulStaticModuleInstance &module,
    const VulStaticBundleLib &bundlelib,
    const vector<string> &logic_hls_codes
);

InlineCode inlineQueueAPIs(
    const VulStaticModuleInstance &module,
    const VulStaticBundleLib &bundlelib,
    const vector<string> &logic_hls_codes,
    const VulDebugLocs &logic_hls_debug
);

} // namespace apiinline
