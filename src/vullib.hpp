// Copyright (c) 2026 Meng Chengzhen, in Shandong University
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

#pragma once

#include <array>
#include <string_view>

inline constexpr std::array<std::string_view, 8> VulLibFiles = {
    "vullib.h",
    "common.h",
    "queue.hpp",
    "ram.hpp",
    "storage.hpp",
    "fixint.hpp",
    "vcdrecord.hpp",
    "main.cpp",
};

inline constexpr std::array<std::string_view, 2> VulRTLLibFiles = {
    "ram_generic.sv",
    "queue.sv",
};

inline constexpr std::array<std::string_view, 4> VulEscapedHeaders = {
    "defhelper.hpp",
    "run.hpp",
    "header.hpp",
    "header.h",
};

