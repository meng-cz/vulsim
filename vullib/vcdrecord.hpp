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

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * 工作流程为：
（1）构造后进入注册阶段，提供函数接口registe，用于以一个信号名和信号宽度注册一个待记录的信号，返回一个uint32_t类型的信号ID。
（2）注册完成后，提供函数接口init，接受文件名、周期时间、写文件间隔参数，进入记录阶段。
（3）在记录阶段，提供函数接口record，接受一个信号ID和一个uint64_t信号值（宽度小于等于64）或vector<uint64_t>信号值（宽度大于64），记录该信号在当前时间点的变化。
（4）在记录阶段，提供函数接口commit，表示当前周期结束，所有在当前周期内记录的信号变化将确定并提交入缓冲区。如果给出了写文件间隔参数，并且当前周期数达到了写文件间隔的倍数，则将缓冲区中的内容写入vcd文件。
（5）提供函数接口close，结束记录阶段，关闭文件并清理资源。
 */

// struct VCDPauseCondition {
//     string signal_name;
//     string expected_value;
// };

class GlobalVCDRecord {
public:
    GlobalVCDRecord() = default;

    ~GlobalVCDRecord() {
        try {
            close();
        } catch (...) {
        }
    }

    uint32_t registe(const std::string &signal_name, uint32_t signal_width) {
        ensure_state(State::Registering, "registe");
        if (signal_name.empty()) {
            throw std::runtime_error("Signal name cannot be empty");
        }
        if (signal_width == 0) {
            throw std::runtime_error("Signal width must be greater than 0");
        }

        const uint32_t id = static_cast<uint32_t>(signals_.size());
        signals_.push_back(SignalInfo{signal_name, signal_width, make_vcd_id(id), std::nullopt, std::nullopt, false});
        return id;
    }

    void init(const std::string &filename, uint64_t cycle_time, uint64_t write_interval) {
        ensure_state(State::Registering, "init");
        if (signals_.empty()) {
            throw std::runtime_error("No signals registered before init");
        }
        if (filename.empty()) {
            throw std::runtime_error("VCD filename cannot be empty");
        }
        if (cycle_time == 0) {
            throw std::runtime_error("Cycle time must be greater than 0");
        }

        ofs_.open(filename, std::ios::out | std::ios::trunc);
        if (!ofs_.is_open()) {
            throw std::runtime_error("Failed to open VCD file: " + filename);
        }

        cycle_time_ = cycle_time;
        write_interval_ = write_interval;
        cycle_count_ = 0;
        buffer_.clear();

        write_header();
        state_ = State::Recording;
    }

    void record(uint32_t signal_id, uint64_t signal_value) {
        ensure_state(State::Recording, "record");
        SignalInfo &sig = get_signal(signal_id);
        if (sig.width > 64) {
            throw std::runtime_error("Signal width > 64 requires vector<uint64_t> overload");
        }

        if (sig.width < 64) {
            const uint64_t mask = (1ULL << sig.width) - 1ULL;
            if ((signal_value & ~mask) != 0) {
                throw std::runtime_error("Signal value exceeds declared width");
            }
        }

        sig.pending_bits = to_bits(signal_value, sig.width);
        sig.has_pending = true;
    }

    void record(uint32_t signal_id, const std::vector<uint64_t> &signal_value) {
        ensure_state(State::Recording, "record");
        SignalInfo &sig = get_signal(signal_id);
        const uint32_t words = (sig.width + 63U) / 64U;
        if (signal_value.size() < words) {
            throw std::runtime_error("Insufficient vector<uint64_t> words for signal width");
        }

        if (sig.width <= 64) {
            uint64_t v = signal_value.empty() ? 0ULL : signal_value[0];
            record(signal_id, v);
            return;
        }

        sig.pending_bits = to_bits(signal_value, sig.width);
        sig.has_pending = true;
    }

    void commit() {
        ensure_state(State::Recording, "commit");
        ++cycle_count_;

        std::string cycle_changes;
        for (auto &sig : signals_) {
            if (!sig.has_pending) {
                continue;
            }

            if (!sig.last_bits.has_value() || sig.pending_bits.value() != sig.last_bits.value()) {
                cycle_changes += format_value_change(sig, sig.pending_bits.value());
                sig.last_bits = sig.pending_bits;
            }

            sig.pending_bits.reset();
            sig.has_pending = false;
        }

        if (!cycle_changes.empty()) {
            buffer_ += "#" + std::to_string(cycle_count_ * cycle_time_) + "\n";
            buffer_ += cycle_changes;
        }

        if (write_interval_ != 0 && (cycle_count_ % write_interval_ == 0)) {
            flush_buffer();
        }
    }

    void close() {
        if (state_ == State::Closed) {
            return;
        }

        if (state_ == State::Recording) {
            flush_buffer();
            if (ofs_.is_open()) {
                ofs_.flush();
                ofs_.close();
            }
        }

        signals_.clear();
        buffer_.clear();
        cycle_time_ = 0;
        write_interval_ = 0;
        cycle_count_ = 0;
        state_ = State::Closed;
    }

private:
    enum class State {
        Registering,
        Recording,
        Closed,
    };

    struct SignalInfo {
        std::string name;
        uint32_t width;
        std::string vcd_id;
        std::optional<std::string> last_bits;
        std::optional<std::string> pending_bits;
        bool has_pending;
    };

private:
    SignalInfo &get_signal(uint32_t signal_id) {
        if (signal_id >= signals_.size()) {
            throw std::runtime_error("Invalid signal id");
        }
        return signals_[signal_id];
    }

    void ensure_state(State expected, const char *api_name) const {
        if (state_ != expected) {
            throw std::runtime_error(std::string("Invalid state for ") + api_name);
        }
    }

    void flush_buffer() {
        if (!ofs_.is_open()) {
            throw std::runtime_error("VCD file is not open");
        }
        if (!buffer_.empty()) {
            ofs_ << buffer_;
            buffer_.clear();
        }
    }

    std::string make_vcd_id(uint32_t idx) const {
        // Use printable ASCII [33, 126], base-94 encoding.
        std::string out;
        uint32_t v = idx;
        do {
            const uint32_t digit = v % 94U;
            out.push_back(static_cast<char>(33 + digit));
            v /= 94U;
        } while (v != 0);
        return out;
    }

    std::string to_bits(uint64_t value, uint32_t width) const {
        std::string bits;
        bits.reserve(width);
        for (int32_t i = static_cast<int32_t>(width) - 1; i >= 0; --i) {
            bits.push_back(((value >> i) & 1ULL) ? '1' : '0');
        }
        return bits;
    }

    std::string to_bits(const std::vector<uint64_t> &words, uint32_t width) const {
        const uint32_t need_words = (width + 63U) / 64U;
        std::string bits;
        bits.reserve(width);

        for (int32_t i = static_cast<int32_t>(width) - 1; i >= 0; --i) {
            const uint32_t wi = static_cast<uint32_t>(i) / 64U;
            const uint32_t bi = static_cast<uint32_t>(i) % 64U;
            if (wi >= words.size()) {
                throw std::runtime_error("Insufficient vector<uint64_t> words for conversion");
            }
            bits.push_back(((words[wi] >> bi) & 1ULL) ? '1' : '0');
        }

        for (size_t i = need_words; i < words.size(); ++i) {
            if (words[i] != 0ULL) {
                throw std::runtime_error("High words contain non-zero bits beyond declared width");
            }
        }

        const uint32_t extra_bits = need_words * 64U - width;
        if (extra_bits > 0U) {
            const uint64_t upper_mask = (~0ULL) << (64U - extra_bits);
            if ((words[need_words - 1U] & upper_mask) != 0ULL) {
                throw std::runtime_error("Signal value exceeds declared width");
            }
        }

        return bits;
    }

    std::string format_value_change(const SignalInfo &sig, const std::string &bits) const {
        if (sig.width == 1) {
            return std::string(1, bits[0]) + sig.vcd_id + "\n";
        }
        return "b" + bits + " " + sig.vcd_id + "\n";
    }

    std::vector<std::string> split_signal_path(const std::string &name) const {
        std::vector<std::string> parts;
        std::string cur;
        for (size_t i = 0; i < name.size(); ++i) {
            if (i + 1 < name.size() && name[i] == ':' && name[i + 1] == ':') {
                if (!cur.empty()) {
                    parts.push_back(cur);
                    cur.clear();
                }
                ++i;
                continue;
            }
            if (name[i] == '.') {
                if (!cur.empty()) {
                    parts.push_back(cur);
                    cur.clear();
                }
                continue;
            }
            cur.push_back(name[i]);
        }
        if (!cur.empty()) {
            parts.push_back(cur);
        }
        return parts;
    }

    struct ScopeNode {
        std::map<std::string, ScopeNode> children;
        std::vector<const SignalInfo *> signals;
    };

    void emit_scope_tree(const ScopeNode &node) {
        // Signals inside the same scope are emitted in lexical order.
        std::vector<const SignalInfo *> sorted_signals = node.signals;
        std::sort(sorted_signals.begin(), sorted_signals.end(),
            [](const SignalInfo *a, const SignalInfo *b) {
                if (a->name != b->name) {
                    return a->name < b->name;
                }
                return a->vcd_id < b->vcd_id;
            });

        for (const auto *sig : sorted_signals) {
            const std::vector<std::string> parts = split_signal_path(sig->name);
            const std::string leaf = parts.empty() ? sig->name : parts.back();
            ofs_ << "$var wire " << sig->width << " " << sig->vcd_id << " " << leaf << " $end\n";
        }

        for (const auto &entry : node.children) {
            ofs_ << "$scope module " << entry.first << " $end\n";
            emit_scope_tree(entry.second);
            ofs_ << "$upscope $end\n";
        }
    }

    void write_header() {
        if (!ofs_.is_open()) {
            throw std::runtime_error("VCD file is not open");
        }

        std::time_t now = std::time(nullptr);
        std::tm tm_now;
#ifdef _WIN32
        localtime_s(&tm_now, &now);
#else
        localtime_r(&now, &tm_now);
#endif

        ofs_ << "$date\n";
        ofs_ << "  " << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S") << "\n";
        ofs_ << "$end\n";
        ofs_ << "$version\n";
        ofs_ << "  GlobalVCDRecord\n";
        ofs_ << "$end\n";
        ofs_ << "$timescale " << cycle_time_ << "ns $end\n";
        ofs_ << "$scope module logic $end\n";

        ScopeNode root;
        for (const auto &sig : signals_) {
            std::vector<std::string> parts = split_signal_path(sig.name);
            if (parts.empty()) {
                root.signals.push_back(&sig);
                continue;
            }

            ScopeNode *node = &root;
            for (size_t i = 0; i + 1 < parts.size(); ++i) {
                node = &node->children[parts[i]];
            }
            node->signals.push_back(&sig);
        }
        emit_scope_tree(root);

        ofs_ << "$upscope $end\n";
        ofs_ << "$enddefinitions $end\n";
        ofs_ << "$dumpvars\n";
        for (const auto &sig : signals_) {
            if (sig.width == 1) {
                ofs_ << "x" << sig.vcd_id << "\n";
            } else {
                ofs_ << "b" << std::string(sig.width, 'x') << " " << sig.vcd_id << "\n";
            }
        }
        ofs_ << "$end\n";
        ofs_.flush();
    }

private:
    State state_ = State::Registering;
    std::vector<SignalInfo> signals_;
    std::ofstream ofs_;
    uint64_t cycle_time_ = 0;
    uint64_t write_interval_ = 0;
    uint64_t cycle_count_ = 0;
    std::string buffer_;
};
