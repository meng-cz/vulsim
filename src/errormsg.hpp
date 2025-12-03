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

#include <string>

#include <inttypes.h>

using std::string;

typedef string ErrorMsg;

inline ErrorMsg EStr(uint32_t code, const string &msg) {
    return string("#") + std::to_string(code) + ": " + msg;
}

constexpr uint32_t ErrLoadProject = 10000;

constexpr uint32_t ErrNameCheck = 11000;
constexpr uint32_t ErrTypeCheck = 12000;
constexpr uint32_t ErrReqConnCheck = 13000;
constexpr uint32_t ErrPipeConnCheck = 14000;
constexpr uint32_t ErrSequenceCheck = 15000;
constexpr uint32_t ErrSaveProject = 16000;

constexpr uint32_t ErrConfMod = 20000;
constexpr uint32_t ErrBundleMod = 21000;
constexpr uint32_t ErrCombineMod = 22000;
constexpr uint32_t ErrInstanceMod = 23000;
constexpr uint32_t ErrPipeMod = 24000;
constexpr uint32_t ErrConnMod = 25000;


constexpr uint32_t ErrConfLib = 30000;
constexpr uint32_t EItemConfNameDup = 30000;
constexpr uint32_t EItemConfNameInvalid = 30001;
constexpr uint32_t EItemConfNameNotFound = 30002;
constexpr uint32_t EItemConfGroupNameDup = 30003;
constexpr uint32_t EItemConfGroupNameInvalid = 30004;
constexpr uint32_t EItemConfGroupNameNotFound = 30005;
constexpr uint32_t EItemConfRefNotFound = 30006;
constexpr uint32_t EItemConfRefLooped = 30007;
constexpr uint32_t EItemConfRenameRef = 30008;
constexpr uint32_t EItemConfRemoveRef = 30009;
constexpr uint32_t EItemConfValueEmpty = 30010;
constexpr uint32_t EItemConfValueTokenInvalid = 30011;
constexpr uint32_t EItemConfValueGrammerInvalid = 30012;


