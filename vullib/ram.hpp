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
#include "fixint.hpp"

#include <cassert>
#include <array>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <cctype>

using std::array;
using std::string;
using std::vector;


template <typename DataT, uint64_t Size>
class VulBRAM1RW {
public:
    static_assert(Size > 1, "Size must be greater than 1");
    static constexpr uint32_t AddrWidth = log2ceil(Size);
    static_assert(AddrWidth < 64, "AddrWidth must be less than 64");

    using AddrType = Int<AddrWidth>;

protected:

    std::array<DataT, Size> memory_{};

    uint64_t addr_index_ = 0;
    DataT write_data_{};
    DataT read_data_{};
    bool read_data_valid_ = false;
    bool write_en_ = false;
    bool req_issued_ = false;

public:
    VulBRAM1RW() : write_en_(false) {}

    void req(const AddrType &addr, const DataT &write_data, bool write_en) {
        assert(!req_issued_ && "VulBRAM1RW::req() may only be called once per cycle");
        const uint64_t addr_index = addr.template to<uint64_t>();
        assert(addr_index < Size && "Address out of range");
        addr_index_ = addr_index;
        write_data_ = write_data;
        write_en_ = write_en;
        req_issued_ = true;
    }

    const DataT& readdata() const {
        assert(read_data_valid_ && "VulBRAM1RW::readdata() is only valid in the cycle after a read request");
        return read_data_;
    }

    void apply_next_tick() {
        if (!req_issued_) {
            read_data_valid_ = false;
        } else if (addr_index_ >= Size) {
            read_data_valid_ = false;
        } else if (write_en_) {
            memory_[addr_index_] = write_data_;
            read_data_valid_ = false;
        } else {
            read_data_ = memory_[addr_index_];
            read_data_valid_ = true;
        }
        write_en_ = false;
        req_issued_ = false;
    }
};

template <typename DataT, uint64_t Size, uint32_t ReadPorts, uint32_t WritePorts>
class VulBRAM {
public:

    static_assert(Size > 1, "Size must be greater than 1");
    static constexpr uint32_t AddrWidth = log2ceil(Size);
    static_assert(AddrWidth < 64, "AddrWidth must be less than 64");
    static_assert(ReadPorts > 0, "ReadPorts must be greater than 0");
    static_assert(WritePorts > 0, "WritePorts must be greater than 0");
    static_assert(WritePorts < 64, "WritePorts must be less than 64");

    using AddrType = Int<AddrWidth>;

protected:

    std::array<DataT, Size> memory_{};

    array<uint64_t, ReadPorts> read_address_indices_{};
    array<DataT, ReadPorts> read_data_{};
    array<bool, ReadPorts> read_data_valid_{};
    array<bool, ReadPorts> readreq_issued_{};

    array<uint64_t, WritePorts> write_address_indices_{};
    array<DataT, WritePorts> write_data_{};
    uint64_t write_enables_ = 0; // 每个位对应一个写端口，1表示该端口有效

    template<int I, int N>
    inline void unroll_loop(auto&& f) {
        if constexpr (I < N) {
            f(std::integral_constant<int, I>{});
            unroll_loop<I + 1, N>(f);
        }
    }

public:
    VulBRAM() : write_enables_(0) {}

    template <uint32_t PortIndex>
    void readreq(const AddrType &addr) {
        static_assert(PortIndex < ReadPorts, "Read port index out of range");
        assert(!readreq_issued_[PortIndex] && "VulBRAM::readreq() may only be called once per cycle for each port");
        const uint64_t addr_index = addr.template to<uint64_t>();
        assert(addr_index < Size && "Address out of range");
        read_address_indices_[PortIndex] = addr_index;
        readreq_issued_[PortIndex] = true;
    }

    template <uint32_t PortIndex>
    const DataT& readdata() const {
        static_assert(PortIndex < ReadPorts, "Read port index out of range");
        assert(read_data_valid_[PortIndex] && "VulBRAM::readdata() is only valid in the cycle after a read request on that port");
        return read_data_[PortIndex];
    }

    template <uint32_t PortIndex>
    void write(const AddrType &addr, const DataT &data) {
        static_assert(PortIndex < WritePorts, "Write port index out of range");
        assert((write_enables_ & (1ULL << PortIndex)) == 0 && "VulBRAM::write() may only be called once per cycle for each port");
        const uint64_t addr_index = addr.template to<uint64_t>();
        assert(addr_index < Size && "Address out of range");
        write_address_indices_[PortIndex] = addr_index;
        write_data_[PortIndex] = data;
        write_enables_ |= 1ULL << PortIndex;
    }

    void apply_next_tick() {
        unroll_loop<0, ReadPorts>([&](auto i) {
            if (readreq_issued_[i] && read_address_indices_[i] < Size) {
                read_data_[i] = memory_[read_address_indices_[i]];
                read_data_valid_[i] = true;
            } else {
                read_data_valid_[i] = false;
            }
            readreq_issued_[i] = false;
        });
        if (write_enables_ != 0) {
            unroll_loop<0, WritePorts>([&](auto i) {
                if ((write_enables_ & (1ULL << i)) && write_address_indices_[i] < Size) {
                    memory_[write_address_indices_[i]] = write_data_[i];
                }
            });
            write_enables_ = 0;
        }
    }

protected:

};

template <uint32_t DataWidth, uint64_t Size, uint32_t ReadPorts>
class VulROM {

public:

    static_assert(DataWidth > 0, "DataWidth must be greater than 0");
    static_assert(Size > 1, "Size must be greater than 1");
    static constexpr uint32_t AddrWidth = log2ceil(Size);
    static_assert(AddrWidth < 64, "AddrWidth must be less than 64");
    static_assert(ReadPorts > 0, "ReadPorts must be greater than 0");

    using DataType = Int<DataWidth>;
    using AddrType = Int<AddrWidth>;

protected:

    array<DataType, Size> memory_{};

    array<uint64_t, ReadPorts> read_address_indices_{};
    array<DataType, ReadPorts> read_data_{};
    array<bool, ReadPorts> read_data_valid_{};
    array<bool, ReadPorts> readreq_issued_{};

    template<int I, int N>
    inline void unroll_loop(auto&& f) {
        if constexpr (I < N) {
            f(std::integral_constant<int, I>{});
            unroll_loop<I + 1, N>(f);
        }
    }

public:

    VulROM(const string &readmemh_path) {
        // if (hex) {
            init_from_readmemh(readmemh_path);
        // } else {
        //     init_from_readmemb(path);
        // }
    }

    template <uint32_t PortIndex>
    void readreq(const AddrType &addr) {
        static_assert(PortIndex < ReadPorts, "Read port index out of range");
        assert(!readreq_issued_[PortIndex] && "VulROM::readreq() may only be called once per cycle for each port");
        const uint64_t addr_index = addr.template to<uint64_t>();
        assert(addr_index < Size && "Address out of range");
        read_address_indices_[PortIndex] = addr_index;
        readreq_issued_[PortIndex] = true;
    }

    template <uint32_t PortIndex>
    const DataType readdata() const {
        static_assert(PortIndex < ReadPorts, "Read port index out of range");
        assert(read_data_valid_[PortIndex] && "VulROM::readdata() is only valid in the cycle after a read request on that port");
        return read_data_[PortIndex];
    }

    void apply_next_tick() {
        unroll_loop<0, ReadPorts>([&](auto i) {
            if (readreq_issued_[i] && read_address_indices_[i] < Size) {
                read_data_[i] = memory_[read_address_indices_[i]];
                read_data_valid_[i] = true;
            } else {
                read_data_valid_[i] = false;
            }
            readreq_issued_[i] = false;
        });
    }


protected:

    static inline uint8_t hex_val(char c) {
        if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
        throw std::runtime_error("Invalid hex digit");
    }

    static inline std::string trim(const std::string &s) {
        size_t l = 0, r = s.size();
        while (l < r && std::isspace(static_cast<unsigned char>(s[l]))) l++;
        while (r > l && std::isspace(static_cast<unsigned char>(s[r - 1]))) r--;
        return s.substr(l, r - l);
    }

    static inline std::string strip_comment(const std::string &line) {
        size_t p1 = line.find("//");
        size_t p2 = line.find('#');
        size_t p = std::min(p1 == std::string::npos ? line.size() : p1,
                            p2 == std::string::npos ? line.size() : p2);
        return line.substr(0, p);
    }

    void init_from_readmemh(const std::string &path, bool strict_width = false) {
        std::ifstream fin(path);
        if (!fin) {
            throw std::runtime_error("Cannot open file: " + path);
        }

        constexpr size_t DEPTH = Size;

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
                auto write_chunk = [&](uint32_t lo, const Int<64>& tmp) {
                    if (lo >= DataWidth) {
                        return;
                    }
                    if (lo + 64 <= DataWidth) {
                        d.template pick<64>(lo) = tmp;
                    } else {
                        if constexpr (DataWidth % 64 != 0) {
                            d.template pick<DataWidth % 64>(lo) = Int<DataWidth % 64>(tmp);
                        } else {
                            assert(false && "partial chunk is only valid for non-64-aligned widths");
                        }
                    }
                };

                // 逐 64bit chunk 写入（从低位开始）
                size_t bit_pos = 0;
                uint64_t chunk = 0;
                int nibble_count = 0;

                for (size_t pos = hex.size(); pos > 0; --pos) {
                    uint8_t v = hex_val(hex[pos - 1]);
                    chunk |= static_cast<uint64_t>(v) << (4 * nibble_count);
                    nibble_count++;

                    if (nibble_count == 16) { // 16 hex = 64bit
                        const uint32_t lo = static_cast<uint32_t>(bit_pos);
                        write_chunk(lo, Int<64>(chunk));

                        bit_pos += 64;
                        chunk = 0;
                        nibble_count = 0;
                    }
                }

                // 剩余不足64bit
                if (nibble_count > 0) {
                    const uint32_t lo = static_cast<uint32_t>(bit_pos);
                    write_chunk(lo, Int<64>(chunk));
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

        constexpr size_t DEPTH = Size;

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

                DataType d; // 如需默认清零，请确保 Int 默认构造为 0
                auto write_chunk = [&](uint32_t lo, const Int<64>& tmp) {
                    if (lo >= DataWidth) {
                        return;
                    }
                    if (lo + 64 <= DataWidth) {
                        d.template pick<64>(lo) = tmp;
                    } else {
                        if constexpr (DataWidth % 64 != 0) {
                            d.template pick<DataWidth % 64>(lo) = Int<DataWidth % 64>(tmp);
                        } else {
                            assert(false && "partial chunk is only valid for non-64-aligned widths");
                        }
                    }
                };

                // 从 LSB 开始打包（与 Verilog 一致：右侧为低位）
                // 按 64bit 分块写入 Int
                size_t bit_pos = 0;   // 当前写入到 d 的 bit 位置（LSB 起）
                uint64_t chunk = 0;
                int bit_in_chunk = 0; // 已填入 chunk 的 bit 数

                // 从字符串末尾（LSB）向前处理
                for (size_t pos = bits.size(); pos > 0; --pos) {
                    if (bits[pos - 1] == '1') {
                        chunk |= (uint64_t(1) << bit_in_chunk);
                    }
                    bit_in_chunk++;

                    if (bit_in_chunk == 64) {
                        const uint32_t lo = static_cast<uint32_t>(bit_pos);
                        write_chunk(lo, Int<64>(chunk));
                        bit_pos += 64;
                        chunk = 0;
                        bit_in_chunk = 0;
                    }
                }

                // 处理最后不足 64bit 的部分
                if (bit_in_chunk > 0) {
                    const uint32_t lo = static_cast<uint32_t>(bit_pos);
                    write_chunk(lo, Int<64>(chunk));
                }

                memory_[cur_addr] = d;
                cur_addr++;
            }
        }
    }


};
