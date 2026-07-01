// MIT License

// Copyright (c) 2025 Meng Chengzhen, in Shandong University

#pragma once

#include "bundlelib.h"
#include "module.h"

namespace apiinline {

vector<string> inlineAPIs(
    const VulStaticModuleInstance &module,
    const VulStaticBundleLib &bundlelib,
    const vector<string> &logic_hls_codes
);

} // namespace apiinline
