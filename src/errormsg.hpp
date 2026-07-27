// Copyright (c) 2025 Meng Chengzhen, in Shandong University
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

