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

#include <functional>

using std::function;

void logSocketInit(const uint32_t port, const bool listen_global = false);
void logSocketTerminate();

enum class LogSocketLevel {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

void logSocketSetLevel(const LogSocketLevel level);
void logSocketSetLevel(const string & level);

void logSocketSetEcho(function<void(const string &)> echo_func);
void logSocketSetErrorEcho(function<void(const string &)> echo_func);

void logSocketFlush();
void logSocketClearBuffer();

enum class LogSocketCategory {
    General,
    Generation,
    Compilation,
    Simulation
};

void logSocketMessage(const LogSocketLevel level, const LogSocketCategory category, const string &message, const bool newline = true);

inline void logSocketDebug(const LogSocketCategory category, const string &message, const bool newline = true) {
    logSocketMessage(LogSocketLevel::Debug, category, message, newline);
}
inline void logSocketInfo(const LogSocketCategory category, const string &message, const bool newline = true) {
    logSocketMessage(LogSocketLevel::Info, category, message, newline);
}
inline void logSocketWarning(const LogSocketCategory category, const string &message, const bool newline = true) {
    logSocketMessage(LogSocketLevel::Warning, category, message, newline);
}
inline void logSocketError(const LogSocketCategory category, const string &message, const bool newline = true) {
    logSocketMessage(LogSocketLevel::Error, category, message, newline);
}
inline void logSocketCritical(const LogSocketCategory category, const string &message, const bool newline = true) {
    logSocketMessage(LogSocketLevel::Critical, category, message, newline);
}


