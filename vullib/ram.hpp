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

#include "common.h"
#include "uint.hpp"

#include <array>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <cctype>

using std::array;
using std::vector;

template <uint32_t DataWidth, uint32_t AddrWidth, uint32_t ReadPorts, uint32_t WritePorts>
class VulBRAM {
public:

    static_assert(DataWidth > 0, "DataWidth must be greater than 0");
    static_assert(AddrWidth > 0, "AddrWidth must be greater than 0");
    static_assert(ReadPorts > 0, "ReadPorts must be greater than 0");
    static_assert(WritePorts > 0, "WritePorts must be greater than 0");
    static_assert(WritePorts < 64, "WritePorts must be less than 64");

    using DataType = UInt<DataWidth>;
    using AddrType = UInt<AddrWidth>;

protected:

    array<DataType, 1ULL << AddrWidth> memory_;

    array<AddrType, ReadPorts> read_addresses_;
    array<DataType, ReadPorts> read_data_;

    array<AddrType, WritePorts> write_addresses_;
    array<DataType, WritePorts> write_data_;
    uint64_t write_enables_; // 每个位对应一个写端口，1表示该端口有效

    template<int I, int N>
    inline void unroll_loop(auto&& f) {
        if constexpr (I < N) {
            f(std::integral_constant<int, I>{});
            unroll_loop<I + 1, N>(f);
        }
    }

public:
    VulBRAM() : write_enables_(0) {}

    VulBRAM(const string &path, bool hex) : write_enables_(0) {
        if (hex) {
            init_from_readmemh(path);
        } else {
            init_from_readmemb(path);
        }
    }

    template <uint32_t PortIndex>
    void readreq(const AddrType &addr) {
        static_assert(PortIndex < ReadPorts, "Read port index out of range");
        read_addresses_[PortIndex] = addr;
    }

    template <uint32_t PortIndex>
    void readdata(DataType &data) const {
        static_assert(PortIndex < ReadPorts, "Read port index out of range");
        data = read_data_[PortIndex];
    }

    template <uint32_t PortIndex>
    void write(const AddrType &addr, const DataType &data) {
        static_assert(PortIndex < WritePorts, "Write port index out of range");
        write_addresses_[PortIndex] = addr;
        write_data_[PortIndex] = data;
        write_enables_ |= 1ULL << PortIndex;
    }

    void apply_next_tick() {
        if (write_enables_ != 0) {
            unroll_loop<0, WritePorts>([&](auto i) {
                if (write_enables_ & (1ULL << i)) {
                    memory_[write_addresses_[i]] = write_data_[i];
                }
            });
            write_enables_ = 0;
        }
        unroll_loop<0, ReadPorts>([&](auto i) {
            read_data_[i] = memory_[read_addresses_[i]];
        });
    }

protected:

    void init_from_readmemh(const std::string &path, bool strict_width = false) {
        std::ifstream fin(path);
        if (!fin) {
            throw std::runtime_error("Cannot open file: " + path);
        }

        constexpr size_t DEPTH = 1ULL << AddrWidth;

        size_t cur_addr = 0;
        std::string line;

        while (std::getline(fin, line)) {
            line = strip_comment(line);
            line = trim(line);
            if (line.empty()) continue;

            std::istringstream iss(line);
            std::string token;

            while (iss >> token) {
                token = trim(token);
                if (token.empty()) continue;

                // 地址跳转 @...
                if (token[0] == '@') {
                    std::string addr_str = token.substr(1);
                    if (addr_str.empty())
                        throw std::runtime_error("Invalid @ address");

                    size_t addr = std::stoull(addr_str, nullptr, 16);
                    if (addr >= DEPTH)
                        throw std::runtime_error("Address out of range");

                    cur_addr = addr;
                    continue;
                }

                // 处理 0x 前缀
                if (token.size() >= 2 &&
                    token[0] == '0' &&
                    (token[1] == 'x' || token[1] == 'X')) {
                    token = token.substr(2);
                }

                // 去掉下划线
                std::string hex;
                for (char c : token) {
                    if (c != '_') hex.push_back(c);
                }

                if (hex.empty()) continue;

                // 计算位宽
                size_t bits = hex.size() * 4;

                if (strict_width && bits > DataWidth) {
                    throw std::runtime_error("Data width overflow");
                }

                if (cur_addr >= DEPTH) {
                    throw std::runtime_error("Memory overflow");
                }

                // 写入 DataType
                DataType d;

                // 逐 64bit chunk 写入（从低位开始）
                size_t bit_pos = 0;
                uint64_t chunk = 0;
                int nibble_count = 0;

                for (int i = (int)hex.size() - 1; i >= 0; --i) {
                    uint8_t v = hex_val(hex[i]);
                    chunk |= (uint64_t)v << (4 * nibble_count);
                    nibble_count++;

                    if (nibble_count == 16) { // 16 hex = 64bit
                        size_t hi = std::min(bit_pos + 63, (size_t)DataWidth - 1);
                        size_t lo = bit_pos;

                        if (lo < DataWidth) {
                            UInt<64> tmp(chunk);
                            d(hi, lo) = tmp;
                        }

                        bit_pos += 64;
                        chunk = 0;
                        nibble_count = 0;
                    }
                }

                // 剩余不足64bit
                if (nibble_count > 0) {
                    size_t hi = std::min(bit_pos + nibble_count * 4 - 1,
                                        (size_t)DataWidth - 1);
                    size_t lo = bit_pos;

                    if (lo < DataWidth) {
                        UInt<64> tmp(chunk);
                        d(hi, lo) = tmp;
                    }
                }

                memory_[cur_addr] = d;
                cur_addr++;
            }
        }
    }

    void init_from_readmemb(
    const std::string &path,
    bool strict_width = false   // 若 token 位数 > DataWidth 是否报错
) {
    std::ifstream fin(path);
    if (!fin) {
        throw std::runtime_error("Cannot open file: " + path);
    }

    constexpr size_t DEPTH = 1ULL << AddrWidth;

    size_t cur_addr = 0;
    std::string line;

    while (std::getline(fin, line)) {
        line = strip_comment(line);
        line = trim(line);
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string token;

        while (iss >> token) {
            token = trim(token);
            if (token.empty()) continue;

            // 地址跳转：@<hex>
            if (token[0] == '@') {
                std::string addr_str = token.substr(1);
                if (addr_str.empty())
                    throw std::runtime_error("Invalid @ address");

                size_t addr = std::stoull(addr_str, nullptr, 16);
                if (addr >= DEPTH)
                    throw std::runtime_error("Address out of range");

                cur_addr = addr;
                continue;
            }

            // 去掉下划线
            std::string bits;
            bits.reserve(token.size());
            for (char c : token) {
                if (c != '_') bits.push_back(c);
            }
            if (bits.empty()) continue;

            // 校验只包含 0/1
            for (char c : bits) {
                if (c != '0' && c != '1') {
                    throw std::runtime_error("Invalid binary digit in token: " + bits);
                }
            }

            const size_t nbits = bits.size();
            if (strict_width && nbits > DataWidth) {
                throw std::runtime_error("Data width overflow (binary token too wide)");
            }
            if (cur_addr >= DEPTH) {
                throw std::runtime_error("Memory overflow");
            }

            DataType d; // 如需默认清零，请确保 UInt 默认构造为 0

            // 从 LSB 开始打包（与 Verilog 一致：右侧为低位）
            // 按 64bit 分块写入 UInt
            size_t bit_pos = 0;   // 当前写入到 d 的 bit 位置（LSB 起）
            uint64_t chunk = 0;
            int bit_in_chunk = 0; // 已填入 chunk 的 bit 数

            // 从字符串末尾（LSB）向前处理
            for (int i = (int)bits.size() - 1; i >= 0; --i) {
                if (bits[i] == '1') {
                    chunk |= (uint64_t(1) << bit_in_chunk);
                }
                bit_in_chunk++;

                if (bit_in_chunk == 64) {
                    size_t lo = bit_pos;
                    if (lo < DataWidth) {
                        size_t hi = std::min(bit_pos + 63, (size_t)DataWidth - 1);
                        UInt<64> tmp(chunk);
                        d(hi, lo) = tmp;
                    }
                    bit_pos += 64;
                    chunk = 0;
                    bit_in_chunk = 0;
                }
            }

            // 处理最后不足 64bit 的部分
            if (bit_in_chunk > 0) {
                size_t lo = bit_pos;
                if (lo < DataWidth) {
                    size_t hi = std::min(bit_pos + (size_t)bit_in_chunk - 1,
                                         (size_t)DataWidth - 1);
                    UInt<64> tmp(chunk);
                    d(hi, lo) = tmp;
                }
            }

            memory_[cur_addr] = d;
            cur_addr++;
        }
    }
}

};
