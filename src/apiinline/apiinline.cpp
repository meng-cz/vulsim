// Copyright (c) 2025 Meng Chengzhen, in Shandong University
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
