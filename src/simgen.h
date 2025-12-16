// MIT License

// Copyright (c) 2025 Meng Chengzhen, in Shandong University

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "errormsg.hpp"
#include "type.h"

#include "bundlelib.h"
#include "configlib.h"
#include "module.h"

namespace simgen {

/**
 * @brief Generate config.h C++ header code for configuration items.
 * @param config_lib The VulConfigLib instance containing configuration items.
 * @param out_lines Output vector of strings to hold the generated header code lines. With \\n in each line.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg genConfigHeaderCode(const VulConfigLib &config_lib, vector<string> &out_lines);

/**
 * @brief Generate bundle.h C++ header code for bundle definitions.
 * @param bundle_lib The VulBundleLib instance containing bundle definitions.
 * @param out_lines Output vector of strings to hold the generated header code lines. With \\n in each line.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg genBundleHeaderCode(const VulBundleLib &bundle_lib, vector<string> &out_lines);


/**
 * @brief Generate module.hpp C++ header code for a module definition.
 * @param module The VulModule instance containing the module definition.
 * @param out_lines Output vector of strings to hold the generated header code lines. With \\n in each line.
 * @return An ErrorMsg indicating failure, empty if success.
 */
ErrorMsg genModuleCodeHpp(const VulModule &module, vector<string> &out_lines, shared_ptr<VulModuleLib> modulelib);


} // namespace simgen
