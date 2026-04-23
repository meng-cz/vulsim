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

#include <inttypes.h>
#include <uint.hpp>
#include <ram.hpp>
#include <storage.hpp>

#include <array>

#define CONST(name, value) constexpr int64_t name = value;

#define PARAMETER(name, value) constexpr int64_t name = value;

#define ALIAS(name, type) using name = type;

#define ALIAS_ARRAY1(name, type, N) using name = std::array<type, N>;

#define ALIAS_ARRAY2(name, type, N1, N2) using name = std::array<std::array<type, N2>, N1>;

#define STRUCT(name) struct name

#define REGISTER(name, type) VulStorageNext<type> name; const type & name##_get(); void name##_setnext(const type& next);

#define REGISTER_INIT(name, type, init) VulStorageNext<type> name(init); const type & name##_get(); void name##_setnext(const type& next);

#define REGISTER_MUL(name, type, portnum) VulStorageNext<type, portnum> name; const type & name##_get(); void name##_setnext(const type& next); void name##_setnext(const type& next, int port);

#define REGISTER_ARRAY1(name, type, size, portnum) VulStorageNextArray<type, size, portnum> name; void name##_setnext(const uint64_t idx, const type& next); void name##_setnext(const uint64_t idx, const type& next, int port);

#define WIRE(name, type, init) type name = init;

#define ARG(type) const type &

#define RESP(type) type &

#define REQUEST_PORT(name, ret, ...) ret name (__VA_ARGS__);

#define SERVICE_PORT(name, ret, ...) ret name (__VA_ARGS__);

#define SERVICE_COND_IMPL(name, ...) bool _##name##_cond (__VA_ARGS__)

#define SERVICE_LOGIC_IMPL(name, ...) void _##name##_impl (__VA_ARGS__)

#define TICK_IMPL() void tick()

#define CHILD_INSTANCE(module, name, ...) void * name = nullptr;

#define CHILD_INSTANCE_PRIO(module, name, order, ...) void * name = nullptr;

#define USE_CHILD_PORT(instance, serv, ret, ...) ret instance##_##serv (__VA_ARGS__);

#define CONNECT_CR_CS(srcmod, srcreq, dstmod, dstserv) ;

#define CONNECT_CR_S(srcmod, srcreq, dstserv) ;

#define CONNECT_CR_R(srcmod, srcreq, dstreq) ;

#define CONNECT_S_CS(srcserv, dstmod, dstserv) ;

#define BRAM(name, datawidth, addrwidth, readports, writeports) VulBRAM<datawidth, addrwidth, readports, writeports> name;

#define BRAM_INIT_H(name, datawidth, addrwidth, readports, writeports, path) VulBRAM<datawidth, addrwidth, readports, writeports> name(path, true);

#define BRAM_INIT_B(name, datawidth, addrwidth, readports, writeports, path) VulBRAM<datawidth, addrwidth, readports, writeports> name(path, false);

#define BRAM_1RW(name, datawidth, addrwidth) VulBRAM1RW<datawidth, addrwidth> name;

#define BRAM_1RW_INIT_H(name, datawidth, addrwidth, path) VulBRAM1RW<datawidth, addrwidth> name(path, true);

#define BRAM_1RW_INIT_B(name, datawidth, addrwidth, path) VulBRAM1RW<datawidth, addrwidth> name(path, false);

