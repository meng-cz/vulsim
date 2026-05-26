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
#include <vector>

#include <inttypes.h>

using std::string;

struct ErrorMsg {
    ErrorMsg() : msg(""), code(0) {}
    ErrorMsg(uint32_t c, const string &m) : msg(m), code(c) {}
    ErrorMsg(const string &m) : msg(m), code(0) {}
    ErrorMsg(uint32_t c, const char *m) : msg(m), code(c) {}
    ErrorMsg(const char *m) : msg(m), code(0) {}

    uint32_t code;
    string msg;

    bool empty() const {
        return msg.empty();
    }
    bool success() const {
        return code == 0;
    }
    bool error() const {
        return code != 0;
    }
    explicit operator bool() const {
        return code != 0;
    }
    string toString() const {
        if (code == 0) {
            return msg;
        }
        return "#" + std::to_string(code) + ": " + msg;
    }
};

inline ErrorMsg EStr(uint32_t code, const string &msg) {
    return {code, msg};
}

inline ErrorMsg operator+(const ErrorMsg &a, const string &b) {
    return {a.code, a.msg + b};
}

inline ErrorMsg &operator+=(ErrorMsg &a, const string &b) {
    a.msg += b;
    return a;
}


using ErrorContextStack = std::vector<string>;

class VulException : public std::exception {
public:
    VulException(const ErrorMsg &err);

    const char *what() const noexcept override {
        return whatStr.c_str();
    }

    const ErrorMsg &getError() const {
        return error;
    }

    const ErrorContextStack &getContext() const {
        return context;
    }

private:
    ErrorMsg error;
    ErrorContextStack context;
    string whatStr;

    string buildWhat() const {
        // [Context1]>[Context2]>...>[ContextN]> Error message
        string res;
        for (const auto &ctx : context) {
            res += "> [" + ctx + "]\n";
        }
        res += ("> " + error.toString());
        return res;
    }
};

class VulErrorContextGuard {
public:
    VulErrorContextGuard(const string &newCtx);
    ~VulErrorContextGuard();

    VulErrorContextGuard(const VulErrorContextGuard &) = delete;
    VulErrorContextGuard &operator=(const VulErrorContextGuard &) = delete;
};

