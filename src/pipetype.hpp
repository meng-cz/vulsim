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

#include "type.h"

typedef string PipeName;

typedef uint64_t PipeSize;

typedef struct {
    PipeName       name;
    DataType       type;
    Comment        comment;
    PipeSize       input_size;
    PipeSize       output_size;
    PipeSize       buffer_size;
    PipeSize       latency;
    bool           has_handshake;
    bool           has_valid; // only for no-handshake pipes
} VulPipe;

enum class PipeImplType {
    Invalid,
    SimpleNoHandshakeNoBuffer,
    SimpleValidNoBuffer,
    SimpleHandshakeNoBuffer,
    SimpleHandshakeBuffer,
    MultiPortHandshakeBuffer,
};

inline PipeImplType determinePipeImplType(const VulPipe &pipe) {
    if (pipe.has_handshake) {
        if (pipe.buffer_size > 0) {
            if (pipe.input_size > 1 && pipe.output_size > 1) {
                return PipeImplType::MultiPortHandshakeBuffer;
            } else {
                return PipeImplType::SimpleHandshakeBuffer;
            }
        } else {
            if (pipe.input_size > 1 && pipe.output_size > 1) {
                return PipeImplType::Invalid;
            } else {
                return PipeImplType::SimpleHandshakeNoBuffer;
            }
        }
    } else {
        if (pipe.input_size > 1 && pipe.output_size > 1) {
            return PipeImplType::Invalid;
        } else if (pipe.has_valid) {
            return PipeImplType::SimpleValidNoBuffer;
        } else {
            return PipeImplType::SimpleNoHandshakeNoBuffer;
        }
    }
}


