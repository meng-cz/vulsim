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

/**
 * The contents of this header file are only used to assist in syntax checking
 * and code completion during coding, and have no connection to the final
 * runtime semantics.
 */

#include <inttypes.h>
#include <uint.hpp>
#include <ram.hpp>
#include <storage.hpp>
#include <queue.hpp>

#include <array>

#define CONST(name, value) constexpr int64_t name = value;

#define PARAMETER(name, value) constexpr int64_t name = value;

#define ALIAS(name, type) using name = type;

#define ALIAS_ARRAY1(name, type, N) using name = std::array<type, N>;

#define ALIAS_ARRAY2(name, type, N1, N2) using name = std::array<std::array<type, N2>, N1>;

#define STRUCT(name) struct name

#define REGISTER(name, type) VulStorageNext<type> name; void __##name##_reset(type& name)

#define REGISTER_MUL(name, type, portnum) VulStorageNext<type, portnum> name; void __##name##_reset(type& name)

#define REGISTER_ARRAY1(name, type, size, portnum) VulStorageNextArray<type, size, portnum> name; void __##name##_reset(type name[size])

#define WIRE(name, type) type name; void __##name##_reset(type& name)

#define ARG(type) const type &

#define RESP(type) type &

#define REQUEST(name, ...) void name (__VA_ARGS__);

#define REQUEST_READY(name, ...) bool name(__VA_ARGS__);

#define SERVICE(name, ...) void __##name (__VA_ARGS__)

#define SERVICE_READY(name, cond, ...) bool __##name(__VA_ARGS__)

#define SERVICE_PRIO(name, priority, ...) void __##name (__VA_ARGS__)

#define SERVICE_PRIO_READY(name, priority, cond, ...) bool __##name(__VA_ARGS__)


#define TICK_IMPL() void tick()

#define CHILD_INSTANCE(module, name, ...) void * name = nullptr;

#define USE_CHILD_SERVICE_PORT(instance, serv, ret, ...) ret instance##_##serv (__VA_ARGS__);

#define CONNECT_CR_CS(srcmod, srcreq, dstmod, dstserv) ;

#define CONNECT_CR_S(srcmod, srcreq, dstserv) ;

#define CONNECT_CR_R(srcmod, srcreq, dstreq) ;

#define CONNECT_S_CS(srcserv, dstmod, dstserv) ;

#define BRAM(name, datatype, addrwidth, readports, writeports) VulBRAM<datatype, addrwidth, readports, writeports> name;

#define BRAM_1RW(name, datatype, addrwidth) VulBRAM1RW<datatype, addrwidth> name;

#define ROM(name, datawidth, addrwidth, readports, init_path) VulROM<datawidth, addrwidth, readports> name(#init_path);

#define QUEUE(name, type, depth) VulQueue<type, depth> name;

#define QUEUE_MP(name, type, depth, enqwidth, deqwidth) VulQueueMP<type, depth, enqwidth, deqwidth> name;
