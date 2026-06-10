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

#include "uint.hpp"
#include "pipe.hpp"
#include "storage.hpp"
#include "ram.hpp"
#include "queue.hpp"

#include <string>
#include <vector>

#ifndef VULSIM_TRACE_BREAK_CONDITION_SPEC_DEFINED
#define VULSIM_TRACE_BREAK_CONDITION_SPEC_DEFINED
struct TraceBreakConditionSpec {
    std::string signal_name;
    std::string expected_bits;
    std::string expected_display;
};
#endif

uint32_t trace_registe_signal(const std::string &signal_name, uint32_t signal_width);

void trace_init(const std::string &filename, uint64_t cycle_time, uint64_t write_interval);

void trace_set_break_history_cycles(uint64_t cycle_count);

void trace_add_break_point(const std::vector<TraceBreakConditionSpec> &conditions, const std::string &expr_text);

void trace_record(uint32_t signal_id, uint64_t signal_value);

void trace_record(uint32_t signal_id, const std::vector<uint64_t> &signal_value);

void trace_commit();

void sim_exit();
