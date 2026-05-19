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

#include <optional>

#include "errormsg.hpp"
#include "type.h"

#include "bundlelib.h"
#include "configlib.h"
#include "module.h"
#include "project.h"
#include "trace.hpp"

namespace simgen {

string genCurrentTimeString();

vector<string> genStaticBundle(const VulStaticBundle &bundle);

vector<string> genStaticConfigHeaderCode(const VulStaticConfigLib &configlib);

vector<string> genStaticBundleHeaderCode(const VulStaticBundleLib &bundlelib);

struct StaticModuleCodeHpp {
    vector<string> decl;
    vector<string> impl;
    vector<string> resource_files;
};

StaticModuleCodeHpp genStaticModuleCodeHpp(const VulStaticModuleInstance &module_instance, const vector<VulTracedSignal> &traced_signals);

vector<string> genStaticTestHarnessHpp(const VulStaticTestHarnessModule &test_module, const VulStaticModuleInstance &top_module, bool enable_tracing);

vector<string> genStaticTestMainHpp(shared_ptr<VulStaticModuleInstance> top_module);

} // namespace simgen
